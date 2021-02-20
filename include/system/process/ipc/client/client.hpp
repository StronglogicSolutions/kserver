#pragma once

#include <system/process/ipc/ipc.hpp>
#include <zmq.hpp>
#include <memory>

struct IPC_Session
{
uint32_t    id;
std::string name;
};

class IPCClient
{
public:
using u_ipc_msg_ptr = ipc_message::u_ipc_msg_ptr;

explicit IPCClient(uint32_t port)
: m_context{1},
  m_socket{nullptr},
  m_addr{"tcp://127.0.0.1:" + std::to_string(port)} {
    ResetSocket();
  }

void ResetSocket() {
  m_socket.reset(new zmq::socket_t{m_context, ZMQ_REQ});
  m_socket->connect(m_addr);
}

bool SendMessage(std::string message) {
  zmq::message_t ipc_msg{message.size()};
  memcpy(ipc_msg.data(), message.data(), message.size());
  zmq::send_result_t result = m_socket->send(std::move(ipc_msg), zmq::send_flags::none);

  return result.has_value();
}

bool SendIPCMessage(uint8_t* data, uint32_t size) {
  return false;
}

void ProcessMessage() {
  if (!m_rx_msg.empty()) {
    KLOG("Processing {}", m_rx_msg);
  }
}

bool ReceiveMessage() {
  zmq::message_t message{};

  zmq::recv_result_t result = m_socket->recv(message, zmq::recv_flags::none);

  if (result.has_value()) {
    m_rx_msg = std::string{
      static_cast<char*>(message.data()),
      static_cast<char*>(message.data()) + message.size()
    };
    KLOG("Received IPC message: {}", m_rx_msg);
    return true;
  }

  ELOG("Failed to receive IPC message");
  return false;
}

bool ReceiveIPCMessage()
{
  std::vector<ipc_message::byte_buffer> received_message{};
  zmq::message_t                        message;
  int                                   more_flag{1};

  zmq::socket_t* socket_ptr = m_socket.get();

  while (more_flag)
  {
    socket_ptr->recv(&message, static_cast<int>(zmq::recv_flags::none));
    size_t size = sizeof(more_flag);
    socket_ptr->getsockopt(ZMQ_RCVMORE, &more_flag, &size);

    received_message.push_back(std::vector<unsigned char>{
        static_cast<char*>(message.data()), static_cast<char*>(message.data()) + message.size()
    });
  }

  ipc_message::u_ipc_msg_ptr ipc_message = DeserializeIPCMessage(std::move(received_message));

  if (ipc_message != nullptr)
  {
    m_rx_msgs.emplace_back(std::move(ipc_message));
    return true;
  }

  return false;
}

bool SendIPCMessage(u_ipc_msg_ptr message)
{
  auto    payload = message->data();
  int32_t frame_num = payload.size();

  auto socket_ptr = m_socket.get();

  for (int i = 0; i < frame_num; i++)
  {
    int  flag   = i == (frame_num - 1) ? 0 : ZMQ_SNDMORE;
    auto data  = payload.at(i);

    zmq::message_t message{data.size()};
    std::memcpy(message.data(), data.data(), data.size());

    socket_ptr->send(message, flag);
  }

  return true;
}

bool Poll() {
  void* socket_ptr = static_cast<void*>(*m_socket.get());

  zmq_pollitem_t items[1]{
    {socket_ptr, 0, ZMQ_POLLIN, 0}
  };

  zmq::poll(&items[0], 1, 0);

  return (items[0].revents & ZMQ_POLLIN);
}

std::vector<u_ipc_msg_ptr> GetMessages()
{
  return std::move(m_rx_msgs);
}

void Shutdown() {
  // TODO: handle shutdown
}

private:
zmq::context_t                 m_context;
std::unique_ptr<zmq::socket_t> m_socket;
std::vector<u_ipc_msg_ptr>     m_tx_msgs;
std::vector<u_ipc_msg_ptr>     m_rx_msgs;
std::string                    m_addr;
std::string                    m_rx_msg;

};
