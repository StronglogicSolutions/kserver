#pragma once

#include <system/process/ipc/ipc.hpp>
#include <zmq.hpp>
#include <memory>

const std::string REQ_ADDRESS{"tcp://0.0.0.0:28473"};
const std::string REP_ADDRESS{"tcp://0.0.0.0:28474"};

static const bool HasIPCMessage(uint8_t mask)
{
  return (mask & 0x01 << 0);
}

static const bool HasMessage(uint8_t mask)
{
  return (mask & 0x01 << 1);
}

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
  m_rep_socket{m_context, ZMQ_REP},
  m_req_socket{m_context, ZMQ_REQ}
  {
    ResetSocket();
  }

void ResetSocket() {
  m_req_socket.connect(REQ_ADDRESS);
  m_rep_socket.bind(REP_ADDRESS);
}

bool SendMessage(std::string message) {
  zmq::message_t ipc_msg{message.size()};
  memcpy(ipc_msg.data(), message.data(), message.size());
  zmq::send_result_t result = m_req_socket.send(std::move(ipc_msg), zmq::send_flags::none);

  return result.has_value();
}

bool SendIPCMessage(u_ipc_msg_ptr message) {
  auto    payload = message->data();
  int32_t frame_num = payload.size();

  for (int i = 0; i < frame_num; i++)
  {
    int  flag  = i == (frame_num - 1) ? 0 : ZMQ_SNDMORE;
    auto data  = payload.at(i);

    zmq::message_t message{data.size()};
    std::memcpy(message.data(), data.data(), data.size());

    m_rep_socket.send(message, flag);
  }

  return true;
}

bool ReplyIPC()
{
  return SendIPCMessage(std::move(std::make_unique<okay_message>()));
}


bool ReceiveMessage() {
  zmq::message_t     message{};
  zmq::recv_result_t result = m_req_socket.recv(message, zmq::recv_flags::none);

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

  while (more_flag)
  {
    m_rep_socket.recv(&message, static_cast<int>(zmq::recv_flags::none));
    size_t size = sizeof(more_flag);
    m_rep_socket.getsockopt(ZMQ_RCVMORE, &more_flag, &size);

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

uint8_t Poll()
{
  uint8_t        poll_mask{0x00};
  void*          rep_socket_ptr = static_cast<void*>(m_rep_socket);
  void*          req_socket_ptr = static_cast<void*>(m_req_socket);
  zmq_pollitem_t items[2]{
    {rep_socket_ptr, 0, ZMQ_POLLIN, 0},
    {req_socket_ptr, 0, ZMQ_POLLIN, 0}
  };

  zmq::poll(&items[0], 2, 0);

  if (items[0].revents & ZMQ_POLLIN)
    poll_mask |= (0x01 << 0);

  if (items[1].revents & ZMQ_POLLIN)
    poll_mask |= (0x01 << 1);

  return poll_mask;
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
zmq::socket_t                  m_rep_socket;
zmq::socket_t                  m_req_socket;
std::vector<u_ipc_msg_ptr>     m_tx_msgs;
std::vector<u_ipc_msg_ptr>     m_rx_msgs;
std::string                    m_addr;
std::string                    m_rx_msg;

};
