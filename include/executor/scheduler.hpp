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
typedef struct {
  int execution_mask;
  std::string datetime;
  std::string envfile;
  std::string execution_flags;
} Task;

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

    auto result = kdb.insert("schedule", {"time", "mask", "flags", "envfile"},
                             {task.datetime, std::to_string(task.execution_mask),
                              task.execution_flags, task.envfile});
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
    std::string today_start_timestamp{"0000000000"};
    std::vector<Task> tasks{};
    auto result = kdb.select("schedule", {"time", "mask", "flags", "envfile"},
                             {{"datetime", today_start_timestamp}});
    // if (!result.empty()) {
    //   for (const auto& v : result) {

    //     // Make sure result is a vector of maps
    //     tasks.push_back(Task{.execution_mask = v["mask"],
    //                          .execution_flags = v["flags"],
    //                          .envfile = v["envfile"],
    //                          .datetime = v["time"]});
    //   }
    // }
    // for (const auto& task : tasks) {
    //   executeTask(task);
    // }

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
