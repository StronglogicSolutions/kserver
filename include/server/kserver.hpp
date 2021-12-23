#pragma once

#include <math.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iomanip>
#include <string>
#include <string_view>
#include <utility>

#include "interface/socket_listener.hpp"
#include "log/logger.h"
#include "kiqoder/kiqoder.hpp"
#include "request/controller.hpp"
#include "system/process/ipc/manager/manager.hpp"

#define IF_NOT_HANDLING_PACKETS_FOR_CLIENT(x) if (m_file_pending_fd != x)

namespace kiq {

struct OutboundFile
{
  int32_t      fd;
  FileMetaData file;
};


/**
 * \mainpage The KServer implements logicp's SocketListener and provides the KIQ
 * service to KStyleYo
 */
class KServer : public SocketListener {
 public:
  /**
   * Constructor
   */
  KServer(int argc, char **argv);
  ~KServer();

private:
  void SystemEvent              (const int32_t&                  client_socket_fd,
                                 const int32_t&                  system_event,
                                 const std::vector<std::string>& args);
  void CloseConnections         ();
  void OnProcessEvent           (const std::string& result, int32_t mask, const std::string& id,
                                 int32_t client_fd, bool error);
  void SendMessage              (const int32_t& client_socket_fd, const std::string& message);
  void SendEvent                (const int32_t& client_fd, std::string event,
                                 std::vector<std::string> argv);
  void OnFileHandled            (const int& socket_fd, uint8_t *&&f_ptr = NULL,
                                 size_t size = 0);
  void ReceiveFileData          (const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                                 const int32_t&                    client_fd,
                                 const size_t&                     size);
  void InitClient               (const std::string& message, const int32_t& client_fd);
  void WaitForFile              (const int32_t& client_fd);
  void EnqueueFiles             (const int32_t& client_fd, const std::vector<std::string>& files);
  void ScheduleRequest          (const std::vector<std::string>& task, const int32_t& client_fd);
  void OperationRequest         (const std::string& message, const int32_t& client_fd);
  virtual void onMessageReceived(int                      client_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                 ssize_t&                 size) override;
  void EndSession               (const int32_t& client_fd);
  virtual void onConnectionClose(int32_t client_fd) override;
  void ReceiveMessage           (std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t client_fd);
  void OnClientExit             (const int32_t& client_fd);
  void EraseFileHandler         (const int32_t& client_fd);
  void DeleteClientFiles        (const int32_t& client_fd);
  void SetFileNotPending        ();
  void SetFilePending           (const int32_t& client_fd);
  bool HandlingFile             (const int32_t& client_fd);
  void SendFile                 (const int32_t& client_fd, const std::string& filename);
  void SendPong                 (int32_t client_fd);
  void Status                   () const;

  using FileHandlers = std::unordered_map<int32_t, Kiqoder::FileHandler>;
  using Sessions     = std::unordered_map<int32_t, KSession>;
  Request::Controller       m_controller;
  IPCManager                m_ipc_manager;
  std::vector<int>          m_client_connections;
  FileHandlers              m_file_handlers;
  FileHandlers              m_message_handlers;
  Sessions                  m_sessions;
  std::vector<ReceivedFile> m_received_files;
  std::deque <OutboundFile> m_outbound_files;
  bool                      m_file_pending;
  int32_t                   m_file_pending_fd;
  bool                      m_file_sending;
  int32_t                   m_file_sending_fd;
  bool                      m_message_pending;
  int32_t                   m_message_pending_fd;
};
};     // namespace kiq
