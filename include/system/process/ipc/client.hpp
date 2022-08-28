#pragma once

#include <zmq.hpp>
#include <future>
#include <system/process/ipc/ipc.hpp>
#include "log/logger.h"

namespace kiq
{

class botbroker_handler : public MessageHandlerInterface
{
public:
  using ipc_msg_t = ipc_message::u_ipc_msg_ptr;
  botbroker_handler(zmq::context_t& ctx, std::string_view target_id, IPCBrokerInterface* broker, bool send_hb = false);
  ~botbroker_handler() final;
  void process_message(ipc_msg_t) final;
  void               start();
  std::future<void>& stop();
  void               send_ipc_message(ipc_msg_t message);

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
