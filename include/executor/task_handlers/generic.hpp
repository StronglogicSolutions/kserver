#ifndef __GENERIC_HPP__
#define __GENERIC_HPP__

#include <codec/util.hpp>
#include <executor/task_handlers/task.hpp>
#include <executor/scheduler.hpp>
#include <iostream>
#include <vector>

namespace Executor {
  /**
   * GenericTaskIndex
   *
   * These indices describe the order of arguments expected for processing of an IGTask
   */
  namespace GenericTaskIndex {
    static constexpr uint8_t MASK = TaskIndexes::MASK;
    static constexpr uint8_t FILEINFO = 1;
    static constexpr uint8_t DATETIME = 2;
    static constexpr uint8_t DESCRIPTION = 3;
    static constexpr uint8_t IS_VIDEO = 4;
    static constexpr uint8_t HEADER = 5;
    static constexpr uint8_t USER = 6;
  }

class GenericTaskHandler : public TaskHandler {
 public:
  virtual Executor::Task prepareTask(std::vector<std::string> argv,
                                     std::string uuid, Task* task_ptr = nullptr) override {
    if (!FileUtils::createTestTaskDirectory(uuid)) {
      std::cout << "UNABLE TO CREATE TASK DIRECTORY! Returning empty task"
                << std::endl;
      return Executor::Task{};
    }

    auto mask = argv.at(GenericTaskIndex::MASK);
    auto file_info = argv.at(GenericTaskIndex::FILEINFO);
    auto is_video = argv.at(GenericTaskIndex::IS_VIDEO) == "1";
    auto datetime = argv.at(GenericTaskIndex::DATETIME);
    auto description = argv.at(GenericTaskIndex::DESCRIPTION);
    auto header = argv.at(GenericTaskIndex::HEADER);
    auto user = argv.at(GenericTaskIndex::USER);

    std::vector<FileInfo> task_files = parseFileInfo(file_info);

    std::string media_filename = get_executable_cwd();
    for (int i = 0; i < task_files.size(); i++) {
      task_files.at(i).first =
          media_filename + "/data/" + uuid + "/" + task_files.at(i).first;
    }

    std::string env_file_string{"#!/usr/bin/env bash\n"};
    env_file_string += "HEADER='" + header + "'\n";
    env_file_string += "DESCRIPTION='" + description + "'\n";
    env_file_string += "FILE_TYPE='";
    env_file_string += is_video ? "video'\n" : "image'\n";
    env_file_string += "USER='" + user + "'\n";

    std::string env_filename = FileUtils::saveEnvFile(env_file_string, uuid);
    if (task_ptr == nullptr) {
      return Executor::Task{
        .execution_mask = std::stoi(mask),
        .datetime = datetime,
        .file = (!task_files.empty()),
        .files = task_files,
        .envfile = env_filename,
        .execution_flags =
            "--description=$DESCRIPTION "
            "--media=$FILE_TYPE "
            "--header=$HEADER --user=$USER"};
    } else {
      task_ptr->execution_mask = std::stoi(mask);
      task_ptr->datetime = datetime;
      task_ptr->file = (!task_files.empty());
      task_ptr->files = task_files;
      task_ptr->envfile = env_filename;
      task_ptr->execution_flags =
            "--description=$DESCRIPTION --hashtags=$HASHTAGS "
            "--requested_by=$REQUESTED_BY --media=$FILE_TYPE "
            "--requested_by_phrase=$REQUESTED_BY_PHRASE "
            "--promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO "
            "--header=$HEADER --user=$USER";
      return *task_ptr;
    }
  }
};
}  // namespace Task

#endif  // __GENERIC_HPP__
