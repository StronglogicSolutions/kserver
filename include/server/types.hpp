#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include <string>
#include <cstring>

static const int MAX_PACKET_SIZE = 4096;
static const int HEADER_SIZE = 4;

template <typename MessageProcessor>
void MessageHandler(MessageProcessor processor, int client_socket_fd,
                    std::string message) {
  processor(client_socket_fd, message);
}

/**
 * SYSTEM EVENTS
 */
static const int SYSTEM_EVENTS__FILE_UPDATE                 = 1;
static const int SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED = 2;
static const int SYSTEM_EVENTS__SCHEDULED_TASKS_READY       = 3;
static const int SYSTEM_EVENTS__SCHEDULED_TASKS_NONE        = 4;
static const int SYSTEM_EVENTS__SCHEDULER_SUCCESS           = 5;
static const int SYSTEM_EVENTS__SCHEDULER_FAIL              = 6;
static const int SYSTEM_EVENTS__REGISTRAR_SUCCESS           = 7;
static const int SYSTEM_EVENTS__REGISTRAR_FAIL              = 8;

/**
 * FILE HANDLING STATES
 */
static constexpr int FILE_HANDLE__SUCCESS = 1;
static constexpr int FILE_HANDLE__FAILURE = 2;

static constexpr const char* const PING = "253";
static constexpr const char* const PONG = "PONG";
static constexpr size_t PONG_SIZE = 4;

bool isPing(std::string s) {
  return s.size() == 3 && strcmp(s.c_str(), PING) == 0;
}
struct ReceivedFile {
  int timestamp;
  int client_fd;
  uint8_t *f_ptr;
  size_t size;

  friend std::ostream &operator<<(std::ostream &out, const ReceivedFile& file) {
      out << "Timestamp: " << file.timestamp
          << "\nClient: " << file.client_fd
          << "\nSize: " << file.size << std::endl;
      return out;
    }
};

#endif  // __TYPES_HPP__
