#ifndef __TYPES_HPP__
#define __TYPES_HPP__

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

typedef struct {
  int timestamp;
  int client_fd;
  uint8_t *f_ptr;
  size_t size;

} ReceivedFile;

#endif  // __TYPES_HPP__
