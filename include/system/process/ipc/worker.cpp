#include "worker.hpp"
#include <logger.hpp>

namespace kiq
{
using namespace kiq::log;

IPCWorker::IPCWorker(zmq::context_t& ctx, std::string_view name, client_handlers_t*  handlers)
: ctx_(ctx),
  backend_(ctx, ZMQ_DEALER),
  handlers_(handlers),
  name_(name)
{
  backend_.set(zmq::sockopt::linger, 0);
  backend_.set(zmq::sockopt::routing_id, name_);
  backend_.set(zmq::sockopt::tcp_keepalive, 1);
  backend_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  backend_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
}
//*******************************************************************//
IPCWorker::IPCWorker (IPCWorker&& other)
: ctx_(other.ctx_),
  backend_(std::move(other.backend_)),
  handlers_(other.handlers_),
  name_(other.name_),
  future_(std::move(other.future_))
{}
//*******************************************************************//
IPCWorker& IPCWorker::operator=(IPCWorker&& other)
{
  if (this != &other)
  {
    ctx_      = std::move(other.ctx_);
    backend_  = std::move(other.backend_);
    handlers_ = other.handlers_;
    name_     = other.name_;
    future_   = std::move(other.future_);
  }

  return *this;
}
//*******************************************************************//
IPCWorker::~IPCWorker()
{
  active_ = false;

  if (future_.valid())
    future_.wait();

  backend_.close();
}
//*******************************************************************//
void
IPCWorker::start()
{
  active_ = true;
  future_ = std::async(std::launch::async, [this] { run(); });
}
//*******************************************************************//
void
IPCWorker::run()
{
  klog().t("{} is ready to receive IPC", name());
  connect();

  for (;;)
  {
    recv();

    if (reconnect_)
    {
      klog().t("Worker handling reconnect request");

      disconnect();
      connect();
    }
    else
    if (!active_)
      break;
  }

  klog().t("{} no longer receiving IPC", name());
}
//*******************************************************************//
void
IPCWorker::recv()
{
  using buffer_vector_t = std::vector<ipc_message::byte_buffer>;

  auto get_part = [](auto& msg) { return buffer_vector_t::value_type{static_cast<uint8_t*>(msg.data()), static_cast<uint8_t*>(msg.data()) + msg.size()}; };

  // klog().t("{} worker recv()", name());
  zmq::message_t  identity;
  backend_.recv(&identity);

  if (identity.empty())
  {
    klog().t("Received IPC with no identity");
    return;
  }

  // klog().t("Received IPC from {}", identity.to_string_view());

  buffer_vector_t received_message{};
  zmq::message_t  message;
  int             more_flag{1};

  while (more_flag)
  {
    if (!backend_.recv(&message, static_cast<int>(zmq::recv_flags::none)))
    {
      klog().t("IPC Worker {} failed to receive on socket", name());
      return;
    }

    size_t size = sizeof(more_flag);
    backend_.getsockopt(ZMQ_RCVMORE, &more_flag, &size);
    received_message.push_back(get_part(message));
  }

  if (ipc_message::u_ipc_msg_ptr ipc_message = DeserializeIPCMessage(std::move(received_message)))
  {
    if (!IsKeepAlive(ipc_message->type()))
    {
      klog().d("Received IPC from {}", identity.to_string());
      send_ipc_message(std::make_unique<okay_message>());
      klog().t("Sent OKAY");
    }

    auto it = handlers_->find(identity.to_string_view());
    if (it == handlers_->end())
      klog().w("No handler available for that name");
    else
      it->second->process_message(std::move(ipc_message));
  }
  else
    klog().e("{} failed to deserialize IPC message", name());
}
//*******************************************************************//
std::future<void>&
IPCWorker::stop()
{
  active_ = false;
  return future_;
}
//******************************************************************//
zmq::socket_t&
IPCWorker::socket()
{
  return backend_;
}
//******************************************************************//
std::string
IPCWorker::name() const
{
  return backend_.get(zmq::sockopt::routing_id);
}
//******************************************************************//
void
IPCWorker::on_done()
{
  (void)(0); // Trace log
}
//******************************************************************//
void
IPCWorker::connect()
{
  klog().d("Worker connecting");

  if (reconnect_)
  {
    klog().d("Replacing socket");

    backend_ = zmq::socket_t{ctx_, ZMQ_DEALER};
    backend_.set(zmq::sockopt::linger, 0);
    backend_.set(zmq::sockopt::routing_id, name_);
    backend_.set(zmq::sockopt::tcp_keepalive, 1);
    backend_.set(zmq::sockopt::tcp_keepalive_idle,  300);
    backend_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
    reconnect_ = false;
  }

  backend_.connect(BACKEND_ADDRESS);

  klog().d("Connected to {}", BACKEND_ADDRESS);
}
//******************************************************************//
void
IPCWorker::reconnect()
{
  klog().d("Requesting reconnect");
  reconnect_ = true;
}
//******************************************************************//
void
IPCWorker::disconnect()
{
  klog().d("Worker disconnecting");
  backend_.close();
}
} // ns kiq
