#ifndef __TASK_HPP__
#define __TASK_HPP__

#include <vector>
#include <string>

#include "common/util.hpp"

#define TIMESTAMP_LENGTH 10

const uint32_t NO_APP_MASK = std::numeric_limits<uint32_t>::max();
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

namespace Field {
  static const char* const MASK      = "schedule.mask";
  static const char* const FLAGS     = "schedule.flags";
  static const char* const ENVFILE   = "schedule.envfile";
  static const char* const TIME      = "schedule.time";
  static const char* const REC_TIME  = "recurring.time";
  static const char* const ID        = "schedule.id";
  static const char* const COMPLETED = "schedule.completed";
  static const char* const RECURRING = "schedule.recurring";
  static const char* const NOTIFY    = "schedule.notify";
  static const char* const RUNTIME   = "schedule.runtime";
} // namespace Field

using TaskArguments = std::vector<std::string>;

struct Task {
  int                      execution_mask;
  std::string              datetime;
  bool                     file;
  std::vector<FileInfo>    files;
  std::string              envfile;
  std::string              execution_flags;
  int                      id = 0;
  int                      completed;
  int                      recurring;
  bool                     notify;
  std::string              runtime;
  std::vector<std::string> filenames;

  bool validate() {
    return !datetime.empty() && !envfile.empty() &&
          !execution_flags.empty();
  }

  std::string toString() const {
    std::string return_string{};
    return_string.reserve(100);
    return_string += "ID: " + std::to_string(id);
    return_string += "\nMask: " + std::to_string(execution_mask);
    return_string += "\nTime: " + datetime;
    return_string += "\nFiles: " + std::to_string(files.size());
    return_string += "\nCompleted: " + std::to_string(completed);
    return_string += "\nRuntime: " + runtime;
    return_string += "\nRecurring: ";
    return_string += Constants::Recurring::names[recurring];
    return_string += "\nEmail notification: ";
    if (notify)   return_string += "Yes";
    else          return_string += "No";
    return return_string;
  }

  std::string filesToString() const {
    std::string files_s{};
    for (const auto& file : filenames) files_s += file + ":";
    if (!files_s.empty())
      files_s.pop_back();
    return files_s;
  }

  friend std::ostream &operator<<(std::ostream &out, const Task &task) {
    return out << task.toString();
  }

  friend bool operator==(const Task& t1, const Task& t2);
  friend bool operator!=(const Task& t1, const Task& t2);

  friend bool operator==(const Task& t1, const Task& t2) {
    return (
      t1.completed       == t2.completed       &&
      t1.datetime        == t2.datetime        &&
      t1.envfile         == t2.envfile         &&
      t1.execution_flags == t2.execution_flags &&
      t1.execution_mask  == t2.execution_mask  &&
      t1.file            == t2.file            &&
      t1.files.size()    == t2.files.size()    && // TODO: implement comparison for FileInfo
      t1.id              == t2.id              &&
      t1.recurring       == t2.recurring       &&
      t1.notify          == t2.notify,
      t1.runtime         == t1.runtime
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
std::vector<FileInfo> parseFileInfo(std::string file_info);

class TaskHandler {
  public:
    virtual Task prepareTask(TaskArguments argv, std::string uuid, Task* task = nullptr) = 0;
};

#endif // __TASK_HPP__
