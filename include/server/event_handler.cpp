#include "kserver.hpp"

namespace kiq
{

SystemEventHandler::SystemEventHandler(KServer* server)
: m_server(server)
{}
//**********************************************************//
void SystemEventHandler::operator()(int32_t fd, int32_t event, const EventPayload& payload)
{
  m_dispatch_table[event](fd, event, payload);
}
//*****************************File_Update******************//
void SystemEventHandler::HandleFile_Update (int32_t client_fd, int32_t event, const EventPayload& payload)
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
//*****************************Process_Execution_Requested**//
void SystemEventHandler::HandleProcess_Execution_Requested (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Process Execution Requested", payload);
}
//*****************************Scheduled_Tasks_Ready********//
void SystemEventHandler::HandleScheduled_Tasks_Ready (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  KLOG("Maintenance worker found tasks");
  m_server->SendEvent(client_fd, "Scheduled Tasks Ready", payload);
}
//*****************************Scheduled_Tasks_None*********//
void SystemEventHandler::HandleScheduled_Tasks_None (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  KLOG("There are currently no tasks ready for execution.");
  m_server->SendEvent(client_fd, "No tasks ready", payload);
}
//*****************************Scheduler_Fetch**************//
void SystemEventHandler::HandleScheduler_Fetch (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Scheduled Tasks", payload);
}
//*****************************Scheduler_Fetch_Tokens*******//
void SystemEventHandler::HandleScheduler_Fetch_Tokens (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Schedule Tokens", payload);
}
//*****************************Scheduler_Update*************//
void SystemEventHandler::HandleScheduler_Update (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Schedule PUT", payload);
}
//*****************************Scheduler_Success************//
void SystemEventHandler::HandleScheduler_Success (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  KLOG("Task successfully scheduled");
  m_server->SendEvent(client_fd, "Task Scheduled", payload);
}
//*****************************Scheduler_Fail***************//
void SystemEventHandler::HandleScheduler_Fail (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************Registrar_Success************//
void SystemEventHandler::HandleRegistrar_Success (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Application was registered", payload);
}
//*****************************Registrar_Fail***************//
void SystemEventHandler::HandleRegistrar_Fail (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Failed to register application", payload);
}
//*****************************Task_Fetch_Flags*************//
void SystemEventHandler::HandleTask_Fetch_Flags (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Application Flags", payload);
}
//*****************************Application_Fetch_Success****//
void SystemEventHandler::HandleApplication_Fetch_Success (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Application was found", payload);
}
//*****************************Application_Fetch_Fail*******//
void SystemEventHandler::HandleApplication_Fetch_Fail (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Application was not found", payload);
}
//*****************************Platform_New_Post************//
void SystemEventHandler::HandlePlatform_New_Post (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  VLOG("Handling Platform New Post");
  m_server->GetController().ProcessSystemEvent(SYSTEM_EVENTS__PLATFORM_NEW_POST, payload);
  m_server->SendEvent(client_fd, "Platform Post", payload);
}
//*****************************Platform_Post_Requested******//
void SystemEventHandler::HandlePlatform_Post_Requested (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  VLOG("Handling Platform Post Requested");
   if (payload.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX) == "bot")
    m_server->GetIPCMgr().ReceiveEvent(event, payload);
  else
    m_server->GetController().ProcessSystemEvent(SYSTEM_EVENTS__PLATFORM_ERROR, payload);
}
//*****************************Platform_Error***************//
void SystemEventHandler::HandlePlatform_Error(int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->GetController().ProcessSystemEvent(event, payload);
  ELOG("Error processing platform post: {}", payload.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX));
  m_server->Broadcast("Platform Error", payload);
}
//*****************************Platform_Request*************//
void SystemEventHandler::HandlePlatform_Request    (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  VLOG("Handling Platform Request");
  m_server->GetController().ProcessSystemEvent(event, payload);
}
//*****************************Platform_Event***************//
void SystemEventHandler::HandlePlatform_Event      (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->GetIPCMgr().ReceiveEvent(event, payload);
}
//*****************************Platform_Info****************//
void SystemEventHandler::HandlePlatform_Info       (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Platform Info", payload);
}
//*****************************Platform_Fetch_Posts*********//
void SystemEventHandler::HandlePlatform_Fetch_Posts(int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->Broadcast("Platform Posts", payload);
}
//*****************************Platform_Update**************//
void SystemEventHandler::HandlePlatform_Update     (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  VLOG("Handling Platform Update");
  m_server->Broadcast("Platform Update", payload);
}
//*****************************Process_Complete*************//
void SystemEventHandler::HandleProcess_Complete    (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************Scheduler_Request************//
void SystemEventHandler::HandleScheduler_Request   (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************Trigger_Add_Success**********//
void SystemEventHandler::HandleTrigger_Add_Success (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************Trigger_Add_Fail*************//
void SystemEventHandler::HandleTrigger_Add_Fail    (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************Files_Send*******************//
void SystemEventHandler::HandleFiles_Send          (int32_t fd, int32_t event, const EventPayload& files)
{
  m_server->GetFileMgr().EnqueueOutbound(fd, files);
  m_server->SendEvent(fd, "File Upload", files);
}
//*****************************Files_Send_Ack***************//
void SystemEventHandler::HandleFiles_Send_Ack (int32_t fd, int32_t event, const EventPayload& payload)
{
  auto& file = m_server->GetFileMgr().OutboundNext();
  m_server->SendEvent(file.fd, "File Upload Meta", file.file.to_string_v());
}
//*****************************Files_Send_Ready*************//
void SystemEventHandler::HandleFiles_Send_Ready (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  auto&& file = m_server->GetFileMgr().Dequeue();
  m_server->SendFile(file.fd, file.file.name);
}
//*****************************Task_Data********************//
void SystemEventHandler::HandleTask_Data (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Task Data", payload);
}
//*****************************Task_Data_Final**************//
void SystemEventHandler::HandleTask_Data_Final (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Task Data Final", payload);
}
//*****************************Process_Research*************//
void SystemEventHandler::HandleProcess_Research (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************Process_Research_Result******//
void SystemEventHandler::HandleProcess_Research_Result (int32_t client_fd, int32_t event, const EventPayload& payload)
{

}
//*****************************KIQ_IPC_Message**************//
void SystemEventHandler::HandleKIQ_IPC_Message (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->GetIPCMgr().ReceiveEvent(SYSTEM_EVENTS__IPC_REQUEST, payload);
}
//*****************************Term_Hits********************//
void SystemEventHandler::HandleTerm_Hits (int32_t client_fd, int32_t event, const EventPayload& payload)
{
  m_server->SendEvent(client_fd, "Term Hits", payload);
}
//**********************************************************//
}
