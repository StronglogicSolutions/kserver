#ifndef __SOCKET_LISTENER_HPP__
#define __SOCKET_LISTENER_HPP__

// Project libraries
#include <interface/listen_interface.hpp>
#include <interface/send_interface.hpp>
#include <task/task_queue.hpp>
// System libraries
#include <sys/socket.h>

// C++ Libraries
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

/**
 * SocketListener
 *
 * SocketListener is extensible to aid in architecting a socket server
 */
class SocketListener : public SendInterface, public ListenInterface {
 public:
  /* public classes whose instances are used by SocketListener */

  /**
   * MessageHandler
   *
   * Instances of this object type wrap a generic, self-contained function and
   * behave as callable functions (functors)
   * @class
   */
  class MessageHandler {
   public:
    MessageHandler(std::function<void(ssize_t)> cb) : m_cb(cb) {}

    void operator()(ssize_t size) { m_cb(size); }

   private:
    std::function<void(ssize_t)> m_cb;
  };
  // constructor
  SocketListener(int arg_num, char** args);

  // destructor
  ~SocketListener();

  /**
   * Send a message to a client socket described by its file descriptor
   * @param[in] {int} client_socket_fd The client socket file descriptor
   * @param[in] {std::string} The message to be sent
   */
  virtual void sendMessage(int client_socket_fd,
                           std::weak_ptr<uint8_t[]> w_buffer_ptr) override;
  /** overload variants */
  void sendMessage(int client_socket_fd, char* message, bool short_message);

  void sendMessage(int client_socket_fd, char* message, size_t size);

  void sendMessage(int client_socket_fd, const char* message, size_t size);

  MessageHandler createMessageHandler(std::function<void(ssize_t)> cb);

  /**
   * Perform intialization work
   */
  void init(bool test_mode = false);

  /**
   * Main message loop
   */
  void run();

  /**
   * Perform any cleanup work
   */
  void cleanup();

 private:
  // private methods
  int createSocket();

  virtual void onMessageReceived(int client_socket_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                 ssize_t& size) override;

  virtual void onConnectionClose(int client_socket_fd) override;

  int waitForConnection(int listening);

  void handleClientSocket(int client_socket_fd,
                          SocketListener::MessageHandler message_handler,
                          const std::shared_ptr<uint8_t[]>& s_buffer_ptr);

  /* private members */
  // Server arguments
  std::string                 m_ip_address;
  int                         m_port;
  std::unique_ptr<TaskQueue>  u_task_queue_ptr;
  bool                        m_service_enabled;
  bool                        m_test_mode;
};

#endif  // __SOCKET_LISTENER_HPP__
