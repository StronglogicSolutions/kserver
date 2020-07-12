#ifndef __SCHEDULER_HPP__
#define __SCHEDULER_HPP__

#include <log/logger.h>
#include <executor/task_handlers/task.hpp>
#include <codec/util.hpp>
#include <database/kdb.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#define NO_COMPLETED_VALUE 99

#define TIMESTAMP_TIME_AS_TODAY \
            "(extract(epoch from (TIMESTAMPTZ 'today')) + "\
            "3600 * extract(hour from(to_timestamp(schedule.time))) + "\
            "60 * extract(minute from(to_timestamp(schedule.time))) + "\
            "extract(second from (to_timestamp(schedule.time))))"

using namespace Executor;

namespace Scheduler {

using ScheduleEventCallback =
    std::function<void(std::string, int, int, std::vector<std::string>)>;

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
};

class CalendarManagerInterface {
 public:
  virtual std::vector<Task> fetchTasks() = 0;
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
    case Executor::Constants::Recurring::HOURLY:
      return 3600;
    case Executor::Constants::Recurring::DAILY:
      return 86400;
    case Executor::Constants::Recurring::MONTHLY:
      return 86400 * 30;
    case Executor::Constants::Recurring::YEARLY:
      return 86400 * 365;
    default:
      return 0;
  }
}

class Scheduler : public DeferInterface, CalendarManagerInterface {
 public:
  Scheduler() : m_kdb(Database::KDB{}) {}
  Scheduler(Database::KDB&& kdb) : m_kdb(std::move(kdb)) {}
  Scheduler(ScheduleEventCallback fn) : m_kdb(Database::KDB{}), m_event_callback(fn) {}

  // TODO: Implement move / copy constructor

  ~Scheduler() { KLOG("Scheduler destroyed"); }

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
            "id");                      // return
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
      if (v.first == Executor::Field::MASK)
        { task.execution_mask      = std::stoi(v.second);       }
 else if (v.first == Executor::Field::FLAGS)
        { task.execution_flags     = v.second;                  }
 else if (v.first == Executor::Field::ENVFILE)
        { task.envfile             = v.second;                  }
 else if (v.first == Executor::Field::TIME)
        { task.datetime            = v.second;                  }
 else if (v.first == Executor::Field::ID)
        { task.id                  = std::stoi(v.second);       }
 else if (v.first == Executor::Field::COMPLETED)
        { task.completed           = std::stoi(v.second);       }
 else if (v.first == Executor::Field::RECURRING)
        { task.recurring           = std::stoi(v.second);       }
 else if (v.first == Executor::Field::NOTIFY)
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
   */
  std::vector<Task> parseTasks(QueryValues&& result) {
    int id{}, completed{NO_COMPLETED_VALUE}, recurring{-1}, notify{-1};
    std::string mask, flags, envfile, time, filename;
    std::vector<Task> tasks;

    for (const auto& v : result) {
      if (v.first == Executor::Field::MASK)      { mask      = v.second; }
 else if (v.first == Executor::Field::FLAGS)     { flags     = v.second; }
 else if (v.first == Executor::Field::ENVFILE)   { envfile   = v.second; }
 else if (v.first == Executor::Field::TIME)      { time      = v.second; }
 else if (v.first == Executor::Field::ID)        { id        = std::stoi(v.second); }
 else if (v.first == Executor::Field::COMPLETED) { completed = std::stoi(v.second); }
 else if (v.first == Executor::Field::RECURRING) { recurring = std::stoi(v.second); }
 else if (v.first == Executor::Field::NOTIFY)    { notify    = v.second.compare("t") == 0; }

      if (!envfile.empty() && !flags.empty() && !time.empty() &&
          !mask.empty()    && completed != NO_COMPLETED_VALUE &&
          id > 0           && recurring > -1 && notify > -1) {
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
          .notify           = (notify == 1)
        });
        id                  = 0;
        recurring           = -1;
        notify              = -1;
        completed           = NO_COMPLETED_VALUE;
        filename.clear();
        envfile.clear();
        flags.clear();
        time.clear();
        mask.clear();
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
    std::string future_5_minute_timestamp =
        std::to_string(TimeUtils::unixtime() + 300);
    return parseTasks(
      m_kdb.selectMultiFilter<CompFilter, CompBetweenFilter, MultiOptionFilter>(
        "schedule", {                                   // table
          Executor::Field::ID,
          Executor::Field::TIME,
          Executor::Field::MASK,
          Executor::Field::FLAGS,
          Executor::Field::ENVFILE,
          Executor::Field::COMPLETED,
          Executor::Field::NOTIFY,
          Executor::Field::RECURRING
        }, std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>{
          CompFilter{                                   // filter
            "recurring",                                // field of comparison
            "0",                                        // value for comparison
            "="                                         // comparator
          },
          CompBetweenFilter{                            // filter
            "time",                                     // field of comparison
            past_15_minute_timestamp,                   // min range
            future_5_minute_timestamp                   // max range
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
  }

  /**
   * fetchRecurringTasks
   *
   * Fetch scheduled tasks that are set to be performed on a recurring basis
   *
   * @return [out] {std::vector<Task>} A vector of Task objects
   */
  std::vector<Task> fetchRecurringTasks() {
    using SelectJoinFilters = std::vector<std::variant<CompFilter, CompBetweenFilter>>;
    return parseTasks(
      m_kdb.selectJoin<SelectJoinFilters>(
        "schedule", {                                                     // table
          Executor::Field::ID,                                            // fields
          Executor::Field::TIME,
          Executor::Field::MASK,
          Executor::Field::FLAGS,
          Executor::Field::ENVFILE,
          Executor::Field::COMPLETED,
          Executor::Field::RECURRING,
          Executor::Field::NOTIFY,
          "recurring.time"
        }, SelectJoinFilters{
          CompFilter{                                                     // filter
            Executor::Field::RECURRING,                                   // field
            "0",                                                          // value
            "<>"                                                          // comparator
          },
          CompFilter{                                                     // filter
            std::to_string(TimeUtils::unixtime()),                        // field
            "(recurring.time + (SELECT get_recurring_seconds(schedule.recurring) - 900))",  // value (from function)
            ">"                                                           // comparator
          }
        },
        Join{
          .table="recurring",                                              // table to join
          .field="sid",                                                    // field to join on
          .join_table="schedule",                                          // table to join to
          .join_field="id"                                                 // field to join to
        }
      )
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
    return parseTask(m_kdb.select(
      "schedule", {       // table
        "mask", "flags",  // fields
        "envfile", "time",
        "completed"
        }, {
          {"id", id}      // filter
        }
    ));
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
    return parseTask(m_kdb.select( // SELECT
      "schedule", {                // table
        Executor::Field::MASK, Executor::Field::FLAGS,           // fields
        Executor::Field::ENVFILE, Executor::Field::TIME,
        Executor::Field::COMPLETED
      }, QueryFilter{
        {"id", std::to_string(id)} // filter
      }
    ));
  }
  /**
   * updateTask
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
