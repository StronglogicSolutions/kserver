#pragma once

#include <future>
#include <kproto/ipc.hpp>

namespace kiq
{
class botbroker_handler : public IPCHandlerInterface
{
public:
  using ipc_msg_t = ipc_message::u_ipc_msg_ptr;
  botbroker_handler(const std::string& addr, zmq::context_t& ctx, std::string_view target_id, IPCBrokerInterface* broker, bool send_hb = false);
  ~botbroker_handler() final;
  void process_message(ipc_msg_t) final;
  void               start();
  std::future<void>& stop();

protected:
  zmq::socket_t& socket() final;
  void on_done() final;

private:
  IPCBrokerInterface* manager_;
  zmq::context_t&     ctx_;
  zmq::socket_t       tx_sink_;
  std::string_view    client_;
  std::string         name_;
  bool                active_;
  std::future<void>   future_;
  bool                send_hb_;
};
} // ns kiq
