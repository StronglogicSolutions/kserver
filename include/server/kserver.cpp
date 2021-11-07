#include "kserver.hpp"
#include "kiqoder/file_iterator.hpp"

namespace KYO {

/**
 * \mainpage The KServer implements logicp's SocketListener and provides the KIQ
 * service to KStyleYo
 */
/**
 * Constructor
 */
KServer::KServer(int argc, char **argv)
: SocketListener(argc, argv),
  m_ipc_manager(
  [this](int32_t event, const std::vector<std::string>& payload)
  {
    systemEventNotify(ALL_CLIENTS, event, payload);
  }),
  m_file_pending(false),
  m_file_pending_fd(-1)
  {
    KLOG("Starting IPC manager");
    m_ipc_manager.start();
  }

/**
 * @destructor
 */
KServer::~KServer()
{
  KLOG("Server shutting down");
  m_file_handlers.clear();
  m_controller.shutdown();
}

/**
 * systemEventNotify
 *
 * Handles notifications sent back to the system delivering event messages
 *
 * @param[in] {int} client_socket_fd
 * @param[in] {int} system_event
 * @param[in] {std::vector<std::string>}
 *
 */
void KServer::systemEventNotify(int client_socket_fd, int system_event,
                        std::vector<std::string> args)
{
  switch (system_event)
  {
    case SYSTEM_EVENTS__SCHEDULED_TASKS_READY:
      if (client_socket_fd == -1)
      {
        KLOG("Maintenance worker found tasks. Sending system-wide broadcast to all clients.");
        for (const auto &session : m_sessions)
          sendEvent(session.fd, "Scheduled Tasks Ready", args);
      }
      else
      {
        KLOG("Informing client {} about scheduled tasks", client_socket_fd);
        sendEvent(client_socket_fd, "Scheduled Tasks Ready", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULED_TASKS_NONE:
      if (client_socket_fd == -1)
      {
        KLOG("Sending system-wide broadcast. There are currently no tasks ready for execution.");
        for (const auto &session : m_sessions)
          sendEvent(session.fd, "No tasks ready", args);
      }
      else
      {
        KLOG("Informing client {} about scheduled tasks", client_socket_fd);
        sendEvent(client_socket_fd, "No tasks ready to run", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_FETCH:
      if (client_socket_fd != -1)
      {
        KLOG("Sending schedule fetch results to client {}", client_socket_fd);
        sendEvent(client_socket_fd, "Scheduled Tasks", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_UPDATE:
      if (client_socket_fd != -1)
      {
        KLOG("Sending schedule update result to client {}", client_socket_fd);
        sendEvent(client_socket_fd, "Schedule PUT", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS:
      if (client_socket_fd != -1)
      {
        KLOG("Sending schedule flag values to client {}", client_socket_fd);
        sendEvent(client_socket_fd, "Schedule Tokens", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_SUCCESS:
      KLOG("Task successfully scheduled");
      if (client_socket_fd == -1)
        for (const auto &session : m_sessions)
          sendEvent(session.fd, "Task Scheduled", args);
       else
        sendEvent(client_socket_fd, "Task Scheduled", args);
    break;
    case SYSTEM_EVENTS__PLATFORM_NEW_POST:
    {
      KLOG("Platform Post event received");
      m_controller.process_system_event(SYSTEM_EVENTS__PLATFORM_NEW_POST, args);

      std::vector<std::string> outgoing_args{};
      outgoing_args.reserve(args.size());
      for (const auto& arg : args)
        outgoing_args.emplace_back(arg);

      if (client_socket_fd == -1)
        for (const auto &session : m_sessions)
          sendEvent(session.fd, "Platform Post", args);
      else
        sendEvent(client_socket_fd, "Platform Post", args);
    }
    break;
    case SYSTEM_EVENTS__PLATFORM_POST_REQUESTED:
      if (args.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX) == "bot")
        m_ipc_manager.ReceiveEvent(SYSTEM_EVENTS__PLATFORM_POST_REQUESTED, args);
      else
      {
        m_controller.process_system_event(SYSTEM_EVENTS__PLATFORM_ERROR, args);
        KLOG("Platform Post requested: Must implement process execution");
      }
    break;
    case SYSTEM_EVENTS__KIQ_IPC_MESSAGE:
        m_ipc_manager.process(args.front(), client_socket_fd);
    break;
    case SYSTEM_EVENTS__PLATFORM_ERROR:
      m_controller.process_system_event(SYSTEM_EVENTS__PLATFORM_ERROR, args);
      ELOG("Error processing platform post: {}", args.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX));

      if (client_socket_fd == -1)
        for (const auto &session : m_sessions)
          sendEvent(session.fd, "Platform Error", args);
      else
        sendEvent(client_socket_fd, "Platform Error", args);
    break;
    case SYSTEM_EVENTS__FILE_UPDATE:
    {
      auto timestamp = args.at(1);
      KLOG("Updating information file information for client {}'s file received at {}", client_socket_fd, timestamp);

      auto received_file = std::find_if(m_received_files.begin(), m_received_files.end(),
        [client_socket_fd, timestamp](const ReceivedFile &file)
        {
          return (file.client_fd                 == client_socket_fd &&
                  std::to_string(file.timestamp) == timestamp);
        });

      if (received_file != m_received_files.end())
      {
        KLOG("Data buffer found. Creating directory and saving file");
        const std::string filename = args.at(0);
        FileUtils::SaveFile(received_file->f_ptr, received_file->size, filename);
        m_received_files.erase(received_file);

        if (args.size() == 4 && args.at(3) == "final file")
          EraseFileHandler(client_socket_fd);

        sendEvent(client_socket_fd, "File Save Success", {timestamp});
      }
      else
      {
        KLOG("Unable to find file");
        sendEvent(client_socket_fd, "File Save Failure", {timestamp});
      }
    }
    break;
    case SYSTEM_EVENTS__FILES_SEND:
    {
      const auto files = args;
      handleFileSend(client_socket_fd, files);
      sendEvent(client_socket_fd, "File Upload", files);
    }
    break;
    case SYSTEM_EVENTS__FILES_SEND_ACK:
    {
      if (m_file_sending_fd == client_socket_fd)
        sendEvent(client_socket_fd, "File Upload Meta", m_outbound_files.front().file.to_string_v());
    }
    break;
    case SYSTEM_EVENTS__FILES_SEND_READY:
    if (m_file_sending_fd == client_socket_fd)
    {
      sendFile(client_socket_fd, m_outbound_files.front().file.name);
      m_outbound_files.pop_front();
    }
    break;
    case SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED:
      sendEvent(client_socket_fd, "Process Execution Requested", args);
      break;

    case SYSTEM_EVENTS__APPLICATION_FETCH_SUCCESS:
      sendEvent(client_socket_fd, "Application was found", args);
      break;

    case SYSTEM_EVENTS__APPLICATION_FETCH_FAIL:
      sendEvent(client_socket_fd, "Application was not found", args);
      break;

    case SYSTEM_EVENTS__REGISTRAR_SUCCESS:
      sendEvent(client_socket_fd, "Application was registered", args);
      break;

    case SYSTEM_EVENTS__REGISTRAR_FAIL:
      sendEvent(client_socket_fd, "Failed to register application", args);
      break;

    case SYSTEM_EVENTS__TASK_FETCH_FLAGS:
      sendEvent(client_socket_fd, "Application Flags", args);
      break;
    case SYSTEM_EVENTS__TASK_DATA:
      sendEvent(client_socket_fd, "Task Data", args);
      break;
    case SYSTEM_EVENTS__TASK_DATA_FINAL:
      sendEvent(client_socket_fd, "Task Data Final", args);
      break;
  }
}

/**
 * Request Handler
 */
void KServer::set_handler(const Request::Controller &&handler)
{
  KLOG("Setting Controller");
  m_controller = handler;
  m_controller.initialize(
    [this](std::string result, int mask, std::string request_id, int client_socket_fd, bool error)
    {
      onProcessEvent(result, mask, request_id, client_socket_fd, error);
    },
    [this](int client_socket_fd, int system_event, std::vector<std::string> args)
    {
      systemEventNotify(client_socket_fd, system_event, args);
    },
    [this](int client_socket_fd, std::vector<Task> tasks)
    {
      onTasksReady(client_socket_fd, tasks);
    }
  );
}

void KServer::onTasksReady(int client_socket_fd, std::vector<Task> tasks)
{
  KLOG("Scheduler has delivered {} tasks for processing", tasks.size());
}

/**
 * onProcessEvent
 *
 * Callback function for receiving the results of executed processes
 *
 * param[in] {std::string} result
 * param[in] {int} mask
 * param[in] {int} client_socket_fd
 * param[in] {bool} error
 *
 * TODO: Place results in a queue if handling file for client
 */
void KServer::onProcessEvent(std::string result, int mask, std::string id,
                    int client_socket_fd, bool error)
{
  KLOG("Received result:\n{}", result);
  std::string              process_executor_result_str{};
  std::vector<std::string> event_args{};

  event_args.reserve((error) ? 4 : 3);
  event_args.insert(event_args.end(), {std::to_string(mask), id, result});

  if (error)
    event_args.push_back("Executed process returned an ERROR");

  if (client_socket_fd == -1)
    for (const auto &session : m_sessions)
      sendEvent(session.fd, "Process Result", event_args);
  else
    sendEvent(client_socket_fd, "Process Result", event_args);

  if (Scheduler::isKIQProcess(mask))
    m_controller.process_system_event(SYSTEM_EVENTS__PROCESS_COMPLETE, {result, std::to_string(mask)}, std::stoi(id));
}

void KServer::sendFile(int32_t client_socket_fd, const std::string& filename)
{
  using F_Iterator = Kiqoder::FileIterator<uint8_t>;
  using P_Wrapper  = Kiqoder::FileIterator<uint8_t>::PacketWrapper;

  F_Iterator iterator{filename};

  while (iterator.has_data())
  {
    P_Wrapper packet = iterator.next();
    KLOG("Sending file packet with size {}", packet.size);
    SocketListener::sendMessage(client_socket_fd, reinterpret_cast<const char*>(packet.data()), packet.size);
  }

  m_file_sending = false;
}

/**
 * sendEvent
 *
 * Helper function for sending event messages to a client
 *
 * @param[in] {int} client_socket_fd
 * @param[in] {std::string} event
 * @param[in] {std::vector<std::string>} argv
 */
void KServer::sendEvent(int client_socket_fd, std::string event, std::vector<std::string> argv)
{
  KLOG("Sending {} event to {}", event, client_socket_fd);
  sendMessage(client_socket_fd, CreateEvent(event, argv));
}

void KServer::sendSessionMessage(int client_socket_fd, int status, std::string message, SessionInfo info)
{
  std::string session_message = CreateSessionEvent(status, message, info);
  KLOG("Sending session message to {}.\nSession info: {}", client_socket_fd, session_message);

  sendMessage(client_socket_fd, session_message);
}

void KServer::sendMessage(const int32_t& client_socket_fd, const std::string& message)
{
  using F_Iterator = Kiqoder::FileIterator<char>;
  using P_Wrapper  = Kiqoder::FileIterator<char>::PacketWrapper;

  F_Iterator iterator{message.data(), message.size()};

  while (iterator.has_data())
  {
    P_Wrapper packet = iterator.next();
    KLOG("Sending message packet with size {}", packet.size);
    SocketListener::sendMessage(client_socket_fd, reinterpret_cast<const char*>(packet.data()), packet.size);
  }
}

/**
 * File Transfer Completion
 */
void KServer::onFileHandled(int socket_fd, uint8_t*&& f_ptr, size_t size)
{
  if (m_file_pending_fd == socket_fd)
  {
    const auto timestamp = TimeUtils::UnixTime();
    m_received_files.push_back(ReceivedFile{.timestamp = timestamp,
                                            .client_fd = socket_fd,
                                            .f_ptr     = f_ptr,
                                            .size      = size});
    KLOG("Finished handling file for client {} at {}", socket_fd, timestamp);
    SetFileNotPending();
    m_controller.setHandlingData(false);
    sendEvent(socket_fd, "File Transfer Complete",
              {std::to_string(timestamp)});

    return;
  }
  KLOG("Lost file intended for {}", socket_fd);
}

/**
 * Ongoing File Transfer
 */
void KServer::handlePendingFile(std::shared_ptr<uint8_t[]> s_buffer_ptr,
                        int client_socket_fd, uint32_t size)
{
  using FileHandler = Kiqoder::FileHandler;
  auto  handler     = m_file_handlers.find(client_socket_fd);

  if (handler != m_file_handlers.end())
    handler->second.processPacket(s_buffer_ptr.get(), size);
  else
  {
    KLOG("creating FileHandler for {}", client_socket_fd);
    m_file_handlers.insert({client_socket_fd, FileHandler{
      [this](int32_t fd, uint8_t* data, size_t size)
      {
        onFileHandled(fd, std::move(data), size);
      }
    }});
    m_file_handlers.at(client_socket_fd).setID(client_socket_fd);
    m_file_handlers.at(client_socket_fd).processPacket(s_buffer_ptr.get(), size);
  }
}
void KServer::handleFileSend(int32_t client_fd, const std::vector<std::string>& files)
{
  m_file_sending    = true;
  m_file_sending_fd = client_fd;
  for (const auto file : FileMetaData::PayloadToMetaData(files))
    m_outbound_files.emplace_back(OutboundFile{.fd = client_fd, .file = file});
}
/**
 * Start Operation
 */
void KServer::handleStart(std::string decoded_message, int client_socket_fd)
{
  const uuids::uuid new_uuid = uuids::uuid_system_generator{}();
  const auto        uuid_str = uuids::to_string(new_uuid);
  m_sessions.push_back(KSession{.fd = client_socket_fd, .status = 1, .id = new_uuid});
  KLOG("New session created for {}. Session ID: {}", client_socket_fd, uuid_str);

  sendMessage(client_socket_fd, CreateMessage("New Session", m_controller("Start")));
  sendSessionMessage(client_socket_fd,
                     SESSION_ACTIVE,
                     "Session started",
                     SessionInfo{{"status", std::to_string(SESSION_ACTIVE)},{"uuid", uuid_str}});
}

/**
 * Execute Operation
 */
void KServer::handleExecute(std::string decoded_message, int client_socket_fd)
{
  const std::vector<std::string> args = GetArgs(decoded_message);
  if (!args.empty() && args.size() > 1)
  {
    const auto mask         = args.at(0);
    const auto request_uuid = args.at(1);
    KLOG("Execute request received.\nMask: {}  ID: {}", mask, request_uuid);
    m_controller(std::stoi(mask), request_uuid, client_socket_fd);
  }
}

/**
 * File Upload Operation
 */
void KServer::handleFileUploadRequest(int client_socket_fd)
{
  SetFilePending(client_socket_fd);
  m_controller.setHandlingData(true);
  sendMessage(client_socket_fd, CreateMessage("File Ready", ""));
}

void KServer::handleSchedule(std::vector<std::string> task, int client_socket_fd)
{
  auto uuid = uuids::to_string(uuids::uuid_system_generator{}());
  sendEvent(client_socket_fd, "Processing Request", {"Schedule Task", uuid});
  m_controller("Schedule", task, client_socket_fd, uuid);
  KLOG("Task delivered to request handler");
}

/**
 * Operations are the processing of requests
 */
void KServer::handleOperation(std::string decoded_message, int client_socket_fd)
{
  KOperation op = GetOperation(decoded_message);
  if (IsStartOperation(op))
  {
    KLOG("Start operation. TODO: Move to Controller::process_client_request()");
    handleStart(decoded_message, client_socket_fd);
    return;
  }
  else
  if (IsStopOperation(op))
  {
    KLOG("Stop operation. Shutting down client and closing connection. TODO: Move to Controller::process_client_request()");
    handleStop(client_socket_fd);
    return;
  }
  else
  if (IsExecuteOperation(op))
  {
    KLOG("Execute operation. TODO: Move to Controller::process_client_request()");
    handleExecute(decoded_message, client_socket_fd);
    return;
  }
  else
  if (IsFileUploadOperation(op))
  {
    KLOG("File upload operation. TODO: Move to Controller::process_client_request()");
    handleFileUploadRequest(client_socket_fd);
    return;
  }
  else
  if (IsIPCOperation(op))
  {
    KLOG("Testing IPC");
    handleIPC(decoded_message, client_socket_fd);
  }
  else
  if (IsAppOperation(op))
  {
    KLOG("App request. TODO: Move to Controller::process_client_request()");
    handleAppRequest(client_socket_fd, decoded_message);
  }
  else
  if (IsScheduleOperation(op))
  {
    KLOG("Fetch schedule request. TODO: Move to Controller::process_client_request()");
    handleScheduleRequest(client_socket_fd, decoded_message);
  }
  else
    m_controller.process_client_request(client_socket_fd, decoded_message);
}

void KServer::handleIPC(std::string message, int32_t client_socket_fd)
{
  m_ipc_manager.process(message, client_socket_fd);
}


void KServer::handleAppRequest(int client_fd, std::string message)
{
  m_controller.process_client_request(client_fd, message);
}

void KServer::handleScheduleRequest(int client_fd, std::string message)
{
  m_controller.process_client_request(client_fd, message);
}

/**
 * Override
 */
void KServer::onMessageReceived(int                      client_socket_fd,
                                std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                ssize_t&                 size)
{
  try
  {
    if (size)
    {
      std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
      if (m_file_pending)                                        // File
        return handlePendingFile(s_buffer_ptr, client_socket_fd, size);
      if (IsPing(s_buffer_ptr.get(), size))                       // Ping
      {
        KLOG("Client {} - keepAlive", client_socket_fd);
        return SocketListener::sendMessage(client_socket_fd, PONG, PONG_SIZE);
      }

      receiveMessage(s_buffer_ptr, size, client_socket_fd);     // Message
    }
  }
  catch(const std::exception& e)
  {
    ELOG("Exception thrown while handling message.\n{}", e.what());
  }
}

void KServer::handleStop(int client_socket_fd)
{
  sendEvent(client_socket_fd, "Close Session", {"KServer is shutting down the socket connection"});

  if (shutdown(client_socket_fd, SHUT_RD) != 0)
    KLOG("Error shutting down socket\nCode: {}\nMessage: {}", errno, strerror(errno));

  if (HandlingFile(client_socket_fd))
    SetFileNotPending();

  auto it_session = std::find_if(m_sessions.begin(), m_sessions.end(),
    [client_socket_fd](KSession session) { return session.fd == client_socket_fd;});

  if (it_session != m_sessions.end())
    m_sessions.erase(it_session);
}

void KServer::closeConnections()
{
  for (const int& fd : m_client_connections)
    handleStop(fd);
}

uint8_t KServer::getNumConnections()
{
  return m_sessions.size();
}

void KServer::onConnectionClose(int client_socket_fd)
{
  KLOG("Connection closed for {}", client_socket_fd);
  auto it_session = std::find_if(m_sessions.begin(), m_sessions.end(),
    [client_socket_fd](KSession session) { return session.fd == client_socket_fd; });
  if (it_session != m_sessions.end())
    m_sessions.erase(it_session);

  EraseFileHandler   (client_socket_fd);
  EraseMessageHandler(client_socket_fd);
  EraseOutgoingFiles (client_socket_fd);
}

void KServer::receiveMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t fd)
{
  using FileHandler = Kiqoder::FileHandler;
  static const bool KEEP_HEADER{true};

  auto handler = m_message_handlers.find(fd);
  if (handler != m_message_handlers.end())
    handler->second.processPacket(s_buffer_ptr.get(), size);
  else
  {
    KLOG("creating message handler for {}", fd);
    m_message_handlers.insert({fd, FileHandler{
      [this](int fd, uint8_t* m_ptr, size_t buffer_size)
      {
        DecodeMessage(m_ptr).leftMap([this, fd](auto decoded_message)
        {
          KLOG("Received message: {}", decoded_message);
          if (IsOperation(decoded_message))
          {
            KLOG("Received operation");
            handleOperation(decoded_message, fd);
          }
          else
          if (IsMessage(decoded_message))
            sendEvent(fd,"Message Received", {"Received by KServer", "Message", GetMessage(decoded_message)});
          return decoded_message;
        })
        .rightMap([this, fd](auto task_args)
        {
          if (!task_args.empty())
          {
            KLOG("Receive buffer contained schedule request");
            handleSchedule(task_args, fd);
          }

          return task_args;
        });
      },
      KEEP_HEADER
    }});
    m_message_handlers.at(fd).setID(fd);
    m_message_handlers.at(fd).processPacket(s_buffer_ptr.get(), size);
  }
}

bool KServer::EraseMessageHandler(int32_t client_socket_fd)
{
  auto it = m_message_handlers.find(client_socket_fd);
  if (it != m_message_handlers.end())
  {
    m_message_handlers.erase(it);
    KLOG("Message handler removed");
    return true;
  }
  return false;
}

bool KServer::EraseFileHandler(int fd)
{
  auto it = m_file_handlers.find(fd);
  if (it != m_file_handlers.end())
  {
    m_file_handlers.erase(it);
    KLOG("File handler removed");
    return true;
  }
  return false;
}

void KServer::EraseOutgoingFiles(int32_t fd)
{
  for (auto file_it = m_outbound_files.begin(); file_it != m_outbound_files.end();)
    if (file_it->fd == fd)
      file_it = m_outbound_files.erase(file_it);
    else
      file_it++;
}

void KServer::SetFileNotPending()
{
  m_file_pending    = false;
  m_file_pending_fd = -1;
}

void KServer::SetFilePending(int32_t fd)
{
  m_file_pending    = true;
  m_file_pending_fd = fd;
}

bool KServer::HandlingFile(int32_t fd)
{
  return (m_file_pending && m_file_pending_fd == fd);
}

} // namespace KYO
