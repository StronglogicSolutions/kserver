#include "client.hpp"
#include "config/config_parser.hpp"
#include <logger.hpp>

namespace kiq
{
static const std::string client_hello_g{"Hello"};
//*******************************************************************//
botbroker_handler::botbroker_handler(const std::string& addr, zmq::context_t& ctx, std::string_view target_id, IPCBrokerInterface* manager, bool send_hb)
: manager_(manager),
  ctx_(ctx),
  tx_sink_(ctx_, ZMQ_DEALER),
  client_(target_id),
  name_(std::string(target_id) + "__worker"),
  send_hb_(send_hb)
{
  tx_sink_.set(zmq::sockopt::linger, 0);
  tx_sink_.set(zmq::sockopt::routing_id, name_);
  tx_sink_.set(zmq::sockopt::tcp_keepalive, 1);
  tx_sink_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  tx_sink_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  tx_sink_.connect(addr);

  if (send_hb_)
    future_ =  std::async(std::launch::async, [this]
    {
      while (active_)
      {
        send_ipc_message(std::make_unique<keepalive>());
        std::this_thread::sleep_for(hb_rate);
      }
    });

  tx_sink_.send(zmq::message_t (name_.begin(), name_.end()), zmq::send_flags::sndmore);
  tx_sink_.send(zmq::message_t (client_hello_g.begin(), client_hello_g.end()));

  kiq::log::klog().t("Client {} sent greeting to {}", name_, get_addr());
}
//*******************************************************************//
botbroker_handler::~botbroker_handler()
{
  active_ = false;

  if (send_hb_ && future_.valid())
    future_.wait();
}
//*******************************************************************//
void
botbroker_handler::process_message(ipc_msg_t msg)
{
  if (IsKeepAlive(msg->type()))
    manager_->on_heartbeat(client_);
  else
    manager_->process_message(std::move(msg));
}
//******************************************************************//
zmq::socket_t&
botbroker_handler::socket()
{
  return tx_sink_;
}
//******************************************************************//
void
botbroker_handler::on_done()
{
  (void)(0); // Trace log
}
} // ns kiq
