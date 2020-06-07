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
static constexpr int SCHEDULED = 0;
static constexpr int SUCCESS = 1;
static constexpr int FAILED = 2;
static constexpr int RETRY_FAIL = 3;

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

class Scheduler : public DeferInterface, CalendarManagerInterface {
 public:
  Scheduler() : m_kdb(Database::KDB{}) {}
  Scheduler(Database::KDB&& kdb) : m_kdb(std::move(kdb)) {}
  Scheduler(ScheduleEventCallback fn) : m_kdb(Database::KDB{}), m_event_callback(fn) {}

  // TODO: Implement move / copy constructor

  ~Scheduler() { KLOG("Scheduler destroyed"); }

  virtual std::string schedule(Task task) override {
    try {
      std::string id =
          m_kdb.insert("schedule", {"time", "mask", "flags", "envfile"},
                      {task.datetime, std::to_string(task.execution_mask),
                        task.execution_flags, task.envfile},
                      "id");
      auto result = !id.empty();

      if (!id.empty()) {
        KLOG("Request to schedule task was accepted\nID {}", id);
        for (const auto& file : task.files) {
          KLOG("Recording file in DB: {}", file.first);
          m_kdb.insert("file", {"name", "sid"}, {file.first, id});
        }
      }
      return id;
    } catch (const pqxx::sql_error &e) {
      ELOG("Insert query failed: {}", e.what());
    } catch (const std::exception &e) {
      ELOG("Insert query failed: {}", e.what());
    }
    return "";
  }

  std::vector<Task> parseTasks(QueryValues&& result) {
    int id{}, completed{NO_COMPLETED_VALUE};
    std::string mask, flags, envfile, time, filename;
    std::vector<Task> tasks;
    for (const auto& v : result) {
      if (v.first == "mask") {
        mask = v.second;
      }
      if (v.first == "flags") {
        flags = v.second;
      }
      if (v.first == "envfile") {
        envfile = v.second;
      }
      if (v.first == "time") {
        time = v.second;
      }
      if (v.first == "id") {
        id = std::stoi(v.second);
      }
      if (v.first == "completed") {
        completed = std::stoi(v.second);
      }
      if (!envfile.empty() && !flags.empty() && !time.empty() &&
          !mask.empty() && completed != NO_COMPLETED_VALUE && id > 0) {
        tasks.push_back(Task{.execution_mask = std::stoi(mask),
                             .datetime = time,
                             .file = true,
                             .files = {},
                             .envfile = envfile,
                             .execution_flags = flags,
                             .id = id,
                             .completed = completed});
        id = 0;
        completed = NO_COMPLETED_VALUE;
        filename.clear();
        envfile.clear();
        flags.clear();
        time.clear();
        mask.clear();
      }
    }
    return tasks;
  }

  Task parseTask(QueryValues&& result) {
    Task task{};
    for (const auto& v : result) {
      if (v.first == "mask") {
        task.execution_mask = std::stoi(v.second);
      }
      if (v.first == "flags") {
        task.execution_flags = v.second;
      }
      if (v.first == "envfile") {
        task.envfile = v.second;
      }
      if (v.first == "time") {
        task.datetime = v.second;
      }
      if (v.first == "id") {
        task.id = std::stoi(v.second);
      }
      if (v.first == "completed") {
        task.completed = std::stoi(v.second);
      }
    }
    return task;
  }

  virtual std::vector<Task> fetchTasks() override {
    std::string past_15_minute_timestamp =
        std::to_string(TimeUtils::unixtime() - 900);
    std::string future_5_minute_timestamp =
        std::to_string(TimeUtils::unixtime() + 300);
    return parseTasks(
        m_kdb.selectMultiFilter<CompBetweenFilter, MultiOptionFilter>(
            "schedule",                                               // table
            {"id", "time", "mask", "flags", "envfile", "completed"},  // fields
            {CompBetweenFilter{"time", std::move(past_15_minute_timestamp),
                               std::move(future_5_minute_timestamp)},
             MultiOptionFilter{"completed",
                               "IN",
                               {Completed::STRINGS[Completed::SCHEDULED],
                                {Completed::STRINGS[Completed::FAILED]}}}}));
  }

  Task getTask(std::string id) {
    return parseTask(m_kdb.select(
        "schedule", {"mask", "flags", "envfile", "time", "completed"},
        {{"id", id}}));
  }

  Task getTask(int id) {
    return parseTask(m_kdb.select(
        "schedule", {"mask", "flags", "envfile", "time", "completed"},
        {{"id", std::to_string(id)}}));
  }

  bool updateStatus(Task* task) {
    return !m_kdb
                .update("schedule",  // table
                        {"completed"},
                        {std::to_string(task->completed)},  // completion status
                        QueryFilter{{"id", std::to_string(task->id)}},
                        "id")  // return value
                .empty();      // !empty = success
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
  ScheduleEventCallback m_event_callback;
  Database::KDB m_kdb;
};
}  // namespace Scheduler

#endif  // __SCHEDULER_HPP__
