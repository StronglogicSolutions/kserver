#ifndef __GENERIC_HPP__
#define __GENERIC_HPP__

#include <iostream>
#include <vector>

#include <system/process/scheduler.hpp>

#include "task.hpp"

namespace Executor {
  /**
   * GenericTaskIndex
   *
   * These indices describe the order of arguments expected for processing of an IGTask
   */
  namespace GenericTaskIndex {
    static const uint8_t MASK        = TaskIndexes::MASK;
    static const uint8_t FILEINFO    = 1;
    static const uint8_t DATETIME    = 2;
    static const uint8_t DESCRIPTION = 3;
    static const uint8_t IS_VIDEO    = 4;
    static const uint8_t HEADER      = 5;
    static const uint8_t USER        = 6;
    static const uint8_t RECURRING   = 7;
    static const uint8_t NOTIFY      = 8;
    static const uint8_t RUNTIME     = 9;
  }

  const std::string GENERIC_TASK_EXECUTION_FLAGS{"--description=$DESCRIPTION "\
                                                 "--media=$FILE_TYPE "\
                                                 "--header=$HEADER --user=$USER"};

class GenericTaskHandler : public TaskHandler {
 public:
  virtual Executor::Task prepareTask(std::vector<std::string> argv,
                                     std::string uuid, Task* task_ptr = nullptr) override {
    if (!FileUtils::createTaskDirectory(uuid)) {
      std::cout << "UNABLE TO CREATE TASK DIRECTORY! Returning empty task"
                << std::endl;
      return Executor::Task{};
    }

    auto mask         = argv.at(GenericTaskIndex::MASK);
    auto file_info    = argv.at(GenericTaskIndex::FILEINFO);
    auto is_video     = argv.at(GenericTaskIndex::IS_VIDEO) == "1";
    auto datetime     = argv.at(GenericTaskIndex::DATETIME);
    auto description  = argv.at(GenericTaskIndex::DESCRIPTION);
    auto header       = argv.at(GenericTaskIndex::HEADER);
    auto user         = argv.at(GenericTaskIndex::USER);
    auto recurring    = argv.at(GenericTaskIndex::RECURRING);
    auto notify       = argv.at(GenericTaskIndex::NOTIFY);
    auto has_files    = !file_info.empty();
    auto runtime_args = argv.at(GenericTaskIndex::RUNTIME);

    std::vector<FileInfo> task_files;

    if (has_files) {
      task_files = parseFileInfo(file_info);
      std::string media_filename = get_executable_cwd();
      for (uint8_t i = 0; i < task_files.size(); i++) {
        task_files.at(i).first =
          media_filename + "/data/" + uuid + "/" + task_files.at(i).first;
      }
    }

    std::string                env_file_string              {"#!/usr/bin/env bash\n"};
    if (!header.empty())       env_file_string +=            "HEADER='" + header + "'\n";
    if (!description.empty())  env_file_string +=            "DESCRIPTION='" + description + "'\n";
    if (!user.empty())         env_file_string +=            "USER='" + user + "'\n";
    if (!runtime_args.empty()) env_file_string +=            "R_ARGS='" + runtime_args + "'\n";
    if (has_files) {
                               env_file_string +=            "FILE_TYPE='" + (is_video) ?
                                                               "video'\n" :
                                                               "image'\n";
    }

    std::string env_filename = FileUtils::saveEnvFile(env_file_string, uuid);

    if (task_ptr == nullptr) {
      return Executor::Task{
        .execution_mask  = std::stoi(mask),
        .datetime        = datetime,
        .file            = (!task_files.empty()),
        .files           = task_files,
        .envfile         = env_filename,
        .execution_flags = GENERIC_TASK_EXECUTION_FLAGS,
        .id              = 0,
        .completed       = 0,
        .recurring       = std::stoi(recurring),
        .notify          = notify.compare("1") == 0
      };
    } else {
      task_ptr->execution_mask  = std::stoi(mask);
      task_ptr->datetime        = datetime;
      task_ptr->file            = (!task_files.empty());
      task_ptr->files           = task_files;
      task_ptr->envfile         = env_filename;
      task_ptr->execution_flags = GENERIC_TASK_EXECUTION_FLAGS,
      task_ptr->recurring       = std::stoi(recurring);
      task_ptr->notify          = notify.compare("1") == 0;
      return *task_ptr;
    }
  }
};
}  // namespace Task

#endif  // __GENERIC_HPP__
