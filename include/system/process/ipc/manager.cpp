#include "manager.hpp"
#include "system/process/executor/task_handlers/task.hpp"
#include <logger.hpp>

namespace kiq
{
  using namespace kiq::log;

  static const char* broker_peer = "botbroker";
  static const char* sentnl_peer = "sentinel";
  static const char* kygui_peer  = "kygui";
  static const char* onprem_peer = "onprem";

  static std::array<std::string_view, 4> ipc_peers
  {
    broker_peer,
    sentnl_peer,
    kygui_peer,
    onprem_peer
  };

  static std::string to_info_type(std::string_view plat, std::string_view type)
  {
    return fmt::format("{}:{}", plat, type);
  }

  static std::string_view find_peer(const std::string& s, bool get_default = false)
  {
    for (const auto& peer : ipc_peers)
      if (s.find(peer) != std::string::npos)
        return peer;

    if (get_default)
        return ipc_peers.front();
    return "";
  }
  //*******************************************************************//
  static bool should_relay(std::string_view addr)
  {
    return (addr.find("0.0.0.0") == addr.npos && addr.find("127.0.0.1") == addr.npos && addr.find("localhost") == addr.npos);
  }
  //*******************************************************************//
  static void log_message(ipc_message* msg)
  {
    if (const auto type = msg->type(); type != constants::IPC_KEEPALIVE_TYPE)
      klog().t("Processing message of type {}", constants::IPC_MESSAGE_NAMES.at(type));
  }
  //*******************************************************************//
  std::unique_ptr<platform_message> deserialize(const Payload &args)
  {
    return std::make_unique<platform_message>(
      args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX), args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_USER_INDEX), args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_URL_INDEX), args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
      std::stoi(args.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)), args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_TIME_INDEX));
  };
  //*******************************************************************//
  IPCManager::IPCManager()
      : m_req_ready(true),
        m_context(1),
        m_public_(m_context, ZMQ_ROUTER),
        m_backend_(m_context, ZMQ_DEALER)
  {
    set_log_fn([](const char* arg) { klog().t(arg); });

    zmq::socket_t monitor(m_context, ZMQ_PULL);

    monitor   .bind(MONITOR_ADDRESS);
    m_public_ .bind(REP_ADDRESS);
    m_backend_.bind(BACKEND_ADDRESS);

    m_future = std::async(std::launch::async, [this, &monitor]
    {
      zmq::proxy(m_public_, m_backend_);
      zmq::message_t identity;
      if (monitor.recv(identity, zmq::recv_flags::dontwait))
        klog().d("New connection received by monitor: {}", identity.to_string_view());
    });
  }
  //*******************************************************************//
  IPCManager::~IPCManager()
  {
    for (auto& worker : m_workers)
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
  IPCManager::ReceiveEvent(int32_t event, const std::vector<std::string>& args)
  {
    klog().i("Processing IPC message for event {}", event);
    if (m_clients.find(broker_peer) == m_clients.end() || m_clients.find(sentnl_peer) == m_clients.end())
    {
      delay_event(event, args);
      return false;
    }

    switch (event)
    {
      case SYSTEM_EVENTS__PLATFORM_POST_REQUESTED:
      case SYSTEM_EVENTS__PLATFORM_EVENT:
        m_clients.at(broker_peer)->send_ipc_message(deserialize(args));
      break;
      case SYSTEM_EVENTS__IPC_REQUEST:
        if (auto peer = find_peer(args.front(), true); !peer.empty())
        {
          if (should_relay(m_clients.at(peer)->get_addr()))
            peer = onprem_peer;                              // relay to on_prem

          klog().d("Sending KIQ message of {} to {}", args.front(), peer);
            m_clients.at(peer)->send_ipc_message(std::make_unique<kiq_message>(args.front()));
        }
        else
          klog().e("Ignoring IPC request from unknown peer: {}", peer);
      break;
      case SYSTEM_EVENTS__PLATFORM_INFO_REQUEST:
        if (const auto peer = find_peer(args.front()); !peer.empty())
        {
          const auto& platform = args.front();
          const auto& info     = args.at(1);
          const auto& type     = args.at(2);

          klog().d("Sending platform_info for {} of type {} and info {} to {} ", platform, type, info, peer);

          m_clients.at(peer)->send_ipc_message(std::make_unique<platform_info>(
            platform, info, to_info_type(platform, type)));
        }
      default:
        return false;
    }
    return true;
  }
  //*******************************************************************//
  void
  IPCManager::start()
  {
    m_workers.push_back(IPCWorker{m_context, "Worker 1", &m_clients});
    m_workers.back().start();
    m_clients.emplace(broker_peer, new botbroker_handler{config::Process::broker_address(), m_context, broker_peer, this, true});
    m_clients.emplace(sentnl_peer, new botbroker_handler{config::Process::sentnl_address(), m_context, sentnl_peer, this, true});
    m_clients.emplace(onprem_peer, new botbroker_handler{config::Process::onprem_address(), m_context, sentnl_peer, this, true});

    for (const auto& peer : ipc_peers)
      m_daemon.add_observer(peer, [&peer] { klog().e("Heartbeat timed out for {}", peer); });
    m_daemon.reset();
  }
  //*******************************************************************//
  void
  IPCManager::process_message(u_ipc_msg_ptr msg)
  {
    klog().d("Received IPC message: {}", msg ? msg->to_string() : "null");
    log_message(msg.get());
    m_dispatch_table[msg->type()](std::move(msg));
  }
  //*******************************************************************//
  void
  IPCManager::on_heartbeat(std::string_view peer)
  {
    if (!m_daemon.validate(peer))
      klog().t("Couldn't validate heartbeat for {}", peer);
  }
  //*******************************************************************//
  void
  IPCManager::delay_event(int32_t event, const std::vector<std::string>& args)
  {
    static const int delay_limit{5};
    static       int delay_count{0};

    if (++delay_count > delay_limit)
      throw std::runtime_error{"Exceeded IPC event delay limit"};

    std::thread{[this, event, args]
    {
      while (m_clients.find(broker_peer) == m_clients.end())
      {
        klog().t("Delaying handling of IPC message");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
      ReceiveEvent(event, args);
    }}.detach();
  }
} // ns kiq
