#ifndef __SOCKET_LISTENER_H__
#define __SOCKET_LISTENER_H__

// Project libraries
#include "send_interface.hpp"
#include "task_queue.hpp"
#include "types.hpp"

// System libraries
#include <sys/socket.h>

// C++ Libraries
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class SocketListener : public SendInterface {
 public:
  class MessageHandler {
   public:
    MessageHandler(std::function<void()> cb) : m_cb(cb) {}

    void operator()() { m_cb(); }

   private:
    std::function<void()> m_cb;
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
                           std::weak_ptr<char[]> w_buffer_ptr) override;

  void sendMessage(int client_socket_fd, char* message, bool short_message);

  void sendMessage(int client_socket_fd, char* message, size_t size);

  MessageHandler createMessageHandler(std::function<void()> cb);
  /**
   * Perform intialization work
   */
  bool init();

  /**
   * Main message loop
   */
  void run();

  /**
   * Perform any cleanup work
   */
  void cleanup();

  // virtual void setMessageHandler(MessageHandler message_handler) override;

 private:
  // private methods
  int createSocket();

  void onMessageReceived(int client_socket_fd,
                         std::weak_ptr<char[]> w_buffer_ptr);

  int waitForConnection(int listening);

  void handleClientSocket(int client_socket_fd,
                          SocketListener::MessageHandler message_handler,
                          const std::shared_ptr<char[]>& s_buffer_ptr);

  /* private members */
  // Server arguments
  std::string m_ip_address;
  int m_port;
  std::unique_ptr<TaskQueue> u_task_queue_ptr;
};

#endif  // __SOCKET_LISTENER_H__
