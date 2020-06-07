#ifndef __TASK_HPP__
#define __TASK_HPP__

#include <vector>
#include <string>
#include <codec/util.hpp>

namespace Executor {
  #define TIMESTAMP_LENGTH 10
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
      return !datetime.empty() && !envfile.empty() &&
            !execution_flags.empty();
    }

    friend std::ostream &operator<<(std::ostream &out, const Task &task) {
      auto file_string = task.file ? std::string{"Yes - " + task.files.size()} : "No";
      out << "ID: " << task.id << "\nMask: " << task.execution_mask << "\nTime: " << task.datetime
          << "\nFiles: " << file_string << "\nCompleted: " << task.completed << std::endl;
      return out;
    }

    friend bool operator==(const Task& t1, const Task& t2);
    friend bool operator!=(const Task& t1, const Task& t2);

    friend bool operator==(const Task& t1, const Task& t2) {
      return (t1.completed == t2.completed &&
      t1.datetime == t2.datetime &&
      t1.envfile == t2.envfile &&
      t1.execution_flags == t2.execution_flags &&
      t1.execution_mask == t2.execution_mask &&
      t1.file == t2.file &&
      t1.files.size() == t2.files.size() && // TODO: implement comparison for FileInfo
      t1.id == t2.id);
    }

    friend bool operator!=(const Task& t1,const Task& t2) {
      return !(t1 == t2);
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
                 32);
  size_t pipe_pos = 0;
  size_t index = 0;
  size_t delim_pos = 0;
  std::string parsing{file_info, file_info.size()};
  do {
    auto timestamp = file_info.substr(index, TIMESTAMP_LENGTH);
    pipe_pos = findIndexAfter(file_info, index, '|');
    auto file_name = file_info.substr(index + TIMESTAMP_LENGTH, (pipe_pos - index - TIMESTAMP_LENGTH));
    delim_pos = findIndexAfter(file_info, index, ':');
    auto type =
        file_info.substr(index + TIMESTAMP_LENGTH + file_name.size() + 1,
                         (delim_pos - index - TIMESTAMP_LENGTH - file_name.size() - 1));
    info_v.push_back(FileInfo{file_name, timestamp});
    index += timestamp.size() + file_name.size() + type.size() +
             3;  // 3 strings + 3 delim chars
  } while (index < file_info.size());
  return info_v;
}

  class TaskHandler {
    public:
      virtual Executor::Task prepareTask(TaskArguments argv, std::string uuid, Task* task = nullptr) = 0;

  };
}

#endif // __TASK_HPP__
