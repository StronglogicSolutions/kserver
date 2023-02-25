#include "kserver.hpp"

namespace kiq
{

SystemEventHandler::SystemEventHandler(KServer* server)
: m_server(server)
{}
//------------------------------------------------------------
void
SystemEventHandler::operator()(int32_t fd, int32_t event, const event_payload_t& payload)
{
  m_dispatch_table[event](fd, event, payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_file_update (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  const std::string& filename     = payload.at(0);
  const auto&        timestamp    = payload.at(1);
  auto&              file_manager = m_server->GetFileMgr();
  KLOG("Updating information file information for client {}'s file received at {}", client_fd, timestamp);

  if (auto it = file_manager.FindReceived(client_fd, timestamp); file_manager.ReceivedExists(it))
  {
    KLOG("Data buffer found. Creating directory and saving file");
    file_manager.SaveFile(it, filename);
    file_manager.EraseReceived(it);

    if (payload.at(3) == "final file")
      m_server->EraseFileHandler(client_fd);
    m_server->SendEvent(client_fd, FILE_SUCCESS_MSG, {timestamp});
  }
  else
  {
    ELOG("Unable to find file");
    m_server->SendEvent(client_fd, FILE_FAIL_MSG, {timestamp});
  }
}
//------------------------------------------------------------
void
SystemEventHandler::on_execution_requested (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Process Execution Requested", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_tasks_ready (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  KLOG("Maintenance worker found tasks");
  m_server->SendEvent(client_fd, "Scheduled Tasks Ready", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_tasks_none (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  KLOG("There are currently no tasks ready for execution.");
  m_server->SendEvent(client_fd, "No tasks ready", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_scheduler_fetch (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Scheduled Tasks", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_scheduler_tokens (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Schedule Tokens", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_scheduler_update (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Schedule PUT", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_scheduler_success (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  KLOG("Task successfully scheduled");
  m_server->SendEvent(client_fd, "Task Scheduled", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_scheduler_fail (int32_t client_fd, int32_t event, const event_payload_t& payload)
{

}
//------------------------------------------------------------
void
SystemEventHandler::on_registrar_success (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Application was registered", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_registrar_fail (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Failed to register application", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_task_flags (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Application Flags", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_application_fetch_success (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Application was found", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_application_fetch_fail (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Application was not found", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_new_post (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  VLOG("Handling Platform New Post");
  m_server->GetController().ProcessSystemEvent(SYSTEM_EVENTS__PLATFORM_NEW_POST, payload);
  m_server->SendEvent(client_fd, "Platform Post", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_post_request (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  VLOG("Handling Platform Post Requested");
   if (payload.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX) == "bot")
    m_server->GetIPCMgr().ReceiveEvent(event, payload);
  else
    m_server->GetController().ProcessSystemEvent(SYSTEM_EVENTS__PLATFORM_ERROR, payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_error(int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->GetController().ProcessSystemEvent(event, payload);
  ELOG("Error processing platform post: {}", payload.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX));
  m_server->Broadcast("Platform Error", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_request    (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  VLOG("Handling Platform Request");
  m_server->GetController().ProcessSystemEvent(event, payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_event      (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->GetIPCMgr().ReceiveEvent(event, payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_info       (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Platform Info", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_fetch_posts(int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->Broadcast("Platform Posts", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_platform_update     (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  VLOG("Handling Platform Update");
  m_server->Broadcast("Platform Update", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_process_complete    (int32_t client_fd, int32_t event, const event_payload_t& payload)
{

}
//------------------------------------------------------------
void
SystemEventHandler::on_scheduler_request   (int32_t client_fd, int32_t event, const event_payload_t& payload)
{

}
//------------------------------------------------------------
void
SystemEventHandler::on_trigger_add_success (int32_t client_fd, int32_t event, const event_payload_t& payload)
{

}
//------------------------------------------------------------
void
SystemEventHandler::on_trigger_add_fail    (int32_t client_fd, int32_t event, const event_payload_t& payload)
{

}
//------------------------------------------------------------
void
SystemEventHandler::on_files_send          (int32_t fd, int32_t event, const event_payload_t& files)
{
  KLOG("Enqueuing and sending files");
  m_server->GetFileMgr().EnqueueOutbound(fd, files);
  m_server->SendEvent(fd, "File Upload", files);
}
//------------------------------------------------------------
void
SystemEventHandler::on_files_send_ack (int32_t fd, int32_t event, const event_payload_t& payload)
{
  KLOG("Acknowledging file send request. Sending file metadata");
  auto& file = m_server->GetFileMgr().OutboundNext();
  m_server->SendEvent(file.fd, "File Upload Meta", file.file.to_string_v());
}
//------------------------------------------------------------
void
SystemEventHandler::on_files_send_ready (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  KLOG("Ready. Sending one file");
  auto&& file = m_server->GetFileMgr().Dequeue();
  m_server->SendFile(file.fd, file.file.name);
}
//------------------------------------------------------------
void
SystemEventHandler::on_task_data (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  KLOG("Sending task data");
  m_server->SendEvent(client_fd, "Task Data", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_task_data_final (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  KLOG("Sending FINAL task data");
  m_server->SendEvent(client_fd, "Task Data Final", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_process_research (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  VLOG("not implemented");
}
//------------------------------------------------------------
void
SystemEventHandler::on_process_research_result (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  VLOG("not implemented");
}
//------------------------------------------------------------
void
SystemEventHandler::on_KIQ_ipc_Message (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->GetIPCMgr().ReceiveEvent(SYSTEM_EVENTS__IPC_REQUEST, payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_term_hits (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Term Hits", payload);
}
//------------------------------------------------------------
void
SystemEventHandler::on_status_report (int32_t client_fd, int32_t event, const event_payload_t& payload)
{
  m_server->SendEvent(client_fd, "Status Report", payload);
}
//------------------------------------------------------------
}
