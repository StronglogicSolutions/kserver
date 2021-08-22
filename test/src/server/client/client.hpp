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
#include <common/util.hpp>
#include <codec/generictask_generated.h>
#include <codec/instatask_generated.h>
#include <codec/kmessage_generated.h>
#include "mock_data.hpp"

#define MAX_PACKET_SIZE 8192

inline std::vector<size_t> findNullIndexes(uint8_t* data) {
  size_t index = 0;
  std::vector<size_t> indexes{};
  if (data != nullptr) {
    while (data) {
      try {
        if (strcmp(reinterpret_cast<char*>(data), "\0") == 0) {
          indexes.push_back(index);
        }
        index++;
        data++;
      } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
      }
    }
  }
  return indexes;
}

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
    if (*data != '\0') {
      Document d;
      d.Parse(data);
      if (d.HasMember("type")) {
        return strcmp(d["type"].GetString(), "event") == 0;
      }
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
  // uint8_t receive_buffer[MAX_PACKET_SIZE];
  // for (;;) {
  //   memset(receive_buffer, 0, MAX_PACKET_SIZE);
  //   ssize_t bytes_received = 0;
  //   bytes_received = recv(m_client_socket_fd, receive_buffer, MAX_PACKET_SIZE, 0);
  //   if (bytes_received < 1) { // Finish message loop
  //       break;
  //   }
  //   size_t null_index{};

  //   if (m_raw_mode) {
  //     std::vector<size_t> null_indexes = findNullIndexes(receive_buffer);
  //     std::cout << "Buffer had " << null_indexes.size() << " indexes" << std::endl;

  //     for (const auto& i : null_indexes) {
  //       std::cout << i;
  //     }

  //     null_index = null_indexes[0];

  //   } else {
  //     null_index = FindNullIndex(receive_buffer);
  //   }

  //   if (null_index > 1) {
  //     std::string data_string{receive_buffer, receive_buffer + null_index};

  //     m_received_message = data_string;

  //     if (!m_raw_mode) {
  //       std::vector<std::string> s_v{};
  //       if (IsNewSession(data_string.c_str())) { // Session Start
  //         m_commands = GetArgMap(data_string.c_str());
  //         for (const auto& [k, v] : m_commands) { // Receive available commands
  //           s_v.push_back(v.data());
  //         }
  //         std::cout << "set new session message" << std::endl;
  //       } else if (serverWaitingForFile(data_string.c_str())) { // Server expects a file
  //         processFileQueue();
  //       } else if (isEvent(data_string.c_str())) { // Receiving event
  //         handleEvent(data_string);
  //       }
  //     }
  //   }
  // }

  // memset(receive_buffer, 0, 2048);
  // ::close(m_client_socket_fd);
}

void close() {
  std::string stop_operation_string = CreateOperation("stop", {});
  // Send operation as an encoded message
  sendEncoded(stop_operation_string);
  // Clean up socket file descriptor
  ::shutdown(m_client_socket_fd, SHUT_RDWR);
  ::close(m_client_socket_fd);
  m_client_socket_fd = -1;
}

MockClient(bool raw_mode = false)
  : m_client_socket_fd{-1}, m_raw_mode(raw_mode) {
}

~MockClient() {

}

void start() {
  if (m_client_socket_fd == -1) {
      m_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_client_socket_fd != -1) {
      sockaddr_in server_socket;
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
          std::string start_operation_string = CreateOperation("start", {});
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
  sendEncoded(CreateOperation("start", {}));
}

void stopSession() {
  sendEncoded(CreateOperation("stop", {}));
}

uint8_t* createTestBuffer() {
  uint8_t* buffer = new uint8_t[1024]{};
  const char* lorem_c_string = loremIpsum.c_str();

  for (uint i = 0; i < 1023; i++) {
    buffer[i] = lorem_c_string[i];
  }

  buffer[1024] = '\0';

  return buffer;
}

void sendMessage() {
  uint8_t* send_buffer = createTestBuffer();

  ::send(m_client_socket_fd, send_buffer, 1024, 0);
}

void sendCustomMessage(std::string message) {
  sendEncoded(CreateMessage(message.c_str(), ""));
}

private:

void processFileQueue() {
  return;
}

void sendEncoded(std::string message) {
  auto builder = flatbuffers::FlatBufferBuilder{};
  std::vector<uint8_t>                              fb_byte_vector{message.begin(), message.end()};
  flatbuffers::Offset<flatbuffers::Vector<uint8_t>> byte_vector = builder.CreateVector(fb_byte_vector);
  auto k_message = CreateMessage(builder, 69, byte_vector);
  builder.Finish(k_message);

  uint32_t size = builder.GetSize();
  // uint32_t size = message.size();

  uint8_t send_buffer[MAX_PACKET_SIZE];
  memset(send_buffer, 0, MAX_PACKET_SIZE);
  send_buffer[0] = (size & 0xFF) >> 24;
  send_buffer[1] = (size & 0xFF) >> 16;
  send_buffer[2] = (size & 0xFF) >> 8;
  send_buffer[3] = (size & 0xFF);
  send_buffer[4] = (TaskCode::GENMSGBYTE & 0xFF);

  uint8_t* encoded_message_buffer = builder.GetBufferPointer();

  std::memcpy(send_buffer + 5, encoded_message_buffer, size);


  // Send start operation
  ::send(m_client_socket_fd, send_buffer, size + 5, 0);
  builder.Clear();
}

int  m_client_socket_fd;
bool m_raw_mode;
std::string m_received_message;
CommandMap                    m_commands;
CommandArgMap                 m_command_arg_map;

};



#endif // __CLIENT_TEST_HPP__
