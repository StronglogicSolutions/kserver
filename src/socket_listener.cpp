#include "interface/socket_listener.hpp"

static const int32_t DEFAULT_SOCKET_LISTENER_PORT{9009};
static const auto    IS_ARGUMENT = [](const std::string& s, const std::string& arg) -> bool
{
  static const size_t  NOT_FOUND{std::string::npos};
  return (s.find(arg) != NOT_FOUND);
};

template <typename T>
static void socket_send(int32_t client_socket_fd, const T* data, ssize_t size)
{
  if constexpr (std::is_integral<T>::value)
  {
    ssize_t bytes_sent{0};
    int     flags     {0};

    try
    {
      while (bytes_sent < size)
        bytes_sent += send(client_socket_fd,
                           data + bytes_sent, // ptr
                           size - bytes_sent, // bytes
                           flags);
    }
    catch(const std::exception& e)
    {
      throw;
    }
  }
}

/**
 * Constructor
 * Initialize with ip_address, port and message_handler
 */
SocketListener::SocketListener(int arg_num, char** args)
: m_ip_address("0.0.0.0"),
  m_port(9009),
  m_service_enabled(true)

{
  for (int i = 0; i < arg_num; i++)
  {
   const std::string argument{args[i]};
   if (IS_ARGUMENT(argument, "ip"))
     m_ip_address = argument.substr(5);
   else
   if (IS_ARGUMENT(argument, "port"))
     m_port = std::stoi(argument.substr(7));
  }
}

/**
 * @destructor
 */
SocketListener::~SocketListener() {}

/**
 * createMessageHandler
 */
SocketListener::MessageHandler SocketListener::createMessageHandler(std::function<void(ssize_t)> cb)
{
  return MessageHandler(cb);
}

/**
 * onMessageReceived
 */
void SocketListener::onMessageReceived(int32_t                  client_socket_fd,
                                       std::weak_ptr<uint8_t[]> w_buffer_ptr,
                                       ssize_t&                 size)
{
  sendMessage(client_socket_fd, w_buffer_ptr);
}

/**
 * onConnectionClose
 */
void SocketListener::onConnectionClose(int client_socket_fd)
{
  throw -1;
}

/**
 * sendMessage
 * @method
 * Send a null-terminated array of characters, supplied as a const uint8_t
 * pointer, to a client socket described by its file descriptor
 */
void SocketListener::sendMessage(int32_t                  client_socket_fd,
                                 std::weak_ptr<uint8_t[]> w_buffer_ptr)
{
  const std::shared_ptr<uint8_t[]> s_buffer_ptr = w_buffer_ptr.lock();
  const uint8_t*                   data         = s_buffer_ptr.get();

  if (data != nullptr)
  {
    size_t size{};
    while (data[size] != '\0')
      size++;
    socket_send(client_socket_fd, data, size);
  }
}

void SocketListener::sendMessage(int client_socket_fd, char buffer[],
                                 size_t size)
{
  socket_send(client_socket_fd, buffer, size);
}

void SocketListener::sendMessage(int client_socket_fd, const char* buffer,
                                 size_t size)
{
  socket_send(client_socket_fd, buffer, size);
}

/**
 * init
 *
 * @param {bool} test_mode
 */
void SocketListener::init(bool test_mode)
{
  m_test_mode = test_mode;
  std::cout << "Initializing socket listener" << std::endl;
  u_task_queue_ptr = std::make_unique<TaskQueue>();
  u_task_queue_ptr->initialize();
}

void SocketListener::handleClientSocket(int32_t                           client_socket_fd,
                                        SocketListener::MessageHandler    message_handler,
                                        const std::shared_ptr<uint8_t[]>& s_buffer_ptr)
{
  while(s_buffer_ptr.get())
  {
    if (!m_fds[client_socket_fd])
    {
      std::cout << client_socket_fd << " will no longer be handled" << std::endl;
      break;
    }

    memset(s_buffer_ptr.get(), 0, MAX_BUFFER_SIZE);
    ssize_t size = recv(client_socket_fd, s_buffer_ptr.get(), MAX_BUFFER_SIZE, 0);
    if (size > 0)
      message_handler(size);
    else
    if (size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
      std::cout << "recv timeout on " << client_socket_fd << std::endl;
      continue; // timeout
    }
    else
    {
      std::cout << client_socket_fd << " disconnected. Errno: " << errno << std::endl;
      onConnectionClose(client_socket_fd);
      memset(s_buffer_ptr.get(), 0, MAX_BUFFER_SIZE);
      break;
    }
  }
  int rc = ::close(client_socket_fd);
  std::cout << "Socket closed with return code " << rc << std::endl;
}

/**
 * run
 * @method
 * Main message loop
 */
void SocketListener::run()
{
  while (m_service_enabled)
  {
    std::cout << "Creating socket" << std::endl;
    int32_t listening_socket_fd = createSocket();

    if (listening_socket_fd == SOCKET_ERROR)
    {
      std::cout << "Socket error: shutting down server" << std::endl;
      break;
    }

    std::cout << "Waiting for connection" << std::endl;

    int32_t client_socket_fd = waitForConnection(listening_socket_fd);

    if (client_socket_fd != SOCKET_ERROR)
    {
      close(listening_socket_fd); // No longer needed
      {
        struct timeval tv;
        tv.tv_sec  = 10;
        tv.tv_usec = 0;

        setsockopt(client_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

        std::shared_ptr<uint8_t[]>   s_buffer_ptr(new uint8_t[MAX_BUFFER_SIZE]);
        std::weak_ptr<uint8_t[]>     w_buffer_ptr(s_buffer_ptr);
        std::function<void(ssize_t)> message_send_fn = [this, client_socket_fd, w_buffer_ptr](ssize_t size)
        {
          this->onMessageReceived(client_socket_fd, w_buffer_ptr, size);
        };

        MessageHandler message_handler = createMessageHandler(message_send_fn);

        std::cout << "Placing " << client_socket_fd << " in queue" << std::endl;

        u_task_queue_ptr->pushToQueue(
            std::bind(&SocketListener::handleClientSocket, this,
                      client_socket_fd, message_handler,
                      std::forward<std::shared_ptr<uint8_t[]>>(s_buffer_ptr)));

        m_fds.insert_or_assign(client_socket_fd, true);

        if (m_test_mode)
          m_service_enabled = false;
      }
    }
  }
}

/**
 * createSocket
 *
 * Open a listening socket and return its file descriptor
 */
int32_t SocketListener::createSocket()
{
  /* Call the system to open a socket passing arguments for
   ipv4 family, tcp type and no additional protocol info */

  int32_t listening_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (listening_socket_fd != SOCKET_ERROR)
  {
    std::cout << "Created listening socket" << std::endl;
    // Create socket structure to hold address and type
    sockaddr_in socket_struct;
    socket_struct.sin_family = AF_INET;       // ipv4
    socket_struct.sin_port   = htons(m_port); // convert byte order of port value from host to network
    inet_pton(AF_INET, m_ip_address.c_str(),  // convert address to binary
              &socket_struct.sin_addr);

    int32_t socket_option = 1;
    // Free up the port to begin listening again
    setsockopt(listening_socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
               sizeof(socket_option));

    int32_t bind_result = bind(listening_socket_fd, (sockaddr*)&socket_struct, sizeof(socket_struct));

    if (bind_result                            != SOCKET_ERROR &&
        listen(listening_socket_fd, SOMAXCONN) != SOCKET_ERROR)
      return listening_socket_fd;
   }

  return WAIT_SOCKET_FAILURE;
}

/**
 * waitForConnection
 *
 * Takes a connection, creates a new socket and returns its file descriptor
 */
int SocketListener::waitForConnection(int listening_socket)
{
  return accept(listening_socket, NULL, NULL);
}

size_t SocketListener::count() const
{
  return u_task_queue_ptr->size();
}

bool SocketListener::revoke(int32_t fd)
{
  if (m_fds.find(fd) != m_fds.end())
  {
    m_fds[fd] = false;
    return true;
  }
  return false;
}