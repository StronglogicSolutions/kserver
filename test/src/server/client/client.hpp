#ifndef __CLIENT_TEST_HPP__
#define __CLIENT_TEST_HPP__
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <iostream>
#include <vector>
#include <codec/util.hpp>
#include <codec/generictask_generated.h>
#include <codec/instatask_generated.h>
#include <codec/kmessage_generated.h>

#define MAX_PACKET_SIZE 8192

// typedef std::unordered_map<int, std::string> CommandMap;
typedef std::map<int, std::vector<std::string>> CommandArgMap;

flatbuffers::FlatBufferBuilder builder(1024);

const unsigned short int CLIENT_TEST_PORT = 9876;
const constexpr char* CLIENT_TEST_IP = "127.0.0.1";

namespace TaskCode {
static constexpr uint32_t GENMSGBYTE = 0xFE;
static constexpr uint32_t PINGBYTE = 0xFD;
}  // namespace TaskCode

bool serverWaitingForFile(const char* data) {
    Document d;
    d.Parse(data);
    if (d.IsObject() && d.HasMember("message")) {
        return strcmp(d["message"].GetString(), "File Ready") == 0;
    }
    return false;
}

bool isEvent(const char* data) {
    Document d;
    d.Parse(data);
    if (d.HasMember("type")); {
      return strcmp(d["type"].GetString(), "event") == 0;
    }
    return false;
}

class MockClient {
public:

bool isPong(const char* bytes) {
  return true;
}

std::string getReceivedMessage() {
  return m_received_message;
}

void handleEvent(std::string) {

}

void handleMessages() {
  uint8_t receive_buffer[MAX_PACKET_SIZE];
  for (;;) {
    memset(receive_buffer, 0, MAX_PACKET_SIZE);
    ssize_t bytes_received = 0;
    bytes_received = recv(m_client_socket_fd, receive_buffer, MAX_PACKET_SIZE, 0);
    if (bytes_received < 1) { // Finish message loop
        break;
    }
    size_t end_idx = findNullIndex(receive_buffer);
    std::string data_string{receive_buffer, receive_buffer + end_idx};

    m_received_message = data_string;
    // if (isPong(data_string.c_str())) {
    //   std::cout << "Server returned pong" << std::endl;
    //   continue;
    // }
    std::vector<std::string> s_v{};
    if (isNewSession(data_string.c_str())) { // Session Start
      m_commands = getArgMap(data_string.c_str());
      for (const auto& [k, v] : m_commands) { // Receive available commands
        s_v.push_back(v.data());
      }
      std::cout << "set new session message" << std::endl;
    } else if (serverWaitingForFile(data_string.c_str())) { // Server expects a file
      processFileQueue();
    } else if (isEvent(data_string.c_str())) { // Receiving event
      handleEvent(data_string);
    }
  }

  memset(receive_buffer, 0, 2048);
  ::close(m_client_socket_fd);
}

void close() {
  std::string stop_operation_string = createOperation("stop", {});
  // Send operation as an encoded message
  sendEncoded(stop_operation_string);
  // Clean up socket file descriptor
  ::shutdown(m_client_socket_fd, SHUT_RDWR);
  ::close(m_client_socket_fd);
  m_client_socket_fd = -1;
}

MockClient()
  : m_client_socket_fd{-1} {
}

~MockClient() {

}

void start() {
  if (m_client_socket_fd == -1) {
      m_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_client_socket_fd != -1) {
      sockaddr_in server_socket;
      char* end;
      server_socket.sin_family = AF_INET;
      auto port_value = CLIENT_TEST_PORT;

      int socket_option = 1;
      // Free up the port to begin listening again
      setsockopt(
        m_client_socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)
      );

      server_socket.sin_port = htons(port_value);
      inet_pton(AF_INET, CLIENT_TEST_IP, &server_socket.sin_addr.s_addr);

      if (::connect(m_client_socket_fd, reinterpret_cast<sockaddr*>(&server_socket),
                    sizeof(server_socket)) != -1) {
          std::string start_operation_string = createOperation("start", {});
          // Send operation as an encoded message
          sendEncoded(start_operation_string);

          this->handleMessages();

      } else {
        std::cout << errno << std::endl;
        ::close(m_client_socket_fd);
      }
    } else {
      std::cout << "Failed to create new connection" << std::endl;
    }
  } else {
    std::cout << "Connection already in progress" << std::endl;
  }
}

void startSession() {
  sendEncoded(createOperation("start", {}));
}

void stopSession() {
  sendEncoded(createOperation("stop", {}));
}

private:

void processFileQueue() {
  return;
}

void sendEncoded(std::string message) {
  std::vector<uint8_t> fb_byte_vector{message.begin(), message.end()};
  auto byte_vector = builder.CreateVector(fb_byte_vector);
  auto k_message = CreateMessage(builder, 69, byte_vector);

  builder.Finish(k_message);

  uint8_t* encoded_message_buffer = builder.GetBufferPointer();
  uint32_t size = builder.GetSize();

  uint8_t send_buffer[MAX_PACKET_SIZE];
  memset(send_buffer, 0, MAX_PACKET_SIZE);
  send_buffer[0] = (size & 0xFF) >> 24;
  send_buffer[1] = (size & 0xFF) >> 16;
  send_buffer[2] = (size & 0xFF) >> 8;
  send_buffer[3] = (size & 0xFF);
  send_buffer[4] = (TaskCode::GENMSGBYTE & 0xFF);
  std::memcpy(send_buffer + 5, encoded_message_buffer, size);
  // std::cout << "Sending encoded message" << std::end;
  std::string message_to_send{};
  for (unsigned int i = 0; i < (size + 5); i++) {
      message_to_send += (char)*(send_buffer + i);
  }
  std::cout << "Encoded message size: " << (size + 5) << std::endl;
  // Send start operation
  ::send(m_client_socket_fd, send_buffer, size + 5, 0);
  builder.Clear();
}

int  m_client_socket_fd;
std::string m_received_message;
CommandMap                    m_commands;
CommandArgMap                 m_command_arg_map;

};



#endif // __CLIENT_TEST_HPP__