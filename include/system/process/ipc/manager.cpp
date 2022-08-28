#include "manager.hpp"
#include "system/process/executor/task_handlers/task.hpp"

namespace kiq
{
  static const char *broker_peer = "botbroker";
  static const char *kygui_peer  = "kygui";

  //*******************************************************************//
  static void log_message(ipc_message* msg)
  {
    const auto type = msg->type();
    if (type != ::constants::IPC_KEEPALIVE_TYPE)
      VLOG("Processing message of type {}", ::constants::IPC_MESSAGE_NAMES.at(type));
  }

  //*******************************************************************//
  std::unique_ptr<platform_message> deserialize(const Payload &args)
  {
    return std::make_unique<platform_message>(
      args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX), args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_USER_INDEX), args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_URL_INDEX), args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
      std::stoi(args.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)), args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX));
  };
  //*******************************************************************//
  IPCManager::IPCManager(SystemCallback_fn_ptr system_event_fn)
      : m_system_event_fn(system_event_fn),
        m_req_ready(true),
        m_context(1),
        m_public_(m_context, ZMQ_ROUTER),
        m_backend_(m_context, ZMQ_DEALER)
  {
    set_log_fn([](const char* arg) { VLOG(arg); });
    m_public_.bind(REP_ADDRESS);
    m_backend_.bind(BACKEND_ADDRESS);
    m_future = std::async(std::launch::async, [this]
                          { zmq::proxy(m_public_, m_backend_); });
  }
  //*******************************************************************//
  IPCManager::~IPCManager()
  {
    for (auto &[_, worker] : m_workers)
      if (const auto &fut = worker.stop(); fut.valid())
        fut.wait();

    for (auto it = m_clients.begin(); it != m_clients.end();)
    {
      MessageHandlerInterface* handler_ptr = it->second;
      delete handler_ptr;
      it = m_clients.erase(it);
    }
  }
  //*******************************************************************//
  bool
  IPCManager::ReceiveEvent(int32_t event, const std::vector<std::string> args)
  {
    KLOG("Processing IPC message for event {}", event);

    switch (event)
    {
    case SYSTEM_EVENTS__PLATFORM_POST_REQUESTED:
    case SYSTEM_EVENTS__PLATFORM_EVENT:
      m_clients.at(broker_peer)->send_ipc_message(deserialize(args));
      break;
    case SYSTEM_EVENTS__IPC_REQUEST:
      m_clients.at(broker_peer)->send_ipc_message(std::make_unique<kiq_message>(args.front()));
      break;
    default:
      return false;
    }
    return true;
  }
  //*******************************************************************//
  void
  IPCManager::start()
  {
    m_workers.emplace(broker_peer, IPCWorker{m_context, broker_peer, &m_clients});
    m_workers.at(broker_peer).start();
    m_clients.emplace(broker_peer, new botbroker_handler{m_context, broker_peer, this, true});
    m_daemon.add_observer(broker_peer, [] { ELOG("Heartbeat timed out for {}", broker_peer); });
  }
  //*******************************************************************//
  void
  IPCManager::process_message(u_ipc_msg_ptr msg)
  {
    log_message(msg.get());
    m_dispatch_table[msg->type()](std::move(msg));
  }
  //*******************************************************************//
  void
  IPCManager::on_heartbeat(std::string_view peer)
  {
    if (!m_daemon.validate(peer))
      VLOG("Couldn't validate heartbeat for {}", peer);
  }
} // ns kiq
