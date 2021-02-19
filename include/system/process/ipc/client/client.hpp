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
  ipc_message ipc{data, size};
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

bool Poll() {
  void* socket_ptr = static_cast<void*>(*m_socket.get());

  zmq_pollitem_t items[1]{
    {socket_ptr, 0, ZMQ_POLLIN, 0}
  };

  zmq::poll(&items[0], 1, 0);

  return (items[0].revents & ZMQ_POLLIN);
}

std::vector<ipc_message> GetMessages()
{
  return m_rx_msgs;
}

void Shutdown() {
  // TODO: handle shutdown
}

private:
zmq::context_t                 m_context;
std::unique_ptr<zmq::socket_t> m_socket;
std::vector<ipc_message>       m_tx_msgs;
std::vector<ipc_message>       m_rx_msgs;
std::string                    m_addr;
std::string                    m_rx_msg;

};
