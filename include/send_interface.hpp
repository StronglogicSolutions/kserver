#ifndef __SEND_INTERFACE_H__
#define __SEND_INTERFACE_H__

#include <memory>
#include <string>

class SendInterface {
 public:
  virtual void sendMessage(int client_socket_fd,
                           std::weak_ptr<char[]> w_buffer_ptr) = 0;
};

#endif  // __SEND_INTERFACE_H__
