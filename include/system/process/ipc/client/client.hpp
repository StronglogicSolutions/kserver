#pragma once

#include <zmq.hpp>
#include <future>
#include <system/process/ipc/ipc.hpp>
#include "log/logger.h"

namespace kiq {
const std::string  REQ_ADDRESS{"tcp://localhost:28473"};
const std::string  REP_ADDRESS{"tcp://0.0.0.0:28474"};
static const char* BACKEND_ADDRESS{"inproc://backend"};
const int32_t      POLLTIMEOUT{50};

static auto IsKeepAlive = [](auto type) { return type == ::constants::IPC_KEEPALIVE_TYPE; };

class IPCBrokerInterface
{
public:
  virtual ~IPCBrokerInterface() = default;
  virtual void on_heartbeat(std::string_view peer) = 0;
  virtual void process_message(ipc_message::u_ipc_msg_ptr) = 0;
};

class IPCWorker
{
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
public:
  IPCWorker(zmq::context_t& ctx, std::string_view target_id, IPCBrokerInterface* broker, bool send_hb = false);
  void               start();
  std::future<void>& stop();
  void               send_ipc_message(u_ipc_msg_ptr message, bool tx = true);

private:
  void run();
  void recv();

  IPCBrokerInterface* manager_;
  zmq::context_t&     ctx_;
  zmq::socket_t       backend_;
  zmq::socket_t       tx_sink_;
  std::string_view    target_;
  std::string         name_;
  bool                active_;
  std::promise<void>  promise_;
  std::future<void>   future_;
  std::future<void>   hb_future_;
  bool                send_hb_;
};
} // ns kiq
