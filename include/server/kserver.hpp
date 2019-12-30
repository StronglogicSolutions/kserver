#ifndef __KSERVER_HPP__
#define __KSERVER_HPP__

#include <codec/uuid.h>
#include <log/logger.h>
#include <math.h>

#include <algorithm>
#include <bitset>
#include <codec/util.hpp>
#include <cstring>
#include <functional>
#include <interface/socket_listener.hpp>
#include <iomanip>
#include <request/request_handler.hpp>
#include <string>
#include <string_view>
#include <types/types.hpp>

class Decoder {
 public:
  Decoder(std::string_view name, int fd, std::function<void(int)> callback)
      : index(0),
        total_packets(0),
        packet_offset(0),
        buffer_offset(0),
        file_size(0),
        file_name(name),
        m_fd(fd),
        m_cb(callback) {}

  void processPacket(uint8_t* data) {
    bool is_first_packet = (index == 0);

    if (is_first_packet) {
      file_size = int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]);
      total_packets = static_cast<uint32_t>(
          ceil(static_cast<double>(file_size + HEADER_SIZE) / MAX_PACKET_SIZE));
      file_buffer = new uint8_t[total_packets * MAX_PACKET_SIZE];
      packet_offset = HEADER_SIZE;
      buffer_offset = 0;
      uint32_t first_packet_size =
          total_packets == 1 ? (file_size - HEADER_SIZE) : MAX_PACKET_SIZE;
      std::memcpy(file_buffer + buffer_offset, data + packet_offset,
                  first_packet_size);
      if (index == (total_packets - 1)) {
        // handle file, cleanup, return
        return;
      }
      index++;
      return;
    }
    bool is_last_packet = (index == (total_packets - 1));
    buffer_offset = (index * MAX_PACKET_SIZE) - HEADER_SIZE;
    if (!is_last_packet) {
      std::memcpy(file_buffer + buffer_offset, data, MAX_PACKET_SIZE);
    } else {
      uint32_t last_packet_size = file_size - buffer_offset;
      // handle file cleanup
      std::memcpy(file_buffer + buffer_offset, data, last_packet_size);
      m_cb(m_fd);
    }
  }

 private:
  uint8_t* file_buffer;
  uint32_t index;
  uint32_t total_packets;
  uint32_t packet_offset;
  uint32_t buffer_offset;
  uint32_t file_size;
  int m_fd;
  std::string_view file_name;
  std::function<void(int)> m_cb;
};

class FileHandler {
 public:
  FileHandler(int client_fd, std::string_view name, uint8_t* first_packet,
              std::function<void(int)> callback)
      : socket_fd(client_fd) {
    m_decoder = new Decoder(name, client_fd, callback);
    m_decoder->processPacket(first_packet);
  }
  void processPacket(uint8_t* data) { m_decoder->processPacket(data); }
  bool isHandlingSocket(int fd) { return fd == socket_fd; }

 private:
  Decoder* m_decoder;
  int socket_fd;
};

// using namespace MinLog;
using namespace KData;

namespace KYO {

KLogger* k_logger_ptr = KLogger::GetInstance();

auto KLOG = k_logger_ptr -> get_logger();

template <typename T>
static std::string toBinaryString(const T& x) {
  std::stringstream ss;
  ss << std::bitset<sizeof(T) * 8>(x);
  return ss.str();
}

const std::vector<int> range_values{1, 2, 3, 4, 5, 6};

bool hasNthBitSet(int value, int n) {
  KLOG->info("Checking for {} bit", n);
  auto result = value & (1 << (n - 1));
  if (result) {
    KLOG->info("{} has bit {} set", value, n);
    return true;
  }
  return false;
}

bool isdigits(const std::string& s) {
  for (char c : s)
    if (!isdigit(c)) {
      return false;
    }
  return true;
}

class KServer : public SocketListener {
 public:
  KServer(int argc, char** argv)
      : SocketListener(argc, argv), file_pending(false), file_pending_fd(-1) {
    KLOG->info("KServer initialized");
  }
  ~KServer() {}

  void set_handler(const Request::RequestHandler&& handler) {
    m_request_handler = handler;
  }

  /* void processFileData(uint8_t* data) { */
  /*   bool is_first_packet = (packet_index == 0); */
  /*   bool is_last_packet = (packet_index == total_packets - 1); */
  /*   int bytes_written = 0; */
  /*   if (!is_last_packet) { */
  /*     if (((packet_index + 1) * MAX_PACKET_SIZE) > incoming_file_size) { */
  /*       // This case might overtake the data size needed */
  /*     } */
  /*   } */
  /*   int bytes_written = is_first_packet ? 0 : (packet_index *
   * MAX_PACKET_SIZE); */
  /*   if (is_first_packet) { */
  /*     KLOG->info("Processing first packet"); */
  /*     int incoming_file_size = */
  /*         int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]); */
  /*     total_packets = (incoming_file_size / MAX_PACKET_SIZE); */
  /*     file_buffer = new uint8_t[total_packets * MAX_PACKET_SIZE]; */
  /*     packet_offset = HEADER_SIZE; */
  /*     buffer_offset = 0; */
  /*     // size -= HEADER_SIZE; */

  /*     // Need to know current packet size which */
  /*     // If not last packet: */
  /*     //    Track total size - ((index + 1) * MAX_PACKET_SIZE) */
  /*     // */

  /*     std::memcpy(file_buffer + buffer_offset, data + packet_offset, size);
   */
  /*     if (is_last_packet) { */
  /*       // cleanup values back to zero */
  /*     } */
  /*   } else { */
  /*     KLOG->info("Processing other packet"); */
  /*     buffer_offset = (packet_index * total_packets) - HEADER_SIZE; */

  /*     std::memcpy(file_buffer + buffer_offset, ) */
  /*   } */
  /* } */

  void onFileHandled(int socket_fd) {
    if (file_pending_fd == socket_fd) {
      KLOG->info("Finished handling file for client {}", socket_fd);
      file_pending_fd = -1;
      file_pending = false;
    }
  }

  void handlePendingFile(std::shared_ptr<uint8_t[]> s_buffer_ptr, int client_socket_fd) {
    auto handler =
          std::find_if(m_file_handlers.begin(), m_file_handlers.end(),
                       [client_socket_fd](FileHandler& handler) {
                         return handler.isHandlingSocket(client_socket_fd);
                       });
    if (handler != m_file_handlers.end()) {
      handler->processPacket(s_buffer_ptr.get());
    } else {
      FileHandler new_handler{client_socket_fd, "Placeholder filename",
                              s_buffer_ptr.get(),
                              std::bind(&KYO::KServer::onFileHandled, this, client_socket_fd)};
      m_file_handlers.push_back(new_handler);
    }
    return;
  }

  std::string getDecodedMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr) {
    // Make sure not an empty buffer
    // Obtain the raw buffer so we can read the header
    uint8_t* raw_buffer = s_buffer_ptr.get();
    auto val1 = *raw_buffer;
    auto val2 = *(raw_buffer + 1);
    auto val3 = *(raw_buffer + 2);
    auto val4 = *(raw_buffer + 3);

    uint32_t message_byte_size = (*raw_buffer << 24 | *(raw_buffer + 1) << 16,
                                  *(raw_buffer + 2) << 8, +(*(raw_buffer + 3)));
    // TODO: Copying into a new buffer for readability - switch to using the
    // original buffer
    uint8_t decode_buffer[message_byte_size];
    std::memcpy(decode_buffer, raw_buffer + 4, message_byte_size);
    // Parse the bytes into an encoded message structure
    auto k_message = GetMessage(&decode_buffer);
    auto id = k_message->id();  // message ID
    KLOG->info("Message ID: {}", id);
    // Get the message bytes and create a string
    const flatbuffers::Vector<uint8_t>* message_bytes = k_message->data();
    std::string decoded_message{message_bytes->begin(), message_bytes->end()};
    return decoded_message;
  }

  void handleStart(std::string decoded_message, int client_socket_fd) {
    uuids::uuid const new_uuid = uuids::uuid_system_generator{}();
    KSession session{
      .fd = client_socket_fd,
      .status = 1,
      .id = new_uuid
    };
    // Fetch available programs from database
    std::map<int, std::string> server_data = m_request_handler("Start");
    // Create welcome message with data
    std::string start_message = createMessage("New Session", server_data);
    sendMessage(client_socket_fd, start_message.c_str(),
                start_message.size());
    // Send session info separately
    std::vector<std::pair<std::string, std::string>> session_info{
        {"status", std::to_string(session.status)},
        {"uuid", uuids::to_string(new_uuid)}};
    std::string session_message =
        createMessage("Session Info", session_info);
    KLOG->info("Sending message: {}", session_message);
    sendMessage(client_socket_fd, session_message.c_str(),
                session_message.size());
  }

  virtual void onMessageReceived(
      int client_socket_fd, std::weak_ptr<uint8_t[]> w_buffer_ptr) override {
    std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();

    if (file_pending) { // Handle packets for incoming file
      KLOG->info("File to be processed");
      handlePendingFile(s_buffer_ptr, client_socket_fd);
      return;
    }
    // For other cases, operations and messages
    std::string decoded_message = getDecodedMessage(s_buffer_ptr);
    std::string json_message = getJsonString(decoded_message);
    KLOG->info("Client message: {}", json_message);
    // Handle operations
    if (isOperation(decoded_message.c_str())) {
      KOperation op = getOperation(decoded_message.c_str());
      KLOG->info("Received operation");
      if (isStartOperation(op.c_str())) { // Start
        KLOG->info("Start operation");
        handleStart(decoded_message, client_socket_fd);
        return;
      } else if (isStopOperation(op.c_str())) { // Stop
        KLOG->info(
            "Stop operation. Shutting down client and closing connection");
        shutdown(client_socket_fd, SHUT_RDWR);
        close(client_socket_fd);
        return;
      } else if (isExecuteOperation(op.c_str())) { // Process execution request
        KLOG->info("Execute operation");
        std::vector<std::string> args = getArgs(decoded_message.c_str());
        if (!args.empty()) {
          KLOG->info("Execute masks received");
          for (const auto& arg : args) {
            KLOG->info("Argument: {}", arg);
            std::string execute_status = m_request_handler(std::stoi(arg));
          }
        }
        std::string execute_response = createMessage("We'll get right on that", "");
        sendMessage(client_socket_fd, execute_response.c_str(),
                    execute_response.size());
      } else if (isFileUploadOperation(op.c_str())) { // File upload request
        KLOG->info("File upload operation");
        file_pending = true;
        file_pending_fd = client_socket_fd;
        std::string file_ready_message = createMessage("File Ready", "");
        sendMessage(client_socket_fd, file_ready_message.c_str(),
                    file_ready_message.size());
      }
    } // isOperation
    // TODO: handle regular messages
  }

 private:
  Request::RequestHandler m_request_handler;
  bool file_pending;
  int file_pending_fd;
  std::vector<FileHandler> m_file_handlers;
};
};      // namespace KYO
#endif  // __KSERVER_HPP__
