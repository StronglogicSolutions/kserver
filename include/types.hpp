#ifndef __TYPES_H__
#define __TYPES_H__

#include <string>

#define MAX_BUFFER_SIZE (49152)
#define SMALL_BUFFER_SIZE (8192)

template <typename MessageProcessor>
void MessageHandler(MessageProcessor processor, int client_socket_fd,
                    std::string message) {
  processor(client_socket_fd, message);
}

#endif  //__TYPES_H__

