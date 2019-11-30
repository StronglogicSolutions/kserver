#include <iostream>

#include "socket_listener.hpp"

class KServer : public SocketListener {
 public:
  KServer(int argc, char** argv) : SocketListener(argc, argv) {}
  ~KServer() {}

  void onMessageReceived(int client_socket_fd,
                         std::weak_ptr<char[]> w_buffer_ptr) {
    std::cout << "We received a message" << std::endl;
  }
};
