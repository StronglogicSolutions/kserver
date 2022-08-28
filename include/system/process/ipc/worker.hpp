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

class IPCWorker
{
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
public:
  IPCWorker(zmq::context_t& ctx, std::string_view target_id, client_handlers_t* handlers);
  void               start();
  std::future<void>& stop();
  void               send_ipc_message(u_ipc_msg_ptr message);

private:
  void run();
  void recv();

  std::string name() const;

  zmq::context_t&     ctx_;
  zmq::socket_t       backend_;
  client_handlers_t*  handlers_;
  bool                active_;
  std::future<void>   future_;
};
} // ns kiq
