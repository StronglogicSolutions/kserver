#include "client.hpp"

namespace kiq
{
IPCWorker::IPCWorker(zmq::context_t& ctx, std::string_view target_id, IPCBrokerInterface* broker, bool send_hb)
: manager_(broker),
  ctx_(ctx),
  tx_sink_(ctx_, ZMQ_DEALER),
  backend_(ctx_, ZMQ_DEALER),
  target_(target_id),
  name_(std::string(target_id) + ":worker"),
  send_hb_(send_hb)
{
  tx_sink_.set(zmq::sockopt::linger, 0);
  backend_.set(zmq::sockopt::linger, 0);
  tx_sink_.set(zmq::sockopt::routing_id, name_);
  backend_.set(zmq::sockopt::routing_id, name_);
}

void IPCWorker::start()
{
  future_ = std::async(std::launch::async, [this] { run(); });
  if (send_hb_)
    hb_future_ =  std::async(std::launch::async, [this]
    {
      while (active_)
      {
        send_ipc_message(std::make_unique<keepalive>());
        std::this_thread::sleep_for(hb_rate);
      }
    });
}

void IPCWorker::run()
{
  VLOG("{} is ready to receive IPC", name_);
  tx_sink_.connect(REQ_ADDRESS);
  backend_.connect(BACKEND_ADDRESS);
  while (active_)
    recv();
  VLOG("{} no longer receiving IPC", name_);
}

void IPCWorker::recv()
{
  using buffer_vector_t = std::vector<ipc_message::byte_buffer>;

  auto get_part = [](auto& msg) { return buffer_vector_t::value_type{static_cast<uint8_t*>(msg.data()), static_cast<uint8_t*>(msg.data()) + msg.size()}; };

  zmq::message_t  identity;
  backend_.recv(&identity);

  if (identity.empty())
    return;

  KLOG("Received message from {}", identity.to_string_view());

  buffer_vector_t received_message{};
  zmq::message_t  message;
  int             more_flag{1};

  while (more_flag)
  {
    if (!backend_.recv(&message, static_cast<int>(zmq::recv_flags::none)))
    {
      VLOG("IPC Worker {} failed to receive on socket", target_);
      return;
    }

    size_t size = sizeof(more_flag);
    backend_.getsockopt(ZMQ_RCVMORE, &more_flag, &size);

    received_message.push_back(get_part(message));
  }

  if (ipc_message::u_ipc_msg_ptr ipc_message = DeserializeIPCMessage(std::move(received_message)))
  {
    if (IsKeepAlive(ipc_message->type()))
      manager_->on_heartbeat(name_);
    else
      send_ipc_message(std::make_unique<okay_message>(), false);
    manager_->process_message(std::move(ipc_message));
  }
}

std::future<void>& IPCWorker::stop()
{
  active_ = false;
  if (send_hb_ && hb_future_.valid())
    hb_future_.wait();
  return future_;
}

void IPCWorker::send_ipc_message(u_ipc_msg_ptr message, bool tx)
{
  KLOG("Worker is sending message");

  zmq::socket_t& socket    = (tx) ? tx_sink_ : backend_;
  const auto     payload   = message->data();
  const int32_t  frame_num = payload.size();

  for (int i = 0; i < frame_num; i++)
  {
    const int  flag  = (i == (frame_num - 1)) ? 0 : ZMQ_SNDMORE;
    const auto data  = payload.at(i);
    zmq::message_t message{data.size()};
    std::memcpy(message.data(), data.data(), data.size());
    socket.send(message, flag);
  }
}
} // ns kiq
