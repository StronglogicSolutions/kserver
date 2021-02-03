#ifndef __SCHEDULER_HPP__
#define __SCHEDULER_HPP__

#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#include "log/logger.h"
#include "executor/task_handlers/task.hpp"
#include "database/kdb.hpp"

#define NO_COMPLETED_VALUE 99

#define TIMESTAMP_TIME_AS_TODAY \
            "(extract(epoch from (TIMESTAMPTZ 'today')) + "\
            "3600 * extract(hour from(to_timestamp(schedule.time))) + "\
            "60 * extract(minute from(to_timestamp(schedule.time))) + "\
            "extract(second from (to_timestamp(schedule.time))))"

namespace Scheduler {


 /**
  * TODO: This should be moved elsewhere. Perhaps the Registrar
  */
 static uint32_t getAppMask(std::string name) {
  const std::string filter_field{"name"};
  const std::string value_field{"mask"};

  QueryValues values = Database::KDB{}.select(
    "apps",
    {
      value_field
    },
    {
      {filter_field, name}
    }
  );

  for (const auto &pair : values) {
    auto key = pair.first;
    auto value = pair.second;

    if (key == value_field)
      return std::stoi(value);
  }

  return std::numeric_limits<uint32_t>::max();
}

using ScheduleEventCallback =
    std::function<void(std::string, int, int, std::vector<std::string>)>;

namespace constants {
const uint8_t PAYLOAD_ID_INDEX        {0x01};
const uint8_t PAYLOAD_NAME_INDEX      {0x02};
const uint8_t PAYLOAD_TIME_INDEX      {0x03};
const uint8_t PAYLOAD_FLAGS_INDEX     {0x04};
const uint8_t PAYLOAD_COMPLETED_INDEX {0x05};
const uint8_t PAYLOAD_RECURRING_INDEX {0x06};
const uint8_t PAYLOAD_NOTIFY_INDEX    {0x07};
const uint8_t PAYLOAD_RUNTIME_INDEX   {0x08};
const uint8_t PAYLOAD_FILES_INDEX     {0x09};

const uint8_t PAYLOAD_SIZE            {0x0A};
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
}  // namespace Completed

namespace Messages {
static constexpr const char* TASK_ERROR_EMAIL =
    "Scheduled task ran but returned an error:\n";
}

class DeferInterface {
 public:
  virtual std::string schedule(Task task) = 0;
  virtual ~DeferInterface() {}
};

class CalendarManagerInterface {
 public:
  virtual std::vector<Task> fetchTasks() = 0;
  virtual ~CalendarManagerInterface() {}
};

/**
 * getIntervalSeconds
 *
 * Helper function returns the number of seconds equivalent to a recurring interval
 *
 * @param  [in]  {uint32_t}  The integer value representing a recurring interval
 * @return [out] {uint32_t}  The number of seconds equivalent to that interval
 */
inline uint32_t getIntervalSeconds(uint32_t interval) {
  switch(interval) {
    case Constants::Recurring::HOURLY:
      return 3600;
    case Constants::Recurring::DAILY:
      return 86400;
    case Constants::Recurring::MONTHLY:
      return 86400 * 30;
    case Constants::Recurring::YEARLY:
      return 86400 * 365;
    default:
      return 0;
  }
}

inline Task args_to_task(std::vector<std::string> args) {
  Task task{};

  if (args.size() == constants::PAYLOAD_SIZE) {
    auto mask = getAppMask(args.at(constants::PAYLOAD_NAME_INDEX));

    if (mask != NO_APP_MASK) {
      task.id              = std::stoi(args.at(constants::PAYLOAD_ID_INDEX));
      task.execution_mask  = mask;
      task.datetime        = args.at(constants::PAYLOAD_TIME_INDEX);
      task.execution_flags = args.at(constants::PAYLOAD_FLAGS_INDEX);
      task.completed       = args.at(constants::PAYLOAD_COMPLETED_INDEX).compare("1") == 0;
      task.recurring       = std::stoi(args.at(constants::PAYLOAD_RECURRING_INDEX));
      task.notify          = args.at(constants::PAYLOAD_NOTIFY_INDEX).compare("1") == 0;
      task.runtime         = stripSQuotes(args.at(constants::PAYLOAD_RUNTIME_INDEX));
      // task.filenames = args.at(constants::PAYLOAD_ID_INDEX;
      KLOG("Can't parse files from schedule payload. Must be implemented");
    }
  }
  return task;
}

class Scheduler : public DeferInterface, CalendarManagerInterface {
 public:
  Scheduler() : m_kdb(Database::KDB{}) {}
  Scheduler(Database::KDB&& kdb) : m_kdb(std::move(kdb)) {}
  Scheduler(ScheduleEventCallback fn) : m_event_callback(fn), m_kdb(Database::KDB{}) {}

  // TODO: Implement move / copy constructor

  virtual ~Scheduler() override {
    KLOG("Scheduler destroyed");
  }

  /**
   * schedule
   *
   * Schedule a task
   *
   * @param  [in]  {Task}        task  The task to schedule
   * @return [out] {std::string}       The string value ID representing the Task in the database
   *                                   or an empty string if the insert failed
   */
  virtual std::string schedule(Task task) override {
    if (task.validate()) {
      try {
        std::string id =
          m_kdb.insert("schedule", {              // table
            "time",                               // fields
            "mask",
            "flags",
            "envfile",
            "recurring",
            "notify"}, {
            task.datetime,                        // values
            std::to_string(task.execution_mask),
            task.execution_flags,
            task.envfile,
            std::to_string(task.recurring),
            std::to_string(task.notify)},
            "id");                                // return
        if (!id.empty()) {
          KLOG("Request to schedule task was accepted\nID {}", id);
          for (const auto& file : task.files) {
            KLOG("Recording file in DB: {}", file.first);
            m_kdb.insert("file", {                  // table
              "name",                               // fields
              "sid"
            }, {
              file.first,                           // values
              id
            });
          }
          if (task.recurring &&
            !m_kdb.insert("recurring", {
              "sid",
              "time"
            }, {
              id,
              std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))
            })) {
              ELOG(
                "Recurring task was scheduled, but there was an error adding"\
                "its entry to the recurring table");
          }
        }
        return id;                                  // schedule id
      } catch (const pqxx::sql_error &e) {
        ELOG("Insert query failed: {}", e.what());
      } catch (const std::exception &e) {
        ELOG("Insert query failed: {}", e.what());
      }
    }
    return "";
  }

  /**
   * parseTask
   *
   * Parses a QueryValues data structure (vector of string tuples) and returns a Task object
   *
   * @param  [in]  {QueryValues}  result  The DB query result consisting of string tuples
   * @return [out] {Task}                 The task
   */
  Task parseTask(QueryValues&& result) {
    Task task{};
    for (const auto& v : result) {
      if (v.first == Field::MASK)
        {
          task.execution_mask      = std::stoi(v.second);
        }
 else if (v.first == Field::FLAGS)
        { task.execution_flags     = v.second;                  }
 else if (v.first == Field::ENVFILE)
        { task.envfile             = v.second;                  }
 else if (v.first == Field::TIME)
        { task.datetime            = v.second;                  }
 else if (v.first == Field::ID)
        { task.id                  = std::stoi(v.second);       }
 else if (v.first == Field::COMPLETED)
        { task.completed           = std::stoi(v.second);       }
 else if (v.first == Field::RECURRING)
        { task.recurring           = std::stoi(v.second);       }
 else if (v.first == Field::NOTIFY)
        { task.notify              = v.second.compare("t") == 0;}
    }
    return task;
  }

  /**
   * parseTasks
   *
   * Parses a QueryValues data structure (vector of string tuples) and returns a vector of Task objects
   *
   * @param  [in]  {QueryValues}        result  The DB query result consisting of string tuples
   * @return [out] {std::vector<Task>}          A vector of Task objects
   *
   * TODO: Remove "parse_files" boolean after refactoring `fetchTasks` (non-recurring tasks method) to use joins
   */
  std::vector<Task> parseTasks(QueryValues&& result, bool parse_files = false) {
    const std::string files_field{"(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files"};
    int id{}, completed{NO_COMPLETED_VALUE}, recurring{-1}, notify{-1};
    std::string mask, flags, envfile, time, filenames;
    std::vector<Task> tasks;
    bool checked_for_files{false};

    for (const auto& v : result) {
      if      (v.first == Field::MASK     ) { mask      = v.second;                   }
      else if (v.first == Field::FLAGS    ) { flags     = v.second;                   }
      else if (v.first == Field::ENVFILE  ) { envfile   = v.second;                   }
      else if (v.first == Field::TIME     ) { time      = v.second;                   }
      else if (v.first == Field::ID       ) { id        = std::stoi(v.second);        }
      else if (v.first == Field::COMPLETED) { completed = std::stoi(v.second);        }
      else if (v.first == Field::RECURRING) { recurring = std::stoi(v.second);        }
      else if (v.first == Field::NOTIFY   ) { notify    = v.second.compare("t") == 0; }
      else if (v.first == files_field     ) { filenames = v.second;
                                              checked_for_files = true;               }

      if (!envfile.empty() && !flags.empty() && !time.empty() &&
          !mask.empty()    && completed != NO_COMPLETED_VALUE &&
          id > 0           && recurring > -1 && notify > -1      ) {

        if (parse_files && !checked_for_files) {
          checked_for_files = true;
          continue;
        } else {
          if (recurring != Constants::Recurring::NO)
          {
            if (recurring == Constants::Recurring::HOURLY)
            {
              time = std::stoi(time) + getIntervalSeconds(recurring);
            }
            else
            {
              time = TimeUtils::time_as_today(time);
            }
          }

          tasks.push_back(Task{
            .execution_mask   = std::stoi(mask),
            .datetime         = time,
            .file             = true,
            .files            = {},
            .envfile          = envfile,
            .execution_flags  = flags,
            .id               = id,
            .completed        = completed,
            .recurring        = recurring,
            .notify           = (notify == 1),
            .runtime          = FileUtils::readRunArgs(envfile),                      // ⬅ set in environment.hpp
            .filenames        = StringUtils::split(filenames, ' ')
          });
          id                  = 0;
          recurring           = -1;
          notify              = -1;
          completed           = NO_COMPLETED_VALUE;
          checked_for_files   = false;
          filenames.clear();
          envfile.clear();
          flags.clear();
          time.clear();
          mask.clear();
        }
      }
    }
    return tasks;
  }

  /**
   * fetchTasks
   *
   * Fetch scheduled tasks which are intended to only be run once
   *
   * @return [out] {std::vector<Task>} A vector of Task objects
   */
  virtual std::vector<Task> fetchTasks() override {
    std::string past_15_minute_timestamp =
        std::to_string(TimeUtils::unixtime() - 900);
    std::string current_timestamp =
        std::to_string(TimeUtils::unixtime());
    std::vector<Task> tasks = parseTasks(
      m_kdb.selectMultiFilter<CompFilter, CompBetweenFilter, MultiOptionFilter>(
        "schedule", {                                   // table
          Field::ID,
          Field::TIME,
          Field::MASK,
          Field::FLAGS,
          Field::ENVFILE,
          Field::COMPLETED,
          Field::NOTIFY,
          Field::RECURRING
        }, std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>{
          CompFilter{                                   // filter
            "recurring",                                // field of comparison
            "0",                                        // value for comparison
            "="                                         // comparator
          },
          CompBetweenFilter{                            // filter
            "time",                                     // field of comparison
            past_15_minute_timestamp,                   // min range
            current_timestamp                           // max range
          },
          MultiOptionFilter{                            // filter
            "completed",                                // field of comparison
            "IN", {                                     // comparison type
              Completed::STRINGS[Completed::SCHEDULED], // set of values for comparison
              Completed::STRINGS[Completed::FAILED]
            }
          }
        }
      )
    );
    // TODO: Convert above to a JOIN query, and remove this code below
    for (auto& task : tasks) {
      task.filenames = getFiles(std::to_string(task.id));
    }

    return tasks;
  }

  /**
   * fetchRecurringTasks
   *
   * Fetch scheduled tasks that are set to be performed on a recurring basis
   *
   * @return [out] {std::vector<Task>} A vector of Task objects
   */
  std::vector<Task> fetchRecurringTasks() {
    using SelectJoinFilters = std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>;
    return parseTasks(
      m_kdb.selectJoin<SelectJoinFilters>(
        "schedule", {                                                     // table
          Field::ID,                                            // fields
          Field::TIME,
          Field::MASK,
          Field::FLAGS,
          Field::ENVFILE,
          Field::COMPLETED,
          Field::RECURRING,
          Field::NOTIFY,
          "recurring.time",                                                                      // TODO: Improve constants
          "(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files" // TODO: replace with grouping
        }, SelectJoinFilters{
          CompFilter{                                                     // filter
            Field::RECURRING,                                             // field
            "0",                                                          // value
            "<>"                                                          // comparator
          },
          CompFilter{                                                     // filter
            std::to_string(TimeUtils::unixtime()),                        // field
            "recurring.time + (SELECT get_recurring_seconds(schedule.recurring))",  // value (from function)
            ">"                                                           // comparator
          },
          MultiOptionFilter{                            // filter
            "completed",                                // field of comparison
            "IN", {                                     // comparison type
              Completed::STRINGS[Completed::SCHEDULED], // set of values for comparison
              Completed::STRINGS[Completed::FAILED]
            }
          }
        },
        Joins{
          Join{
            .table      ="recurring",                                     // table to join
            .field      ="sid",                                           // field to join on
            .join_table ="schedule",                                      // table to join to
            .join_field ="id",                                            // field to join to
            .type       = JoinType::INNER                                 // type of join
          },
          Join{
            .table      ="file",
            .field      ="sid",
            .join_table ="schedule",
            .join_field ="id",
            .type       = JoinType::OUTER
          }
        }
      ),
      true // ⬅ Parse files
    );
  }

  std::vector<Task> fetchAllTasks() {
    return parseTasks(
      m_kdb.selectJoin<QueryFilter>(
        "schedule",
        {
          Field::ID,                                            // fields
          Field::TIME,
          Field::MASK,
          Field::FLAGS,
          Field::ENVFILE,
          Field::COMPLETED,
          Field::RECURRING,
          Field::NOTIFY,
          "(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files" // TODO: replace with grouping
        },
        {},
        Joins{
          Join{
              .table      ="file",
              .field      ="sid",
              .join_table ="schedule",
              .join_field ="id",
              .type       = JoinType::OUTER
            }
        }
      ),
      true
    );
  }

  /**
   * getTask
   *
   * get one task by ID
   *
   * @param  [in]  {std::string}  id  The task ID
   * @return [out] {Task}             A task
   */
  Task getTask(std::string id) {
    Task task = parseTask(m_kdb.select(
      "schedule", {       // table
        Field::MASK,    Field::FLAGS, // fields
        Field::ENVFILE, Field::TIME,
        Field::COMPLETED
        }, {
          {"id", id}      // filter
        }
    ));

    task.filenames = getFiles(id);

    return task;
  }

  std::vector<std::string> getFiles(std::string sid) {
    std::vector<std::string> file_names{};

    QueryValues result = m_kdb.select(
      "file", {        // table
        "name",        // fields
        }, {
          {"sid", sid} // filter
        }
    );

    for (const auto& v : result) if (v.first == "name")
                                   file_names.push_back(v.second);
    return file_names;
  }
  /**
   * getTask
   *
   * get one task by ID
   *
   * @param  [in]  {int}  id  The task ID
   * @return [out] {Task}     A task
   */
  Task getTask(int id) {
    Task task =  parseTask(m_kdb.select(                  // SELECT
      "schedule", {                                       // table
        Field::MASK,    Field::FLAGS, // fields
        Field::ENVFILE, Field::TIME,
        Field::COMPLETED
      }, QueryFilter{
        {"id", std::to_string(id)}                        // filter
      }
    ));

    task.filenames = getFiles(std::to_string(id));        // files

    return task;
  }
  /**
   * updateStatus
   *
   * update the completion status of a task
   *
   * @param   [in] {Task}   task  The task to update
   * @return [out] {bool}         Whether the UPDATE query was successful
   */
  bool updateStatus(Task* task) {
    return !m_kdb.update(                // UPDATE
      "schedule", {                      // table
        "completed"                      // field
      }, {
        std::to_string(task->completed)  // value
      }, QueryFilter{
        {"id", std::to_string(task->id)} // filter
      },
      "id"                               // returning value
    )
    .empty();                            // not empty = success
  }

   /**
   * update
   *
   * @param   [in] {Task}   task  The task to update
   * @return [out] {bool}         Whether the UPDATE query was successful
   */
  bool update(Task task) {
    KLOG("Runtime flags cannot be updated. Must be implemented");
    // TODO: implement writing of R_FLAGS to envfile
    return !m_kdb.update(                // UPDATE
      "schedule", {                      // table
            "mask",
            "time",
            "flags",
            "completed",
            "recurring",
            "notify",
            "runtime"
      }, {
        std::to_string(task.execution_mask),
        task.datetime,
        task.execution_flags,
        std::to_string(task.completed),
        std::to_string(task.recurring),
        std::to_string(task.notify),
        task.runtime
      }, QueryFilter{
        {"id", std::to_string(task.id)} // filter
      },
      "id"                               // returning value
    )
    .empty();                            // not empty = success
  }

  /**
   * updateRecurring
   *
   * update the last time a recurring task was run
   *
   * @param   [in] {Task}   task  The task to update
   * @return [out] {bool}         Whether the UPDATE query was successful
   */
  bool updateRecurring(Task* task) {
    return !m_kdb.update(                // UPDATE
      "recurring", {                      // table
        "time"                      // field
      }, {
        task->datetime  // value
      }, QueryFilter{
        {"sid", std::to_string(task->id)} // filter
      },
      "id"                               // returning value
    )
    .empty();                            // not empty = success
  }

  template <typename T>
  std::string getTaskInfo(T id) {
    Task task = NULL;
    if constexpr ((std::is_same_v<T, std::string>) || std::is_same_v<T, int>) {
      task = getTask(id);
    }
    return "";
  }

 private:
  ScheduleEventCallback   m_event_callback;
  Database::KDB           m_kdb;
};
}  // namespace Scheduler

#endif  // __SCHEDULER_HPP__
