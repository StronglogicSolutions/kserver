#include "generic.hpp"
#include <logger.hpp>

namespace kiq {
using namespace kiq::log;

template <typename T>
Task GenericTaskHandler::Create(const T&                        app_mask,
                                const std::string&              description,
                                const std::string&              header     ,
                                const std::string&              user       ,
                                const std::vector<std::string>& args       )
{
  std::string mask;
  if constexpr (std::is_integral<T>::value)
    mask = std::to_string(app_mask);
  else
    mask = app_mask;
  const std::string username = (user.empty()) ? config::System::admin() : user;
        Task task{};
  std::vector<std::string> argv{
    mask,                       // 0 mask
    "",                         // 1 fileinfo
    TimeUtils::Now(),           // 2 datetime
    description,                // 3 description
    "false",                    // 4 is video
    header,                     // 5 header
    username,                   // 6 user
    "0",                        // 7 recurring
    "0",                        // 8 notify
    StringUtils::Tokenize(args) // 9 runtime arguments
  };
  GenericTaskHandler handler{};
  handler.prepareTask(argv, StringUtils::GenerateUUIDString(), &task);
  return task;
}

template
Task GenericTaskHandler::Create(const int32_t&                  app_mask,
                                const std::string&              description,
                                const std::string&              header     ,
                                const std::string&              user       ,
                                const std::vector<std::string>& args       );
template
Task GenericTaskHandler::Create(const std::string&              app_mask,
                                const std::string&              description,
                                const std::string&              header     ,
                                const std::string&              user       ,
                                const std::vector<std::string>& args       );

Task GenericTaskHandler::prepareTask(const std::vector<std::string>& argv,
                                     const std::string&              uuid,
                                     Task*                           task_ptr)
{
  if (!FileUtils::CreateTaskDirectory(uuid))
  {
    klog().e("UNABLE TO CREATE TASK DIRECTORY! Returning empty task");
    return Task{};
  }

  auto mask         = argv.at(GenericTaskIndex::MASK);
  auto file_info    = argv.at(GenericTaskIndex::FILEINFO);
  auto datetime     = argv.at(GenericTaskIndex::DATETIME);
  auto description  = argv.at(GenericTaskIndex::DESCRIPTION);
  auto is_video     = argv.at(GenericTaskIndex::IS_VIDEO) == "1";
  auto header       = argv.at(GenericTaskIndex::HEADER);
  auto user         = argv.at(GenericTaskIndex::USER);
  auto recurring    = argv.at(GenericTaskIndex::RECURRING);
  auto notify       = argv.at(GenericTaskIndex::NOTIFY);
  auto runtime_args = argv.at(GenericTaskIndex::RUNTIME);
  auto has_files    = !file_info.empty();

  std::vector<FileInfo> task_files;

  if (has_files)
  {
                task_files     = parseFileInfo(file_info);
    std::string media_filename = GetExecutableCWD();

    for (uint8_t i = 0; i < task_files.size(); i++)
      task_files.at(i).first = media_filename + "/data/" + uuid + "/" + task_files.at(i).first;
  }

  std::string                env_file_string   {"#!/usr/bin/env bash\n"};
  if (!header.empty())       env_file_string += "HEADER=\"" + header + "\""           + ARGUMENT_SEPARATOR + "\n";
  if (!description.empty())  env_file_string += "DESCRIPTION=\"" + description + "\"" + ARGUMENT_SEPARATOR + "\n";
  if (!user.empty())         env_file_string += "USER=\"" + user + "\""               + ARGUMENT_SEPARATOR + "\n";
  if (!runtime_args.empty()) env_file_string += "R_ARGS=\"" + runtime_args + "\""     + ARGUMENT_SEPARATOR + "\n";
  if (has_files)
  {
    if (is_video)
    {
      env_file_string += "FILE_TYPE=\"video\"\x1f\n";
    }
    else
    {
      env_file_string += "FILE_TYPE=\"image\"\x1f\n";
    }
  }

  std::string env_filename = FileUtils::SaveEnvFile(env_file_string, uuid);

  if (!task_ptr)
  {
    return Task{
      .mask  = std::stoi(mask),
      .datetime        = datetime,
      .file            = (!task_files.empty()),
      .files           = task_files,
      .env         = env_filename,
      .flags = GENERIC_TASK_EXECUTION_FLAGS,
      .task_id         = 0,
      .completed       = 0,
      .recurring       = std::stoi(recurring),
      .notify          = notify.compare("1") == 0
    };
  }
  else
  {
    task_ptr->mask  = std::stoi(mask);
    task_ptr->datetime        = datetime;
    task_ptr->file            = (!task_files.empty());
    task_ptr->files           = task_files;
    task_ptr->env         = env_filename;
    task_ptr->flags = GENERIC_TASK_EXECUTION_FLAGS,
    task_ptr->recurring       = std::stoi(recurring);
    task_ptr->notify          = notify.compare("1") == 0;

    return *task_ptr;
  }
}
} // ns kiq
