#include "manager.hpp"
#include "system/process/executor/task_handlers/task.hpp"
namespace kiq {

std::unique_ptr<platform_message> Deserialize(const Payload& args)
{
  return std::make_unique<platform_message>(
    args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX),       args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
    args.at(constants::PLATFORM_PAYLOAD_USER_INDEX),           args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
    args.at(constants::PLATFORM_PAYLOAD_URL_INDEX),            args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
    std::stoi(args.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)), args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX));
};

IPCManager::IPCManager(SystemCallback_fn_ptr system_event_fn)
: m_system_event_fn(system_event_fn),
  m_req_ready(true),
  m_context(1),
  m_public_(m_context, ZMQ_ROUTER),
  m_backend_(m_context, ZMQ_DEALER)
{
  m_public_ .bind(REP_ADDRESS);
  m_backend_.bind(BACKEND_ADDRESS);
  m_future = std::async(std::launch::async, [this]
  {
    zmq::proxy(m_public_, m_backend_);
  });
}

IPCManager::~IPCManager()
{
  for (auto& [_, worker] : m_clients)
    if (const auto& fut = worker.stop(); fut.valid())
      fut.wait();
}

void IPCManager::process(std::string message, int32_t fd)
{
  KLOG("Received outgoing IPC request");
  m_clients.at(0).send_ipc_message(std::make_unique<kiq_message>(message));
}

bool IPCManager::ReceiveEvent(int32_t event, const std::vector<std::string> args)
{
  KLOG("Processing IPC message for event {}", event);
  bool received{true};

  if (event == SYSTEM_EVENTS__PLATFORM_POST_REQUESTED || event == SYSTEM_EVENTS__PLATFORM_EVENT)
    m_clients.at(0).send_ipc_message(Deserialize(args));
  else
    received = false;

  return received;
}

void IPCManager::start()
{
  static const char* broker_peer = "botbroker_worker";
  static const char* kygui_peer  = "kygui_worker";
  m_clients.emplace(0, IPCWorker{m_context, broker_peer, this, true});
  m_clients.emplace(1, IPCWorker{m_context, kygui_peer , this});
  m_clients.at(0).start();
  m_clients.at(1).start();
  m_daemon.add_observer(broker_peer, [] { ELOG("Heartbeat timed out for {}", broker_peer); });
  m_daemon.add_observer(kygui_peer,  [] { ELOG("Heartbeat timed out for {}", kygui_peer);  });
}

void IPCManager::process_message(u_ipc_msg_ptr msg)
{
  m_dispatch_table[msg->type()](std::move(msg));
}

void IPCManager::on_heartbeat(std::string_view peer)
{
  if (!m_daemon.validate(peer))
    VLOG("Heartbeat timer reset for {}", peer);
}
} // ns kiq
