#include "task.hpp"


#define TIMESTAMP_LENGTH 10


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
