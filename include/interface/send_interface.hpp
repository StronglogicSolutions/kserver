#ifndef __SEND_INTERFACE_H__
#define __SEND_INTERFACE_H__

#include <memory>
#include <string>

/**
 * SendInterface
 *
 * A public interface whose implementation sends a buffer of characters to the
 * socket connection indicated by the client_socket_fd (client socket file
 * descriptor)
 *
 * @interface
 */
class SendInterface {
 public:
  virtual void sendMessage(int client_socket_fd,
                           std::weak_ptr<uint8_t[]> w_buffer_ptr) = 0;
};

#endif  // __SEND_INTERFACE_H__
