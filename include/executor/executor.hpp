#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <string_view>

#include "execxx.hpp"
/** Function Types */
typedef std::function<void(std::string, int, int)> EventCallback;
typedef std::function<void(std::string, int, std::string, int)>
    TrackedEventCallback;
/** Manager Interface */
class ProcessManager {
 public:
  virtual void request(std::string_view path, int mask, int client_id,
                       std::vector<std::string> argv) = 0;
  virtual void request(std::string_view path, int mask, int client_id,
                       std::string request_id,
                       std::vector<std::string> argv) = 0;

  virtual void setEventCallback(EventCallback callback_function) = 0;
  virtual void setEventCallback(TrackedEventCallback callback_function) = 0;

  virtual void notifyProcessEvent(std::string status, int mask,
                                  int client_id) = 0;
  virtual void notifyTrackedProcessEvent(std::string status, int mask,
                                         std::string request_id,
                                         int client_id) = 0;
};

const char *findWorkDir(std::string_view path) {
  return path.substr(0, path.find_last_of("/")).data();
}

/** Impl */
std::string run_(std::string_view path, std::vector<std::string> argv) {
  std::vector<std::string> v_args{};
  v_args.push_back(std::string(path));
  for (const auto &arg : argv) {
    v_args.push_back(arg);
  }

  const char *executable_path = path.data();

  std::string work_dir{findWorkDir(path)};

  /* qx wraps calls to fork() and exec() */
  return std::string(qx(v_args, work_dir));
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
    ~ProcessDaemon(){/* Clean up */};
    /** Disable copying */
    ProcessDaemon(const ProcessDaemon &) = delete;
    ProcessDaemon(ProcessDaemon &&) = delete;
    ProcessDaemon &operator=(const ProcessDaemon &) = delete;
    ProcessDaemon &operator=(ProcessDaemon &&) = delete;

    /** Uses async and future to call implementation*/
    std::string run() {
      std::future<std::string> result_future =
          std::async(std::launch::async, &run_, m_path, m_argv);
      std::string result = result_future.get();
      return result;
    }

   private:
    std::string_view m_path;
    std::vector<std::string> m_argv;
  };
  /** Constructor / Destructor */
  ProcessExecutor() {}
  ~ProcessExecutor() {
    std::cout << "Executor destroyed"
              << std::endl; /* Kill processes? Log for processes? */
  }
  /** Disable copying */
  ProcessExecutor(const ProcessExecutor &) = delete;
  ProcessExecutor(ProcessExecutor &&) = delete;
  ProcessExecutor &operator=(const ProcessExecutor &) = delete;
  ProcessExecutor &operator=(ProcessExecutor &&) = delete;
  /** Set the callback */
  virtual void setEventCallback(EventCallback f) override { m_callback = f; }
  virtual void setEventCallback(TrackedEventCallback f) override {
    m_tracked_callback = f;
  }
  /** Callback to be used upon process completion */
  virtual void notifyProcessEvent(std::string status, int mask,
                                  int client_socket_fd) override {
    m_callback(status, mask, client_socket_fd);
  }
  virtual void notifyTrackedProcessEvent(std::string status, int mask,
                                         std::string request_id,
                                         int client_socket_fd) override {
    m_tracked_callback(status, mask, request_id, client_socket_fd);
  }

  /* Request execution of an anonymous task */
  virtual void request(std::string_view path, int mask, int client_socket_fd,
                       std::vector<std::string> argv) override {
    if (path[0] != '\0') {
      ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
      auto process_std_out = pd_ptr->run();
      if (!process_std_out.empty()) {
        notifyProcessEvent(process_std_out, mask, client_socket_fd);
      }
      delete pd_ptr;
    }
  }
  /** Request the running of a process being tracked with an ID */
  virtual void request(std::string_view path, int mask, int client_socket_fd,
                       std::string request_id,
                       std::vector<std::string> argv) override {
    if (path[0] != '\0') {
      ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
      auto process_std_out = pd_ptr->run();
      if (!process_std_out.empty()) {
        notifyTrackedProcessEvent(process_std_out, mask, request_id,
                                  client_socket_fd);
      }
      delete pd_ptr;
    }
  }

 private:
  EventCallback m_callback;
  TrackedEventCallback m_tracked_callback;
  void *m_config;
};
