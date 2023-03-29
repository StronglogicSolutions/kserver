#include "instagram.hpp"

namespace kiq {
/**
 * @brief
 *
 * @param argv
 * @param uuid
 * @param task_ptr
 * @return Task
 */
Task IGTaskHandler::prepareTask(const std::vector<std::string>& argv,
                                const std::string&              uuid,
                                Task*                           task_ptr)
{
  if (!FileUtils::CreateTaskDirectory(uuid))
    return Task{};

  auto mask                = argv.at(IGTaskIndex::MASK);
  auto file_info           = argv.at(IGTaskIndex::FILEINFO);
  auto datetime            = argv.at(IGTaskIndex::DATETIME);
  auto description         = argv.at(IGTaskIndex::DESCRIPTION);
  auto hashtags            = argv.at(IGTaskIndex::HASHTAGS);
  auto requested_by        = argv.at(IGTaskIndex::REQUESTED_BY);
  auto requested_by_phrase = argv.at(IGTaskIndex::REQUESTED_BY_PHRASE);
  auto promote_share       = argv.at(IGTaskIndex::PROMOTE_SHARE);
  auto link_bio            = argv.at(IGTaskIndex::LINK_BIO);
  auto is_video            = argv.at(IGTaskIndex::IS_VIDEO) == "1";
  auto header              = argv.at(IGTaskIndex::HEADER);
  auto user                = argv.at(IGTaskIndex::USER);

  std::vector<FileInfo> task_files = parseFileInfo(file_info);

  std::string media_filename = GetExecutableCWD();

  for (uint8_t i = 0; i < task_files.size(); i++)
    task_files.at(i).first = media_filename + "/data/" + uuid + "/" + task_files.at(i).first;

  std::string env_file_string{"#!/usr/bin/env bash\n"};
  env_file_string += "HEADER=\""              + header              + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "DESCRIPTION=\""         + description         + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "HASHTAGS=\""            + hashtags            + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "REQUESTED_BY=\""        + requested_by        + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "REQUESTED_BY_PHRASE=\"" + requested_by_phrase + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "PROMOTE_SHARE=\""       + promote_share       + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "LINK_BIO=\""            + link_bio            + "\"" + ARGUMENT_SEPARATOR + "\n";
  env_file_string += "FILE_TYPE=\"";
  env_file_string += (is_video) ? "video\"\x1f\n" : "image\"\x1f\n";
  env_file_string += "USER=\""                + user                + "\"" + ARGUMENT_SEPARATOR + "\n";

  std::string env_filename = FileUtils::SaveEnvFile(env_file_string, uuid);

  if (!task_ptr)
    return Task{
      .mask = std::stoi(mask),
      .datetime = datetime,
      .file = (!task_files.empty()),
      .files = task_files,
      .env = env_filename,
      .flags = // TODO: this should come from the database
        "--description=$DESCRIPTION --hashtags=$HASHTAGS "
        "--requested_by=$REQUESTED_BY --media=$FILE_TYPE "
        "--requested_by_phrase=$REQUESTED_BY_PHRASE "
        "--promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO "
        "--header=$HEADER --user=$USER"};

  task_ptr->mask = std::stoi(mask);
  task_ptr->datetime = datetime;
  task_ptr->file = (!task_files.empty());
  task_ptr->files = task_files;
  task_ptr->env = env_filename;
  task_ptr->flags =
    "--description=$DESCRIPTION --hashtags=$HASHTAGS "
    "--requested_by=$REQUESTED_BY --media=$FILE_TYPE "
    "--requested_by_phrase=$REQUESTED_BY_PHRASE "
    "--promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO "
    "--header=$HEADER --user=$USER";

  return *task_ptr;
}
} // ns kiq