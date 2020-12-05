#include "executor.hpp"

namespace constants {
const uint8_t IMMEDIATE_REQUEST = 0;
const uint8_t SCHEDULED_REQUEST = 1;
const uint8_t RECURRING_REQUEST = 2;
} // namespace constants

const char* findWorkDir(std::string_view path) {
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

ProcessDaemon::ProcessDaemon(std::string_view path, std::vector<std::string> argv)
  : m_path(std::move(path)), m_argv(std::move(argv)) {}

/** Uses async and future to call implementation*/
ProcessResult ProcessDaemon::run() {
  std::future<ProcessResult> result_future =
      std::async(std::launch::async, &run_, m_path, m_argv);
  return result_future.get();
}

ProcessExecutor::ProcessExecutor(const ProcessExecutor &e)
: m_callback(e.m_callback), m_tracked_callback(e.m_tracked_callback) {}

ProcessExecutor::ProcessExecutor(ProcessExecutor &&e)
: m_callback(e.m_callback), m_tracked_callback(e.m_tracked_callback) {
  e.m_callback         = nullptr;
  e.m_tracked_callback = nullptr;
}

ProcessExecutor& ProcessExecutor::operator=(const ProcessExecutor &e) {
  this->m_callback         = nullptr;
  this->m_tracked_callback = nullptr;
  this->m_callback         = e.m_callback;
  this->m_tracked_callback = e.m_tracked_callback;
  return *this;
};

ProcessExecutor& ProcessExecutor::operator=(ProcessExecutor &&e) {
  if (&e != this) {
    m_callback           = e.m_callback;
    m_tracked_callback   = e.m_tracked_callback;
    e.m_callback         = nullptr;
    e.m_tracked_callback = nullptr;
  }
  return *this;
}

void ProcessExecutor::setEventCallback(ProcessEventCallback f) {
  m_callback = f;
}
void ProcessExecutor::setEventCallback(TrackedEventCallback f) {
  m_tracked_callback = f;
}

void ProcessExecutor::notifyProcessEvent(std::string std_out,
                                         int         mask,
                                         int         client_socket_fd,
                                         bool error) {
  m_callback(std_out, mask, client_socket_fd, error);
}

void ProcessExecutor::notifyTrackedProcessEvent(std::string std_out, int mask,
                                        std::string id, int client_socket_fd,
                                        bool error) {
  m_tracked_callback(std_out, mask, id, client_socket_fd, error);
}

//   /* Request execution of an anonymous task */
void ProcessExecutor::request(std::string_view         path,
                              int                      mask,
                              int                      client_socket_fd,
                              std::vector<std::string> argv) {
  if (path[0] != '\0') {
    ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
    auto result = pd_ptr->run();
    if (!result.output.empty()) {
      (result.output, mask, client_socket_fd, result.error);
    }
    delete pd_ptr;
  }
}
//   /** Request the running of a process being tracked with an ID */
void ProcessExecutor::request(std::string_view         path,
                              int                      mask,
                              int                      client_socket_fd,
                              std::string              id,
                              std::vector<std::string> argv,
                              uint8_t                  type) {
  if (path[0] != '\0') {
    ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
    auto result = pd_ptr->run();
    if (!result.output.empty()) {
      notifyTrackedProcessEvent(result.output, mask, id, client_socket_fd,
                                result.error);
      if (!result.error && type != constants::IMMEDIATE_REQUEST) {
        Database::KDB kdb{};

        auto COMPLETED =
          type == constants::RECURRING_REQUEST ?
           Scheduler::Completed::STRINGS[Scheduler::Completed::SCHEDULED] :
           Scheduler::Completed::STRINGS[Scheduler::Completed::SUCCESS];

        std::string result = kdb.update("schedule",               // table
                                        {"completed"},            // field
                                        {COMPLETED},              // value
                                        QueryFilter{{"id", id}},  // filter
                                        "id"  // field value to return
        );
        KLOG("Updated task {} to reflect its completion", result);
      }
    }
    delete pd_ptr;
  }
}

void ProcessExecutor::executeTask(int client_socket_fd, Task task) {
  KLOG("Executing task");

  Environment environment{};
  environment.setTask(task);

  if (environment.prepareRuntime()) {
    ExecutionState exec_state = environment.get();
    request(
      exec_state.path,
      task.execution_mask,
      client_socket_fd,
      std::to_string(task.id),
      exec_state.argv,
      (task.recurring) ?
        constants::RECURRING_REQUEST :
        constants::SCHEDULED_REQUEST
    );
  }
}

KApplication ProcessExecutor::getAppInfo(int mask) {
  Database::KDB kdb{}; KApplication k_app{};

  QueryValues values = kdb.select(
    "apps",                           // table
    {
      "path", "data", "name"          // fields
    },
    {
      {"mask", std::to_string(mask)}  // filter
    }
  );

  for (const auto &value_pair : values) {
    if (value_pair.first == "path")
      k_app.path = value_pair.second;
    else
    if (value_pair.first == "data")
      k_app.data = value_pair.second;
    else
    if (value_pair.first == "name")
      k_app.name = value_pair.second;
  }
  return k_app;
}
