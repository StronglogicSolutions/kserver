#ifndef __SCHEDULER_HPP__
#define __SCHEDULER_HPP__

#include <database/DatabaseConnection.h>
#include <log/logger.h>

#include <codec/util.hpp>
#include <config/config_parser.hpp>
#include <database/kdb.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace Executor {

typedef std::function<void(std::string, int, int)> EventCallback;

struct Task {
  int execution_mask;
  std::string datetime;
  std::string filename;
  std::string envfile;
  std::string execution_flags;
  int id = 0;
};

namespace {
class DeferInterface {
 public:
  virtual void schedule(Task task) = 0;
};

class CalendarManagerInterface {
 public:
  virtual std::vector<Task> fetchTasks() = 0;

 private:
  virtual void executeTask(int client_id, Task task) = 0;
};

auto KLOG = KLogger::GetInstance() -> get_logger();

class Scheduler : public DeferInterface, CalendarManagerInterface {
 public:
  Scheduler() {}
  Scheduler(EventCallback fn) : m_event_callback(fn) {
    if (!ConfigParser::initConfig()) {
      KLOG->info("Unable to load config");
    }
  }

  ~Scheduler() { KLOG->info("Scheduler destroyed"); }

  virtual void schedule(Task task) {
    // verify and put in database
    Database::KDB kdb{};

    auto result =
        kdb.insert("schedule", {"time", "mask", "file", "flags", "envfile"},
                   {task.datetime, std::to_string(task.execution_mask),
                    task.filename, task.execution_flags, task.envfile});
    KLOG->info("Request to schedule task was {}",
               result ? "Accepted" : "Rejected");
  }

  static KApplication getAppInfo(int mask) {
    Database::KDB kdb{};
    KApplication k_app{};
    QueryValues values = kdb.select("apps", {"path", "data", "name"},
                                    {{"mask", std::to_string(mask)}});

    for (const auto &value_pair : values) {
      if (value_pair.first == "path") {
        k_app.path = value_pair.second;
      } else if (value_pair.first == "data") {
        k_app.data = value_pair.second;
      } else if (value_pair.first == "name") {
        k_app.name = value_pair.second;
      }
    }
    return k_app;
  }

  void onProcessComplete(std::string value, int mask, int client_fd) {
    KLOG->info("Value returned from process:\n{}", value);
    if (m_event_callback != nullptr) {
      m_event_callback(value, mask, client_fd);
    }
  }

  virtual std::vector<Task> fetchTasks() {
    // get tasks from database
    // now' a good time to execute them, or place them in cron
    Database::KDB kdb{};
    // get today timestamp
    std::string current_timestamp = std::to_string(TimeUtils::unixtime());
    std::string future_timestamp_24hr =
        std::to_string(TimeUtils::unixtime() + 86400);
    std::vector<Task> tasks{};
    int id{};
    QueryComparisonBetweenFilter filter{
        {"time", current_timestamp, future_timestamp_24hr}};
    // TODO: Implement >, <, <> filtering
    auto result = kdb.selectCompare(
        "schedule", {"id", "time", "file", "mask", "flags", "envfile"}, filter);
    if (!result.empty() && result.at(0).first.size() > 0) {
      std::string mask, flags, envfile, time, filename;
      for (const auto &v : result) {
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
        if (v.first == "file") {
          filename = v.second;
        }
        if (v.first == "id") {
          id = std::stoi(v.second);
        }
        if (!filename.empty() && !envfile.empty() && !flags.empty() &&
            !time.empty() && !mask.empty() && id > 0) {
          tasks.push_back(Task{.execution_mask = std::stoi(mask),
                               .datetime = time,
                               .filename = filename,
                               .envfile = envfile,
                               .execution_flags = flags,
                               .id = id});
          id = 0;
          filename.clear();
          envfile.clear();
          flags.clear();
          time.clear();
          mask.clear();
        }
      }
    }
    return tasks;
  }

  virtual void executeTask(int client_socket_fd, Task task) {
    KLOG->info("Executing task");
    KApplication app_info = getAppInfo(task.execution_mask);
    auto is_ready_to_execute = std::stoi(task.datetime) > 0;  // if close to now
    auto flags = task.execution_flags;
    auto envfile = task.envfile;

    if (is_ready_to_execute) {
      ProcessExecutor executor{};
      executor.setEventCallback(
          [this](std::string result, int mask, int client_socket_fd) {
            onProcessComplete(result, mask, client_socket_fd);
          });

      std::string id_value{std::to_string(task.id)};

      executor.request(ConfigParser::getExecutorScript(), task.execution_mask,
                       client_socket_fd, {id_value});
    }
  }

 private:
  EventCallback m_event_callback;
};

}  // namespace
}  // namespace Executor

#endif  // __SCHEDULER_HPP__
