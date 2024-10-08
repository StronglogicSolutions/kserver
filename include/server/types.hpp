#pragma once

#include <string>
#include <cstring>
#include <cstdint>
#include <ostream>

namespace kiq {
static const int MAX_PACKET_SIZE = 4096;
static const int HEADER_SIZE     = 4;

template <typename MessageProcessor>
void MessageHandler(MessageProcessor processor,
                    int32_t          client_socket_fd,
                    std::string      message)
{
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
static const int32_t SYSTEM_EVENTS__PLATFORM_CREATED            = 38;
static const int32_t SYSTEM_EVENTS__PLATFORM_NEW_POST           = 15;
static const int32_t SYSTEM_EVENTS__PLATFORM_POST_REQUESTED     = 16;
static const int32_t SYSTEM_EVENTS__PLATFORM_ERROR              = 17;
static const int32_t SYSTEM_EVENTS__PLATFORM_REQUEST            = 18;
static const int32_t SYSTEM_EVENTS__PLATFORM_EVENT              = 19;
static const int32_t SYSTEM_EVENTS__PLATFORM_INFO               = 20;
static const int32_t SYSTEM_EVENTS__PLATFORM_INFO_REQUEST       = 21;
static const int32_t SYSTEM_EVENTS__PLATFORM_FETCH_POSTS        = 22;
static const int32_t SYSTEM_EVENTS__PLATFORM_UPDATE             = 23;
static const int32_t SYSTEM_EVENTS__PROCESS_COMPLETE            = 24;
static const int32_t SYSTEM_EVENTS__SCHEDULER_REQUEST           = 25;
static const int32_t SYSTEM_EVENTS__TRIGGER_ADD_SUCCESS         = 26;
static const int32_t SYSTEM_EVENTS__TRIGGER_ADD_FAIL            = 27;
static const int32_t SYSTEM_EVENTS__FILES_SEND                  = 28;
static const int32_t SYSTEM_EVENTS__FILES_SEND_ACK              = 29;
static const int32_t SYSTEM_EVENTS__FILES_SEND_READY            = 30;
static const int32_t SYSTEM_EVENTS__TASK_DATA                   = 31;
static const int32_t SYSTEM_EVENTS__TASK_DATA_FINAL             = 32;
static const int32_t SYSTEM_EVENTS__PROCESS_RESEARCH            = 33;
static const int32_t SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT     = 34;
static const int32_t SYSTEM_EVENTS__KIQ_IPC_MESSAGE             = 35;
static const int32_t SYSTEM_EVENTS__TERM_HITS                   = 36;
static const int32_t SYSTEM_EVENTS__IPC_REQUEST                 = 37;
static const int32_t SYSTEM_EVENTS__STATUS_REPORT               = 38;
static const int32_t SYSTEM_EVENTS__IPC_RECONNECT_REQUEST       = 39;

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

struct ReceivedFile {
int timestamp;
int client_fd;
uint8_t *f_ptr;
size_t size;

friend std::ostream &operator<<(std::ostream &out, const ReceivedFile& file)
{
  out << "Timestamp: " << file.timestamp
      << "\nClient: " << file.client_fd
      << "\nSize: " << file.size << std::endl;
  return out;
}
};
} // ns kiq
