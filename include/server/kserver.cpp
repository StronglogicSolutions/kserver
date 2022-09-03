#include "event_handler.hpp"
#include "kserver.hpp"
#include "kiqoder/file_iterator.hpp"

namespace kiq {

static const int32_t NONE_PENDING    {-1};
static const bool    KEEP_HEADER     {true};
static const char*   STATUS          {"status"};
static const char*   NEW_SESSION     {"New Session"};
static const char*   CLOSE_SESSION   {"Close Session"};
static const char*   WELCOME_MSG     {"Session started"};
static const char*   REJECTED_MSG    {"Session rejected"};
static const char*   GOODBYE_MSG     {"KServer is shutting down the socket connection"};
static const char*   RECV_MSG        {"Received by KServer"};


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
  m_event_handler(this),
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
void KServer::SystemEvent(int32_t client_fd, int32_t system_event, const std::vector<std::string>& args)
{
  m_event_handler(client_fd, system_event, args);
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

  Broadcast("Process Result", event_args);

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
 * @param[in] {int32_t}                  client_fd
 * @param[in] {std::string}              event
 * @param[in] {std::vector<std::string>} argv
 */
void KServer::SendEvent(const int32_t& client_fd, const std::string& event, const std::vector<std::string>& argv)
{
  KLOG("Sending {} event to {}", event, client_fd);
  if (client_fd == ALL_CLIENTS)
    Broadcast(event, argv);
  else
    SendMessage(client_fd, CreateEvent(event, argv));
}

void KServer::SendMessage(const int32_t& client_fd, const std::string& message)
{
  using F_Iterator = Kiqoder::FileIterator<char>;
  using P_Wrapper  = Kiqoder::FileIterator<char>::PacketWrapper;

  const size_t size = message.size();
  F_Iterator iterator{message.data(), size};
  VLOG("Sending {} bytes to {}", size, client_fd);

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
    m_file_manager.Add({timestamp, socket_fd, f_ptr, size});
    KLOG("Received file from {} at {}", socket_fd, timestamp);
    SendEvent(socket_fd, "File Transfer Complete", {std::to_string(timestamp)});
    SetFileNotPending();
  }
}

/**
 * Ongoing File Transfer
 */
void KServer::ReceiveFileData(const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                              const size_t                      size,
                              const int32_t                     client_fd)
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
void KServer::InitClient(const std::string& message, const int32_t& fd)
{
  auto GetData    = [this]()                { return CreateMessage(NEW_SESSION, m_controller.CreateSession());   };
  auto GetInfo    = [](auto state, auto id) { return SessionInfo{{STATUS, std::to_string(state)}, {"uuid", id}}; };
  auto GetError   = [](const auto reason)   { return SessionInfo{{STATUS, std::to_string(SESSION_INVALID)},
                                                                 {"reason", reason}};                            };
  auto NewSession = [&fd](const uuid& id, const User& user) { return KSession{user, fd, SESSION_ACTIVE, id}; };

  const User        user    = GetAuth(message);
  const uuids::uuid n_uuid  = uuids::uuid_system_generator{}();
  const std::string uuid_s  = uuids::to_string(n_uuid);

  if (m_sessions.init(user.name, NewSession(n_uuid, user)) && ValidateToken(user))
  {
    KLOG("Started session {} for {}", uuid_s, fd);
    SendMessage(fd, GetData());
    SendMessage(fd, CreateSessionEvent(SESSION_ACTIVE, WELCOME_MSG, GetInfo(SESSION_ACTIVE, uuid_s)));
  }
  else
  {
    ELOG("Rejected session request for {} on {}", user.name, fd);
    SendMessage(fd, CreateSessionEvent(SESSION_INVALID, REJECTED_MSG, GetError("Invalid token")));
    EndSession(fd, SESSION_INVALID);
  }
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
  const auto uuid = uuids::to_string(uuids::uuid_system_generator{}());
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
  else if (IsIPCOperation       (op)) m_ipc_manager.ReceiveEvent(SYSTEM_EVENTS__IPC_REQUEST, {message});
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
    (m_file_pending) ?
      ReceiveFileData(s_buffer_ptr, size, client_fd) :
      ReceiveMessage (s_buffer_ptr, size, client_fd);
  }
  catch(const std::exception& e)
  {
    ELOG("Exception thrown while handling message.\n{}", e.what());
  }
}

void KServer::SendPong(int32_t client_fd)
{
  m_sessions.at(client_fd).notify();
  SendMessage(client_fd, PONG);
}

void KServer::EndSession(const int32_t& client_fd, int32_t status)
{
  static const int SUCCESS{0};
  auto GetStats = [](const KSession& session) { return "RX: " + std::to_string(session.rx) +
                                                     "\nTX: " + std::to_string(session.tx); };

  if (m_sessions.has(client_fd))
  {
    KLOG("Shutting down session for client {}.\nStatistics:\n{}", client_fd, GetStats(m_sessions.at(client_fd)));
    m_sessions.at(client_fd).status = status;
    if (HandlingFile(client_fd)) SetFileNotPending();
  }
  else
    KLOG("Shutting down socket for client with no session");

  VLOG("Calling shutdown on fd {}", client_fd);
  if (shutdown(client_fd, SHUT_RD) != SUCCESS)
    ELOG("Error shutting down socket\nCode: {}\nMessage: {}", errno, strerror(errno));
}

void KServer::CloseConnections()
{
  for (const int& fd : m_client_connections)
    if (m_sessions.at(fd).active())
      EndSession(fd);
}


void KServer::ReceiveMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t fd)
{
  using FileHandler = Kiqoder::FileHandler;
  auto TrackDataStats = [this](int fd, size_t size)     { if (m_sessions.has(fd)) m_sessions.at(fd).rx += size; };
  auto ProcessMessage = [this](int fd, uint8_t* m_ptr, size_t buffer_size)
  {
    DecodeMessage(m_ptr).leftMap([this, fd](auto&& message)
    {
      auto HasValidToken  = [this](int fd, const auto& msg) { return (GetToken(msg) == m_sessions.at(fd).user.token); };

      if (message.empty())
        ELOG("Failed to decode message");
      else
      if (IsPing(message))
        SendPong(fd);
      else
      if (m_sessions.has(fd) && m_sessions.at(fd).active() && !HasValidToken(fd, message))
        EndSession(fd);
      else
      if (IsOperation(message))
        OperationRequest(message, fd);
      else
      if (IsMessage(message))
        SendEvent(fd, "Message Received", {RECV_MSG, "Message", GetMessage(message)});
      else
        EndSession(fd);
      return message;
    })
    .rightMap([this, fd](auto&& args)
    {
      (args.size()) ? ScheduleRequest(args, fd) :
                      ELOG("Failed to decode message");
      return args;
    });
  };

  try
  {
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
    TrackDataStats(fd, size);
  }
  catch(const std::exception& e)
  {
    ELOG("Exception caught while processing client message: {}", e.what());
    throw;
  }
}

void KServer::Broadcast(const std::string& event, const std::vector<std::string>& argv)
{
  for (const auto& [_, session] : m_sessions)
    if (session.active())
      SendEvent(session.fd, event, argv);
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

  EraseMessageHandler(client_fd);
  EraseFileHandler   (client_fd);
  DeleteFiles        (client_fd);
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

void KServer::EraseMessageHandler(const int32_t& fd)
{
  auto it = m_message_handlers.find(fd);
  if (it != m_message_handlers.end())
  {
    m_message_handlers.erase(it);
    KLOG("Removed message handler");
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
  KLOG("Connection closed for {}", client_fd);
  if (GetSession(client_fd).active())
    EndSession(client_fd);
  OnClientExit(client_fd);
}

KSession KServer::GetSession(const int32_t& client_fd) const
{
  auto it = m_sessions.find(client_fd);
  return (it != m_sessions.fdend()) ? *it->second : KSession{};
}

void KServer::Status()
{
  size_t      tx{};
  size_t      rx{};
  std::string client_s;

  for (auto it = m_sessions.begin(); it != m_sessions.end();)
  {
    auto& session = it->second;
    session.verify();
    client_s += session.info();
    tx += session.tx;
    rx += session.rx;
    if (session.expired())
    {
      EndSession(session.fd);
      it = m_sessions.erase(it);
    }
    else
      it++;
  }

  VLOG("Server Status\nBytes sent: {}\nBytes recv: {}\nClients:\n{}", tx, rx, client_s);
  VLOG("Thread pool: there are currently {} active workers tending to sockets", SocketListener::count());
}

Request::Controller& KServer::GetController()
{
  return m_controller;
}

IPCManager& KServer::GetIPCMgr()
{
  return m_ipc_manager;
}

FileManager& KServer::GetFileMgr()
{
  return m_file_manager;
}
} // ns kiq
