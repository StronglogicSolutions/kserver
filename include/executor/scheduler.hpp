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

typedef std::function<void(std::string, int, int, int)> EventCallback;

struct Task {
  int execution_mask;
  std::string datetime;
  bool file;
  std::vector<std::string> file_names;
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

    std::string insert_id =
        kdb.insert("schedule", {"time", "mask", "flags", "envfile"},
                   {task.datetime, std::to_string(task.execution_mask),
                    task.execution_flags, task.envfile},
                   "id");
    KLOG->info("Request to schedule task was {}\nID {}",
               !insert_id.empty() ? "Accepted" : "Rejected", insert_id);

    if (!insert_id.empty()) {
      for (const auto &filename : task.file_names) {
        KLOG->info("Recording file in DB: {}", filename);
        kdb.insert("file", {"name", "sid"}, {filename, insert_id});
      }
    }
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

  void onProcessComplete(std::string value, int mask, int id, int client_fd) {
    KLOG->info("Value returned from process:\n{}", value);

    if (true) {  // if success - how do we determine this from the output?
      Database::KDB kdb{};

      QueryFilter filter{{"id", std::to_string(id)}};
      std::string result = kdb.update("schedule", {"completed"}, {"true"}, filter, "id");

      if (!result.empty()) {
        KLOG->info("Updated task {} to reflect its completion", result);
      }
      // TODO: We need to discriminate success and failure
      if (m_event_callback != nullptr) {
        m_event_callback(value, mask, id, client_fd);
      }
    }
  }

  std::vector<Task> parseTasks(QueryValues&& result) {
    int id{};
    std::string mask, flags, envfile, time, filename;
    std::vector<Task> tasks;
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
      if (v.first == "id") {
        id = std::stoi(v.second);
      }
      if (!envfile.empty() && !flags.empty() && !time.empty() &&
          !mask.empty() && id > 0) {
        // TODO: Get files and add to task before pushing into vector
        tasks.push_back(
            Task{.execution_mask = std::stoi(mask),
                 .datetime = time,
                 .file = true,  // Change this default value later after we
                                // implement booleans in the DB abstraction
                 .file_names = {},
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
    return tasks;
  }

  virtual std::vector<Task> fetchTasks() {
    Database::KDB kdb{};  // get DB
    std::string current_timestamp = std::to_string(TimeUtils::unixtime());
    std::string future_timestamp_24hr =
        std::to_string(TimeUtils::unixtime() + 86400);
    return parseTasks(kdb.selectMultiFilter(
        "schedule",                                  // table
        {"id", "time", "mask", "flags", "envfile"},  // fields
        {{GenericFilter{.comparison =
                            {// BETWEEN
                             "time", current_timestamp, future_timestamp_24hr},
                        .type = FilterTypes::COMPARISON},
          GenericFilter{.comparison =
                            {// EQUALS
                             "completed", "=", "false"},
                        .type = FilterTypes::STANDARD}}}));
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
          [this, task](std::string result, int mask, int client_socket_fd) {
            onProcessComplete(result, mask, task.id, client_socket_fd);
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
