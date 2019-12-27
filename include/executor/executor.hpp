#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <string_view>

#include "execxx.hpp"
/** Function Types */
typedef std::function<std::string(std::string)> EventCallback;
/** Manager Interface */
class ProcessManager {
 public:
  virtual void request(std::string_view path) = 0;
  virtual void setEventCallback(EventCallback callback_function) = 0;
  virtual void notifyProcessEvent(std::string status) = 0;
};

/** Impl */
    std::string run_(std::string_view path) {
      std::vector<std::string> v_args{};
      v_args.push_back(std::string(path));
      v_args.push_back(std::string("--test"));
      /* qx wraps calls to fork() and exec() */
      return std::string(qx(v_args));
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
      std::future<std::string> result_future =
          std::async(&run_, m_path);
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
  virtual void notifyProcessEvent(std::string status) override {
    m_callback(status);
  }
  /** Request the running of a process */
  virtual void request(std::string_view path) override {
    if (path[0] != '\0') {
      ProcessDaemon* pd_ptr = new ProcessDaemon(path);
      auto process_std_out = pd_ptr->run();
      if (!process_std_out.empty()) {
        std::string status_report{"Result from "};
        status_report += path;
        status_report += "\n";
        status_report += process_std_out;
        notifyProcessEvent(status_report);
      }
    }
  }

 private:
  EventCallback m_callback;
  void* m_config;
};
