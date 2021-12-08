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
#include "kiqoder/kiqoder.hpp"

#include "request/controller.hpp"
#include "system/process/ipc/manager/manager.hpp"

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
void InitClient               (const std::string& message, const int32_t& client_fd);
void CloseConnections         ();
void EndSession               (const int32_t& client_fd);

void SystemEvent              (const int32_t&                  client_socket_fd,
                               const int32_t&                  system_event,
                               const std::vector<std::string>& args);

virtual void onConnectionClose(int32_t client_fd)                         override;
virtual void onMessageReceived(int                      client_socket_fd,
                               std::weak_ptr<uint8_t[]> w_buffer_ptr,
                               ssize_t&                 size)             override;

void OnTasksReady             (const int32_t& client_fd, const std::vector<Task>& tasks);
void OnProcessEvent           (const std::string& result,
                               const uint32_t&    mask,
                               const std::string& request_id,
                               const int32_t&     client_fd,
                               const bool&        error);
void OnFileHandled            (const int32_t& client_fd, uint8_t*&& f_ptr = nullptr,
                               const size_t& size = 0);
void OnClientExit             (const int32_t& client_fd);


void ScheduleRequest          (const std::vector<std::string>& task, const int32_t& client_fd);
void OperationRequest         (const std::string& message, const int32_t& client_fd);

void ReceiveMessage           (const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                               const uint32_t&                   size,
                               const int32_t&                    client_socket_fd);
void ReceiveFileData          (const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                               const int32_t&                    client_fd,
                               const size_t&                     size);

void SendMessage              (const int32_t& client_fd, const std::string& message);
void SendFile                 (const int32_t& client_fd, const std::string& filename);
void SendEvent                (const int32_t& client_fd, const std::string& event,
                               const std::vector<std::string>& argv);


void SetFileNotPending        ();
void SetFilePending           (const int32_t& fd);


void WaitForFile              (const int32_t& client_fd);
void EnqueueFiles             (const int32_t& client_fd, const std::vector<std::string>& files);
void DeleteClientFiles        (const int32_t& client_fd);
void EraseFileHandler         (const int32_t& client_fd);
bool HandlingFile             (const int32_t& client_fd);

using FileHandlers = std::unordered_map<int32_t, Kiqoder::FileHandler>;

Request::Controller       m_controller;
IPCManager                m_ipc_manager;
std::vector<int>          m_client_connections;
FileHandlers              m_file_handlers;
FileHandlers              m_message_handlers;
std::vector<KSession>     m_sessions;
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
