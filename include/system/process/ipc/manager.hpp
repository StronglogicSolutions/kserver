#pragma once

#include "interface/worker_interface.hpp"
#include "server/types.hpp"
#include "config/config_parser.hpp"
#include "worker.hpp"
#include "client.hpp"

namespace kiq {
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
using SystemCallback_fn_ptr = std::function<void(int32_t, const std::vector<std::string>&)>;
using SystemDispatch_t      = std::function<void(u_ipc_msg_ptr)>;
using Payload               = std::vector<std::string>;

static auto GetPayload = [](auto m) { return Payload{m->platform(), m->id(),   m->user(), "", m->content(), m->urls(), std::to_string(m->repost()), m->args()}; };
static auto GetInfo    = [](auto m) { return Payload{m->platform(), m->type(), m->info()};                                                                      };
static auto GetError   = [](auto m) { return Payload{m->name(),     m->id(),   m->user(), m->error(), ""};                                                      };
static auto GetRequest = [](auto m) { return Payload{m->platform(), m->id(),   m->user(), m->content(), m->args()};                                             };

static const uint32_t DEFAULT_PORT{static_cast<uint32_t>(std::stoul(config::Process::ipc_port()))};

static auto NOOP = [] { (void)(0); };

class IPCManager : public IPCBrokerInterface
{
public:

IPCManager(SystemCallback_fn_ptr system_event_fn);
~IPCManager() final;
bool ReceiveEvent(int32_t event, const std::vector<std::string>& args);
void close(int32_t fd);
void process_message(u_ipc_msg_ptr msg);
void start();
void on_heartbeat(std::string_view peer);

private:
  void loop();
  void delay_event(int32_t event, const std::vector<std::string>& args);

  std::map<uint8_t, SystemDispatch_t> m_dispatch_table{
  {::constants::IPC_PLATFORM_TYPE   , [&, this](auto it) { m_system_event_fn(SYSTEM_EVENTS__PLATFORM_NEW_POST, GetPayload(static_cast<platform_message*>(it.get()))); }},
  {::constants::IPC_PLATFORM_REQUEST, [&, this](auto it) { m_system_event_fn(SYSTEM_EVENTS__PLATFORM_REQUEST,  GetRequest(static_cast<platform_request*>(it.get()))); }},
  {::constants::IPC_PLATFORM_INFO   , [&, this](auto it) { m_system_event_fn(SYSTEM_EVENTS__PLATFORM_INFO,     GetInfo   (static_cast<platform_info*>   (it.get()))); }},
  {::constants::IPC_PLATFORM_ERROR  , [&, this](auto it) { m_system_event_fn(SYSTEM_EVENTS__PLATFORM_ERROR,    GetError  (static_cast<platform_error*>  (it.get()))); }},
  {::constants::IPC_KEEPALIVE_TYPE  , [&, this](auto it) { m_daemon.reset();                                                                                            }},
  {::constants::IPC_OK_TYPE         , [&, this](auto it) { NOOP();                                                                                                      }}};

  using workers_t = std::vector<IPCWorker>;

  workers_t             m_workers;
  client_handlers_t     m_clients;
  SystemCallback_fn_ptr m_system_event_fn;
  std::mutex            m_mutex;
  bool                  m_req_ready;
  session_daemon        m_daemon;
  zmq::context_t        m_context;
  zmq::socket_t         m_public_;
  zmq::socket_t         m_backend_;
  std::future<void>     m_future;
};
} // ns kiq
