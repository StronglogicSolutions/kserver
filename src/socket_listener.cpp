// Project headers
#include <interface/socket_listener.hpp>
#include <types/constants.hpp>
// System libraries
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
// C++ Libraries
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#define MAX_BUFFER_SIZE (4096)
#define SMALL_BUFFER_SIZE (8192)

/**
 * Constructor
 * Initialize with ip_address, port and message_handler
 */
SocketListener::SocketListener(int arg_num, char** args) : m_port(-1) {
  for (int i = 0; i < arg_num; i++) {
    std::string argument = std::string(args[i]);
    std::cout << args[i] << std::endl;
    if (argument.find("--ip") != -1) {
      m_ip_address = argument.substr(5);
      continue;
    }
    if (argument.find("--port") != -1) {
      m_port = std::stoi(argument.substr(7));
      continue;
    }
    if (m_ip_address.empty()) {
      m_ip_address = "0.0.0.0";
    }
    if (m_port == -1) {
      m_port = 9009;
    }
  }
}

/**
 * Destructor
 * TODO: Determine if we should make buffer a class member
 */
SocketListener::~SocketListener() { cleanup(); }

SocketListener::MessageHandler SocketListener::createMessageHandler(
    std::function<void(ssize_t)> cb) {
  return MessageHandler(cb);
}

void SocketListener::onMessageReceived(int client_socket_fd,
                                       std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                       ssize_t& size) {
  std::cout << "This should be overridden" << std::endl;
  sendMessage(client_socket_fd, w_buffer_ptr);
}

void SocketListener::onConnectionClose(int client_socket_fd) {
  std::cout << "This should be overridden" << std::endl;
}

/**
 * sendMessage
 * @method
 * Send a null-terminated array of characters, supplied as a const uint8_t
 * pointer, to a client socket described by its file descriptor
 */
void SocketListener::sendMessage(int client_socket_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr) {
  std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
  if (s_buffer_ptr) {
    send(client_socket_fd, s_buffer_ptr.get(),
         static_cast<size_t>(MAX_BUFFER_SIZE) + 1, 0);
  } else {
    std::cout << "Could not send message to client " << client_socket_fd
              << ". Buffer does not exist." << std::endl;
  }
}

void SocketListener::sendMessage(int client_socket_fd, char* message,
                                 bool short_message) {
  if (short_message) {
    send(client_socket_fd, message, static_cast<size_t>(SMALL_BUFFER_SIZE) + 1,
         0);
  } else {
    send(client_socket_fd, message, static_cast<size_t>(MAX_BUFFER_SIZE) + 1,
         0);
  }
}

void SocketListener::sendMessage(int client_socket_fd, char buffer[],
                                 size_t size) {
  send(client_socket_fd, buffer, size + 1, 0);
}

void SocketListener::sendMessage(int client_socket_fd, const char* buffer,
                                 size_t size) {
  send(client_socket_fd, buffer, size + 1, 0);
}

/**
 * init
 * TODO: Initialize buffer memory, if buffer is to be a class member
 */
bool SocketListener::init() {
  std::cout << "Initializing socket listener" << std::endl;
  u_task_queue_ptr = std::make_unique<TaskQueue>();
  u_task_queue_ptr->initialize();
  return true;
}

void SocketListener::handleClientSocket(
    int client_socket_fd, SocketListener::MessageHandler message_handler,
    const std::shared_ptr<uint8_t[]>& s_buffer_ptr) {
  for (;;) {
    memset(s_buffer_ptr.get(), 0,
           MAX_BUFFER_SIZE);  // Zero the character buffer
    // Receive and write incoming data to buffer and return the number of
    // bytes received
    ssize_t size = recv(client_socket_fd, s_buffer_ptr.get(),
                        MAX_BUFFER_SIZE,  // Leave room for null-termination ?
                        0);
    //    s_buffer_ptr.get()[MAX_BUFFER_SIZE - 1] =
    //        0;  // Null-terminate the character buffer
    if (size > 0) {
      std::cout << "Client " << client_socket_fd << "\nBytes received: " << size
                << "\nData: " << std::hex << s_buffer_ptr.get() << std::endl;
      // Handle incoming message
      message_handler(size);
    } else {
      std::cout << "Client " << client_socket_fd << " disconnected"
                << std::endl;
      onConnectionClose(client_socket_fd);
      // Zero the buffer again before closing
      memset(s_buffer_ptr.get(), 0, MAX_BUFFER_SIZE);
      break;
    }
  }
  // TODO: Determine if we should free memory, or handle as class member
  close(client_socket_fd);  // Destroy client socket and deallocate its fd
}

/**
 * run
 * @method
 * Main message loop
 * TODO: Implement multithreading
 */
void SocketListener::run() {
  // Begin listening loop
  while (true) {
    std::cout << "Begin" << std::endl;
    // Call system to open a listening socket, and return its file descriptor
    int listening_socket_fd = createSocket();

    if (listening_socket_fd == SOCKET_ERROR) {
      std::cout << "Socket error: shutting down server" << std::endl;
      break;
    }
    std::cout << "Attempting to wait for connection" << std::endl;
    // wait for a client connection and get its socket file descriptor
    int client_socket_fd = waitForConnection(listening_socket_fd);

    if (client_socket_fd != SOCKET_ERROR) {
      // Destroy listening socket and deallocate its file descriptor. Only use
      // the client socket now.
      close(listening_socket_fd);
      {
        std::shared_ptr<uint8_t[]> s_buffer_ptr(new uint8_t[MAX_BUFFER_SIZE]);
        std::weak_ptr<uint8_t[]> w_buffer_ptr(s_buffer_ptr);
        // TODO: Stop the use of this size variable, by changing the
        // specification of handleClientSocket
        std::function<void(ssize_t)> message_send_fn =
            [this, client_socket_fd, w_buffer_ptr](ssize_t size) {
              this->onMessageReceived(client_socket_fd, w_buffer_ptr, size);
            };
        MessageHandler message_handler = createMessageHandler(message_send_fn);
        std::cout << "Pushing client to queue" << std::endl;
        u_task_queue_ptr->pushToQueue(
            std::bind(&SocketListener::handleClientSocket, this,
                      client_socket_fd, message_handler,
                      std::forward<std::shared_ptr<uint8_t[]>>(s_buffer_ptr)));
      }
    }
  }
}

/**
 * cleanUp
 * @method
 * TODO: Determine if we should be cleaning up buffer memory
 */
void SocketListener::cleanup() { std::cout << "Cleaning up" << std::endl; }
/**
 * createSocket
 * Open a listening socket and return its file descriptor
 */
int SocketListener::createSocket() {
  /* Call the system to open a socket passing arguments for
   ipv4 family, tcp type and no additional protocol info */
  int listening_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (listening_socket_fd != SOCKET_ERROR) {
    std::cout << "Created listening socket" << std::endl;
    // Create socket structure to hold address and type
    sockaddr_in socket_struct;
    socket_struct.sin_family = AF_INET;  // ipv4
    socket_struct.sin_port =
        htons(m_port);  // convert byte order of port value from host to network
    inet_pton(AF_INET, m_ip_address.c_str(),  // convert address to binary
              &socket_struct.sin_addr);

    int socket_option = 1;
    // Free up the port to begin listening again
    setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
               sizeof(socket_option));

    // Bind local socket address to socket file descriptor
    int bind_result = bind(
        listening_socket_fd,        // TODO: Use C++ cast on next line?
        (sockaddr*)&socket_struct,  // cast socket_struct to more generic type
        sizeof(socket_struct));
    if (bind_result != SOCKET_ERROR) {
      // Listen for connections to socket and allow up to max number of
      // connections for queue
      int listen_result = listen(listening_socket_fd, SOMAXCONN);
      if (listen_result == SOCKET_ERROR) {
        return WAIT_SOCKET_FAILURE;
      }
    } else {
      return WAIT_SOCKET_FAILURE;
    }
  }
  return listening_socket_fd;  // Return socket file descriptor
}
/**
 * waitForConnection
 * @method
 * Takes first connection on queue of pending connections, creates a new
 * socket and returns its file descriptor
 */
int SocketListener::waitForConnection(int listening_socket) {
  int client_socket_fd = accept(listening_socket, NULL, NULL);
  return client_socket_fd;
}
