#include "kserver.hpp"
#include "kiqoder/file_iterator.hpp"

namespace kiq {

static const int32_t NONE_PENDING    {-1};
static const bool    KEEP_HEADER     {true};
static const char*   STATUS          {"status"};
static const char*   NEW_SESSION     {"New Session"};
static const char*   CLOSE_SESSION   {"Close Session"};
static const char*   WELCOME_MSG     {"Session started"};
static const char*   GOODBYE_MSG     {"KServer is shutting down the socket connection"};
static const char*   RECV_MSG        {"Received by KServer"};
static const char*   FILE_SUCCESS_MSG{"File Save Success"};
static const char*   FILE_FAIL_MSG   {"File Save Failure"};
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
  [this](int32_t event, const std::vector<std::string>& payload) -> void
  {
    SystemEvent(ALL_CLIENTS, event, payload);
  }),
  m_file_pending(false),
  m_file_pending_fd(NONE_PENDING)
  {
    KLOG("Initializing controller");
    m_controller.Initialize(
    [this](std::string result, int mask, std::string request_id, const int32_t& client_fd, bool error)
    {
      OnProcessEvent(result, mask, request_id, client_fd, error);
    },
    [this](const int32_t& client_fd, int system_event, std::vector<std::string> args)
    {
      SystemEvent(client_fd, system_event, args);
    },
    [this]()
    {
      Status();
    });

    KLOG("Starting IPC manager");
    m_ipc_manager.start();
  }

/**
 * @destructor
 */
KServer::~KServer()
{
  KLOG("Server shutting down");
  CloseConnections();
  m_controller.Shutdown();
}

/**
 * SystemEvent
 *
 * Handles notifications sent back to the system delivering event messages
 *
 * @param[in] {int} client_fd
 * @param[in] {int} system_event
 * @param[in] {std::vector<std::string>}
 *
 */
void KServer::SystemEvent(const int32_t&                  client_fd,
                          const int32_t&                  system_event,
                          const std::vector<std::string>& args)
{
  switch (system_event)
  {
    case SYSTEM_EVENTS__SCHEDULED_TASKS_READY:
      KLOG("Maintenance worker found tasks");
      if (client_fd == ALL_CLIENTS)
        Broadcast("Scheduled Tasks Ready", args);
      else
        SendEvent(client_fd, "Scheduled Tasks Ready", args);
    break;
    case SYSTEM_EVENTS__SCHEDULED_TASKS_NONE:
      KLOG("There are currently no tasks ready for execution.");
      if (client_fd == ALL_CLIENTS)
        Broadcast("No tasks ready", args);
      else
        SendEvent(client_fd, "No tasks ready", args);
    break;
    case SYSTEM_EVENTS__SCHEDULER_FETCH:
      if (client_fd != ALL_CLIENTS)
      {
        KLOG("Sending schedule fetch results to client {}", client_fd);
        SendEvent(client_fd, "Scheduled Tasks", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_UPDATE:
      if (client_fd != ALL_CLIENTS)
      {
        KLOG("Sending schedule update result to client {}", client_fd);
        SendEvent(client_fd, "Schedule PUT", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS:
      if (client_fd != ALL_CLIENTS)
      {
        KLOG("Sending schedule flag values to client {}", client_fd);
        SendEvent(client_fd, "Schedule Tokens", args);
      }
    break;
    case SYSTEM_EVENTS__SCHEDULER_SUCCESS:
      KLOG("Task successfully scheduled");
      if (client_fd == ALL_CLIENTS)
        Broadcast("Task Scheduled", args);
       else
        SendEvent(client_fd, "Task Scheduled", args);
    break;
    case SYSTEM_EVENTS__PLATFORM_NEW_POST:
    {
      KLOG("Platform Post event received");
      m_controller.ProcessSystemEvent(SYSTEM_EVENTS__PLATFORM_NEW_POST, args);

      std::vector<std::string> outgoing_args{};
      outgoing_args.reserve(args.size());
      for (const auto& arg : args)
        outgoing_args.emplace_back(arg);

      if (client_fd == ALL_CLIENTS)
        Broadcast("Platform Post", args);
      else
        SendEvent(client_fd, "Platform Post", args);
    }
    break;
    case SYSTEM_EVENTS__PLATFORM_POST_REQUESTED:
      if (args.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX) == "bot")
        m_ipc_manager.ReceiveEvent(system_event, args);
      else
        m_controller.ProcessSystemEvent(SYSTEM_EVENTS__PLATFORM_ERROR, args);
    break;
    case SYSTEM_EVENTS__PLATFORM_REQUEST:
      m_controller.ProcessSystemEvent(system_event, args);
    break;
    case SYSTEM_EVENTS__PLATFORM_EVENT:
      m_ipc_manager.ReceiveEvent(system_event, args);
    break;
    case SYSTEM_EVENTS__KIQ_IPC_MESSAGE:
        m_ipc_manager.process(args.front(), client_fd);
    break;
    case SYSTEM_EVENTS__PLATFORM_ERROR:
      m_controller.ProcessSystemEvent(system_event, args);
      ELOG("Error processing platform post: {}", args.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX));

      if (client_fd == ALL_CLIENTS)
        Broadcast("Platform Error", args);
      else
        SendEvent(client_fd, "Platform Error", args);
    break;
    case SYSTEM_EVENTS__FILE_UPDATE:
    {
      const auto timestamp = args.at(1);
      KLOG("Updating information file information for client {}'s file received at {}", client_fd, timestamp);

      auto received_file = std::find_if(m_received_files.begin(), m_received_files.end(),
        [client_fd, timestamp](const ReceivedFile &file)
        {
          return (file.client_fd                 == client_fd &&
                  std::to_string(file.timestamp) == timestamp);
        });

      if (received_file != m_received_files.end())
      {
        KLOG("Data buffer found. Creating directory and saving file");
        const std::string filename = args.at(0);
        FileUtils::SaveFile(received_file->f_ptr, received_file->size, filename);
        m_received_files.erase(received_file);

        if (args.size() > 3 && args.at(3) == "final file")
          EraseFileHandler(client_fd);

        SendEvent(client_fd, FILE_SUCCESS_MSG, {timestamp});
      }
      else
      {
        ELOG("Unable to find file");
        SendEvent(client_fd, FILE_FAIL_MSG, {timestamp});
      }
    }
    break;
    case SYSTEM_EVENTS__FILES_SEND:
    {
      const auto files = args;
      EnqueueFiles(client_fd, files);
      SendEvent(client_fd, "File Upload", files);
    }
    break;
    case SYSTEM_EVENTS__FILES_SEND_ACK:
    {
      if (m_file_sending_fd == client_fd)
        SendEvent(client_fd, "File Upload Meta", m_outbound_files.front().file.to_string_v());
    }
    break;
    case SYSTEM_EVENTS__FILES_SEND_READY:
    if (m_file_sending_fd == client_fd)
    {
      SendFile(client_fd, m_outbound_files.front().file.name);
      m_outbound_files.pop_front();
    }
    break;
    case SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED:
      SendEvent(client_fd, "Process Execution Requested", args);
      break;

    case SYSTEM_EVENTS__APPLICATION_FETCH_SUCCESS:
      SendEvent(client_fd, "Application was found", args);
      break;

    case SYSTEM_EVENTS__APPLICATION_FETCH_FAIL:
      SendEvent(client_fd, "Application was not found", args);
      break;

    case SYSTEM_EVENTS__REGISTRAR_SUCCESS:
      SendEvent(client_fd, "Application was registered", args);
      break;

    case SYSTEM_EVENTS__REGISTRAR_FAIL:
      SendEvent(client_fd, "Failed to register application", args);
    break;

    case SYSTEM_EVENTS__TASK_FETCH_FLAGS:
      SendEvent(client_fd, "Application Flags", args);
    break;

    case SYSTEM_EVENTS__TASK_DATA:
      SendEvent(client_fd, "Task Data", args);
    break;

    case SYSTEM_EVENTS__TASK_DATA_FINAL:
      SendEvent(client_fd, "Task Data Final", args);
    break;

    case SYSTEM_EVENTS__TERM_HITS:
      SendEvent(client_fd, "Term Hits", args);
    break;
  }
}

/**
 * onProcessEvent
 *
 * Callback function for receiving the results of executed processes
 *
 * param[in] {std::string} result
 * param[in] {int} mask
 * param[in] {int} client_fd
 * param[in] {bool} error
 *
 * TODO: Place results in a queue if handling file for client
 */
void KServer::OnProcessEvent(const std::string& result, int32_t mask, const std::string& id,
                             int32_t client_fd, bool error)
{
  VLOG("Task {} completed with result:\n{}", id, result);
  std::string              process_executor_result_str{};
  std::vector<std::string> event_args{};

  event_args.reserve((error) ? 4 : 3);
  event_args.insert(event_args.end(), {std::to_string(mask), id, result});

  if (error)
    event_args.push_back("Executed process returned an ERROR");

  if (client_fd == ALL_CLIENTS)
    Broadcast("Process Result", event_args);
  else
    SendEvent(client_fd, "Process Result", event_args);

  if (Scheduler::isKIQProcess(mask))
    m_controller.ProcessSystemEvent(SYSTEM_EVENTS__PROCESS_COMPLETE, {result, std::to_string(mask)}, std::stoi(id));
}

void KServer::SendFile(const int32_t& client_fd, const std::string& filename)
{
  using F_Iterator = Kiqoder::FileIterator<uint8_t>;
  using P_Wrapper  = Kiqoder::FileIterator<uint8_t>::PacketWrapper;

  F_Iterator iterator{filename};

  while (iterator.has_data())
  {
    P_Wrapper packet = iterator.next();
    SocketListener::sendMessage(client_fd, reinterpret_cast<const char*>(packet.data()), packet.size);
  }

  VLOG("Sent {} file bytes",     iterator.GetBytesRead());
  m_sessions.at(client_fd).tx += iterator.GetBytesRead();
  m_file_sending = false;
}

/**
 * SendEvent
 *
 * Helper function for sending event messages to a client
 *
 * @param[in] {int} client_fd
 * @param[in] {std::string} event
 * @param[in] {std::vector<std::string>} argv
 */
void KServer::SendEvent(const int32_t& client_fd, const std::string& event, const std::vector<std::string>& argv)
{
  KLOG("Sending {} event to {}", event, client_fd);
  SendMessage(client_fd, CreateEvent(event, argv));
}

void KServer::SendMessage(const int32_t& client_fd, const std::string& message)
{
  using F_Iterator = Kiqoder::FileIterator<char>;
  using P_Wrapper  = Kiqoder::FileIterator<char>::PacketWrapper;

  const size_t size = message.size();
  F_Iterator iterator{message.data(), size};
  KLOG("Sending {} bytes to {}", size, client_fd);

  while (iterator.has_data())
  {
    P_Wrapper packet = iterator.next();
    SocketListener::sendMessage(client_fd, reinterpret_cast<const char*>(packet.data()), packet.size);
  }

  m_sessions.at(client_fd).tx += iterator.GetBytesRead();
}

/**
 * File Transfer Completion
 */
void KServer::OnFileHandled(const int& socket_fd, uint8_t*&& f_ptr, size_t size)
{
  if (m_file_pending_fd != socket_fd)
    KLOG("Lost file intended for {}", socket_fd);
  else
  {
    const auto timestamp = TimeUtils::UnixTime();
    m_received_files.emplace_back(ReceivedFile{timestamp, socket_fd, f_ptr, size});
    KLOG("Received file from {} at {}", socket_fd, timestamp);
    SetFileNotPending();
    SendEvent(socket_fd, "File Transfer Complete", {std::to_string(timestamp)});
  }
}

/**
 * Ongoing File Transfer
 */
void KServer::ReceiveFileData(const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                              const int32_t&                    client_fd,
                              const size_t&                     size)
{
  using FileHandler = Kiqoder::FileHandler;
  auto  handler     = m_file_handlers.find(client_fd);

  if (handler != m_file_handlers.end())
    handler->second.processPacket(s_buffer_ptr.get(), size);
  else
  {
    KLOG("creating FileHandler for {}", client_fd);
    m_file_handlers.insert({client_fd, FileHandler{
      [this](int32_t fd, uint8_t* data, size_t size) { OnFileHandled(fd, std::move(data), size); }
    }});
    m_file_handlers.at(client_fd).setID(client_fd);
    m_file_handlers.at(client_fd).processPacket(s_buffer_ptr.get(), size);
  }

  m_sessions.at(client_fd).rx += size;
}
void KServer::EnqueueFiles(const int32_t& client_fd, const std::vector<std::string>& files)
{
  m_file_sending    = true;
  m_file_sending_fd = client_fd;
  for (const auto file : FileMetaData::PayloadToMetaData(files))
    m_outbound_files.emplace_back(OutboundFile{client_fd, file});
}
/**
 * Start Operation
 */
void KServer::InitClient(const std::string& message, const int32_t& client_fd)
{
  auto NewSession = [&client_fd](const uuid& id)      { return KSession{client_fd, READY_STATUS, id};                        };
  auto GetData    = [this]      ()                    { return CreateMessage(NEW_SESSION, m_controller.CreateSession());   };
  auto GetInfo    = []          (auto state, auto id) { return SessionInfo{{STATUS, std::to_string(state)}, {"uuid", id}}; };
  const uuids::uuid n_uuid = uuids::uuid_system_generator{}();
  const std::string uuid_s = uuids::to_string(n_uuid);

  m_sessions.emplace(client_fd, NewSession(n_uuid));
  KLOG("Started session {} for {}", uuid_s, client_fd);
  SendMessage(client_fd, GetData());
  SendMessage(client_fd, CreateSessionEvent(SESSION_ACTIVE, WELCOME_MSG, GetInfo(SESSION_ACTIVE, uuid_s)));
}

/**
 * File Upload Operation
 */
void KServer::WaitForFile(const int32_t& client_fd)
{
  SetFilePending(client_fd);
  SendMessage(client_fd, CreateMessage("File Ready", ""));
}

void KServer::ScheduleRequest(const std::vector<std::string>& task, const int32_t& client_fd)
{
  auto uuid = uuids::to_string(uuids::uuid_system_generator{}());
  SendEvent(client_fd, "Processing Schedule Request", {"Schedule Task", uuid});
  m_controller("Schedule", task, client_fd, uuid);
  KLOG("Legacy Schedule Request: delivered to controller");
}

/**
 * Operations are the processing of requests
 */
void KServer::OperationRequest(const std::string& message, const int32_t& client_fd)
{
  const KOperation op = GetOperation(message);
  if      (IsStartOperation     (op)) InitClient(message, client_fd);
  else if (IsStopOperation      (op)) EndSession(client_fd);
  else if (IsFileUploadOperation(op)) WaitForFile(client_fd);
  else if (IsIPCOperation       (op)) m_ipc_manager.process(message, client_fd);
  else                                m_controller.ProcessClientRequest(client_fd, message);
}

/**
 * Override
 */
void KServer::onMessageReceived(int                      client_fd,
                                std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                ssize_t&                 size)
{
  if (!size) return;

  try
  {
    std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
    if (m_file_pending)
      ReceiveFileData(s_buffer_ptr, client_fd, size);
    else
      ReceiveMessage(s_buffer_ptr, size, client_fd);
  }
  catch(const std::exception& e)
  {
    ELOG("Exception thrown while handling message.\n{}", e.what());
  }
}

void KServer::SendPong(int32_t client_fd)
{
  KLOG("Client {} - keepalive", client_fd);
  SendMessage(client_fd, PONG);
}

void KServer::EndSession(const int32_t& client_fd, bool close_socket)
{
  auto GetStats = [](const KSession& session) { return "RX: " + std::to_string(session.rx) +
                                                     "\nTX: " + std::to_string(session.tx); };
  static const int SUCCESS{0};
  const std::string stats = GetStats(m_sessions.at(client_fd));
  KLOG("Shutting down session for client {}.\nStatistics:\n{}", client_fd, stats);

  if (HandlingFile(client_fd))
    SetFileNotPending();

  m_sessions.at(client_fd).status = SESSION_INACTIVE;

  if (close_socket)
  {
    SendEvent(client_fd, CLOSE_SESSION, {GOODBYE_MSG, stats});

    if (shutdown(client_fd, SHUT_RD) != SUCCESS)
      KLOG("Error shutting down socket\nCode: {}\nMessage: {}", errno, strerror(errno));
  }

  OnClientExit(client_fd);
}

void KServer::CloseConnections()
{
  for (const int& fd : m_client_connections)
    if (m_sessions.at(fd).status == SESSION_ACTIVE)
      EndSession(fd);
}


void KServer::ReceiveMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t fd)
{
  using FileHandler = Kiqoder::FileHandler;
  auto ProcessMessage = [this](int fd, uint8_t* m_ptr, size_t buffer_size)
  {
    DecodeMessage(m_ptr).leftMap([this, fd](auto&& message)
    {
      if (message.empty())
        ELOG("Failed to decode message");
      else
      if (IsPing(message))
        SendPong(fd);
      else
      if (IsOperation(message))
        OperationRequest(message, fd);
      else
      if (IsMessage(message))
        SendEvent(fd, "Message Received", {RECV_MSG, "Message", GetMessage(message)});
      return message;
    })
    .rightMap([this, fd](auto&& args)
    {
      if (args.empty())
        ELOG("Failed to decode message");
      else
        ScheduleRequest(args, fd);
      return args;
    });
  };
  auto it = m_message_handlers.find(fd);
  if (it != m_message_handlers.end())
    it->second.processPacket(s_buffer_ptr.get(), size);
  else
  {
    KLOG("Creating message handler for {}", fd);
    m_message_handlers.insert({fd, FileHandler{ProcessMessage, KEEP_HEADER}});
    m_message_handlers.at(fd).setID(fd);
    m_message_handlers.at(fd).processPacket(s_buffer_ptr.get(), size);
  }
  m_sessions.at(fd).rx += size;
}

void KServer::Broadcast(const std::string& event, const std::vector<std::string>& argv)
{
  for (const auto& [fd, session] : m_sessions)
    if (session.active())
      SendEvent(fd, event, argv);
}

void KServer::OnClientExit(const int32_t& client_fd)
{
  auto DeleteFiles = [this](const auto& fd)
  {
    for (auto it = m_outbound_files.begin(); it != m_outbound_files.end();)
    if (it->fd == fd)
      it = m_outbound_files.erase(it);
    else
      it++;
  };

  EraseFileHandler(client_fd);
  DeleteFiles     (client_fd);
}

void KServer::EraseFileHandler(const int32_t& fd)
{
  auto it = m_file_handlers.find(fd);
  if (it != m_file_handlers.end())
  {
    m_file_handlers.erase(it);
    KLOG("Removed file handler");
  }
}

void KServer::SetFileNotPending()
{
  m_file_pending    = false;
  m_file_pending_fd = -1;
}

void KServer::SetFilePending(const int32_t& fd)
{
  m_file_pending    = true;
  m_file_pending_fd = fd;
}

bool KServer::HandlingFile(const int32_t& fd)
{
  return (m_file_pending && m_file_pending_fd == fd);
}

void KServer::onConnectionClose(int32_t client_fd)
{
  static const bool close_socket{false};

  KLOG("Connection closed for {}", client_fd);
  if (GetSession(client_fd).active())
    EndSession(client_fd, close_socket);
}

KSession KServer::GetSession(const int32_t& client_fd) const
{
  auto it = m_sessions.find(client_fd);
  if (it != m_sessions.end())
    return it->second;
  return KSession{};
}

void KServer::Status() const
{
  size_t tx_bytes{}, rx_bytes{};
  for (const auto& [fd, session] : m_sessions)
  {
    tx_bytes += session.tx;
    rx_bytes += session.rx;
  }

  VLOG("Server Status\nSent {} bytes\nRecv {} bytes", tx_bytes, rx_bytes);
}
} // ns kiq
