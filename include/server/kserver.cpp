#include "kserver.hpp"


#define IF_NOT_HANDLING_PACKETS_FOR_CLIENT(x) if (m_file_pending_fd != x)

namespace KYO {
using namespace Decoder;
/**
 * \mainpage The KServer implements logicp's SocketListener and provides the KIQ
 * service to KStyleYo
 */
/**
 * Constructor
 */
KServer::KServer(int argc, char **argv)
: SocketListener(argc, argv),
  m_ipc_manager(IPCManager{
  [this](int32_t event, std::vector<std::string> payload)
    {
      systemEventNotify(ALL_CLIENTS, event, payload);
    }
  }),
  m_file_pending(false),
  m_file_pending_fd(-1)
  {
    KLOG("Starting IPC manager");
    m_ipc_manager.start();
  }

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
 * TODO: place messages in queue if handling file for client
 */
void KServer::systemEventNotify(int client_socket_fd, int system_event,
                        std::vector<std::string> args)
{
  switch (system_event) {
    case SYSTEM_EVENTS__SCHEDULED_TASKS_READY:
      if (client_socket_fd == -1) {
        KLOG(
            "Maintenance worker found tasks. Sending system-wide broadcast "
            "to all clients.");
        for (const auto &session : m_sessions) {
          sendEvent(session.fd, "Scheduled Tasks Ready", args);
        }
      } else {
        KLOG(
          "Informing client {} about "
          "scheduled tasks",
          client_socket_fd);
        sendEvent(client_socket_fd, "Scheduled Tasks Ready", args);
      }
      break;

    case SYSTEM_EVENTS__SCHEDULED_TASKS_NONE:
      if (client_socket_fd == -1) {
        KLOG("Sending system-wide broadcast. There are currently no tasks ready for execution.");
        for (const auto &session : m_sessions)
          sendEvent(session.fd, "No tasks ready", args);

      } else {
        KLOG("Informing client {} about scheduled tasks", client_socket_fd);
        sendEvent(client_socket_fd, "No tasks ready to run", args);
      }

      break;

    case SYSTEM_EVENTS__SCHEDULER_FETCH:
      if (client_socket_fd != -1) {
        KLOG("Sending schedule fetch results to client {}", client_socket_fd);
        sendEvent(client_socket_fd, "Scheduled Tasks", args);
      }
      break;

    case SYSTEM_EVENTS__SCHEDULER_UPDATE:
      if (client_socket_fd != -1) {
        KLOG("Sending schedule update result to client {}", client_socket_fd);
        sendEvent(client_socket_fd, "Schedule PUT", args);
      }
      break;

    case SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS:
      if (client_socket_fd != -1) {
        KLOG("Sending schedule flag values to client {}", client_socket_fd);
        sendEvent(client_socket_fd, "Schedule Tokens", args);
      }
    break;

    case SYSTEM_EVENTS__SCHEDULER_SUCCESS:
      KLOG("Task successfully scheduled");
      if (client_socket_fd == -1) {
        for (const auto &session : m_sessions) {
          sendEvent(session.fd, "Task Scheduled", args);
        }
      } else {
        sendEvent(client_socket_fd, "Task Scheduled", args);
      }
      break;

    case SYSTEM_EVENTS__PLATFORM_NEW_POST:
    {
      KLOG("Platform Post event received");

      m_controller.process_system_event(
        SYSTEM_EVENTS__PLATFORM_NEW_POST, args
      );

      std::vector<std::string> outgoing_args{};
      outgoing_args.reserve(args.size());
      for (const auto& arg : args) outgoing_args.emplace_back(
        (arg.size() > 2046) ?
          arg.substr(0, 2046) :
          arg
      );

      if (client_socket_fd == -1) {
        for (const auto &session : m_sessions) {
          sendEvent(session.fd, "Platform Post", args);
        }
      } else {
        sendEvent(client_socket_fd, "Platform Post", args);
      }
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

    case SYSTEM_EVENTS__PLATFORM_ERROR:
      m_controller.process_system_event(SYSTEM_EVENTS__PLATFORM_ERROR, args);

      ELOG("Error processing platform post: {}", args.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX));

      if (client_socket_fd == -1) {
        for (const auto &session : m_sessions) {
          sendEvent(session.fd, "Platform Error", args);
        }
      } else {
        sendEvent(client_socket_fd, "Platform Error", args);
      }

      break;

    case SYSTEM_EVENTS__FILE_UPDATE:
    {
      auto timestamp = args.at(1);
      KLOG("Updating information file information for client "
            "{}'s file received at {}",
            client_socket_fd, timestamp
      );

      auto received_file = std::find_if(m_received_files.begin(), m_received_files.end(),
        [client_socket_fd, timestamp](const ReceivedFile &file) {
          return (file.client_fd                 == client_socket_fd &&
                  std::to_string(file.timestamp) == timestamp);
        }
      );

      if (received_file != m_received_files.end()) {
        // We must assume that these files match, just by virtue of the
        // client file descriptor ID. Again, we should be matching by UUID.
        KLOG("Data buffer found. Creating directory and saving file");
        std::string filename = args.at(0);
        FileUtils::saveFile(received_file->f_ptr, received_file->size,
                            filename.c_str());
        m_received_files.erase(received_file);

        if (args.size() == 4 && args.at(3) == "final file") {
          eraseFileHandler(client_socket_fd);  // this client's file handler
                                                // is no longer needed
        }
        sendEvent(client_socket_fd, "File Save Success", {timestamp});
      } else {
        KLOG("Unable to find file");
        sendEvent(client_socket_fd, "File Save Failure", {timestamp});
      }
      break;
    }

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
      [this](std::string result, int mask, std::string request_id,
              int client_socket_fd, bool error) {
        onProcessEvent(result, mask, request_id, client_socket_fd, error);
      },
      [this](int client_socket_fd, int system_event,
              std::vector<std::string> args) {
        systemEventNotify(client_socket_fd, system_event, args);
      },
      [this](int client_socket_fd, std::vector<Task> tasks) {
        onTasksReady(client_socket_fd, tasks);
      });
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
void KServer::onProcessEvent(std::string result, int mask, std::string request_id,
                    int client_socket_fd, bool error)
{
  std::string              process_executor_result_str{};
  std::vector<std::string> event_args{};

  if (error)
    event_args.reserve(4);
    else
    event_args.reserve(3);

  KLOG("Received result:\n{}", result);
  if (result.size() <= 2046)
    event_args.insert(event_args.end(), {std::to_string(mask), request_id, result});
    else
    event_args.insert(event_args.end(),
      {std::to_string(mask), request_id,
      std::string{result.end() - 2000, result.end()}}
    );

  if (error)
    event_args.push_back("Executed process returned an ERROR");

  if (client_socket_fd == -1)  // Send response to all active sessions
    for (const auto &session : m_sessions)
      sendEvent(session.fd, "Process Result", event_args);
    else   // Send response to specifically indicated session
    sendEvent(client_socket_fd, "Process Result", event_args);

  if (Scheduler::isKIQProcess(mask)) {
    m_controller.process_system_event(
      SYSTEM_EVENTS__PROCESS_COMPLETE,
      {result, std::to_string(mask)}
    );
  }
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
void KServer::sendEvent(int client_socket_fd, std::string event,
                std::vector<std::string> argv)
{
  KLOG("Sending {} event to {}", event, client_socket_fd);
  for (const auto &arg : argv) {
    KLOG("Event arg - {}", arg);
  }
  std::string event_string = createEvent(event.c_str(), argv);
  sendMessage(client_socket_fd, event_string.c_str(), event_string.size());
}

void KServer::sendSessionMessage(int client_socket_fd, int status,
                        std::string message, SessionInfo info)
{
  std::string session_message = createSessionEvent(status, message, info);
  KLOG("Sending session message to {}.\nSession info: {}", client_socket_fd,
        session_message);
  sendMessage(client_socket_fd, session_message.c_str(),
              session_message.size());
}

/**
 * File Transfer Completion
 */
void KServer::onFileHandled(int socket_fd, int result, uint8_t*&& f_ptr,
                    size_t size)
{
  if (m_file_pending_fd == socket_fd) {
    if (result == FILE_HANDLE__SUCCESS) {
      // Push to received files immediately so that we ensure it's ready to be
      // used in subsequent client requests
      if (size > 0) {
        auto timestamp = TimeUtils::unixtime();
        m_received_files.push_back(ReceivedFile{.timestamp = timestamp,
                                                .client_fd = socket_fd,
                                                .f_ptr = f_ptr,
                                                .size = size});
        KLOG("Finished handling file for client {} at {}", socket_fd,
              timestamp);
        m_file_pending_fd = -1;
        m_file_pending = false;
        m_controller.setHandlingData(false);
        sendEvent(socket_fd, "File Transfer Complete",
                  {std::to_string(timestamp)});

        return;
      }
    }
    KLOG("File transfer failed");
    sendEvent(socket_fd, "File Transfer Failed", {});  // Nothing saved
  }
  KLOG("Lost file intended for {}", socket_fd);
}

/**
 * Ongoing File Transfer
 */
void KServer::handlePendingFile(std::shared_ptr<uint8_t[]> s_buffer_ptr,
                        int client_socket_fd, uint32_t size)
{
  auto handler = m_file_handlers.find(client_socket_fd);

  if (handler != m_file_handlers.end()) {
    handler->second.processPacket(s_buffer_ptr.get(), size);
  } else {
    KLOG("creating FileHandler for {}", client_socket_fd);
    m_file_handlers.insert({client_socket_fd, FileHandler{client_socket_fd, "", s_buffer_ptr.get(), size,
      [this](int socket_fd, int result, uint8_t *f_ptr, size_t buffer_size)
      {
        onFileHandled(socket_fd, result, std::move(f_ptr), buffer_size);
      }
    }});
  }
}

/**
 * Start Operation
 */
void KServer::handleStart(std::string decoded_message, int client_socket_fd)
{
  // Session
  uuids::uuid const new_uuid = uuids::uuid_system_generator{}();
  m_sessions.push_back(
      KSession{.fd = client_socket_fd, .status = 1, .id = new_uuid});
  // Database fetch
  ServerData server_data = m_controller("Start");
  // Send welcome
  std::string start_message = createMessage("New Session", server_data);
  sendMessage(client_socket_fd, start_message.c_str(), start_message.size());
  auto uuid_str = uuids::to_string(new_uuid);
  KLOG("New session created for {}. Session ID: {}", client_socket_fd,
        uuid_str);
  // Send session info
  SessionInfo session_info{{"status", std::to_string(SESSION_ACTIVE)},
                            {"uuid", uuid_str}};
  sendSessionMessage(client_socket_fd, SESSION_ACTIVE, "Session started",
                      session_info);
}

/**
 * Execute Operation
 */
void KServer::handleExecute(std::string decoded_message, int client_socket_fd)
{
  std::vector<std::string> args = getArgs(decoded_message.c_str());
  if (!args.empty() && args.size() > 1) {
    KLOG("Execute request received");
    auto mask = args.at(0);
    auto request_uuid = args.at(1);
    KLOG("Mask: {}  ID: {}", mask, request_uuid);
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

  std::string file_ready_message = createMessage("File Ready", "");
  sendMessage(client_socket_fd, file_ready_message.c_str(),
              file_ready_message.size());
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
  KOperation op = getOperation(decoded_message.c_str());
  if (isStartOperation(op.c_str())) {        // Start
    KLOG("Start operation");
    handleStart(decoded_message, client_socket_fd);
    return;
  }
  else
  if (isStopOperation(op.c_str())) {         // Stop
    KLOG("Stop operation. Shutting down client and closing connection");
    handleStop(client_socket_fd);
    return;
  }
  else
  if (isExecuteOperation(op.c_str())) {      // Process execution request
    KLOG("Execute operation");
    handleExecute(decoded_message, client_socket_fd);
    return;
  }
  else
  if (isFileUploadOperation(op.c_str())) {   // File upload request
    KLOG("File upload operation");
    handleFileUploadRequest(client_socket_fd);
    return;
  }
  else
  if (isIPCOperation(op.c_str())) {          // IPC request
    KLOG("Testing IPC");
    handleIPC(decoded_message, client_socket_fd);
  }
  else
  if (isAppOperation(op.c_str())) {          // Register app
    KLOG("App request");
    handleAppRequest(client_socket_fd, decoded_message);
  }
  else
  if (isScheduleOperation(op.c_str())) {     // Fetch schedule
    KLOG("Fetch schedule request");
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
    if (size) {
      std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
      if (m_file_pending && size != 5)                            // File
      {
        handlePendingFile(s_buffer_ptr, client_socket_fd, size);
        return;
      }
      if (isPing(s_buffer_ptr.get(), size))                     // Ping
      {
        KLOG("Client {} - keepAlive", client_socket_fd);
        sendMessage(client_socket_fd, PONG, PONG_SIZE);
      }
      else
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
  sendEvent(client_socket_fd, "Close Session",
  {"KServer is shutting down the socket connection"});

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

uint8_t KServer::getNumConnections() {
  return m_sessions.size();
}

void KServer::onConnectionClose(int client_socket_fd)
{
  KLOG("Connection closed for {}", client_socket_fd);
  auto it_session = std::find_if(m_sessions.begin(), m_sessions.end(),
                                  [client_socket_fd](KSession session) {
                                    return session.fd == client_socket_fd;
                                  });
  if (it_session != m_sessions.end())
    m_sessions.erase(it_session);

  eraseFileHandler   (client_socket_fd);
  eraseMessageHandler(client_socket_fd);
}

void KServer::receiveMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t client_socket_fd)
{
  static const bool KEEP_HEADER{true};

  auto handler = m_message_handlers.find(client_socket_fd);
  if (handler != m_message_handlers.end())
    handler->second.processPacket(s_buffer_ptr.get(), size);
  else
  {
    KLOG("creating message handler for {}", client_socket_fd);
    m_message_handlers.insert({client_socket_fd, FileHandler{client_socket_fd, "", s_buffer_ptr.get(), size,
      [this, client_socket_fd](int socket_fd, int result, uint8_t* m_ptr, size_t buffer_size)
      {
        DecodeMessage(m_ptr).leftMap([this, client_socket_fd](auto decoded_message)
        {
          KLOG("Received message: {}", decoded_message);
          // Handle operations
          if (isOperation(decoded_message.c_str()))
          {
            KLOG("Received operation");
            handleOperation(decoded_message, client_socket_fd);
          }
          else
          if (isMessage(decoded_message.c_str()))
            sendEvent(client_socket_fd,
                      "Message Received",
                      {"Received by KServer", "Message", getMessage(decoded_message)});

          return decoded_message;
        })
        .rightMap([this, client_socket_fd](auto task_args) {
          if (!task_args.empty())
          {
            KLOG("Receive buffer contained schedule request");
            handleSchedule(task_args, client_socket_fd);
          }

          return task_args;
        });
      },
      KEEP_HEADER
    }});
  }
}

bool KServer::eraseMessageHandler(int32_t client_socket_fd)
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

bool KServer::eraseFileHandler(int client_socket_fd)
{
  auto it = m_file_handlers.find(client_socket_fd);
  if (it != m_file_handlers.end())
  {
    m_file_handlers.erase(it);
    KLOG("File handler removed");
    return true;
  }
  return false;
}

void KServer::SetFileNotPending()
{
  m_file_pending    = false;
  m_file_pending_fd = -1;
}

void KServer::SetFilePending(int32_t fd)
{
  m_file_pending = true;
  m_file_pending_fd = fd;
}

bool KServer::HandlingFile(int32_t fd)
{
  return (m_file_pending && m_file_pending_fd == fd);
}

} // namespace KYO
