#ifndef __KSERVER_HPP__
#define __KSERVER_HPP__

#include <codec/uuid.h>
#include <log/logger.h>
#include <math.h>

#include <algorithm>
#include <codec/util.hpp>
#include <cstring>
#include <functional>
#include <interface/socket_listener.hpp>
#include <iomanip>
#include <request/request_handler.hpp>
#include <server/types.hpp>
#include <string>
#include <string_view>
#include <types/types.hpp>
#include <utility>
namespace KYO {

KLogger *k_logger_ptr = KLogger::GetInstance();

auto KLOG = k_logger_ptr->get_logger();

class FileHandler {
 public:
  class File {
   public:
    uint8_t *b_ptr;
    uint32_t size;
    bool complete;
  };

  class Decoder {
   public:
    Decoder(int fd, std::string name,
            std::function<void(uint8_t *, int, std::string)> file_callback)
        : index(0),
          file_buffer(nullptr),
          total_packets(0),
          packet_offset(0),
          buffer_offset(0),
          file_size(0),
          filename(name),
          m_fd(fd),
          m_file_cb(file_callback) {
            KLOG->info("FileHandler::Decoder::Decoder() - instantiated");
          }

    ~Decoder() {
      KLOG->info("FileHandler::Decoder::~Decoder() - destructor called");
      if (file_buffer != nullptr) {
        KLOG->info("FileHandler::Decoder::~Decoder() - Deleting buffer");
        delete[] file_buffer;
        file_buffer = nullptr;
      }
    }

    void processPacket(uint8_t *data) {
      bool is_first_packet = (index == 0);
      if (is_first_packet) {
        KLOG->info("Decoder::processPacket() - processing first packet");
        file_size =
            int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) -
            HEADER_SIZE;
        total_packets = static_cast<uint32_t>(ceil(
            static_cast<double>(file_size + HEADER_SIZE) / MAX_PACKET_SIZE));
        file_buffer = new uint8_t[total_packets * MAX_PACKET_SIZE];
        packet_offset = HEADER_SIZE;
        buffer_offset = 0;
        uint32_t first_packet_size =
            total_packets == 1 ? (file_size - HEADER_SIZE) : MAX_PACKET_SIZE;
        std::memcpy(file_buffer + buffer_offset, data + packet_offset,
                    first_packet_size);
        if (index == (total_packets - 1)) {
          // handle file, cleanup, return
          m_file_cb(file_buffer, file_size, filename);
          return;
        }
        index++;
        return;
      } else {
        buffer_offset = (index * MAX_PACKET_SIZE) - HEADER_SIZE;
        bool is_last_packet = (index == (total_packets - 1));
        if (!is_last_packet) {
          std::memcpy(file_buffer + buffer_offset, data, MAX_PACKET_SIZE);
          index++;
        } else {
          KLOG->info("Decoder::processPacket() - processing last packet");
          uint32_t last_packet_size = file_size - buffer_offset;
          std::memcpy(file_buffer + buffer_offset, data, last_packet_size);
          // completion callback
          m_file_cb(file_buffer, file_size, filename);
          m_files.push_back(
              File{.b_ptr = file_buffer, .size = file_size, .complete = true});
          index = 0;
          packet_offset = 0;
          total_packets = 0;
          buffer_offset = 0;
          file_size = 0;
          KLOG->info("Decoder::processPacket() - cleaning up");
        }
      }
    }

   private:
    uint8_t *file_buffer;
    uint32_t index;
    uint32_t total_packets;
    uint32_t packet_offset;
    uint32_t buffer_offset;
    uint32_t file_size;
    std::string filename;
    int m_fd;
    std::vector<File> m_files;
    std::function<void(int)> m_cb;
    std::function<void(uint8_t *data, int size, std::string)> m_file_cb;
  };

  FileHandler(int client_fd, std::string name, uint8_t *first_packet,
              std::function<void(int, int, uint8_t *, size_t)> callback)
      : socket_fd(client_fd) {
        KLOG->info("FileHandler() - Instantiated. Creating new Decoder");
    m_decoder =
        new Decoder(client_fd, name,
                    [this, client_fd, callback](uint8_t *data, int size,
                                                std::string filename) {
                      if (size > 0) {
                        if (!filename.empty()) {
                          // Read to save TODO: Check to see if pointer is not
                          // null and size > 0?
                          FileUtils::saveFile(data, size, filename);
                        } else {
                          callback(client_fd, FILE_HANDLE__SUCCESS, data, size);
                        }
                      }
                    });
    m_decoder->processPacket(first_packet);
  }

  FileHandler(FileHandler &&f)
      : m_decoder(f.m_decoder), socket_fd(f.socket_fd) {
    f.m_decoder = nullptr;
  }

  FileHandler(const FileHandler &f)
      : m_decoder(new Decoder{*(f.m_decoder)}), socket_fd(f.socket_fd) {}

  FileHandler &operator=(const FileHandler &f) {
    if (&f != this) {
      delete m_decoder;
      m_decoder = nullptr;
      m_decoder = new Decoder{*(f.m_decoder)};
    }
    return *this;
  }

  FileHandler &operator=(FileHandler &&f) {
    if (&f != this) {
      delete m_decoder;
      m_decoder = f.m_decoder;
      f.m_decoder = nullptr;
    }
    return *this;
  }

  ~FileHandler() { delete m_decoder; }
  void processPacket(uint8_t *data) { m_decoder->processPacket(data); }
  bool isHandlingSocket(int fd) { return fd == socket_fd; }

 private:
  Decoder *m_decoder;
  int socket_fd;
};

class KServer : public SocketListener {
 public:
  /**
   * Constructor
   */
  KServer(int argc, char **argv)
      : SocketListener(argc, argv), file_pending(false), file_pending_fd(-1) {
    KLOG->info("KServer() - initialized");
  }
  ~KServer() {
    KLOG->info("~KServer() - Server shutting down");
    m_file_handlers.clear();
  }

  /**
   * systemEventNotify
   *
   * Handles notifications sent back to the system delivering event messages
   *
   * @param[in] {int} client_socket_fd
   * @param[in] {int} system_event
   * @param[in] {std::vector<std::string>}
   */
  void systemEventNotify(int client_socket_fd, int system_event,
                         std::vector<std::string> args) {
    switch (system_event) {
      case SYSTEM_EVENTS__SCHEDULED_TASKS_READY: {
        if (client_socket_fd == -1) {
          KLOG->info("KServer::systemEventNotify() - "
              "Maintenance worker found tasks. Sending system-wide broadcast "
              "to all clients.");
          args.push_back("SYSTEM-WIDE BROADCAST was intended for all clients");
          for (const auto &session : m_sessions) {
            sendEvent(session.fd, "Scheduled Tasks Ready", args);
          }
          break;
        } else {
          KLOG->info("KServer::systemEventNotify() - Informing client {} about scheduled tasks",
                     client_socket_fd);
          sendEvent(client_socket_fd, "Scheduled Tasks Ready", args);
          break;
        }
      }
      case SYSTEM_EVENTS__SCHEDULED_TASKS_NONE: {
        if (client_socket_fd == -1) {
          KLOG->info("KServer::systemEventNotify() - "
              "Sending system-wide broadcast. There are currently no "
              "tasks ready for execution.");
          args.push_back("SYSTEM-WIDE BROADCAST was intended for all clients");
          for (const auto &session : m_sessions) {
            sendEvent(session.fd, "No tasks ready", args);
          }
          break;
        } else {
          KLOG->info("KServer::systemEventNotify () - Informing client {} about scheduled tasks",
                     client_socket_fd);
          sendEvent(client_socket_fd, "No tasks ready to run", args);
          break;
        }
        break;
      }
      case SYSTEM_EVENTS__FILE_UPDATE: {
        // incoming file has new information, such as a filename to be
        // assigned to it
        auto timestamp = args.at(1);
        KLOG->info(
            "KServer::systemEventNotify() - Updating information file information for client "
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
          // // TODO: We must do this
          KLOG->info("KServer::systemEventNotify() - Data buffer found. Creating directory and saving file");
          std::string uuid = args.at(2);
          std::string filename{"data/"};
          filename += uuid.c_str();
          filename += +"/";
          filename += args.at(0);
          FileUtils::createDirectory(uuid.c_str());
          FileUtils::saveFile(received_file->f_ptr, received_file->size,
                              filename.c_str());
          m_received_files.erase(received_file);

          if (args.size() == 4 && args.at(3) == "final file") {
            eraseFileHandler(client_socket_fd); // this client's file handler is no longer needed
          }
          sendEvent(client_socket_fd, "File Save Success", { timestamp });
        } else {
          KLOG->info("KServer::systemEventNotify() - Unable to find file");
          sendEvent(client_socket_fd, "File Save Failure", { timestamp });
        }
        break;
      }
      case SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED: {
        sendEvent(client_socket_fd, "Process Execution Requested", args);
        break;
      }
    }
  }

  /**
   * Request Handler
   */
  void set_handler(const Request::RequestHandler &&handler) {
    KLOG->info("KServer::setHandler() - Setting RequestHandler");
    m_request_handler = handler;
    m_request_handler.initialize(
        [this](std::string result, int mask, std::string request_id,
               int client_socket_fd) {
          onProcessEvent(result, mask, request_id, client_socket_fd);
        },
        [this](int client_socket_fd, int system_event,
               std::vector<std::string> args) {
          systemEventNotify(client_socket_fd, system_event, args);
        },
        [this](int client_socket_fd, std::vector<Executor::Task> tasks) {
          onTasksReady(client_socket_fd, tasks);
        });
  }

  void onTasksReady(int client_socket_fd, std::vector<Executor::Task> tasks) {
    KLOG->info("Scheduler has delivered {} tasks for processing", tasks.size());
  }

  /**
   * onProcessEvent
   *
   * Callback function for receiving the results of executed processes
   *
   * param[in] {std::string} result
   * param[in] {int} mask
   * param[in] {int} client_socket_fd
   */
  void onProcessEvent(std::string result, int mask, std::string request_id,
                      int client_socket_fd) {
    if (result.size() <= 2046) {
      std::vector<std::string> event_args{std::to_string(mask), request_id,
                                          result};
      std::string process_executor_result_str =
          createEvent("Process Result", event_args);
      sendMessage(client_socket_fd, process_executor_result_str.c_str(),
                  process_executor_result_str.size());
    } else {
      KLOG->info(
          "KServer::onProcessEvent() - result too big to send in one "
          "message. Result was \n{}", result);
      std::vector<std::string> event_args{
          std::to_string(mask), request_id,
          "Result completed, but was too big to display"};
      std::string process_executor_result_str =
          createEvent("Process Result", event_args);
      sendMessage(client_socket_fd, process_executor_result_str.c_str(),
                  process_executor_result_str.size());
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
    KLOG->info("KServer::sendEvent() - Sending {} event to {}", event, client_socket_fd);
    for (const auto& arg : argv) {
      KLOG->info("KServer::sendEvent() - Event arg - {}", arg);
    }
    std::string event_string = createEvent(event.c_str(), argv);
    KLOG->info("Event JSON: \n{}", event_string);
    sendMessage(client_socket_fd, event_string.c_str(), event_string.size());
  }

  void sendSessionMessage(int client_socket_fd, int status,
                          std::string message = "", SessionInfo info = {}) {
    std::string session_message = createSessionEvent(status, message, info);
    KLOG->info("KServer::sendSessionMessage() - Sending session message to {}.\n Session info: {}",
               client_socket_fd, session_message);
    sendMessage(client_socket_fd, session_message.c_str(),
                session_message.size());
  }

  /**
   * File Transfer Completion
   */
  void onFileHandled(int socket_fd, int result, uint8_t *f_ptr = NULL,
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
          KLOG->info("KServer::onFileHandled() - Finished handling file for client {}", socket_fd);
          file_pending_fd = -1;
          file_pending = false;
          sendEvent(socket_fd, "File Transfer Complete",
                    {std::to_string(timestamp)});
          return;
        }
      }
      KLOG->info("KServer::onFileHandled() - File transfer failed");
      sendEvent(socket_fd, "File Transfer Failed", {});  // Nothing saved
    }
    KLOG->info("KServer::onFileHandled() - Lost file intended for {}", socket_fd);
  }

  /**
   * Ongoing File Transfer
   */
  void handlePendingFile(std::shared_ptr<uint8_t[]> s_buffer_ptr,
                         int client_socket_fd) {
    auto handler =
        std::find_if(m_file_handlers.begin(), m_file_handlers.end(),
                     [client_socket_fd](FileHandler &handler) {
                       return handler.isHandlingSocket(client_socket_fd);
                     });
    if (handler != m_file_handlers.end()) {
      handler->processPacket(s_buffer_ptr.get());
    } else {
      KLOG->info("KServer::handlePendingFile() - creating FileHandler for {}", client_socket_fd);
      FileHandler file_handler{
          client_socket_fd, "", s_buffer_ptr.get(),
          [this](int socket_fd, int result, uint8_t *f_ptr, size_t size) {
            onFileHandled(socket_fd, result, f_ptr, size);
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
    KLOG->info("KServer::handleStart() - New session created for {}. Session ID: {}", client_socket_fd, uuid_str);
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
      KLOG->info("KServer::handleExecute() - Execute request received");
      auto mask = args.at(0);
      auto request_uuid = args.at(1);
      KLOG->info("KServer::handleExecute() - Mask: {}  ID: {}", mask, request_uuid);
      m_request_handler(std::stoi(mask), request_uuid, client_socket_fd);
    }
  }

  /**
   * File Upload Operation
   */
  void handleFileUploadRequest(int client_socket_fd) {
    file_pending = true;
    file_pending_fd = client_socket_fd;
    std::string file_ready_message = createMessage("File Ready", "");
    sendMessage(client_socket_fd, file_ready_message.c_str(),
                file_ready_message.size());
  }

  void handleSchedule(std::vector<std::string> task, int client_socket_fd) {
    m_request_handler("Schedule", task, client_socket_fd,
                      uuids::to_string(uuids::uuid_system_generator{}()));
    KLOG->info("KServer::handleSchedule() - Task delivered to request handler");
  }

  /**
   * Operations are the processing of requests
   */
  void handleOperation(std::string decoded_message, int client_socket_fd) {
    KOperation op = getOperation(decoded_message.c_str());
    if (isStartOperation(op.c_str())) {  // Start
      KLOG->info("Start operation");
      handleStart(decoded_message, client_socket_fd);
      return;
    } else if (isStopOperation(op.c_str())) {  // Stop
      KLOG->info("Stop operation. Shutting down client and closing connection");
      auto it_session = std::find_if(m_sessions.begin(), m_sessions.end(),
                                     [client_socket_fd](KSession session) {
                                       return session.fd == client_socket_fd;
                                     });
      if (it_session != m_sessions.end()) {
        m_sessions.erase(it_session);
      }
      if (!m_file_handlers.empty()) eraseFileHandler(client_socket_fd);
      sendEvent(client_socket_fd, "Close Session",
                {"KServer is shutting down the socket connection"});
      shutdown(client_socket_fd, SHUT_RDWR);
      close(client_socket_fd);
      return;
    } else if (isExecuteOperation(op.c_str())) {  // Process execution request
      KLOG->info("Execute operation");
      handleExecute(decoded_message, client_socket_fd);
      return;
    } else if (isFileUploadOperation(op.c_str())) {  // File upload request
      KLOG->info("File upload operation");
      handleFileUploadRequest(client_socket_fd);
      return;
    }
  }

  /**
   * Override
   */
  virtual void onMessageReceived(
      int client_socket_fd, std::weak_ptr<uint8_t[]> w_buffer_ptr) override {
    // Get ptr to data
    std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();

    if (file_pending) {  // Handle packets for incoming file
      KLOG->info("KServer::onMessageReceived() - Handling packet for file");
      handlePendingFile(s_buffer_ptr, client_socket_fd);
      return;
    }
    // For other cases, handle operations or read messages
    neither::Either<std::string, std::vector<std::string>> decoded =
        // neither::Either<std::string, int> decoded =
        getSafeDecodedMessage(s_buffer_ptr);  //
    decoded
        .leftMap([this, client_socket_fd](auto decoded_message) {
          std::string json_message = getJsonString(decoded_message);
          KLOG->info("KServer::onMessageReceived() - Decoded: {}",
                     decoded_message);
          KLOG->info("KServer::onMessageReceived() - Pretty: {}", json_message);
          // Handle operations
          if (isOperation(decoded_message.c_str())) {
            KLOG->info("KServer::onMessageReceived() - Received operation");
            handleOperation(decoded_message, client_socket_fd);
          } else if (isMessage(decoded_message.c_str())) {
            // isOperation
            if (strcmp(getMessage(decoded_message.c_str()).c_str(),
                       "scheduler") == 0) {
              KLOG->info("KServer::onMessageReceived() - Testing scheduler");
              m_request_handler(client_socket_fd, "Test",
                                Request::DevTest::Schedule);
            } else if (strcmp(getMessage(decoded_message.c_str()).c_str(),
                              "execute") == 0) {
              KLOG->info("KServer::onMessageReceived() - Testing task execution");
              m_request_handler(client_socket_fd, "Test",
                                Request::DevTest::ExecuteTask);
            }
            sendEvent(client_socket_fd, "Message Received",
                      {"Message received by KServer",
                       "The following was your message",
                       getMessage(decoded_message.c_str())});
          }
          return decoded_message;
        })
        .rightMap([this, client_socket_fd](auto task_args) {
          KLOG->info("KServer::onMessageReceived() - New message schema type received");
          if (!task_args.empty()) {
            KLOG->info("KServer::onMessageReceived() - Scheduling operation received");
            handleSchedule(task_args, client_socket_fd);
          } else {
            KLOG->info("KServer::onMessageReceived() - Empty task");
          }
          return task_args;
        });
  }

 private:
  virtual void onConnectionClose(int client_socket_fd) {}

  bool eraseFileHandler(int client_socket_fd) {
    KLOG->info("KServer::eraseFileHandler() called with {}", client_socket_fd);
    if (!m_file_handlers.empty()) {
      for (auto it = m_file_handlers.begin(); it != m_file_handlers.end();
           it++) {
        if (it->isHandlingSocket(client_socket_fd)) {
          m_file_handlers.erase(it);
          KLOG->info("KServer::eraseFileHandler() - file handler removed");
          return true;
        }
      }
    }
    return false;
  }

  Request::RequestHandler m_request_handler;
  bool file_pending;
  int file_pending_fd;
  std::vector<int> m_client_connections;
  std::vector<FileHandler> m_file_handlers;
  std::vector<KSession> m_sessions;
  std::vector<ReceivedFile> m_received_files;
};
};      // namespace KYO
#endif  // __KSERVER_HPP__
