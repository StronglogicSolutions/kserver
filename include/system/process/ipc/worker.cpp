#include "worker.hpp"

namespace kiq
{
IPCWorker::IPCWorker(zmq::context_t& ctx, std::string_view name, client_handlers_t*  handlers)
: ctx_(ctx),
  backend_(ctx_, ZMQ_DEALER),
  handlers_(handlers)
{
  backend_.set(zmq::sockopt::linger, 0);
  backend_.set(zmq::sockopt::routing_id, name);
}
//*******************************************************************//
void
IPCWorker::start()
{
  future_ = std::async(std::launch::async, [this] { run(); });
}
//*******************************************************************//
void
IPCWorker::run()
{
  VLOG("{} is ready to receive IPC", name());
  backend_.connect(BACKEND_ADDRESS);
  while (active_)
    recv();
  VLOG("{} no longer receiving IPC", name());
}
//*******************************************************************//
void
IPCWorker::recv()
{
  using buffer_vector_t = std::vector<ipc_message::byte_buffer>;

  auto get_part = [](auto& msg) { return buffer_vector_t::value_type{static_cast<uint8_t*>(msg.data()), static_cast<uint8_t*>(msg.data()) + msg.size()}; };

  zmq::message_t  identity;
  backend_.recv(&identity);

  if (identity.empty())
    return;

  buffer_vector_t received_message{};
  zmq::message_t  message;
  int             more_flag{1};

  while (more_flag)
  {
    if (!backend_.recv(&message, static_cast<int>(zmq::recv_flags::none)))
    {
      VLOG("IPC Worker {} failed to receive on socket", name());
      return;
    }

    size_t size = sizeof(more_flag);
    backend_.getsockopt(ZMQ_RCVMORE, &more_flag, &size);
    received_message.push_back(get_part(message));
  }

  if (ipc_message::u_ipc_msg_ptr ipc_message = DeserializeIPCMessage(std::move(received_message)))
  {
    if (!IsKeepAlive(ipc_message->type()))
      send_ipc_message(std::make_unique<okay_message>());

    handlers_->at(identity.to_string_view())->process_message(std::move(ipc_message));
  }
  else
    ELOG("{} failed to deserialize IPC message", name());
}
//*******************************************************************//
std::future<void>&
IPCWorker::stop()
{
  active_ = false;
  return future_;
}
//*******************************************************************//
void
IPCWorker::send_ipc_message(u_ipc_msg_ptr message)
{
  const auto     payload   = message->data();
  const size_t   frame_num = payload.size();

  for (int i = 0; i < frame_num; i++)
  {
    const int      flag  = (i == (frame_num - 1)) ? 0 : ZMQ_SNDMORE;
    const auto     data  = payload.at(i);
    zmq::message_t message{data.size()};
    std::memcpy(message.data(), data.data(), data.size());
    backend_.send(message, flag);
  }
}

std::string
IPCWorker::name() const
{
  return backend_.get(zmq::sockopt::routing_id);
}
} // ns kiq
