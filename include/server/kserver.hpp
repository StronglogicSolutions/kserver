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
#include "request/request_handler.hpp"
#include "system/process/ipc/manager/manager.hpp"

#define IF_NOT_HANDLING_PACKETS_FOR_CLIENT(x) if (file_pending_fd != x)

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
  KServer(int argc, char **argv)
  : SocketListener(argc, argv),
    m_ipc_manager(IPCManager{
      [this](int32_t event, std::vector<std::string> payload)
        {
          systemEventNotify(ALL_CLIENTS, event, payload);
        }
    }),
    file_pending(false),
    file_pending_fd(-1) {
      KLOG("Starting IPC manager");
      m_ipc_manager.start();
    }

  ~KServer() {
    KLOG("Server shutting down");
    m_file_handlers.clear();
    m_request_handler.shutdown();
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
  void systemEventNotify(int client_socket_fd, int system_event,
                         std::vector<std::string> args) {
    switch (system_event) {
      case SYSTEM_EVENTS__SCHEDULED_TASKS_READY: {
        if (client_socket_fd == -1) {
          KLOG(
              "Maintenance worker found tasks. Sending system-wide broadcast "
              "to all clients.");
          for (const auto &session : m_sessions) {
            IF_NOT_HANDLING_PACKETS_FOR_CLIENT(session.fd)
              sendEvent(session.fd, "Scheduled Tasks Ready", args);
          }
          break;
        } else {
          KLOG(
              "Informing client {} about "
              "scheduled tasks",
              client_socket_fd);
          IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
            sendEvent(client_socket_fd, "Scheduled Tasks Ready", args);
          break;
        }
      }
      case SYSTEM_EVENTS__SCHEDULED_TASKS_NONE: {
        if (client_socket_fd == -1) {
          KLOG(
              "Sending system-wide broadcast. There are currently no "
              "tasks ready for execution.");
          for (const auto &session : m_sessions) {
            IF_NOT_HANDLING_PACKETS_FOR_CLIENT(session.fd)
              sendEvent(session.fd, "No tasks ready", args);
          }
          break;
        } else {
          KLOG("Informing client {} about scheduled tasks", client_socket_fd);
          IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
            sendEvent(client_socket_fd, "No tasks ready to run", args);
          break;
        }
        break;
      }
      case SYSTEM_EVENTS__SCHEDULER_FETCH: {
        if (client_socket_fd != -1) {
          KLOG("Sending schedule fetch results to client {}", client_socket_fd);
          IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
            sendEvent(client_socket_fd, "Scheduled Tasks", args);
        }
        break;
      }
      case SYSTEM_EVENTS__SCHEDULER_UPDATE: {
        if (client_socket_fd != -1) {
          KLOG("Sending schedule update result to client {}", client_socket_fd);
          IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
            sendEvent(client_socket_fd, "Schedule PUT", args);
        }
        break;
      }
      case SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS: {
        if (client_socket_fd != -1) {
          KLOG("Sending schedule flag values to client {}", client_socket_fd);
          IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
            sendEvent(client_socket_fd, "Schedule Tokens", args);
        }
      break;
      }
      case SYSTEM_EVENTS__SCHEDULER_SUCCESS: {
        KLOG("Task successfully scheduled");
        if (client_socket_fd == -1) {
          for (const auto &session : m_sessions) {
            IF_NOT_HANDLING_PACKETS_FOR_CLIENT(session.fd)
              sendEvent(session.fd, "Task Scheduled", args);
          }
        } else {
          IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
            sendEvent(client_socket_fd, "Task Scheduled", args);
        }
        break;
      }
      case SYSTEM_EVENTS__PLATFORM_NEW_POST: {
        KLOG("Platform Post event received");
        if (m_request_handler.getScheduler().savePlatformPost(args))
        {
          std::vector<std::string> outgoing_args{};
          outgoing_args.reserve(args.size());
          for (const auto& arg : args) outgoing_args.emplace_back(
              (arg.size() > 2046) ?
                arg.substr(0, 2046) :
                arg
            );
          if (client_socket_fd == -1) {
            for (const auto &session : m_sessions) {
              IF_NOT_HANDLING_PACKETS_FOR_CLIENT(session.fd)
                sendEvent(session.fd, "Platform Post", args);
            }
          } else {
            IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
              sendEvent(client_socket_fd, "Platform Post", args);
          }

          // TODO: Find out what platforms have not yet reposted and
          //       send event to the ipc manager
          // m_ipc_manager.ReceiveEvent(SYSTEM_EVENTS__PLATFORM_NEW_POST)
        }
      }
      break;
      case SYSTEM_EVENTS__PLATFORM_POST_REQUESTED: {
        auto method = args.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX);
        if (method == "bot")
          m_ipc_manager.ReceiveEvent(SYSTEM_EVENTS__PLATFORM_POST_REQUESTED, args);
        else
          KLOG("Platform Post requested: Must implement process execution");
      }
      break;
      case SYSTEM_EVENTS__FILE_UPDATE: {
        // metadata for a received file
        auto timestamp = args.at(1);
        KLOG(
            "Updating information file information for client "
            "{}'s file received at {}",
            client_socket_fd, timestamp);

        auto received_file =
            std::find_if(m_received_files.begin(), m_received_files.end(),
                         [client_socket_fd, timestamp](ReceivedFile &file) {
                           // TODO: We need to change this so we are matching
                           // by UUID
                           return (file.client_fd == client_socket_fd &&
                                   std::to_string(file.timestamp) == timestamp);
                         });

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
      case SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED: {
        IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
          sendEvent(client_socket_fd, "Process Execution Requested", args);
        break;
      }
      case SYSTEM_EVENTS__REGISTRAR_SUCCESS: {
        IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
          sendEvent(client_socket_fd, args.front(), {args.begin() + 1, args.end()});
        break;
      }
      case SYSTEM_EVENTS__REGISTRAR_FAIL: {
        IF_NOT_HANDLING_PACKETS_FOR_CLIENT(client_socket_fd)
          sendEvent(client_socket_fd, args.front(), {args.begin() + 1, args.end()});
        break;
      }
    }
  }

  /**
   * Request Handler
   */
  void set_handler(const Request::RequestHandler &&handler) {
    KLOG("Setting RequestHandler");
    m_request_handler = handler;
    m_request_handler.initialize(
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

  void onTasksReady(int client_socket_fd, std::vector<Task> tasks) {
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
  void onProcessEvent(std::string result, int mask, std::string request_id,
                      int client_socket_fd, bool error) {
    std::string process_executor_result_str{};
    std::vector<std::string> event_args{};

    if (error) {
      event_args.reserve(4);
    } else {
      event_args.reserve(3);
    }

    KLOG("Received result {}", result);
    if (result.size() <=
        2046) {  // if process' stdout is small enough to send in one packet
      event_args.insert(event_args.end(),
                        {std::to_string(mask), request_id, result});
    } else {
      KLOG(
          "result too big to send in one message. Returning back the bottom "
          "2000 bytes of: \n{}",
          result);
      event_args.insert(event_args.end(),
                        {std::to_string(mask), request_id,
                         std::string{result.end() - 2000, result.end()}});
    }
    if (error) {
      event_args.push_back("Executed process returned an ERROR");
    }
    if (client_socket_fd == -1) {  // Send response to all active sessions
      for (const auto &session : m_sessions) {
        sendEvent(session.fd, "Process Result", event_args);
      }
    } else {  // Send response to specifically indicated session
      sendEvent(client_socket_fd, "Process Result", event_args);
    }

    if (Scheduler::isKIQProcess(mask)) {
      m_request_handler.getScheduler().handleProcessOutput(result, mask);
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
  void sendEvent(int client_socket_fd, std::string event,
                 std::vector<std::string> argv) {
    KLOG("Sending {} event to {}", event, client_socket_fd);
    for (const auto &arg : argv) {
      KLOG("Event arg - {}", arg);
    }
    std::string event_string = createEvent(event.c_str(), argv);
    sendMessage(client_socket_fd, event_string.c_str(), event_string.size());
  }

  void sendSessionMessage(int client_socket_fd, int status,
                          std::string message = "", SessionInfo info = {}) {
    std::string session_message = createSessionEvent(status, message, info);
    KLOG("Sending session message to {}.\nSession info: {}", client_socket_fd,
         session_message);
    sendMessage(client_socket_fd, session_message.c_str(),
                session_message.size());
  }

  /**
   * File Transfer Completion
   */
  void onFileHandled(int socket_fd, int result, uint8_t *&&f_ptr = NULL,
                     size_t size = 0) {
    if (file_pending_fd == socket_fd) {
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
          file_pending_fd = -1;
          file_pending = false;
          m_request_handler.setHandlingData(false);
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
  void handlePendingFile(std::shared_ptr<uint8_t[]> s_buffer_ptr,
                         int client_socket_fd, uint32_t size) {
    auto handler =
        std::find_if(m_file_handlers.begin(), m_file_handlers.end(),
                     [client_socket_fd](FileHandler &handler) {
                       return handler.isHandlingSocket(client_socket_fd);
                     });
    if (handler != m_file_handlers.end()) {
      handler->processPacket(s_buffer_ptr.get(), size);
    } else {
      KLOG("creating FileHandler for {}", client_socket_fd);
      FileHandler file_handler{client_socket_fd, "", s_buffer_ptr.get(), size,
                               [this](int socket_fd, int result, uint8_t *f_ptr,
                                      size_t buffer_size) {
                                 onFileHandled(socket_fd, result,
                                               std::move(f_ptr), buffer_size);
                               }};
      m_file_handlers.push_back(std::forward<FileHandler>(file_handler));
    }
    return;
  }

  /**
   * Start Operation
   */
  void handleStart(std::string decoded_message, int client_socket_fd) {
    // Session
    uuids::uuid const new_uuid = uuids::uuid_system_generator{}();
    m_sessions.push_back(
        KSession{.fd = client_socket_fd, .status = 1, .id = new_uuid});
    // Database fetch
    ServerData server_data = m_request_handler("Start");
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
  void handleExecute(std::string decoded_message, int client_socket_fd) {
    std::vector<std::string> args = getArgs(decoded_message.c_str());
    if (!args.empty() && args.size() > 1) {
      KLOG("Execute request received");
      auto mask = args.at(0);
      auto request_uuid = args.at(1);
      KLOG("Mask: {}  ID: {}", mask, request_uuid);
      m_request_handler(std::stoi(mask), request_uuid, client_socket_fd);
    }
  }

  /**
   * File Upload Operation
   */
  void handleFileUploadRequest(int client_socket_fd) {
    file_pending = true;
    file_pending_fd = client_socket_fd;
    m_request_handler.setHandlingData(true);

    std::string file_ready_message = createMessage("File Ready", "");
    sendMessage(client_socket_fd, file_ready_message.c_str(),
                file_ready_message.size());
  }

  void handleSchedule(std::vector<std::string> task, int client_socket_fd) {
    auto uuid = uuids::to_string(uuids::uuid_system_generator{}());
    sendEvent(client_socket_fd, "Processing Request", {"Schedule Task", uuid});
    m_request_handler("Schedule", task, client_socket_fd, uuid);
    KLOG("Task delivered to request handler");
  }

  /**
   * Operations are the processing of requests
   */
  void handleOperation(std::string decoded_message, int client_socket_fd) {
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
  }

  void handleIPC(std::string message, int32_t client_socket_fd) {
    m_ipc_manager.process(message, client_socket_fd);
  }


  void handleAppRequest(int client_fd, std::string message) {
    m_request_handler.process(client_fd, message);
  }

  void handleScheduleRequest(int client_fd, std::string message) {
    m_request_handler.process(client_fd, message);
  }

  /**
   * Override
   */
  virtual void onMessageReceived(int client_socket_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                 ssize_t &size) override {
    if (size > 0 && size != 4294967295) {
      // Get ptr to data
      std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
      if (file_pending && size != 5) {  // Handle packets for incoming file
        handlePendingFile(s_buffer_ptr, client_socket_fd, size);
        return;
      }
      // For other cases, handle operations or read messages
      neither::Either<std::string, std::vector<std::string>> decoded =
          getDecodedMessage(s_buffer_ptr);  //
      decoded
          .leftMap([this, client_socket_fd](auto decoded_message) {
            if (isPing(decoded_message)) {
              KLOG("Client {} - keepAlive", client_socket_fd);
              sendMessage(client_socket_fd, PONG, PONG_SIZE);
              return decoded_message;
            }
            std::string json_message = getJsonString(decoded_message);
            KLOG("Received message: {}", json_message);
            // Handle operations
            if (isOperation(decoded_message.c_str())) {
              KLOG("Received operation");
              handleOperation(decoded_message, client_socket_fd);
            } else if (isMessage(decoded_message.c_str())) {
              if (strcmp(getMessage(decoded_message.c_str()).c_str(),
                         "scheduler") == 0) {
                KLOG("Testing scheduler");
                m_request_handler(client_socket_fd, "Test",
                                  Request::DevTest::Schedule);
              } else if (strcmp(getMessage(decoded_message.c_str()).c_str(),
                                "execute") == 0) {
                KLOG("Testing task execution");
                m_request_handler(client_socket_fd, "Test",
                                  Request::DevTest::ExecuteTask);
              } else if (strcmp(getMessage(decoded_message.c_str()).c_str(), "schedule") == 0) {
                // TODO: temporary. This should be done by the client application
                std::string fetch_schedule_operation = createOperation(
                  "Schedule", {std::to_string(Request::RequestType::FETCH_SCHEDULE)}
                );
                m_request_handler.process(client_socket_fd, fetch_schedule_operation);
              }
              sendEvent(client_socket_fd, "Message Received",
                        {"Message received by KServer",
                         "The following was your message",
                         getMessage(decoded_message.c_str())});
            }
            return decoded_message;
          })
          .rightMap([this, client_socket_fd](auto task_args) {
            KLOG(
                "New message schema type "
                "received");
            if (!task_args.empty()) {
              KLOG(
                  "Scheduling operation "
                  "received");
              handleSchedule(task_args, client_socket_fd);
            } else {
              KLOG("Empty task");
            }
            return task_args;
          });
    }
  }

  void handleStop(int client_socket_fd) {
    sendEvent(
      client_socket_fd,
      "Close Session",
      {"KServer is shutting down the socket connection"}
    );

    shutdown(client_socket_fd, SHUT_RD);

    if (file_pending && file_pending_fd == client_socket_fd) {
      file_pending = false;
      file_pending_fd = -1;
    }

    auto it_session = std::find_if(m_sessions.begin(), m_sessions.end(),
                                   [client_socket_fd](KSession session) {
                                     return session.fd == client_socket_fd;
                                   });
    if (it_session != m_sessions.end()) {
      m_sessions.erase(it_session);
    }
  }

  void closeConnections() {
    for (const int& fd : m_client_connections) {
      handleStop(fd);
    }
  }

  uint8_t getNumConnections() {
    return m_sessions.size();
  }

 private:
  virtual void onConnectionClose(int client_socket_fd) {
    KLOG("Connection closed for {}", client_socket_fd);
    auto it_session = std::find_if(m_sessions.begin(), m_sessions.end(),
                                   [client_socket_fd](KSession session) {
                                     return session.fd == client_socket_fd;
                                   });
    if (it_session != m_sessions.end()) {
      m_sessions.erase(it_session);
    }
    if (!m_file_handlers.empty()) eraseFileHandler(client_socket_fd);
  }

  bool eraseFileHandler(int client_socket_fd) {
    KLOG("eraserFileHandler called with {}", client_socket_fd);
    if (!m_file_handlers.empty()) {
      for (auto it = m_file_handlers.begin(); it != m_file_handlers.end();
           it++) {
        if (it->isHandlingSocket(client_socket_fd)) {
          m_file_handlers.erase(it);
          KLOG("file handler removed");
          return true;
        }
      }
    }
    return false;
  }

  Request::RequestHandler     m_request_handler;
  IPCManager                  m_ipc_manager;
  std::vector<int>            m_client_connections;
  std::vector<FileHandler>    m_file_handlers;
  std::vector<KSession>       m_sessions;
  std::vector<ReceivedFile>   m_received_files;
  bool                        file_pending;
  int                         file_pending_fd;
};
};     // namespace KYO
