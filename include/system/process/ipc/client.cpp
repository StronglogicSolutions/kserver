#include "client.hpp"
#include "config/config_parser.hpp"
#include <logger.hpp>

namespace kiq
{
using namespace kiq::log;

static const std::string client_hello_g{"Hello"};
//*******************************************************************//
botbroker_handler::botbroker_handler(const std::string& addr, zmq::context_t& ctx, std::string_view target_id, IPCBrokerInterface* manager, bool send_hb)
: manager_(manager),
  ctx_(ctx),
  tx_sink_(ctx_, ZMQ_DEALER),
  client_(target_id),
  name_(std::string(target_id) + "__worker"),
  addr_(addr),
  send_hb_(send_hb)
{
  connect();

  if (send_hb_)
    future_ =  std::async(std::launch::async, [this]
    {
      int hb_count = 0;
      while (active_)
      {
        send_ipc_message(std::make_unique<keepalive>());

        if (++hb_count % 200 == 0)
          klog().d("Sent {} HB -> {}", hb_count, name_);
        std::this_thread::sleep_for(hb_rate);
      }
      klog().d("Stopping HB with {}", name_);
    });

  klog().d("Client {} sent greeting to {}", name_, get_addr());
}
//*******************************************************************//
botbroker_handler::~botbroker_handler()
{
  klog().d("Cleaning up for {} at {}", tx_sink_.get(zmq::sockopt::routing_id), get_addr());
  active_ = false;

  if (send_hb_ && future_.valid())
    future_.wait();

  tx_sink_.close();
}
//*******************************************************************//
void
botbroker_handler::connect()
{
  tx_sink_.set(zmq::sockopt::linger, 0);
  tx_sink_.set(zmq::sockopt::routing_id, name_);
  tx_sink_.set(zmq::sockopt::tcp_keepalive, 1);
  tx_sink_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  tx_sink_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  tx_sink_.connect(addr_);
}
//*******************************************************************//
void
botbroker_handler::process_message(ipc_msg_t msg)
{
  switch(msg->type())
  {
    case constants::IPC_KEEPALIVE_TYPE:
      manager_->on_heartbeat(client_);
      if (++hb_count_ % 200 == 0)
        klog().d("Received {} HB from {}", hb_count_, get_addr());
    break;
    case constants::IPC_STATUS:
      send_ipc_message(std::make_unique<okay_message>());
      connect();
    break;
    default:
      klog().d("Received from {}", get_addr());
      manager_->process_message(std::move(msg));
    break;
  }
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
//--------------------------------------------------------------------
void botbroker_handler::reconnect()
{

  tx_sink_.close();
  tx_sink_ = zmq::socket_t(ctx_, ZMQ_DEALER);
  connect();
}
} // ns kiq
