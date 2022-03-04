#pragma once

#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <string>

#include "log/logger.h"
#include "execxx.hpp"
#include "environment.hpp"
#include "kapplication.hpp"

namespace kiq {
namespace constants {
extern const uint8_t IMMEDIATE_REQUEST;
extern const uint8_t SCHEDULED_REQUEST;
} // namespace constants

/** Function Types */
typedef std::function<void(std::string, int, int, bool)> ProcessEventCallback;
typedef std::function<void(std::string, int, std::string, int, bool)>
    TrackedEventCallback;
/** Manager Interface */
class ProcessManager {
 public:
  virtual void request(std::string path, int mask, int client_id,
                       std::vector<std::string> argv) = 0;
  virtual void request(std::string path, int mask, int client_id,
                       std::string id, std::vector<std::string> argv,
                       uint8_t type) = 0;

  virtual void setEventCallback(ProcessEventCallback callback_function) = 0;
  virtual void setEventCallback(TrackedEventCallback callback_function) = 0;

  virtual void notifyProcessEvent(std::string status, int mask, int client_id,
                                  bool error) = 0;
  virtual void notifyTrackedProcessEvent(std::string status, int mask,
                                         std::string id, int client_id,
                                         bool error) = 0;
  virtual ~ProcessManager() {}
};

/** Impl */
ProcessResult run_(std::string path, std::vector<std::string> argv);

/** Daemon to run process */
class ProcessDaemon {
  public:
  /** Constructor/Destructor */
  ProcessDaemon(std::string path, std::vector<std::string> argv);
  ~ProcessDaemon() {};
  /** Disable copying */
  ProcessDaemon(const ProcessDaemon &)            = delete;
  ProcessDaemon(ProcessDaemon &&)                 = delete;
  ProcessDaemon &operator=(const ProcessDaemon &) = delete;
  ProcessDaemon &operator=(ProcessDaemon &&)      = delete;

  /** Uses async and future to call implementation*/
  ProcessResult run();

  private:
  std::string m_path;
  std::vector<std::string> m_argv;
};

/** Process Executor - implements Manager interface */
class ProcessExecutor : public ProcessManager {
 public:
  /** Constructor / Destructor */
  ProcessExecutor() {}
  virtual ~ProcessExecutor() override {
    std::cout << "Executor destroyed"
              << std::endl; /* Kill processes? Log for processes? */
  }

  ProcessExecutor(const ProcessExecutor &e);
  ProcessExecutor(      ProcessExecutor &&e);
  ProcessExecutor &operator=(const ProcessExecutor &e);
  ProcessExecutor &operator=(      ProcessExecutor &&e);

  /** Set the callback */
  virtual void setEventCallback(ProcessEventCallback f) override;
  virtual void setEventCallback(TrackedEventCallback f) override;
  /** Callback to be used upon process completion */
  virtual void notifyProcessEvent(std::string std_out, int mask,
                                  int client_socket_fd, bool error) override;

  virtual void notifyTrackedProcessEvent(std::string std_out, int mask,
                                         std::string id, int client_socket_fd,
                                         bool error) override;

  /* Request execution of an anonymous task */
  virtual void request(std::string         path,
                       int                      mask,
                       int                      client_socket_fd,
                       std::vector<std::string> argv) override;

  /** Request the running of a process being tracked with an ID */
  virtual void request(std::string         path,
                       int                      mask,
                       int                      client_socket_fd,
                       std::string              id,
                       std::vector<std::string> argv,
                       uint8_t                  type);

  void executeTask(int client_socket_fd, Task task);

  template <typename T = uint8_t>
  bool saveResult(uint32_t mask, T status, uint32_t time);
  template <typename T = std::string>
  static KApplication GetAppInfo(const int32_t& mask = -1, const T& name = "");
private:
  ProcessEventCallback m_callback;
  TrackedEventCallback m_tracked_callback;
};
} // ns kiq
