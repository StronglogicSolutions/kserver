#ifndef __INSTAGRAM_HPP__
#define __INSTAGRAM_HPP__

#include <vector>
#include <executor/scheduler.hpp>
#include <codec/util.hpp>
#include <iostream>

namespace Task {

  class IGTaskHandler {

    public:

    static Executor::Task prepareTask(std::vector<std::string> argv, std::string uuid) {

      if (!FileUtils::createTaskDirectory(uuid)) {
        std::cout << "UNABLE TO CREATE TASK DIRECTORY! Returning empty task" << std::endl;
        return Executor::Task{};
      }

      auto file_info = argv.at(0);

      std::vector<FileInfo> task_files = FileUtils::parseFileInfo(file_info);

      std::string media_filename = get_executable_cwd();
      for (int i = 0; i < task_files.size(); i++) {
        std::cout << "Filename returned: " << task_files.at(i).first << std::endl;
        task_files.at(i).first = media_filename + "/data/" + uuid + "/" + task_files.at(i).first;
      }
      // notify KServer of filename received
      auto datetime = argv.at(1);
      auto description = argv.at(2);
      auto hashtags = argv.at(3);
      auto requested_by = argv.at(4);
      auto requested_by_phrase = argv.at(5);
      auto promote_share = argv.at(6);
      auto link_bio = argv.at(7);
      auto is_video = argv.at(8) == "1";
      auto mask = argv.at(9);
      auto header = argv.at(10);

      std::string env_file_string{"#!/usr/bin/env bash\n"};
      env_file_string += "HEADER='" + header + "'\n";
      env_file_string += "DESCRIPTION='" + description + "'\n";
      env_file_string += "HASHTAGS='" + hashtags + "'\n";
      env_file_string += "REQUESTED_BY='" + requested_by + "'\n";
      env_file_string +=
          "REQUESTED_BY_PHRASE='" + requested_by_phrase + "'\n";
      env_file_string += "PROMOTE_SHARE='" + promote_share + "'\n";
      env_file_string += "LINK_BIO='" + link_bio + "'\n";
      env_file_string += "FILE_TYPE='";
      env_file_string += is_video ? "video'\n" : "image'\n";

      std::string env_filename = FileUtils::saveEnvFile(env_file_string, uuid);

      return Executor::Task{
        .execution_mask = std::stoi(mask),
        .datetime = datetime,
        .file = (!task_files.empty()),
        .files = task_files,
        .envfile = env_filename,
        .execution_flags =
            "--description=$DESCRIPTION --hashtags=$HASHTAGS "
            "--requested_by=$REQUESTED_BY --media=$FILE_TYPE "
            "--requested_by_phrase=$REQUESTED_BY_PHRASE "
            "--promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO "
            "--header=$HEADER"
      };
    }
  };
}

#endif // __INSTAGRAM_HPP__
