#pragma once

#include <future>
#include "kproto/ipc.hpp"

namespace kiq {
const std::string  REQ_ADDRESS{"tcp://localhost:28473"};
const std::string  REP_ADDRESS{"tcp://0.0.0.0:28474"};
static const char* BACKEND_ADDRESS{"inproc://backend"};
static const char* CONTROL_ADDRESS{"inproc://proxy_control"};
const int32_t      POLLTIMEOUT{50};

class IPCWorker : public IPCTransmitterInterface
{
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
public:
  IPCWorker(zmq::context_t& ctx, std::string_view target_id, client_handlers_t* handlers);
  ~IPCWorker() final;

  IPCWorker(const IPCWorker&) = delete;
  IPCWorker& operator=(const IPCWorker&) = delete;

  IPCWorker(IPCWorker&& other)           ;// noexcept;
  IPCWorker& operator=(IPCWorker&& other);// noexcept;

  void               start();
  std::future<void>& stop();
  void               connect();
  void               disconnect();
  void               reconnect();

protected:
  zmq::socket_t& socket()  final;
  void           on_done() final;

private:
  void run();
  void recv();

  std::string name() const;

  zmq::context_t&     ctx_;
  zmq::socket_t       backend_;
  zmq::socket_t       monitor_;
  client_handlers_t*  handlers_;
  std::string         name_;
  bool                active_{true};
  bool                reconnect_{false};
  std::future<void>   future_;
};
} // ns kiq
