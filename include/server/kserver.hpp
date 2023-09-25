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
#include "kiqoder/kiqoder.hpp"
#include "request/controller.hpp"
#include "system/process/ipc/manager.hpp"
#include "session.hpp"

#define IF_NOT_HANDLING_PACKETS_FOR_CLIENT(x) if (m_file_pending_fd != x)

namespace kiq {


struct OutboundFile
{
  int32_t      fd;
  FileMetaData file;
};

class FileManager
{
public:
using ReceivedFiles_t = std::vector<ReceivedFile>;
using OutboundFiles_t = std::deque <OutboundFile>;

ReceivedFiles_t::iterator FindReceived(int32_t fd, std::string_view time)
{
  return std::find_if(m_received_files.begin(), m_received_files.end(), [fd, time](const ReceivedFile &file)
  { return (file.client_fd == fd && std::to_string(file.timestamp) == time); });
}

bool ReceivedExists(const ReceivedFiles_t::iterator& it)
{
  return it != m_received_files.end();
}

void Add(const ReceivedFile&& file)
{
  m_received_files.emplace_back(file);
}

void SaveFile(const ReceivedFiles_t::iterator& it, std::string_view filename)
{
  FileUtils::SaveFile(it->f_ptr, it->size, filename);
}

void EraseReceived(ReceivedFiles_t::iterator it)
{
  m_received_files.erase(it);
}

void EnqueueOutbound(int32_t fd, const std::vector<std::string>& files)
{
  m_file_sending    = true;
  m_file_sending_fd = fd;
  for (const auto file : FileMetaData::PayloadToMetaData(files))
    m_outbound_files.emplace_back(OutboundFile{fd, file});
}

OutboundFile& OutboundNext()
{
  return m_outbound_files.front();
}

OutboundFile Dequeue()
{
  const auto file = OutboundNext();
  m_outbound_files.pop_front();
  return file;
}

private:
  bool            m_file_sending   {false};
  int32_t         m_file_sending_fd{0};
  ReceivedFiles_t m_received_files;
  OutboundFiles_t m_outbound_files;
};

/**
 * \mainpage The KServer implements logicp's SocketListener and provides the KIQ
 * service to KStyleYo
 */
class KServer : public SocketListener
{
public:
  /**
   * Constructor
   */
  KServer(int argc, char **argv);
  ~KServer();

  void            Broadcast                (const std::string& event, const std::vector<std::string>& argv);
  void            SendEvent                (const int32_t& client_fd, const std::string& event,
                                            const std::vector<std::string>& argv);
  void            SendFile                 (const int32_t& client_fd, const std::string& filename);
  void            EraseMessageHandler      (const int32_t& client_fd);
  void            EraseFileHandler         (const int32_t& client_fd);
  Controller&     GetController            ();
  IPCManager&     GetIPCMgr                ();
  FileManager&    GetFileMgr               ();

private:
  using FileHandlers = std::unordered_map<int32_t, kiqoder::FileHandler>;


  virtual void onMessageReceived(int                      client_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                 ssize_t&                 size)         final;
  virtual void onConnectionClose(int32_t                  client_fd)    final;

  void        CloseConnections ();
  void        OnProcessEvent   (const std::string& result, int32_t mask, const std::string& id,
                                int32_t            client_fd, bool error);
  void        SendMessage      (const int32_t&     client_socket_fd, const std::string& message);
  void        OnFileHandled    (const int&         socket_fd, uint8_t *&&f_ptr = NULL,
                                size_t             size = 0);
  void        ReceiveFileData  (const std::shared_ptr<uint8_t[]>& s_buffer_ptr,
                                const size_t                      size,
                                const int32_t                     client_fd);
  void        InitClient       (const std::string& message, const int32_t& client_fd);
  void        WaitForFile      (const int32_t& client_fd);
  void        EnqueueFiles     (const int32_t& client_fd, const std::vector<std::string>& files);
  void        ScheduleRequest  (const std::vector<std::string>& task, const int32_t& client_fd);
  void        OperationRequest (const std::string& message, const int32_t& client_fd);
  void        EndSession       (const int32_t& client_fd, int32_t status = SESSION_INACTIVE);
  void        ReceiveMessage   (std::shared_ptr<uint8_t[]> s_buffer_ptr, uint32_t size, int32_t client_fd);
  void        OnClientExit     (const int32_t& client_fd);
  void        DeleteClientFiles(const int32_t& client_fd);
  void        SetFileNotPending();
  void        SetFilePending   (const int32_t& client_fd);
  bool        HandlingFile     (const int32_t& client_fd);
  void        SendPong         (int32_t client_fd);
  void        ValidateClients  ();
  std::string Status           ()                         const;
  KSession    GetSession       (const int32_t& client_fd) const;

  Controller                m_controller;
  IPCManager                m_ipc_manager;
  std::vector<int>          m_client_connections;
  FileHandlers              m_file_handlers;
  FileHandlers              m_message_handlers;
  SessionMap                m_sessions;
  FileManager               m_file_manager;

  std::deque <OutboundFile> m_outbound_files;
  bool                      m_file_pending;
  int32_t                   m_file_pending_fd;
  bool                      m_file_sending;
  int32_t                   m_file_sending_fd;
  bool                      m_message_pending;
  int32_t                   m_message_pending_fd;
  uint32_t                  m_errors{0};
};

};     // namespace kiq
