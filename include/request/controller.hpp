#pragma once

#include <stdlib.h>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>
#include <chrono>
#include <condition_variable>

#include "common/util.hpp"
#include "log/logger.h"
#include "codec/kmessage_generated.h"
#include "config/config_parser.hpp"
#include "database/kdb.hpp"
#include "system/process/executor/executor.hpp"
#include "system/process/scheduler.hpp"
#include "system/process/registrar.hpp"
#include "system/process/executor/task_handlers/instagram.hpp"
#include "system/process/executor/task_handlers/generic.hpp"
#include "server/types.hpp"
#include "system/cron.hpp"
#include "types.hpp"

namespace kiq::Request {
enum DevTest {
  Schedule = 1,
  ExecuteTask = 2
};

using namespace KData;

/**
 * Controller
 *
 * Handles incoming requests coming from the KY_GUI Application
 */
class Controller {
 using ProcessCallbackFn = std::function<void(std::string, int, std::string, int, bool)>;
 using SystemCallbackFn  = std::function<void(int, int, std::vector<std::string>)>;
 using StatusCallbackFn  = std::function<void(void)>;

 public:
  Controller();
  Controller(Controller &&r);
  Controller(const Controller &r);
  Controller &operator=(const Controller &handler);
  Controller &operator=(Controller &&handler);
  ~Controller();


  void                      initialize(ProcessCallbackFn  event_callback_fn,
                                       SystemCallbackFn system_callback_fn,
                                       StatusCallbackFn status_callback_fn);
  void                      shutdown();
  void                      SetWait(const bool& wait);

  Scheduler                 getScheduler();
  void                      InfiniteLoop();
  void                      handlePendingTasks();
  void                      operator()(const KOperation&               op,
                                       const std::vector<std::string>& argv,
                                       const int32_t&                  client_socket_fd,
                                       const std::string&              uuid);
  std::vector<KApplication> CreateSession();
  void                      Execute   (const uint32_t&    mask,
                                       const std::string& request_id,
                                       const int32_t&     client_socket_fd);
  void                      process_system_event(const int32_t&                  event,
                                                 const std::vector<std::string>& payload,
                                                 const int32_t&                  id = 0);
  void                      process_client_request(const int32_t&     client_fd,
                                                   const std::string& message);

 private:
  void onProcessComplete(const std::string& value,
                         const int32_t&     mask,
                         const std::string& id,
                         const int32_t&     client_socket_fd,
                         const bool&        error,
                         const bool&        scheduled_task = false);
  void onSchedulerEvent(const int32_t&                  client_socket_fd,
                        const int32_t&                  event,
                        const std::vector<std::string>& args = {});
  void Status() const;

  ProcessCallbackFn                 m_process_event;
  SystemCallbackFn                  m_system_event;
  StatusCallbackFn                  m_server_status;

  std::map<int, std::vector<Task>>  m_tasks_map;
  std::mutex                        m_mutex;
  std::condition_variable           m_condition;
  std::atomic<bool>                 m_wait;
  bool                              m_active;

  Registrar::Registrar              m_registrar;
  ProcessExecutor*                  m_executor;
  Scheduler                         m_scheduler;
  std::thread                       m_maintenance_worker;
  Database::KDB                     m_kdb;

  uint32_t                          m_ps_exec_count;
  uint32_t                          m_client_rq_count;
  uint32_t                          m_system_rq_count;
  uint32_t                          m_err_count;
};
}  // ns kiq::Request
