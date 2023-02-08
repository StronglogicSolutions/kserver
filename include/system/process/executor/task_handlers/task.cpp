#include "task.hpp"


#define TIMESTAMP_LENGTH 10

namespace kiq {
std::string GetPostStatus(PlatformPostState state)
{
  switch (state)
  {
    case (PlatformPostState::PROCESSING): return "PROCESSING"; break;
    case (PlatformPostState::SUCCESS   ): return "SUCCESS   "; break;
    case (PlatformPostState::FAILURE   ): return "FAILURE   "; break;
  }
  return "UNKNOWN";
}

std::string AppendExecutionFlag(std::string flag_s, const std::string& flag)
{
  const std::string exec_flag = constants::PARAM_KEY_MAP.at(flag);
  if (!exec_flag.empty())
    flag_s += ' ' + exec_flag + "=$" + flag;

  return flag_s;
}

std::string AsExecutionFlag(const std::string& flag, const std::string& prefix)
{
  const std::string exec_flag = constants::PARAM_KEY_MAP.at(flag);
  if (!exec_flag.empty())
    return prefix + exec_flag + "=$" + flag;
  return "";
}

static int findIndexAfter(std::string s, int pos, char c)
{
  for (uint8_t i = pos; i < s.size(); i++)
    if (s.at(i) == c)
      return i;
  return -1;
}

  /**
 * parseFileInfo
 *
 * Deduces information about a files sent by a client using KY_GUI
 *
 * @param[in] {std::string} `file_info` The information string
 * @returns {std::vector<FileInfo>} A vector of FileInfo objects
 *
 */
std::vector<FileInfo> parseFileInfo(std::string file_info) {
  std::vector<FileInfo> info_v{};
  info_v.reserve(file_info.size() / 32); // estimate ~ 32 characters for each file's metadata

  uint32_t index     = 0; // index points to beginning of each file's metadata
  uint32_t pipe_pos  = 0; // file name delimiter
  uint32_t delim_pos = 0; // file metadata delimiter
  do {
    auto timestamp = file_info.substr(index, TIMESTAMP_LENGTH);
    pipe_pos = findIndexAfter(file_info, index, '|');

    auto file_name = file_info.substr(
      index + TIMESTAMP_LENGTH,
      (pipe_pos - index - TIMESTAMP_LENGTH)
    );

    delim_pos = findIndexAfter(file_info, index, ':');

    auto type = file_info.substr(
      index + TIMESTAMP_LENGTH + file_name.size() + 1,
      (delim_pos - index - TIMESTAMP_LENGTH - file_name.size() - 1)
    );

    info_v.push_back(FileInfo{file_name, timestamp}); // add metadata to vector
    index += timestamp.size() + // move index
             file_name.size() +
             type.size() +
             Constants::FILE_DELIMITER_CHARACTER_COUNT;
  } while (index < file_info.size());
  return info_v;
}
} // ns kiq
