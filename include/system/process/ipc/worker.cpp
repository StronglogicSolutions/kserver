#include "worker.hpp"
#include <logger.hpp>

namespace kiq
{
using namespace kiq::log;

IPCWorker::IPCWorker(zmq::context_t& ctx, std::string_view name, client_handlers_t*  handlers)
: ctx_(ctx),
  backend_(ctx_, ZMQ_DEALER),
  handlers_(handlers)
{
  backend_.set(zmq::sockopt::linger, 0);
  backend_.set(zmq::sockopt::routing_id, name);
  backend_.set(zmq::sockopt::tcp_keepalive, 1);
  backend_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  backend_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
}
//*******************************************************************//
IPCWorker::~IPCWorker()
{
  active_ = false;
  if (monfut_.valid())
    monfut_.wait();
  if (future_.valid())
    future_.wait();
}
//*******************************************************************//
IPCWorker::IPCWorker(const IPCWorker& w)
: ctx_(w.ctx_),
  handlers_(w.handlers_),
  active_(w.active_)
{}

IPCWorker::IPCWorker(IPCWorker&& w) noexcept
: ctx_(w.ctx_),
  handlers_(w.handlers_),
  active_(w.active_),
  monfut_(std::move(w.monfut_)),
  future_(std::move(w.future_))
{}

// Allow move assignment operator
IPCWorker& IPCWorker::operator=(IPCWorker&& w) noexcept
{
  if (this != &w)
  {
    if (w.future_.valid())
      future_ = std::move(w.future_);

    if (w.monfut_.valid())
      monfut_ = std::move(w.monfut_);

    ctx_ = std::move(w.ctx_);
    handlers_ = w.handlers_;
    active_ = w.active_;
  }
  return *this;
}

void
IPCWorker::start()
{
  monfut_ = std::async(std::launch::async, [this] { monitor(); });

  future_ = std::async(std::launch::async, [this] { run(); });
}
//*******************************************************************//
void
IPCWorker::run()
{
  klog().t("{} is ready to receive IPC", name());
  backend_.connect(BACKEND_ADDRESS);
  send_ipc_message(std::make_unique<status_check>());

  while (active_)
    recv();
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

void
IPCWorker::monitor()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  void* monitor_socket = zmq_socket(ctx_.handle(), ZMQ_PAIR);
  if (!monitor_socket)
  {
    klog().e("Error creating monitor socket");
    return;
  }

  if (zmq_socket_monitor(socket().handle(), "inproc://monitors.sock", ZMQ_EVENT_ALL) != 0)
  {
    klog().e("Error starting socket monitor. Error: {}", zmq_strerror(zmq_errno()));
    zmq_close(monitor_socket);
    return;
  }

  int timeout_ms = 5000; // 5 seconds
  zmq_setsockopt(socket(), ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

  while (true)
  {
    if (!active_)
      return;

    zmq_msg_t event_msg;
    zmq_msg_init(&event_msg);
    klog().t("@@ZMQMONITOR! Waiting for message");
    if (zmq_msg_recv(&event_msg, monitor_socket, 0) == -1)
    {
      klog().e("@@ZMQMONITOR! Error receiving message");
      zmq_msg_close(&event_msg);
      continue;
    }

    klog().d("@@ZMQMONITOR! Processing ZMQ event message");

    zmq_event_t event;
    memcpy(&event, zmq_msg_data(&event_msg), sizeof(zmq_event_t));
    zmq_msg_close(&event_msg);

    switch (event.event)
    {
      case ZMQ_EVENT_CONNECTED:
        klog().t("@@ZMQMONITOR! Client connected");
      break;
      case ZMQ_EVENT_DISCONNECTED:
        klog().t("@@ZMQMONITOR! Client disconnected");
      break;
      case ZMQ_EVENT_CLOSED:
        klog().t("@@ZMQMONITOR! Socket closed");
      break;
      case ZMQ_EVENT_ACCEPTED:
        klog().t("@@ZMQMONITOR! Client accepted");
      break;
      default:
        klog().t("@@ZMQMONITOR! Other event occurred: {}", event.event);
     }
    }

    zmq_close(monitor_socket);
}

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
} // ns kiq
