#include "generic.hpp"

Task GenericTaskHandler::prepareTask(std::vector<std::string> argv,
                                     std::string uuid, Task* task_ptr) {
  if (!FileUtils::createTaskDirectory(uuid)) {
    std::cout << "UNABLE TO CREATE TASK DIRECTORY! Returning empty task"
              << std::endl;
    return Task{};
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
    return Task{
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