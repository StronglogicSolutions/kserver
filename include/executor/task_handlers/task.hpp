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

  namespace Constants {
    static constexpr uint8_t FILE_DELIMITER_CHARACTER_COUNT = 2;

    namespace Recurring {
      static constexpr uint8_t NO       = 0x00;
      static constexpr uint8_t HOURLY   = 0x01;
      static constexpr uint8_t DAILY    = 0x02;
      static constexpr uint8_t WEEKLY   = 0x03;
      static constexpr uint8_t MONTHLY  = 0x04;
      static constexpr uint8_t YEARLY   = 0x05;
      static const char* const names[6] = {
        "No",
        "Hourly",
        "Daily",
        "Weekly",
        "Monthly",
        "Yearly"
      };
    } // namespace Recurring
  } // namespace Constants

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
    int recurring;
    bool notify;

    bool validate() {
      return !datetime.empty() && !envfile.empty() &&
            !execution_flags.empty();
    }

    std::string toString() const {
      auto recurring_string = Constants::Recurring::names[recurring];
      std::string return_string{};
      return_string.reserve(75);
      return_string += "ID: " + id;
      return_string += "\nMask: " + std::to_string(execution_mask);
      return_string += "\nTime: " + datetime;
      return_string += "\nFiles: " + std::to_string(files.size());
      return_string += "\nCompleted: " + completed;
      return_string += "\nRecurring: ";
      return_string += "\nEmail notification: " + notify ? "Yes" : "No";
      return_string += recurring_string;
      std::cout << "task string size: " << return_string.size() << std::endl;
      return return_string;
    }

    friend std::ostream &operator<<(std::ostream &out, const Task &task) {
      return out << task.toString();
    }

    friend bool operator==(const Task& t1, const Task& t2);
    friend bool operator!=(const Task& t1, const Task& t2);

    friend bool operator==(const Task& t1, const Task& t2) {
      return (
        t1.completed == t2.completed &&
        t1.datetime == t2.datetime &&
        t1.envfile == t2.envfile &&
        t1.execution_flags == t2.execution_flags &&
        t1.execution_mask == t2.execution_mask &&
        t1.file == t2.file &&
        t1.files.size() == t2.files.size() && // TODO: implement comparison for FileInfo
        t1.id == t2.id &&
        t1.recurring == t2.recurring &&
        t1.notify == t2.notify
      );
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
  info_v.reserve(file_info.size() / 32); // estimate ~ 32 characters for each file's metadata

  uint32_t index      = 0; // index points to beginning of each file's metadata
  uint32_t pipe_pos   = 0; // file name delimiter
  uint32_t delim_pos  = 0; // file metadata delimiter
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

  class TaskHandler {
    public:
      virtual Executor::Task prepareTask(TaskArguments argv, std::string uuid, Task* task = nullptr) = 0;
  };
}

#endif // __TASK_HPP__
