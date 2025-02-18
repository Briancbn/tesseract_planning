﻿/**
 * @file raster_only_global_taskflow.cpp
 * @brief Plans raster paths
 *
 * @author Matthew Powelson
 * @date July 15, 2020
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2020, Southwest Research Institute
 *
 * @par License
 * Software License Agreement (Apache License)
 * @par
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * @par
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <tesseract_common/macros.h>
TESSERACT_COMMON_IGNORE_WARNINGS_PUSH
#include <functional>
#include <taskflow/taskflow.hpp>
TESSERACT_COMMON_IGNORE_WARNINGS_POP

#include <tesseract_process_managers/core/utils.h>
#include <tesseract_process_managers/taskflow_generators/raster_only_global_taskflow.h>

#include <tesseract_command_language/instruction_type.h>
#include <tesseract_command_language/composite_instruction.h>
#include <tesseract_command_language/plan_instruction.h>
#include <tesseract_command_language/utils/get_instruction_utils.h>

#include <tesseract_common/utils.h>

using namespace tesseract_planning;

RasterOnlyGlobalTaskflow::RasterOnlyGlobalTaskflow(TaskflowGenerator::UPtr global_taskflow_generator,
                                                   TaskflowGenerator::UPtr transition_taskflow_generator,
                                                   TaskflowGenerator::UPtr raster_taskflow_generator,
                                                   std::string name)
  : global_taskflow_generator_(std::move(global_taskflow_generator))
  , transition_taskflow_generator_(std::move(transition_taskflow_generator))
  , raster_taskflow_generator_(std::move(raster_taskflow_generator))
  , name_(std::move(name))
{
}

const std::string& RasterOnlyGlobalTaskflow::getName() const { return name_; }

TaskflowContainer RasterOnlyGlobalTaskflow::generateTaskflow(TaskInput input,
                                                             TaskflowVoidFn done_cb,
                                                             TaskflowVoidFn error_cb)
{
  // This should make all of the isComposite checks so that you can safely cast below
  if (!checkTaskInput(input))
  {
    CONSOLE_BRIDGE_logError("Invalid Process Input");
    throw std::runtime_error("Invalid Process Input");
  }

  TaskflowContainer container;
  container.taskflow = std::make_unique<tf::Taskflow>(name_);
  std::vector<tf::Task> tasks;

  const Instruction* input_instruction = input.getInstruction();
  TaskflowContainer sub_container = global_taskflow_generator_->generateTaskflow(
      input,
      [=]() { successTask(input, name_, input_instruction->getDescription(), done_cb); },
      [=]() { failureTask(input, name_, input_instruction->getDescription(), error_cb); });

  container.input = container.taskflow->composed_of(*(sub_container.taskflow)).name("global");
  container.containers.push_back(std::move(sub_container));

  auto global_post_task =
      container.taskflow->emplace([input]() { globalPostProcess(input); }).name("global post process");
  container.input.precede(global_post_task);

  // Generate all of the raster tasks. They don't depend on anything
  std::size_t raster_idx = 0;
  for (std::size_t idx = 0; idx < input.size(); idx += 2)
  {
    TaskInput raster_input = input[idx];
    if (idx == 0)
      raster_input.setStartInstruction(input_instruction->as<CompositeInstruction>().getStartInstruction());
    else
      raster_input.setStartInstruction(std::vector<std::size_t>({ idx - 1 }));

    if (idx < (input.size() - 1))
      raster_input.setEndInstruction(std::vector<std::size_t>({ idx + 1 }));

    TaskflowContainer sub_container = raster_taskflow_generator_->generateTaskflow(
        raster_input,
        [=]() { successTask(input, name_, raster_input.getInstruction()->getDescription(), done_cb); },
        [=]() { failureTask(input, name_, raster_input.getInstruction()->getDescription(), error_cb); });

    auto raster_step =
        container.taskflow->composed_of(*(sub_container.taskflow))
            .name("Raster #" + std::to_string(raster_idx + 1) + ": " + raster_input.getInstruction()->getDescription());
    container.containers.push_back(std::move(sub_container));
    global_post_task.precede(raster_step);
    tasks.push_back(raster_step);
    raster_idx++;
  }

  // Loop over all transitions
  std::size_t transition_idx = 0;
  for (std::size_t input_idx = 1; input_idx < input.size() - 1; input_idx += 2)
  {
    // This use to extract the start and end, but things were changed so the seed is generated as part of the
    // taskflow. So the seed is only a skeleton and does not contain move instructions. So instead we provide the
    // composite and let the generateTaskflow extract the start and end waypoint from the composite. This is also more
    // robust because planners could modify composite size, which is rare but does happen when using OMPL where it is
    // not possible to simplify the trajectory to the desired number of states.
    TaskInput transition_input = input[input_idx];
    transition_input.setStartInstruction(std::vector<std::size_t>({ input_idx - 1 }));
    transition_input.setEndInstruction(std::vector<std::size_t>({ input_idx + 1 }));
    TaskflowContainer sub_container = transition_taskflow_generator_->generateTaskflow(
        transition_input,
        [=]() { successTask(input, name_, transition_input.getInstruction()->getDescription(), done_cb); },
        [=]() { failureTask(input, name_, transition_input.getInstruction()->getDescription(), error_cb); });

    auto transition_step = container.taskflow->composed_of(*(sub_container.taskflow))
                               .name("Transition #" + std::to_string(transition_idx + 1) + ": " +
                                     transition_input.getInstruction()->getDescription());
    container.containers.push_back(std::move(sub_container));

    // Each transition is independent and thus depends only on the adjacent rasters
    transition_step.succeed(tasks[transition_idx]);
    transition_step.succeed(tasks[transition_idx + 1]);

    transition_idx++;
  }

  return container;
}

void RasterOnlyGlobalTaskflow::globalPostProcess(TaskInput input)
{
  if (input.isAborted())
    return;

  auto& results = input.getResults()->as<CompositeInstruction>();
  auto& composite = results.at(0).as<CompositeInstruction>();
  composite.setStartInstruction(results.getStartInstruction());
  composite.setManipulatorInfo(results.getManipulatorInfo());
  for (std::size_t i = 1; i < results.size(); ++i)
  {
    auto& composite0 = results.at(i - 1).as<CompositeInstruction>();
    MoveInstruction lmi = *getLastMoveInstruction(composite0);
    lmi.setMoveType(MoveInstructionType::START);

    auto& composite1 = results.at(i).as<CompositeInstruction>();
    composite1.setStartInstruction(lmi);
    composite1.setManipulatorInfo(results.getManipulatorInfo());
  }
}

bool RasterOnlyGlobalTaskflow::checkTaskInput(const tesseract_planning::TaskInput& input)
{
  // -------------
  // Check Input
  // -------------
  if (!input.env)
  {
    CONSOLE_BRIDGE_logError("TaskInput env is a nullptr");
    return false;
  }

  // Check the overall input
  const Instruction* input_instruction = input.getInstruction();
  if (!isCompositeInstruction(*input_instruction))
  {
    CONSOLE_BRIDGE_logError("TaskInput Invalid: input.instructions should be a composite");
    return false;
  }
  const auto& composite = input_instruction->as<CompositeInstruction>();

  // Check that it has a start instruction
  if (!composite.hasStartInstruction() && isNullInstruction(input.getStartInstruction()))
  {
    CONSOLE_BRIDGE_logError("TaskInput Invalid: input.instructions should have a start instruction");
    return false;
  }

  // Check rasters and transitions
  for (const auto& c : composite)
  {
    // Both rasters and transitions should be a composite
    if (!isCompositeInstruction(c))
    {
      CONSOLE_BRIDGE_logError("TaskInput Invalid: Both rasters and transitions should be a composite");
      return false;
    }
  }

  return true;
}
