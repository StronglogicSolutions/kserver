#pragma once

#include <map>
#include <vector>
#include <functional>
#include "types.hpp"

namespace kiq
{
static const char*   FILE_SUCCESS_MSG{"File Save Success"};
static const char*   FILE_FAIL_MSG   {"File Save Failure"};

class KServer;

using event_payload_t = std::vector<std::string>;
using event_handler_t = std::function<void(int32_t, int32_t, const event_payload_t&)>;
using sys_dispatch_t  = std::map<int32_t, event_handler_t>;

class evt
{
public:
  void set_server(KServer* server);
  void operator()                     (int32_t fd, int32_t evt, const event_payload_t& data);
  void operator()                     (            int32_t evt, const event_payload_t& data);
  void on_file_update                 (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_execution_requested         (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_tasks_ready                 (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_tasks_none                  (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_scheduler_fetch             (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_scheduler_tokens            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_scheduler_update            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_scheduler_success           (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_scheduler_fail              (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_registrar_success           (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_registrar_fail              (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_task_flags                  (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_application_fetch_success   (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_application_fetch_fail      (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_new_post           (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_created            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_post_request       (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_error              (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_request            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_event              (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_info               (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_info_request       (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_fetch_posts        (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_platform_update             (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_process_complete            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_scheduler_request           (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_trigger_add_success         (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_trigger_add_fail            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_files_send                  (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_files_send_ack              (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_files_send_ready            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_task_data                   (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_task_data_final             (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_process_research            (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_process_research_result     (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_KIQ_ipc_Message             (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_term_hits                   (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_status_report               (int32_t fd, int32_t evt, const event_payload_t& data);
  void on_ipc_reconnect_request       (int32_t fd, int32_t evt, const event_payload_t& data);

  static evt& instance();

private:
  evt() = default;

  static evt* s_instance_ptr_;
         KServer*       m_server;

  sys_dispatch_t m_dispatch_table
  {
    { SYSTEM_EVENTS__FILE_UPDATE,
      [this](auto fd, auto evt, const auto& data) { on_file_update(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
      [this](auto fd, auto evt, const auto& data) { on_execution_requested(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
      [this](auto fd, auto evt, const auto& data) { on_tasks_ready(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,
      [this](auto fd, auto evt, const auto& data) { on_tasks_none(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_FETCH,
      [this](auto fd, auto evt, const auto& data) { on_scheduler_fetch(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS,
      [this](auto fd, auto evt, const auto& data) { on_scheduler_tokens(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_UPDATE,
      [this](auto fd, auto evt, const auto& data) { on_scheduler_update(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { on_scheduler_success(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_FAIL,
      [this](auto fd, auto evt, const auto& data) { on_scheduler_fail(fd, evt, data); }
    },
    { SYSTEM_EVENTS__REGISTRAR_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { on_registrar_success(fd, evt, data); }
    },
    { SYSTEM_EVENTS__REGISTRAR_FAIL,
      [this](auto fd, auto evt, const auto& data) { on_registrar_fail(fd, evt, data); }
    },
    { SYSTEM_EVENTS__TASK_FETCH_FLAGS,
      [this](auto fd, auto evt, const auto& data) { on_task_flags(fd, evt, data); }
    },
    { SYSTEM_EVENTS__APPLICATION_FETCH_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { on_application_fetch_success(fd, evt, data); }
    },
    { SYSTEM_EVENTS__APPLICATION_FETCH_FAIL,
      [this](auto fd, auto evt, const auto& data) { on_application_fetch_fail(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__PLATFORM_CREATED,
      [this](auto fd, auto evt, const auto& data) { on_platform_created(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_NEW_POST,
      [this](auto fd, auto evt, const auto& data) { on_platform_new_post(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_POST_REQUESTED,
      [this](auto fd, auto evt, const auto& data) { on_platform_post_request(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_ERROR,
      [this](auto fd, auto evt, const auto& data) { on_platform_error(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_REQUEST,
      [this](auto fd, auto evt, const auto& data) { on_platform_request(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_EVENT,
      [this](auto fd, auto evt, const auto& data) { on_platform_event(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_INFO,
      [this](auto fd, auto evt, const auto& data) { on_platform_info(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_INFO_REQUEST,
      [this](auto fd, auto evt, const auto& data) { on_platform_info_request(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_FETCH_POSTS,
      [this](auto fd, auto evt, const auto& data) { on_platform_fetch_posts(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_UPDATE,
      [this](auto fd, auto evt, const auto& data) { on_platform_update(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PROCESS_COMPLETE,
      [this](auto fd, auto evt, const auto& data) { on_process_complete(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_REQUEST,
      [this](auto fd, auto evt, const auto& data) { on_scheduler_request(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TRIGGER_ADD_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { on_trigger_add_success(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TRIGGER_ADD_FAIL,
      [this](auto fd, auto evt, const auto& data) { on_trigger_add_fail(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__FILES_SEND,
      [this](auto fd, auto evt, const auto& data) { on_files_send(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__FILES_SEND_ACK,
      [this](auto fd, auto evt, const auto& data) { on_files_send_ack(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__FILES_SEND_READY,
      [this](auto fd, auto evt, const auto& data) { on_files_send_ready(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TASK_DATA,
      [this](auto fd, auto evt, const auto& data) { on_task_data(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TASK_DATA_FINAL,
      [this](auto fd, auto evt, const auto& data) { on_task_data_final(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__PROCESS_RESEARCH,
      [this](auto fd, auto evt, const auto& data) { on_process_research(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT,
      [this](auto fd, auto evt, const auto& data) { on_process_research_result(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__KIQ_IPC_MESSAGE,
      [this](auto fd, auto evt, const auto& data) { on_KIQ_ipc_Message(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TERM_HITS,
      [this](auto fd, auto evt, const auto& data) { on_term_hits(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__STATUS_REPORT,
      [this](auto fd, auto evt, const auto& data) { on_status_report(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__IPC_RECONNECT_REQUEST,
      [this](auto fd, auto evt, const auto& data) { on_ipc_reconnect_request(fd, evt, data); }
    }
  };
};

using event_handler = evt;
} // ns kiq