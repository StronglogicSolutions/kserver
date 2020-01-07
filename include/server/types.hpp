#ifndef __TYPES_HPP__
#define __TYPES_HPP__

static const int SYSTEM_EVENTS__FILE_UPDATE = 1;

typedef struct {
  int timestamp;
  int client_fd;
  uint8_t* f_ptr;
  size_t size;

} ReceivedFile;

static const int FILE_HANDLE__SUCCESS = 1;
static const int FILE_HANDLE__FAILURE = 2;

#endif  // __TYPES_HPP__
