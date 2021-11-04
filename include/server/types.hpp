#pragma once

#include <string>
#include <cstring>

static const int MAX_PACKET_SIZE = 4096;
static const int HEADER_SIZE     = 4;

template <typename MessageProcessor>
void MessageHandler(MessageProcessor processor, int client_socket_fd,
                    std::string message) {
  processor(client_socket_fd, message);
}

/**
 * SYSTEM EVENTS
 */
static const int32_t SYSTEM_EVENTS__FILE_UPDATE                 = 1;
static const int32_t SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED = 2;
static const int32_t SYSTEM_EVENTS__SCHEDULED_TASKS_READY       = 3;
static const int32_t SYSTEM_EVENTS__SCHEDULED_TASKS_NONE        = 4;
static const int32_t SYSTEM_EVENTS__SCHEDULER_FETCH             = 5;
static const int32_t SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS      = 6;
static const int32_t SYSTEM_EVENTS__SCHEDULER_UPDATE            = 7;
static const int32_t SYSTEM_EVENTS__SCHEDULER_SUCCESS           = 8;
static const int32_t SYSTEM_EVENTS__SCHEDULER_FAIL              = 9;
static const int32_t SYSTEM_EVENTS__REGISTRAR_SUCCESS           = 10;
static const int32_t SYSTEM_EVENTS__REGISTRAR_FAIL              = 11;
static const int32_t SYSTEM_EVENTS__TASK_FETCH_FLAGS            = 12;
static const int32_t SYSTEM_EVENTS__APPLICATION_FETCH_SUCCESS   = 13;
static const int32_t SYSTEM_EVENTS__APPLICATION_FETCH_FAIL      = 14;
static const int32_t SYSTEM_EVENTS__PLATFORM_NEW_POST           = 15;
static const int32_t SYSTEM_EVENTS__PLATFORM_POST_REQUESTED     = 16;
static const int32_t SYSTEM_EVENTS__PLATFORM_ERROR              = 17;
static const int32_t SYSTEM_EVENTS__PROCESS_COMPLETE            = 18;
static const int32_t SYSTEM_EVENTS__SCHEDULER_REQUEST           = 19;
static const int32_t SYSTEM_EVENTS__TRIGGER_ADD_SUCCESS         = 20;
static const int32_t SYSTEM_EVENTS__TRIGGER_ADD_FAIL            = 21;
static const int32_t SYSTEM_EVENTS__FILES_SEND                  = 22;
static const int32_t SYSTEM_EVENTS__FILES_SEND_ACK              = 23;
static const int32_t SYSTEM_EVENTS__FILES_SEND_READY            = 24;
static const int32_t SYSTEM_EVENTS__TASK_DATA                   = 25;
static const int32_t SYSTEM_EVENTS__TASK_DATA_FINAL             = 26;
static const int32_t SYSTEM_EVENTS__PROCESS_RESEARCH            = 27;
static const int32_t SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT     = 28;
static const int32_t SYSTEM_EVENTS__KIQ_IPC_MESSAGE             = 29;


static const uint8_t EVENT_PROCESS_OUTPUT_INDEX{0x00};
static const uint8_t EVENT_PROCESS_MASK_INDEX  {0x01};



/**
 * FILE HANDLING STATES
 */
static constexpr int FILE_HANDLE__SUCCESS = 1;
static constexpr int FILE_HANDLE__FAILURE = 2;

static constexpr const char* const PING = "253";
static constexpr const char* const PONG = "PONG";
static constexpr size_t PONG_SIZE = 4;

// // TODO: Create an implementation file, or move this to utilities
// inline bool IsPing(std::string s) {
//   return s.size() == 3 && strcmp(s.c_str(), PING) == 0;
// }
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
