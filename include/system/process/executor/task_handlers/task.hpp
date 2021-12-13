#pragma once

#include <vector>
#include <string>
#include <functional>

#include "common/util.hpp"

#define TIMESTAMP_LENGTH 10

namespace kiq {
using SystemEventcallback =
    std::function<void(int32_t, int32_t, const std::vector<std::string>&)>;

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

namespace constants {
// Scheduled Tasks
static const uint8_t     PAYLOAD_ID_INDEX                {0x01};
static const uint8_t     PAYLOAD_NAME_INDEX              {0x02};
static const uint8_t     PAYLOAD_TIME_INDEX              {0x03};
static const uint8_t     PAYLOAD_FLAGS_INDEX             {0x04};
static const uint8_t     PAYLOAD_COMPLETED_INDEX         {0x05};
static const uint8_t     PAYLOAD_RECURRING_INDEX         {0x06};
static const uint8_t     PAYLOAD_NOTIFY_INDEX            {0x07};
static const uint8_t     PAYLOAD_RUNTIME_INDEX           {0x08};
static const uint8_t     PAYLOAD_FILES_INDEX             {0x09};
static const uint8_t     PAYLOAD_ENVFILE_INDEX           {0x0A};
static const uint8_t     PAYLOAD_SIZE                    {0x0B};

// Platform Posts
static const uint8_t     PLATFORM_PAYLOAD_PLATFORM_INDEX {0x00};
static const uint8_t     PLATFORM_PAYLOAD_ID_INDEX       {0x01};
static const uint8_t     PLATFORM_PAYLOAD_USER_INDEX     {0x02};
static const uint8_t     PLATFORM_PAYLOAD_TIME_INDEX     {0x03};
static const uint8_t     PLATFORM_PAYLOAD_ERROR_INDEX    {0x03};
static const uint8_t     PLATFORM_PAYLOAD_CONTENT_INDEX  {0x04};
static const uint8_t     PLATFORM_PAYLOAD_URL_INDEX      {0x05}; // concatenated string
static const uint8_t     PLATFORM_PAYLOAD_REPOST_INDEX   {0x06};
static const uint8_t     PLATFORM_PAYLOAD_METHOD_INDEX   {0x07};
static const uint8_t     PLATFORM_PAYLOAD_ARGS_INDEX     {0x08};
static const uint8_t     PLATFORM_PAYLOAD_CMD_INDEX      {0x09};
static const uint8_t     PLATFORM_MINIMUM_PAYLOAD_SIZE   {0x07};
static const uint8_t     PLATFORM_DEFAULT_COMMAND        {0x00};

       const std::string NO_ORIGIN_PLATFORM_EXISTS       {"2"};
       const std::string PLATFORM_POST_INCOMPLETE        {"0"};
       const std::string PLATFORM_POST_COMPLETE          {"1"};

static const uint8_t     PLATFORM_POST_CONTENT_INDEX     {0x00};
static const uint8_t     PLATFORM_POST_URL_INDEX         {0x01};
static const uint8_t     PLATFORM_POST_ARGS_INDEX        {0x02};

static const uint8_t     FETCH_TASK_MASK_INDEX           {0x01};
static const uint8_t     FETCH_TASK_DATE_RANGE_INDEX     {0x02};
static const uint8_t     FETCH_TASK_ROW_COUNT_INDEX      {0x03};
static const uint8_t     FETCH_TASK_MAX_ID_INDEX         {0x04};
static const uint8_t     FETCH_TASK_ORDER_INDEX          {0x05};

       const std::string SHOULD_REPOST                   {"true"};
       const std::string PLATFORM_PROCESS_METHOD         {"process"};
       const std::string VIDEO_TYPE_ARGUMENT             {"video\""};
       const std::string IMAGE_TYPE_ARGUMENT             {"image\""};
       const char        LINE_BREAK                      {'\n'};

static const char* DESCRIPTION_KEY        {"DESCRIPTION"};
static const char* FILE_TYPE_KEY          {"FILE_TYPE"};
static const char* HEADER_KEY             {"HEADER"};
static const char* USER_KEY               {"USER"};
static const char* HASHTAGS_KEY           {"HASHTAGS"};
static const char* LINK_BIO_KEY           {"LINK_BIO"};
static const char* REQUESTED_BY_KEY       {"REQUESTED_BY"};
static const char* REQUESTED_BY_PHRASE_KEY{"REQUESTED_BY_PHRASE"};
static const char* PROMOTE_SHARE_KEY      {"PROMOTE_SHARE"};
static const char* DIRECT_MESSAGE_KEY     {"DIRECT_MESSAGE"};

static const std::unordered_map<std::string, std::string> PARAM_KEY_MAP{
  {DESCRIPTION_KEY,         "--description"},
  {FILE_TYPE_KEY,           "--media"},
  {HEADER_KEY,              "--header"},
  {USER_KEY,                "--user"},
  {HASHTAGS_KEY,            "--hashtags"},
  {LINK_BIO_KEY,            "--link_bio"},
  {REQUESTED_BY_KEY,        "--requested_by"},
  {REQUESTED_BY_PHRASE_KEY, "--requested_by_phrase"},
  {PROMOTE_SHARE_KEY,       "--promote_share"},
  {DIRECT_MESSAGE_KEY,      "--direct_message"}
};

static const char INSTAGRAM_DIRECT_MESSAGE[]{"IG DM"};
static const char INSTAGRAM_FEED[]{"IG Feed"};
static const char YOUTUBE_FEED[]{"YT Feed"};
static const char TWITTER_SEARCH[]{"TW Search"};
static const char IG_DIRECT_MESSAGE_FLAG[]{" --direct_message=$DIRECT_MESSAGE"};
} // namespace constants

/**
 * \note Scheduled Task Completion States
 *
 * SCHEDULED  - Has not yet run
 * COMPLETED  - Ran successfully
 * FAILED     - Ran once and failed
 * RETRY_FAIL - Failed on retry
 */
namespace Completed {
static constexpr uint8_t SCHEDULED  = 0;
static constexpr uint8_t SUCCESS    = 1;
static constexpr uint8_t FAILED     = 2;
static constexpr uint8_t RETRY_FAIL = 3;

static constexpr const char* STRINGS[4] = {"0", "1", "2", "3"};
static const std::vector<std::string> NAMES{
  "Scheduled", "Success", "Failed", "Retry Failed"
};
}  // namespace Completed

namespace Messages {
static constexpr const char* TASK_ERROR_EMAIL =
    "Scheduled task ran but returned an error:\n";
}

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

static const uint8_t TASK_PAYLOAD_SIZE{12};
struct Task {
  int32_t                  execution_mask;
  std::string              datetime;
  bool                     file;
  std::vector<FileInfo>    files;
  std::string              envfile;
  std::string              execution_flags;
  int32_t                  task_id{0};
  int32_t                  completed;
  int32_t                  recurring;
  bool                     notify;
  std::string              runtime;
  std::vector<std::string> filenames;

  std::string id() const
  {
    return std::to_string(task_id);
  }

  static Task clone_basic(const Task& task, int new_mask = -1, bool recurring = false)
  {
    Task new_task{};
    new_task.datetime        = task.datetime;
    new_task.execution_mask  = (new_mask >= 0) ? new_mask : task.execution_mask;
    new_task.file            = task.file;
    new_task.files           = task.files;
    new_task.execution_flags = task.execution_flags;
    new_task.runtime         = task.runtime;
    new_task.filenames       = task.filenames;

    return new_task;
  }

  bool validate() {
    return !datetime.empty() && !envfile.empty() &&
          !execution_flags.empty();
  }

  std::vector<std::string> payload()
  {
    std::vector<std::string> payload{};
    payload.reserve(8);
    payload.emplace_back(id());
    payload.emplace_back(datetime);
    payload.emplace_back(execution_flags);
    payload.emplace_back(std::to_string(completed));
    payload.emplace_back(std::to_string(recurring));
    payload.emplace_back(std::to_string(notify));
    payload.emplace_back(runtime);
    payload.emplace_back(filesToString());
    return payload;
  }

  std::string toString() const {
    std::string return_string{};
    return_string.reserve(100);
    return_string += "ID: " + id();
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

  std::string filesToString() const
  {
    std::string files_s{};
    for (const auto& file : filenames) files_s += file + ":";
    if (!files_s.empty())
      files_s.pop_back();
    return files_s;
  }

  std::string GetToken(const std::string& flag) const
  {
    return FileUtils::ReadEnvToken(envfile, flag);
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
      t1.task_id         == t2.task_id         &&
      t1.recurring       == t2.recurring       &&
      t1.notify          == t2.notify,
      t1.runtime         == t1.runtime
    );
  }

  friend bool operator!=(const Task& t1,const Task& t2) {
    return !(t1 == t2);
  }
};

struct FileMetaData
{
std::string task_id;
std::string id;
std::string name;
std::string type;

bool complete() const
{
  return (!id.empty() && !name.empty() && !type.empty()); // TODO: task_id ?
}

void clear()
{
  DataUtils::ClearArgs(id, name, type);
}

std::vector<std::string> to_string_v() const
{
  return std::vector<std::string>{
    task_id, id, name, type
  };
}

static std::vector<std::string> MetaDataToPayload(const std::vector<FileMetaData>& files)
{
  std::vector<std::string> payload{};
  payload.reserve((files.size() * 4) + 1);
  payload.emplace_back(std::to_string(files.size()));
  for (const auto& file : files)
  {
    auto data = file.to_string_v();
    payload.insert(payload.end(), std::make_move_iterator(data.begin()), std::make_move_iterator(data.end()));
  }

  return payload;
}

static std::vector<FileMetaData> PayloadToMetaData(const std::vector<std::string>& data)
{
  const int32_t             file_num = std::stoi(data.front());
  std::vector<FileMetaData> files{};
  files.reserve(file_num);

  for (auto i = 0; i < file_num; i++)
    files.emplace_back(FileMetaData{
      .task_id = data[1 + (4 * i)],
      .id      = data[2 + (4 * i)],
      .name    = data[3 + (4 * i)],
      .type    = data[4 + (4 * i)]});

  return files;
}
};

std::string AppendExecutionFlag(std::string flag_s, const std::string& flag);
std::string AsExecutionFlag(const std::string& flag, const std::string& prefix = " ");

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
    virtual Task prepareTask(const TaskArguments& argv, const std::string& uuid, Task* task = nullptr) = 0;
};

enum class PlatformPostState{ PROCESSING = 0x00, SUCCESS = 0x01, FAILURE = 0x02};

const std::string PLATFORM_STATUS_PENDING{"0"};
const std::string PLATFORM_STATUS_SUCCESS{"1"};
const std::string PLATFORM_STATUS_FAILURE{"2"};

struct platform_pair_hash
{
  template <class T1, class T2>
  std::size_t operator() (const std::pair<T1, T2> &pair) const
  {
    return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
  }
};

using PlatformRequestMap =
std::unordered_map<std::pair<std::string, std::string>, PlatformPostState, platform_pair_hash>;

struct PlatformPost {
std::string pid;
std::string o_pid = constants::NO_ORIGIN_PLATFORM_EXISTS;
std::string id;
std::string user;
std::string time;
std::string content;
std::string urls;
std::string repost;
std::string name;
std::string args;
std::string method;
std::string cmd{std::to_string(constants::PLATFORM_DEFAULT_COMMAND)};

const bool is_valid() const
{
  return (!(pid.empty()) && !(content.empty()));
}

const std::string ToString() const
{
  return std::string{"PID: "     + pid    + '\n' +
                     "Origin: "  + o_pid  + '\n' +
                     "ID: "      + id     + '\n' +
                     "User: "    + user   + '\n' +
                     "Time: "    + time   + '\n' +
                     "Content: " + content+ '\n' +
                     "URLS: "    + urls   + '\n' +
                     "Repost: "  + repost + '\n' +
                     "Name: "    + name   + '\n' +
                     "Args: "    + args   + '\n' +
                     "Method: "  + method};
}

std::vector<std::string> GetPayload() const
{
  /**
   * TODO: Args need to be JSON string:
   *
   * const auto  args  = kbot::keleqram::GetArgs(request.args);
     const auto  dest  = args[0];
     const auto  type  = args[1];
  */
  std::vector<std::string> payload{};
  payload.resize(10);
  payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX) = name;
  payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX)       = id;
  payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX)     = user;
  payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX)     = time;
  payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX)  = content;
  payload.at(constants::PLATFORM_PAYLOAD_URL_INDEX)      = urls;
  payload.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX)   = repost;
  payload.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX)   = method;
  payload.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX)     = args;
  payload.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)      = cmd;

  return payload;
}

};
} // ns kiq
