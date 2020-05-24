#ifndef __TASK_HPP__
#define __TASK_HPP__

#include <vector>
#include <string>
#include <codec/util.hpp>

namespace Task {
  namespace TaskIndexes {
    static constexpr uint8_t MASK = 0;
  }

  namespace Name {
    static constexpr const char* GENERIC = "Generic";
  }

  using TaskArguments = std::vector<std::string>;

  struct Task {
    int execution_mask;
    std::string datetime;
    bool file;
    std::vector<FileInfo> files;
    std::string envfile;
    std::string execution_flags;
    int id = 0;
    int completed;

    bool validate() {
      return execution_mask > 0 && !datetime.empty() && !envfile.empty() &&
            !execution_flags.empty();
    }
  };

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
  info_v.reserve(file_info.size() /
                 25);  // Estimating number of files being represented
  size_t pipe_pos = 0;
  size_t index = 0;
  size_t delim_pos = 0;
  std::string parsing{file_info, file_info.size()};
  do {
    auto timestamp = file_info.substr(index, 10);
    pipe_pos = findIndexAfter(file_info, index, '|');
    auto file_name = file_info.substr(index + 10, (pipe_pos - index - 10));
    delim_pos = findIndexAfter(file_info, index, ':');
    auto type =
        file_info.substr(index + 10 + file_name.size() + 1,
                         (delim_pos - index - 10 - file_name.size() - 1));
    info_v.push_back(FileInfo{file_name, timestamp});
    index += timestamp.size() + file_name.size() + type.size() +
             3;  // 3 strings + 3 delim chars
  } while (index < file_info.size());
  return info_v;
}

  class TaskHandler {
    public:
      virtual Scheduler::Task prepareTask(TaskArguments argv, std::string uuid) = 0;
  };
}

#endif // __TASK_HPP__
