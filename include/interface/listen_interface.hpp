#ifndef __LISTEN_INTERFACE_H__
#define __LISTEN_INTERFACE_H__

#include <memory>
#include <string>

/**
 * ListenInterface
 *
 * A public interface whose implementation handles the receival of a character
 * buffer assumed have been sent from a client socket connection, indicated by a
 * file descriptor, communicating with the implementor.
 *
 * @interface
 */
class ListenInterface {
 public:
  virtual void onMessageReceived(int client_socket_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr) = 0;
};

#endif  // __LISTEN_INTERFACE_H__
