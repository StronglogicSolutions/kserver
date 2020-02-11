#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include <string>
#include <cstring>

#define MAX_BUFFER_SIZE (49152)
#define SMALL_BUFFER_SIZE (8192)

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
static const int SYSTEM_EVENTS__FILE_UPDATE = 1;
static const int SYSTEM_EVENTS__SCHEDULED_TASKS_READY = 2;
static const int SYSTEM_EVENTS__SCHEDULED_TASKS_NONE = 3;
static const int SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED = 4;

/**
 * FILE HANDLING STATES
 */
static const int FILE_HANDLE__SUCCESS = 1;
static const int FILE_HANDLE__FAILURE = 2;

static const char* PING = "253";
static const std::string PONG{"PONG"};

bool isPing(std::string s) {
  return s.size() == 3 && strcmp(s.c_str(), PING) == 0;
}
typedef struct {
  int timestamp;
  int client_fd;
  uint8_t *f_ptr;
  size_t size;

} ReceivedFile;

#endif  // __TYPES_HPP__
