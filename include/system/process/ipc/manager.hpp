#pragma once

#include "interface/worker_interface.hpp"
#include "server/types.hpp"
#include "config/config_parser.hpp"
#include "worker.hpp"
#include "client.hpp"
#include "server/event_handler.hpp"
#include <logger.hpp>

namespace kiq {
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
using SystemCallback_fn_ptr = std::function<void(int32_t, const std::vector<std::string>&)>;
using SystemDispatch_t      = std::function<void(u_ipc_msg_ptr)>;
using Payload               = std::vector<std::string>;
template <uint8_t type = 0x00>
static auto GetPayload = [](auto&& ipc)
{
  using namespace constants;
  switch (type)
  {
    case IPC_PLATFORM_TYPE:
    {
      const auto& m = static_cast<platform_message*>(ipc.get());
      return Payload{m->platform(), m->id(), m->user(), "", m->content(), m->urls(), std::to_string(m->repost()), m->args()};
    }

    case IPC_PLATFORM_REQUEST:
    {
      log::klog().t("Getting payload from {}", ipc->to_string());
      const auto& m = static_cast<platform_request*>(ipc.get());
      const auto platform = m->platform();
      log::klog().t("Got {}", platform);
      const auto id = m->id();
      log::klog().t("Got {}", id);
      const auto user = m->user();
      log::klog().t("Got {}", user);
      const auto content = m->content();
      log::klog().t("Got {}", content);
      const auto args = m->args() ;
      log::klog().t("Got {}", args);
      return Payload{platform, id, user, content, args};
    }

    case IPC_PLATFORM_INFO:
    {
      const auto& m = static_cast<platform_info*>(ipc.get());
      return Payload{std::string{constants::IPC_MESSAGE_NAMES.at(IPC_PLATFORM_INFO)}, m->platform(), m->id(), m->type(), m->info()}; // TODO: compliant to task.hpp platform indexes
    }                                                                   // TODO: all should be compliant

    case IPC_PLATFORM_ERROR:
    {
      const auto& m = static_cast<platform_error*>(ipc.get());
      return Payload{m->name(), m->id(), m->user(), m->error(), ""};
    }

    case IPC_KIQ_MESSAGE:
    {
      const auto& m = static_cast<kiq_message*>(ipc.get());
      return Payload{m->payload(), m->platform()};
    }

    default:
      log::klog().w("Can't get payload for unhandled type {}", type);
  }
  return Payload{};
};


static const uint32_t DEFAULT_PORT{static_cast<uint32_t>(std::stoul(config::Process::ipc_port()))};

static auto NOOP = [] { (void)(0); };

class IPCManager : public IPCBrokerInterface
{
public:

IPCManager();
~IPCManager() final;
bool ReceiveEvent(int32_t event, const std::vector<std::string>& args);
void close(int32_t fd);
void process_message(u_ipc_msg_ptr msg);
void start();
void on_heartbeat(std::string_view peer);

private:
  void loop();
  void delay_event(int32_t event, const std::vector<std::string>& args);

  using evt = event_handler;

  std::map<uint8_t, SystemDispatch_t> m_dispatch_table{
  {constants::IPC_PLATFORM_TYPE,    [&, this](auto it)
  {
    log::klog().t("Calling event handler for {}", constants::IPC_MESSAGE_NAMES.at(it->type()));
    evt::instance()(SYSTEM_EVENTS__PLATFORM_NEW_POST, GetPayload<constants::IPC_PLATFORM_TYPE>(it));
  }},
  {constants::IPC_PLATFORM_REQUEST, [&, this](auto it)
  {
    log::klog().t("Calling event handler for {}", constants::IPC_MESSAGE_NAMES.at(it->type()));
    evt::instance()(SYSTEM_EVENTS__PLATFORM_REQUEST, GetPayload<constants::IPC_PLATFORM_REQUEST>(it));
  }},
  {constants::IPC_PLATFORM_INFO,    [&, this](auto it)
  {
    log::klog().t("Calling event handler for {} with {} frames", constants::IPC_MESSAGE_NAMES.at(it->type()), it->data().size());
    const auto payload = GetPayload<constants::IPC_PLATFORM_INFO>(it);
    log::klog().t("Payload size is {}", payload.size());
    evt::instance()(SYSTEM_EVENTS__PLATFORM_INFO, payload);
  }},
  {constants::IPC_PLATFORM_ERROR,   [&, this](auto it)
  {
    log::klog().t("Calling event handler for {}", constants::IPC_MESSAGE_NAMES.at(it->type()));
    evt::instance()(SYSTEM_EVENTS__PLATFORM_ERROR, GetPayload<constants::IPC_PLATFORM_ERROR>(it));
  }},
  {constants::IPC_KIQ_MESSAGE,      [&, this](auto it)
  {
    log::klog().t("Calling event handler for {}", constants::IPC_MESSAGE_NAMES.at(it->type()));
    evt::instance()(SYSTEM_EVENTS__KIQ_IPC_MESSAGE, GetPayload<constants::IPC_KIQ_MESSAGE>(it));
  }},
  {constants::IPC_KEEPALIVE_TYPE,   [&, this](auto it) { m_daemon.reset();   }},
  {constants::IPC_OK_TYPE,          [&, this](auto it) { NOOP();             }},
  {constants::IPC_STATUS,           [&, this](auto it) { NOOP();             }}};

  using workers_t = std::vector<IPCWorker>;

  workers_t             m_workers;
  client_handlers_t     m_clients;
  std::mutex            m_mutex;
  bool                  m_req_ready;
  session_daemon        m_daemon;
  zmq::context_t        m_context;
  zmq::socket_t         m_public_;
  zmq::socket_t         m_backend_;
  std::future<void>     m_future;
};
} // ns kiq
