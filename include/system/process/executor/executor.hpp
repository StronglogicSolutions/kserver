#include <log/logger.h>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <string_view>

#include <system/process/scheduler.hpp>
#include <database/kdb.hpp>
#include "execxx.hpp"
#include "kapplication.hpp"

namespace Executor {

enum ExecutionRequestType { IMMEDIATE = 0, SCHEDULED = 1 };

/** Function Types */
typedef std::function<void(std::string, int, int, bool)> ProcessEventCallback;
typedef std::function<void(std::string, int, std::string, int, bool)>
    TrackedEventCallback;
/** Manager Interface */
class ProcessManager {
 public:
  virtual void request(std::string_view path, int mask, int client_id,
                       std::vector<std::string> argv) = 0;
  virtual void request(std::string_view path, int mask, int client_id,
                       std::string id, std::vector<std::string> argv,
                       ExecutionRequestType type) = 0;

  virtual void setEventCallback(ProcessEventCallback callback_function) = 0;
  virtual void setEventCallback(TrackedEventCallback callback_function) = 0;

  virtual void notifyProcessEvent(std::string status, int mask, int client_id,
                                  bool error) = 0;
  virtual void notifyTrackedProcessEvent(std::string status, int mask,
                                         std::string id, int client_id,
                                         bool error) = 0;
  virtual ~ProcessManager() {}
};

const char *findWorkDir(std::string_view path) {
  return path.substr(0, path.find_last_of("/")).data();
}

/** Impl */
ProcessResult run_(std::string_view path, std::vector<std::string> argv) {
  std::vector<std::string> v_args{};
  v_args.reserve(argv.size() + 1);
  v_args.push_back(std::string(path));
  for (auto&& arg : argv) {
    v_args.push_back(arg);
  }

  std::string work_dir{findWorkDir(path)};

  /* qx wraps calls to fork() and exec() */
  return qx(v_args, work_dir);
}
/** Process Executor - implements Manager interface */
class ProcessExecutor : public ProcessManager {
 public:
  /** Daemon to run process */
  class ProcessDaemon {
   public:
    /** Constructor/Destructor */
    ProcessDaemon(std::string_view path, std::vector<std::string> argv)
        : m_path(std::move(path)), m_argv(std::move(argv)) {}
    ~ProcessDaemon() {/* Clean up */};
    /** Disable copying */
    ProcessDaemon(const ProcessDaemon &) = delete;
    ProcessDaemon(ProcessDaemon &&) = delete;
    ProcessDaemon &operator=(const ProcessDaemon &) = delete;
    ProcessDaemon &operator=(ProcessDaemon &&) = delete;

    /** Uses async and future to call implementation*/
    ProcessResult run() {
      std::future<ProcessResult> result_future =
          std::async(std::launch::async, &run_, m_path, m_argv);
      return result_future.get();
    }

   private:
    std::string_view m_path;
    std::vector<std::string> m_argv;
  };
  /** Constructor / Destructor */
  ProcessExecutor() {}
  virtual ~ProcessExecutor() override {
    std::cout << "Executor destroyed"
              << std::endl; /* Kill processes? Log for processes? */
  }
  /** Disable copying */
  ProcessExecutor(const ProcessExecutor &e)
      : m_callback(e.m_callback), m_tracked_callback(e.m_tracked_callback) {}
  ProcessExecutor(ProcessExecutor &&e)
      : m_callback(e.m_callback), m_tracked_callback(e.m_tracked_callback) {
    e.m_callback = nullptr;
    e.m_tracked_callback = nullptr;
  }

  ProcessExecutor &operator=(const ProcessExecutor &e) {
    this->m_callback = nullptr;
    this->m_tracked_callback = nullptr;
    this->m_callback = e.m_callback;
    this->m_tracked_callback = e.m_tracked_callback;
    return *this;
  };

  ProcessExecutor &operator=(ProcessExecutor &&e) {
    if (&e != this) {
      m_callback = e.m_callback;
      m_tracked_callback = e.m_tracked_callback;
      e.m_callback = nullptr;
      e.m_tracked_callback = nullptr;
    }
    return *this;
  }
  /** Set the callback */
  virtual void setEventCallback(ProcessEventCallback f) override {
    m_callback = f;
  }
  virtual void setEventCallback(TrackedEventCallback f) override {
    m_tracked_callback = f;
  }
  /** Callback to be used upon process completion */
  virtual void notifyProcessEvent(std::string std_out, int mask,
                                  int client_socket_fd, bool error) override {
    m_callback(std_out, mask, client_socket_fd, error);
  }
  virtual void notifyTrackedProcessEvent(std::string std_out, int mask,
                                         std::string id, int client_socket_fd,
                                         bool error) override {
    m_tracked_callback(std_out, mask, id, client_socket_fd, error);
  }

  /* Request execution of an anonymous task */
  virtual void request(std::string_view path, int mask, int client_socket_fd,
                       std::vector<std::string> argv) override {
    if (path[0] != '\0') {
      ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
      auto result = pd_ptr->run();
      if (!result.output.empty()) {
        notifyProcessEvent(result.output, mask, client_socket_fd, result.error);
      }
      delete pd_ptr;
    }
  }
  /** Request the running of a process being tracked with an ID */
  virtual void request(std::string_view path, int mask, int client_socket_fd,
                       std::string id, std::vector<std::string> argv,
                       ExecutionRequestType type) override {
    if (path[0] != '\0') {
      ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
      auto result = pd_ptr->run();
      if (!result.output.empty()) {
        notifyTrackedProcessEvent(result.output, mask, id, client_socket_fd,
                                  result.error);
        if (!result.error && type == ExecutionRequestType::SCHEDULED) {
          // TODO: Get rid of this? Handle in request_handler
          Database::KDB kdb{};
          auto SUCCESS =
              Scheduler::Completed::STRINGS[Scheduler::Completed::SUCCESS];
          std::string result = kdb.update("schedule",               // table
                                          {"completed"},            // field
                                          {SUCCESS},                // value
                                          QueryFilter{{"id", id}},  // filter
                                          "id"  // field value to return
          );
          KLOG("Updated task {} to reflect its completion", result);
        }
      }
      delete pd_ptr;
    }
  }

  void executeTask(int client_socket_fd, Task task) {
    KLOG("Executing task");
    KApplication app_info = getAppInfo(task.execution_mask);
    auto is_ready_to_execute = std::stoi(task.datetime) > 0;  // if close to now
    auto flags = task.execution_flags;
    auto envfile = task.envfile;

    if (is_ready_to_execute) {
      std::string id{std::to_string(task.id)};

      request(ConfigParser::Process::executor(), task.execution_mask,
              client_socket_fd, id, {id}, ExecutionRequestType::SCHEDULED);
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

private:
  ProcessEventCallback m_callback;
  TrackedEventCallback m_tracked_callback;
};
}  // namespace Executor
