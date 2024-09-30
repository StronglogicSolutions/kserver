#include "event_handler.hpp"
#include "kserver.hpp"
#include "kiqoder/file_iterator.hpp"
#include <logger.hpp>
#include <sys/select.h>


namespace kiq
{
using namespace kiq::log;

static const int32_t NONE_PENDING {-1};
static const bool    KEEP_HEADER  {true};
static const char*   STATUS       {"status"};
static const char*   NEW_SESSION  {"New Session"};
static const char*   CLOSE_SESSION{"Close Session"};
static const char*   WELCOME_MSG  {"Session started"};
static const char*   REJECTED_MSG {"Session rejected"};
static const char*   GOODBYE_MSG  {"KServer is shutting down the socket connection"};
static const char*   RECV_MSG     {"Received by KServer"};

/**
 * \mainpage The KServer implements logicp's SocketListener and provides the KIQ
 * service to KStyleYo
 */

KServer::KServer(int argc, char **argv)
: SocketListener(argc, argv),
  m_controller(m_control_sock),
  m_file_pending(false),
  m_file_pending_fd(NONE_PENDING)
{
  evt::instance().set_server(this);

  klog().i("Starting IPC manager");
  m_ipc_manager.start();

  klog().i("Initializing controller");
  m_controller.Initialize(
    [this](std::string result, int mask, std::string request_id, const int32_t &client_fd, bool error)
    {
      OnProcessEvent(result, mask, request_id, client_fd, error);
    },
    [this]()
    {
      return Status();
    },
    [this]()
    {
      ValidateClients();
    });
}
//-----------------------------------------------------------------------------------------------------
KServer::~KServer()
{
  klog().i("Server shutting down");
  CloseConnections();
  m_controller.Shutdown();
}
//-----------------------------------------------------------------------------------------------------
void KServer::OnProcessEvent(const std::string& result, int32_t mask, const std::string& id, int32_t client_fd, bool error)
{
  klog().t("Task {} completed with result:\n{}", id, result);
  std::string process_executor_result_str{};
  std::vector<std::string> event_args{};

  event_args.reserve((error) ? 4 : 3);
  event_args.insert(event_args.end(), {std::to_string(mask), id, result});

  if (error)
    event_args.push_back("Executed process returned an ERROR");

  Broadcast("Process Result", event_args);

  if (Scheduler::isKIQProcess(mask))
    m_controller.ProcessSystemEvent(SYSTEM_EVENTS__PROCESS_COMPLETE, {result, std::to_string(mask)}, std::stoi(id));
}
//-----------------------------------------------------------------------------------------------------
void KServer::SendFile(const int32_t& client_fd, const std::string& filename)
{
  using F_Iterator = kiqoder::FileIterator<uint8_t>;
  using P_Wrapper  = kiqoder::FileIterator<uint8_t>::PacketWrapper;

  F_Iterator iterator{filename};

  while (iterator.has_data())
  {
    P_Wrapper packet = iterator.next();
    SocketListener::sendMessage(client_fd, reinterpret_cast<const char *>(packet.data()), packet.size);
  }

  klog().t("Sent {} file bytes", iterator.GetBytesRead());
  m_sessions.at(client_fd).tx += iterator.GetBytesRead();
  m_file_sending = false;
}
//-----------------------------------------------------------------------------------------------------
void KServer::SendEvent(const int32_t& client_fd, const std::string& event, const std::vector<std::string>& argv)
{
  klog().i("Sending {} event to {}", event, client_fd);
  if (client_fd == ALL_CLIENTS)
    Broadcast(event, argv);
  else
    SendMessage(client_fd, CreateEvent(event, argv));
}
//-----------------------------------------------------------------------------------------------------
void KServer::SendMessage(const int32_t& client_fd, const std::string& message)
{
  using F_Iterator = kiqoder::FileIterator<char>;
  using P_Wrapper  = kiqoder::FileIterator<char>::PacketWrapper;

  const size_t& size    {message.size()};
  F_Iterator    iterator{message.data(), size};
  klog().t("Sending {} bytes to {}", size, client_fd);

  while (iterator.has_data())
  {
    P_Wrapper packet = iterator.next();
    SocketListener::sendMessage(client_fd, reinterpret_cast<const char *>(packet.data()), packet.size);
  }

  m_sessions.at(client_fd).tx += iterator.GetBytesRead();
}
//-----------------------------------------------------------------------------------------------------
void KServer::OnFileHandled(const int& socket_fd, uint8_t*&& f_ptr, size_t size)
{
  if (m_file_pending_fd != socket_fd)
    klog().i("Lost file intended for {}", socket_fd);
  else
  {
    const auto timestamp = TimeUtils::UnixTime();
    m_file_manager.Add({timestamp, socket_fd, f_ptr, size});
    klog().i("Received file from {} at {}", socket_fd, timestamp);
    SendEvent(socket_fd, "File Transfer Complete", {std::to_string(timestamp)});
    SetFileNotPending();
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::ReceiveFileData(const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                              const size_t                      size,
                              const int32_t                     client_fd)
{
  using FileHandler = kiqoder::FileHandler;

  if (auto handler = m_file_handlers.find(client_fd); handler != m_file_handlers.end())
    handler->second.processPacket(s_buffer_ptr.get(), size);
  else
  {
    klog().i("creating FileHandler for {}", client_fd);
    m_file_handlers.insert({client_fd, FileHandler{
                                            [this](int32_t fd, uint8_t *data, size_t size)
                                            { OnFileHandled(fd, std::move(data), size); }}});
    m_file_handlers.at(client_fd).setID(client_fd);
    m_file_handlers.at(client_fd).processPacket(s_buffer_ptr.get(), size);
  }

  m_sessions.at(client_fd).rx += size;
}
//-----------------------------------------------------------------------------------------------------
void KServer::EnqueueFiles(const int32_t& client_fd, const std::vector<std::string>& files)
{
  m_file_sending    = true;
  m_file_sending_fd = client_fd;
  for (const auto file : FileMetaData::PayloadToMetaData(files))
    m_outbound_files.emplace_back(OutboundFile{client_fd, file});
}
//-----------------------------------------------------------------------------------------------------
void KServer::InitClient(const std::string& message, const int32_t& fd)
{
  auto GetData = [this]()
  { return CreateMessage(NEW_SESSION, m_controller.CreateSession()); };
  auto GetInfo = [](auto state, auto id)
  { return SessionInfo{{STATUS, std::to_string(state)}, {"uuid", id}}; };
  auto GetError = [](const auto reason)
  { return SessionInfo{{STATUS, std::to_string(SESSION_INVALID)},
                        {"reason", reason}}; };
  auto NewSession = [&fd](const uuid &id, const User &user)
  { return KSession{user, fd, SESSION_ACTIVE, id}; };

  const User user = GetAuth(message);
  const uuids::uuid n_uuid = uuids::uuid_system_generator{}();
  const std::string uuid_s = uuids::to_string(n_uuid);

  if (m_sessions.init(user.name, NewSession(n_uuid, user)) && ValidateToken(user))
  {
    klog().i("Started session {} for {}", uuid_s, fd);
    SendMessage(fd, GetData());
    SendMessage(fd, CreateSessionEvent(SESSION_ACTIVE, WELCOME_MSG, GetInfo(SESSION_ACTIVE, uuid_s)));
  }
  else
  {
    klog().e("Rejected session request for {} on {}", user.name, fd);
    SendMessage(fd, CreateSessionEvent(SESSION_INVALID, REJECTED_MSG, GetError("Invalid token")));
    EndSession(fd, SESSION_INVALID);
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::WaitForFile(const int32_t& client_fd)
{
  SetFilePending(client_fd);
  SendMessage(client_fd, CreateMessage("File Ready", ""));
}
//-----------------------------------------------------------------------------------------------------
void KServer::ScheduleRequest(const std::vector<std::string>& task, const int32_t& client_fd)
{
  const auto uuid = uuids::to_string(uuids::uuid_system_generator{}());
  SendEvent(client_fd, "Processing Schedule Request", {"Schedule Task", uuid});
  m_controller("Schedule", task, client_fd, uuid);
  klog().i("Legacy Schedule Request: delivered to controller");
}
//-----------------------------------------------------------------------------------------------------
void KServer::OperationRequest(const std::string& message, const int32_t& client_fd)
{
  const KOperation op = GetOperation(message);
  if (IsStartOperation(op))
  {
    try
    {
      InitClient(message, client_fd);
    }
    catch (const std::exception &e)
    {
      klog().e("Exception thrown during InitClient: {}", e.what());
    }
  }
  else if (IsStopOperation(op))
  {
    EndSession(client_fd);
  }
  else if (IsFileUploadOperation(op))
  {
    WaitForFile(client_fd);
  }
  // else if (IsIPCOperation(op))
  // {
  //   m_ipc_manager.ReceiveEvent(SYSTEM_EVENTS__IPC_REQUEST, {message});
  // }
  else
  {
    try
    {
      m_controller.ProcessClientRequest(client_fd, message);
    }
    catch (const std::exception &e)
    {
      klog().e("Exception thrown during Controller::ProcessClientRequest: {}", e.what());
    }
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::onMessageReceived(int                      client_fd,
                                std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                ssize_t&                 size)
{
  if (!size)
    return;

  try
  {
    const auto s_buffer_ptr = w_buffer_ptr.lock();
    (m_file_pending) ? ReceiveFileData(s_buffer_ptr, size, client_fd) : ReceiveMessage(s_buffer_ptr, size, client_fd);
  }
  catch (const std::exception &e)
  {
    klog().e("Exception thrown while handling message.\n{}", e.what());
    m_errors++;
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::SendPong(int32_t client_fd)
{
  try
  {
    m_sessions.at(client_fd).notify();
    SendMessage(client_fd, PONG);
  }
  catch (const std::exception &e)
  {
    klog().e("Exception thrown from SendPong: {}", e.what());
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::EndSession(const int32_t& client_fd, int32_t status)
{
  static const int SUCCESS{0};
  auto GetStats = [](const KSession &session)
  { return "RX: " + std::to_string(session.rx) +
            "\nTX: " + std::to_string(session.tx); };

  if (m_sessions.has(client_fd))
  {
    klog().i("Shutting down session for client {}.\nStatistics:\n{}", client_fd, GetStats(m_sessions.at(client_fd)));
    m_sessions.at(client_fd).status = status;
    if (HandlingFile(client_fd))
      SetFileNotPending();
  }
  else
    klog().i("Shutting down socket for client with no session");

  klog().t("Calling shutdown on fd {}", client_fd);
  if (shutdown(client_fd, SHUT_RD) != SUCCESS)
    klog().e("Error shutting down socket\nCode: {}\nMessage: {}", errno, strerror(errno));

  if (SocketListener::revoke(client_fd))
    klog().i("Revoked {}", client_fd);
  else
    klog().e("Failed to revoke {}", client_fd);
}
//-----------------------------------------------------------------------------------------------------
void KServer::CloseConnections()
{
  for (const int &fd : m_client_connections)
    EndSession(fd);
}
//-----------------------------------------------------------------------------------------------------
void KServer::ReceiveMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t fd)
{
  using FileHandler = kiqoder::FileHandler;
  auto TrackDataStats = [this](int fd, size_t size)
  { if (m_sessions.has(fd)) m_sessions.at(fd).rx += size; };
  auto ProcessMessage = [this](int fd, uint8_t *m_ptr, size_t buffer_size)
  {
    DecodeMessage(m_ptr).leftMap([this, fd](auto &&message)
                                  {
    auto HasValidToken  = [this](int fd, const auto& msg){ return (GetToken(msg) == m_sessions.at(fd).user.token); };
    try
    {
      if (message.empty())
        klog().e("Failed to decode message");
      else
      if (IsPing(message))
        SendPong(fd);
      else
      if (m_sessions.has(fd) && m_sessions.at(fd).active() && !HasValidToken(fd, message))
        EndSession(fd);
      else
      if (IsOperation(message))
      {
        try
        {
          OperationRequest(message, fd);
        }
        catch (const std::exception& e)
        {
          klog().e("Exception thrown during OperationRequest: {}", e.what());
        }
      }
      else
      if (IsMessage(message))
        SendEvent(fd, "Message Received", {RECV_MSG, "Message", GetMessage(message)});
      else
        EndSession(fd);
      }
      catch (const std::exception& e)
      {
        klog().e("Exception thrown from DecodeMessage LEFT callback: {}", e.what());
      }

    return message; })
        .rightMap([this, fd](auto &&args)
                  {
    (args.size()) ? ScheduleRequest(args, fd) :
                    klog().e("Failed to decode message");
    return args; });
  };

  try
  {
    auto it = m_message_handlers.find(fd);
    try
    {
      if (it != m_message_handlers.end())
        it->second.processPacket(s_buffer_ptr.get(), size);
      else
      {
        klog().i("Creating message handler for {}", fd);
        m_message_handlers.insert({fd, FileHandler{ProcessMessage, KEEP_HEADER}});
        m_message_handlers.at(fd).setID(fd);
        m_message_handlers.at(fd).processPacket(s_buffer_ptr.get(), size);
      }
    }
    catch (const std::exception &e)
    {
      klog().e("Exception thrown from ReceiveMessage while processing a packet?: {}", e.what());
    }
    TrackDataStats(fd, size);
  }
  catch (const std::exception &e)
  {
    klog().e("Exception caught while processing client message: {}", e.what());
    throw;
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::Broadcast(const std::string& event, const std::vector<std::string>& argv)
{
  for (const auto &[_, session] : m_sessions)
    if (session.active())
      SendEvent(session.fd, event, argv);
}
//-----------------------------------------------------------------------------------------------------
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
  EraseFileHandler(client_fd);
  DeleteFiles(client_fd);
}
//-----------------------------------------------------------------------------------------------------
void KServer::EraseFileHandler(const int32_t& fd)
{
  auto it = m_file_handlers.find(fd);
  if (it != m_file_handlers.end())
  {
    m_file_handlers.erase(it);
    klog().i("Removed file handler");
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::EraseMessageHandler(const int32_t& fd)
{
  auto it = m_message_handlers.find(fd);
  if (it != m_message_handlers.end())
  {
    m_message_handlers.erase(it);
    klog().i("Removed message handler");
  }
}
//-----------------------------------------------------------------------------------------------------
void KServer::SetFileNotPending()
{
  m_file_pending    = false;
  m_file_pending_fd = -1;
}
//-----------------------------------------------------------------------------------------------------
void KServer::SetFilePending(const int32_t& fd)
{
  m_file_pending    = true;
  m_file_pending_fd = fd;
}
//-----------------------------------------------------------------------------------------------------
bool KServer::HandlingFile(const int32_t& fd)
{
  return (m_file_pending && m_file_pending_fd == fd);
}
//-----------------------------------------------------------------------------------------------------
void KServer::onConnectionClose(int32_t client_fd)
{
  klog().i("Connection closed for {}", client_fd);
  klog().i("Ending session");
  EndSession(client_fd);
  OnClientExit(client_fd);
}
//-----------------------------------------------------------------------------------------------------
KSession KServer::GetSession(const int32_t& client_fd) const
{
  try
  {
    auto it = m_sessions.find(client_fd);
    if (it != m_sessions.fdend())
      *it->second;
  }
  catch (const std::exception &e)
  {
    klog().e("Exception thrown in GetSession: {}", e.what());
  }
  return KSession{};
}
//-----------------------------------------------------------------------------------------------------
std::string KServer::Status() const
{
  size_t tx{};
  size_t rx{};
  std::string client_s;

  for (const auto &[_, session] : m_sessions)
  {
    client_s += session.info();
    tx += session.tx;
    rx += session.rx;
  }

  return fmt::format("Server Status\nBytes sent: {}\nBytes recv: {}\nErrors: {}\nClients:\n{}\nThread pool: there are currently {} active workers tending to sockets", tx, rx, m_errors, client_s, SocketListener::count());
}
//-----------------------------------------------------------------------------------------------------
void KServer::ValidateClients()
{
  for (auto it = m_sessions.begin(); it != m_sessions.end();)
  {
    auto &session = it->second;
    session.verify();
    if (session.expired())
    {
      EndSession(session.fd);
      it = m_sessions.erase(it);
    }
    else
      it++;
  }
}
//-----------------------------------------------------------------------------------------------------
Controller &KServer::GetController()
{
  return m_controller;
}
//-----------------------------------------------------------------------------------------------------
IPCManager &KServer::GetIPCMgr()
{
  return m_ipc_manager;
}
//-----------------------------------------------------------------------------------------------------
FileManager &KServer::GetFileMgr()
{
  return m_file_manager;
}

//-----------------------------------------------------------------------------------------------------
void KServer::run()
{
  const auto client_thread = std::thread(&SocketListener::run, this);

  int control_sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, control_sockets) < 0)
  {
    klog().e("Socket pair creation failed");
    throw std::runtime_error("Could not create control socket pair to govern KServer run behaviour");
  }

  fd_set read_fds;
  bool housekeeping_running = true;

  int& control_sock = control_sockets[0]; // read from in this method only
  m_control_sock    = control_sockets[1]; // passed to controller

  for (;;)
  {
    FD_ZERO(&read_fds);
    FD_SET(control_sock, &read_fds);
    int max_fd = control_sock;

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (activity > 0 && FD_ISSET(control_sock, &read_fds))
    {
      // Read control message
      int control_message;
      ssize_t bytes_read = read(control_sock, &control_message, sizeof(control_message));
      if (bytes_read)
      {
        if (!control_message)
        {
          klog().w("Controller has requested shutdown");
          break;
        }
        else
          klog().i("KServer is running");
      }
    }
  }
}
} // ns kiq
