#ifndef __SCHEDULER_HPP__
#define __SCHEDULER_HPP__

#include <database/DatabaseConnection.h>
#include <log/logger.h>

#include <config/config_parser.hpp>
#include <database/kdb.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace Executor {

struct Task {
  int execution_mask;
  std::string datetime;
  std::string filename;
  std::string envfile;
  std::string execution_flags;

  // friend std::ostream & operator << (std::ostream &out, const Executor::Task& t) {
  //     out << t.datetime;
  //     out << " - Mask: " << std::to_string(t.execution_mask) << "\n. Args: " << t.filename << " - " << t.envfile << " - " << t.execution_flags;
  //     return out;
  // }
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
  virtual void executeTask(Task task) = 0;
};

auto KLOG = KLogger::GetInstance() -> get_logger();

class Scheduler : public DeferInterface, CalendarManagerInterface {
 public:
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
    // get from DB
    return KApplication{};
  }

  void onProcessComplete(std::string value, int mask, int client_fd) {
    KLOG->info("Value returned from process:\n{}", value);
  }

  virtual std::vector<Task> fetchTasks() {
    // get tasks from database
    // now' a good time to execute them, or place them in cron
    Database::KDB kdb{};
    // get today timestamp
    std::string current_timestamp = std::to_string(TimeUtils::unixtime());
    std::string future_timestamp_24hr = std::to_string(TimeUtils::unixtime() + 86400);
    std::vector<Task> tasks{};
    QueryComparisonBetweenFilter filter{{"time", current_timestamp, future_timestamp_24hr}};
    // TODO: Implement >, <, <> filtering
    auto result = kdb.selectCompare("schedule", {"time", "file", "mask", "flags", "envfile"}, filter);
    if (!result.empty() && result.at(0).first.size() > 0) {
      std::string mask, flags, envfile, time, filename;
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
        if (v.first == "file") {
          filename = v.second;
        }
        if (!filename.empty() && !envfile.empty() && !flags.empty() && !time.empty() && !mask.empty()) {
          tasks.push_back(Task{.execution_mask = std::stoi(mask),
                                      .datetime = time,
                                      .filename = filename,
                                      .envfile = envfile,
                                      .execution_flags = flags});
              }
        }
      }
    return tasks;
  }

  virtual void executeTask(Task task) {
    std::cout << "Executing" << std::endl;
    KApplication app_info = getAppInfo(task.execution_mask);
    auto is_ready_to_execute = std::stoi(task.datetime) > 0;  // if close to now
    auto flags = task.execution_flags;
    auto envfile = task.envfile;

    if (is_ready_to_execute) {
      // Make this member?
      ProcessExecutor executor{};
      executor.setEventCallback(
          [this](std::string result, int mask, int client_socket_fd) {
            onProcessComplete(result, mask, client_socket_fd);
          });
      executor.request(app_info.path, task.execution_mask, -69);
    }
  }
};

}  // namespace
}  // namespace Executor

#endif  // __SCHEDULER_HPP__
