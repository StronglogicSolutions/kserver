#include "worker.hpp"
#include <logger.hpp>

namespace kiq
{
using namespace kiq::log;

IPCWorker::IPCWorker(zmq::context_t& ctx, std::string_view name, client_handlers_t*  handlers)
: ctx_(ctx),
  backend_(ctx_, ZMQ_DEALER),
  monitor_(ctx, ZMQ_PUSH),
  handlers_(handlers)
{
  backend_.set(zmq::sockopt::linger, 0);
  backend_.set(zmq::sockopt::routing_id, name);
  backend_.set(zmq::sockopt::tcp_keepalive, 1);
  backend_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  backend_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
}
//*******************************************************************//
// IPCWorker::~IPCWorker()
// {

// }
//*******************************************************************//
void
IPCWorker::start()
{
  future_ = std::async(std::launch::async, [this] { run(); });

  monfut_ = std::async(std::launch::async, [this] { monitor(); });
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
  // Create a monitoring socket
    void* monitor_socket = zmq_socket(ctx_.handle(), ZMQ_PAIR);
    if (!monitor_socket) {
        // std::cerr << "Error creating monitor socket" << std::endl;
        return;
    }

    // Bind the monitoring socket to an in-process transport address
    if (zmq_bind(monitor_socket, "inproc://monitor.sock") != 0) {
        // std::cerr << "Error binding monitor socket" << std::endl;
        zmq_close(monitor_socket);
        return;
    }

    // Start monitoring the original socket
    if (zmq_socket_monitor(socket().handle(), "inproc://monitor.sock", ZMQ_EVENT_ALL) != 0) {
        // std::cerr << "Error starting socket monitor" << std::endl;
        zmq_close(monitor_socket);
        return;
    }

    while (true) {
        zmq_msg_t event_msg;
        zmq_msg_init(&event_msg);

        if (zmq_msg_recv(&event_msg, monitor_socket, 0) == -1) {
            // std::cerr << "Error receiving message" << std::endl;
            zmq_msg_close(&event_msg);
            continue;
        }

        // Get event data
        zmq_event_t event = *(reinterpret_cast<zmq_event_t *>(zmq_msg_data(&event_msg)));

        // Process the event type
        switch (event.event) {
            case ZMQ_EVENT_CONNECTED:
                // std::cout << "Client connected" << std::endl;
                break;
            case ZMQ_EVENT_DISCONNECTED:
                // std::cout << "Client disconnected" << std::endl;
                break;
            case ZMQ_EVENT_CLOSED:
                // std::cout << "Socket closed" << std::endl;
                break;
            case ZMQ_EVENT_ACCEPTED:
                // std::cout << "Client accepted" << std::endl;
                break;
            default:
              (void)(0);
                // std::cout << "Other event occurred: " << event.event << std::endl;
        }

        zmq_msg_close(&event_msg);
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
