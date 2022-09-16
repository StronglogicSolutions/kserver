#pragma once

#include <map>
#include "types.hpp"
// #include "kserver.hpp"

namespace kiq
{
static const char*   FILE_SUCCESS_MSG{"File Save Success"};
static const char*   FILE_FAIL_MSG   {"File Save Failure"};

class KServer;

using EventPayload        = std::vector<std::string>;
using EventHandler  = std::function<void(int32_t, int32_t, const EventPayload&)>;
using SystemDispatchTable = std::map<int32_t, EventHandler>;

class SystemEventHandler
{
public:
  explicit SystemEventHandler(KServer* server);
  void operator()(int32_t fd, int32_t event, const EventPayload& payload);
  void HandleFile_Update                 (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleProcess_Execution_Requested (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduled_Tasks_Ready       (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduled_Tasks_None        (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduler_Fetch             (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduler_Fetch_Tokens      (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduler_Update            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduler_Success           (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduler_Fail              (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleRegistrar_Success           (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleRegistrar_Fail              (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleTask_Fetch_Flags            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleApplication_Fetch_Success   (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleApplication_Fetch_Fail      (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_New_Post           (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Post_Requested     (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Error              (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Request            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Event              (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Info               (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Fetch_Posts        (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandlePlatform_Update             (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleProcess_Complete            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleScheduler_Request           (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleTrigger_Add_Success         (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleTrigger_Add_Fail            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleFiles_Send                  (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleFiles_Send_Ack              (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleFiles_Send_Ready            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleTask_Data                   (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleTask_Data_Final             (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleProcess_Research            (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleProcess_Research_Result     (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleKIQ_IPC_Message             (int32_t client_fd, int32_t event, const EventPayload& payload);
  void HandleTerm_Hits                   (int32_t client_fd, int32_t event, const EventPayload& payload);

private:
  KServer* m_server;

  SystemDispatchTable m_dispatch_table
  {
    { SYSTEM_EVENTS__FILE_UPDATE,
      [this](auto fd, auto evt, const auto& data) { HandleFile_Update(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
      [this](auto fd, auto evt, const auto& data) { HandleProcess_Execution_Requested(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
      [this](auto fd, auto evt, const auto& data) { HandleScheduled_Tasks_Ready(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,
      [this](auto fd, auto evt, const auto& data) { HandleScheduled_Tasks_None(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_FETCH,
      [this](auto fd, auto evt, const auto& data) { HandleScheduler_Fetch(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS,
      [this](auto fd, auto evt, const auto& data) { HandleScheduler_Fetch_Tokens(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_UPDATE,
      [this](auto fd, auto evt, const auto& data) { HandleScheduler_Update(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { HandleScheduler_Success(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_FAIL,
      [this](auto fd, auto evt, const auto& data) { HandleScheduler_Fail(fd, evt, data); }
    },
    { SYSTEM_EVENTS__REGISTRAR_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { HandleRegistrar_Success(fd, evt, data); }
    },
    { SYSTEM_EVENTS__REGISTRAR_FAIL,
      [this](auto fd, auto evt, const auto& data) { HandleRegistrar_Fail(fd, evt, data); }
    },
    { SYSTEM_EVENTS__TASK_FETCH_FLAGS,
      [this](auto fd, auto evt, const auto& data) { HandleTask_Fetch_Flags(fd, evt, data); }
    },
    { SYSTEM_EVENTS__APPLICATION_FETCH_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { HandleApplication_Fetch_Success(fd, evt, data); }
    },
    { SYSTEM_EVENTS__APPLICATION_FETCH_FAIL,
      [this](auto fd, auto evt, const auto& data) { HandleApplication_Fetch_Fail(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_NEW_POST,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_New_Post(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_POST_REQUESTED,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Post_Requested(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_ERROR,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Error(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_REQUEST,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Request(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_EVENT,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Event(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_INFO,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Info(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_FETCH_POSTS,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Fetch_Posts(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PLATFORM_UPDATE,
      [this](auto fd, auto evt, const auto& data) { HandlePlatform_Update(fd, evt, data); }
    },
    { SYSTEM_EVENTS__PROCESS_COMPLETE,
      [this](auto fd, auto evt, const auto& data) { HandleProcess_Complete(fd, evt, data); }
    },
    { SYSTEM_EVENTS__SCHEDULER_REQUEST,
      [this](auto fd, auto evt, const auto& data) { HandleScheduler_Request(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TRIGGER_ADD_SUCCESS,
      [this](auto fd, auto evt, const auto& data) { HandleTrigger_Add_Success(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TRIGGER_ADD_FAIL,
      [this](auto fd, auto evt, const auto& data) { HandleTrigger_Add_Fail(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__FILES_SEND,
      [this](auto fd, auto evt, const auto& data) { HandleFiles_Send(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__FILES_SEND_ACK,
      [this](auto fd, auto evt, const auto& data) { HandleFiles_Send_Ack(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__FILES_SEND_READY,
      [this](auto fd, auto evt, const auto& data) { HandleFiles_Send_Ready(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TASK_DATA,
      [this](auto fd, auto evt, const auto& data) { HandleTask_Data(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TASK_DATA_FINAL,
      [this](auto fd, auto evt, const auto& data) { HandleTask_Data_Final(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__PROCESS_RESEARCH,
      [this](auto fd, auto evt, const auto& data) { HandleProcess_Research(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT,
      [this](auto fd, auto evt, const auto& data) { HandleProcess_Research_Result(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__KIQ_IPC_MESSAGE,
      [this](auto fd, auto evt, const auto& data) { HandleKIQ_IPC_Message(fd, evt, data); }
    },
    {
      SYSTEM_EVENTS__TERM_HITS,
      [this](auto fd, auto evt, const auto& data) { HandleTerm_Hits(fd, evt, data); }
    }
  };
};

} // ns kiq