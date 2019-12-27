#ifndef __KSERVER_HPP__
#define __KSERVER_HPP__

#include <codec/uuid.h>
#include <log/logger.h>

#include <algorithm>
#include <bitset>
#include <codec/util.hpp>
#include <cstring>
#include <interface/socket_listener.hpp>
#include <iomanip>
#include <request/request_handler.hpp>
#include <string>
#include <string_view>
#include <types/types.hpp>

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
  KServer(int argc, char** argv) : SocketListener(argc, argv) {
    KLOG->info("KServer initialized");
  }
  ~KServer() {}

  void set_handler(const Request::RequestHandler&& handler) {
    m_request_handler = handler;
  }

  virtual void onMessageReceived(
      int client_socket_fd, std::weak_ptr<uint8_t[]> w_buffer_ptr) override {
    std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
    size_t null_index = findNullIndex(s_buffer_ptr.get());

    std::string message_string =
        std::string(s_buffer_ptr.get(), s_buffer_ptr.get() + null_index);

    size_t br_pos = message_string.find("\r\n");
    if (br_pos != std::string::npos) {
      message_string.erase(message_string.find("\r\n"));
    }

    if (message_string.size() > 0 && isdigits(message_string)) {
      KLOG->info("Numerical data discovered");
      std::vector<uint32_t> bits{};
      try {
        unsigned int message_code = stoi(message_string);
        std::string binary_string = toBinaryString<int>(+message_code);
        KLOG->info("{}\n{}", binary_string, +message_code);
        for (const auto& i : range_values) {
          if (hasNthBitSet(stoi(message_string), i)) {
            bits.push_back(static_cast<uint32_t>(i));
          }
        }

        std::pair<uint8_t*, int> tuple_data = m_request_handler(bits);
        uint8_t* byte_buffer = tuple_data.first;

        auto message = GetMessage(byte_buffer);
        auto id = message->id();

        const flatbuffers::Vector<uint8_t>* data = message->data();
        std::string message_string{};

        for (auto it = data->begin(); it != data->end(); it++) {
          message_string += (char)*it;
        }
        KLOG->info("ID: {}\n Message: {}", id, message_string);

        sendMessage(client_socket_fd, const_cast<char*>(message_string.c_str()),
                    message_string.size());
      } catch (std::out_of_range& e) {
        KLOG->info("Error: {}", e.what());
        const char* return_message{"Out of range\0"};
        sendMessage(client_socket_fd, return_message, 13);
      }
    } else {
      std::string return_message = createMessage("Value was not accepted", "");
      // Obtain the raw buffer so we can read the header
      uint8_t* raw_buffer = s_buffer_ptr.get();
      auto val1 = *raw_buffer;
      auto val2 = *(raw_buffer + 1);
      auto val3 = *(raw_buffer + 2);
      auto val4 = *(raw_buffer + 3);

      uint32_t message_byte_size =
          (*raw_buffer << 24 | *(raw_buffer + 1) << 16, *(raw_buffer + 2) << 8,
           +(*(raw_buffer + 3)));
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
      std::string json_message = getJsonString(decoded_message);
      KLOG->info("Client message: {}", json_message);
      // Handle operations
      if (isOperation(decoded_message.c_str())) {
        KLOG->info("Received operation");
        if (isStartOperation(decoded_message.c_str())) {
          KLOG->info("Start operation");
          uuids::uuid const new_uuid = uuids::uuid_system_generator{}();
          KSession session{.fd = client_socket_fd, .status = 1, .id = new_uuid};
          std::map<int, std::string> server_data =
              m_request_handler(getOperation(decoded_message.c_str()));
          std::string start_message = createMessage("New Session", server_data);
          sendMessage(client_socket_fd, start_message.c_str(),
                      start_message.size());
          std::vector<std::pair<std::string, std::string>> session_info{
              {"status", std::to_string(session.status)},
              {"uuid", uuids::to_string(new_uuid)}};
          std::string session_message =
              createMessage("Session Info", session_info);
          KLOG->info("Sending message: {}", session_message);
          sendMessage(client_socket_fd, session_message.c_str(),
                      session_message.size());
          return;
        } else if (isStopOperation(decoded_message.c_str())) {
          KLOG->info(
              "Stop operation. Shutting down client and closing connection");
          shutdown(client_socket_fd, SHUT_RDWR);
          close(client_socket_fd);
          return;
        } else {
          std::string operation = getOperation(decoded_message.c_str());
          if (isExecuteOperation(operation.c_str())) {
            KLOG->info("Execute operation");
            std::vector<std::string> args = getArgs(decoded_message.c_str());
            if (!args.empty()) {
              // handler needs to act on these masks
              KLOG->info("Execute masks received");
              for (const auto& arg : args) {
                KLOG->info("Argument: {}", arg);
                std::string execute_status = m_request_handler(std::stoi(arg));
              }
            }
          }
          std::string execute_response =
              createMessage("We'll get right on that", "");
          sendMessage(client_socket_fd, execute_response.c_str(),
                      execute_response.size());
        }
      }
      sendMessage(client_socket_fd, return_message.c_str(),
                  return_message.size());
      memset(raw_buffer, 0, MAX_BUFFER_SIZE);
    }
  }

 private:
  Request::RequestHandler m_request_handler;
};
};      // namespace KYO
#endif  // __KSERVER_HPP__
