/**
 * @file has_seed_task_generator.h
 * @brief Process generator for checking if the request already has a seed
 *
 * @author Levi Armstrong
 * @date November 2. 2021
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2021, Southwest Research Institute
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
#ifndef TESSERACT_PROCESS_MANAGERS_HAS_SEED_TASK_GENERATOR_H
#define TESSERACT_PROCESS_MANAGERS_HAS_SEED_TASK_GENERATOR_H

#include <tesseract_process_managers/core/task_generator.h>

namespace tesseract_planning
{
class HasSeedTaskGenerator : public TaskGenerator
{
public:
  using UPtr = std::unique_ptr<HasSeedTaskGenerator>;

  HasSeedTaskGenerator(std::string name = "Has Seed");

  ~HasSeedTaskGenerator() override = default;
  HasSeedTaskGenerator(const HasSeedTaskGenerator&) = delete;
  HasSeedTaskGenerator& operator=(const HasSeedTaskGenerator&) = delete;
  HasSeedTaskGenerator(HasSeedTaskGenerator&&) = delete;
  HasSeedTaskGenerator& operator=(HasSeedTaskGenerator&&) = delete;

  int conditionalProcess(TaskInput input, std::size_t unique_id) const override;

  void process(TaskInput input, std::size_t unique_id) const override;
};

class HasSeedTaskInfo : public TaskInfo
{
public:
  using Ptr = std::shared_ptr<HasSeedTaskInfo>;
  using ConstPtr = std::shared_ptr<const HasSeedTaskInfo>;

  HasSeedTaskInfo(std::size_t unique_id, std::string name = "Has Seed");
};
}  // namespace tesseract_planning

#endif  // TESSERACT_PROCESS_MANAGERS_HAS_SEED_TASK_GENERATOR_H
