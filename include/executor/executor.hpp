#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <string_view>

#include "execxx.hpp"
/** Function Types */
typedef std::function<void(std::string, int, int)> EventCallback;
/** Manager Interface */
class ProcessManager {
 public:
  virtual void request(std::string_view path, int mask, int client_id) = 0;
  virtual void setEventCallback(EventCallback callback_function) = 0;
  virtual void notifyProcessEvent(std::string status, int mask,
                                  int client_id) = 0;
};

const char* findWorkDir(std::string_view path) {
  return path.substr(0, path.find_last_of("/")).data();
}

/** Impl */
std::string run_(std::string_view path) {
  std::vector<std::string> v_args{};
  v_args.push_back(std::string(path));

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
    ProcessDaemon(std::string_view path) : m_path(std::move(path)) {}
    ~ProcessDaemon(){/* Clean up */};
    /** Disable copying */
    ProcessDaemon(const ProcessDaemon&) = delete;
    ProcessDaemon(ProcessDaemon&&) = delete;
    ProcessDaemon& operator=(const ProcessDaemon&) = delete;
    ProcessDaemon& operator=(ProcessDaemon&&) = delete;

    /** Uses async and future to call implementation*/
    std::string run() {
      std::future<std::string> result_future = std::async(&run_, m_path);
      std::string result = result_future.get();
      return result;
    }

   private:
    std::string_view m_path;
  };
  /** Constructor / Destructor */
  ProcessExecutor() {}
  ~ProcessExecutor() { /* Kill processes? Log for processes? */
  }
  /** Disable copying */
  ProcessExecutor(const ProcessExecutor&) = delete;
  ProcessExecutor(ProcessExecutor&&) = delete;
  ProcessExecutor& operator=(const ProcessExecutor&) = delete;
  ProcessExecutor& operator=(ProcessExecutor&&) = delete;
  /** Set the callback */
  virtual void setEventCallback(EventCallback f) override { m_callback = f; }
  /** Callback to be used upon process completion */
  virtual void notifyProcessEvent(std::string status, int mask,
                                  int client_socket_fd) override {
    m_callback(status, mask, client_socket_fd);
  }
  /** Request the running of a process */
  virtual void request(std::string_view path, int mask,
                       int client_socket_fd) override {
    if (path[0] != '\0') {
      ProcessDaemon* pd_ptr = new ProcessDaemon(path);
      auto process_std_out = pd_ptr->run();
      if (!process_std_out.empty()) {
        notifyProcessEvent(process_std_out, mask, client_socket_fd);
      }
      delete pd_ptr;
    }
  }

 private:
  EventCallback m_callback;
  void* m_config;
};
