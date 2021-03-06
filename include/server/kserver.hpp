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
#include "codec/uuid.h"
#include "codec/decoder.hpp"
#include "request/controller.hpp"
#include "system/process/ipc/manager/manager.hpp"

#define IF_NOT_HANDLING_PACKETS_FOR_CLIENT(x) if (m_file_pending_fd != x)

namespace KYO {
using namespace Decoder;
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
  void set_handler              (const Request::Controller &&handler);


private:
  void systemEventNotify        (int client_socket_fd, int system_event,
                                 std::vector<std::string> args);
  void closeConnections         ();
  uint8_t getNumConnections     ();
  void onTasksReady             (int client_socket_fd, std::vector<Task> tasks) ;
  void onProcessEvent           (std::string result, int mask, std::string request_id,
                                 int client_socket_fd, bool error);
  void sendEvent                (int client_socket_fd, std::string event,
                                 std::vector<std::string> argv);
  void sendSessionMessage       (int client_socket_fd, int status,
                                 std::string message = "", SessionInfo info = {});
  void onFileHandled            (int socket_fd, int result, uint8_t *&&f_ptr = NULL,
                                 size_t size = 0);
  void handlePendingFile        (std::shared_ptr<uint8_t[]> s_buffer_ptr,
                                 int client_socket_fd, uint32_t size);
  void handleStart              (std::string decoded_message, int client_socket_fd);
  void handleExecute            (std::string decoded_message, int client_socket_fd);
  void handleFileUploadRequest  (int client_socket_fd);
  void handleSchedule           (std::vector<std::string> task, int client_socket_fd);
  void handleOperation          (std::string decoded_message, int client_socket_fd);
  void handleIPC                (std::string message, int32_t client_socket_fd);
  void handleAppRequest         (int client_fd, std::string message);
  void handleScheduleRequest    (int client_fd, std::string message);
  virtual void onMessageReceived(int                      client_socket_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                 ssize_t&                 size) override;
  void handleStop               (int client_socket_fd);
  virtual void onConnectionClose(int client_socket_fd) override;
  void receiveMessage           (std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t client_socket_fd);
  bool eraseMessageHandler      (int32_t client_socket_fd);
  bool eraseFileHandler         (int client_socket_fd);
  void SetFileNotPending        ();
  void SetFilePending           (int32_t fd);
  bool HandlingFile             (int32_t fd);

  using FileHandlers = std::unordered_map<int32_t, FileHandler>;

  Request::Controller       m_controller;
  IPCManager                m_ipc_manager;
  std::vector<int>          m_client_connections;
  FileHandlers              m_file_handlers;
  FileHandlers              m_message_handlers;
  std::vector<KSession>     m_sessions;
  std::vector<ReceivedFile> m_received_files;
  bool                      m_file_pending;
  int                       m_file_pending_fd;
  bool                      m_message_pending;
  int32_t                   m_message_pending_fd;
};
};     // namespace KYO
