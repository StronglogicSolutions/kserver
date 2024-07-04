#include "manager.hpp"
#include "system/process/executor/task_handlers/task.hpp"
#include <logger.hpp>

namespace kiq
{
  using namespace kiq::log;

  static const char* broker_peer = "botbroker";
  static const char* sentnl_peer = "sentinel";
  static const char* kygui_peer  = "kygui";
  static const char* kai_peer    = "kai";

  static const std::array<std::string_view, 4> ipc_peers
  {
    broker_peer,
    sentnl_peer,
    kygui_peer,
    kai_peer
  };

  static std::string to_info_type(std::string_view plat, std::string_view type)
  {
    return fmt::format("{}:{}", plat, type);
  }

  static std::string_view find_peer(const std::string& input, bool get_default = false)
  {
    const auto s = StringUtils::ToLower(input);
    klog().d("Finding peer for {}", s);
    for (const auto& peer : ipc_peers)
      if (s.find(peer) != std::string::npos)
        return peer;

    if (get_default)
        return ipc_peers.front();
    return "";
  }
  //*******************************************************************//
  auto to_string_max = [](const auto& str, size_t max)
  {
    const auto size = str.size();
    return (size > max) ? std::string(str.data(), max) : str;
  };

  static void log_message(ipc_message* msg)
  {

    using namespace constants;

    if (!msg)
    {
      klog().w("Received null IPC message");
      return;
    }

    const auto type = msg->type();
    if (type == constants::IPC_KEEPALIVE_TYPE)
      return;

    klog().t("Processing message of type {}", constants::IPC_MESSAGE_NAMES.at(type));
    klog().d("View message: {}", to_string_max(msg->to_string(), 650));
    klog().t("For event handler {} with {} frames", IPC_MESSAGE_NAMES.at(msg->type()),
                                                                         msg->data().size());

  }
  //*******************************************************************//
  enum class ipc_payload_t {
    PLATFORM = 0x00,
    IPC_MSG  = 0x01
  };

  struct out_ipc_t
  {
    std::unique_ptr<ipc_message> msg;
    std::string                  platform{""};

    std::unique_ptr<ipc_message> get()
    {
      return std::move(msg);
    }

  };
  template <ipc_payload_t N>
  out_ipc_t deserialize(const Payload &args)
  {
    if constexpr (N == ipc_payload_t::PLATFORM)
      return {std::make_unique<platform_message>(
          args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX),
          args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
          args.at(constants::PLATFORM_PAYLOAD_USER_INDEX),
          args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
          args.at(constants::PLATFORM_PAYLOAD_URL_INDEX),
          args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
std::stoi(args.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)),
          args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX),
          args.at(constants::PLATFORM_PAYLOAD_TIME_INDEX)),
          args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX)};
    else
    {
      klog().t("Getting IPC Message value for {}", args.front());
      uint8_t message_type = constants::IPC_MESSAGE_VALUES.at(args.front());
      switch (message_type)
      {
        case (constants::IPC_KIQ_MESSAGE):
          return {std::make_unique<kiq_message>     (args.at(constants::IPC_ARGS_INDEX),
                                                     args.at(constants::IPC_PLATFORM_INDEX)),
                                                     args.at(constants::IPC_PLATFORM_INDEX)};

        case (constants::IPC_PLATFORM_TYPE):
          return {std::make_unique<platform_message>(args.at(constants::IPC_PLATFORM_INDEX),
                                                    args.at(constants::IPC_ID_INDEX),
                                                    args.at(constants::IPC_USER_INDEX),
                                                    args.at(constants::IPC_CONTENT_INDEX),
                                                    args.at(constants::IPC_URL_INDEX),
                                                    args.at(constants::IPC_REPOST_INDEX) == "y",
                                          std::stoi(args.at(constants::IPC_CMD_INDEX)),
                                                    args.at(constants::IPC_ARGS_INDEX),
                                                    args.at(constants::IPC_TIME_INDEX)),
                                                    args.at(constants::IPC_PLATFORM_INDEX)};

        case (constants::IPC_PLATFORM_INFO):
          return {std::make_unique<platform_info>   (args.at(constants::IPC_PLATFORM_INDEX),
                                                     args.at(constants::IPC_INFO_INDEX ),
                                                     args.at(constants::IPC_TYPE_INDEX ),
                                                     args.at(constants::IPC_ID_INDEX   )),
                                                     args.at(constants::IPC_PLATFORM_INDEX)};
        default:
          return {nullptr};
      }
    }
  };
  //*******************************************************************//
  IPCManager::IPCManager()
      : m_req_ready(true),
        m_context(1),
        m_public_ (m_context, ZMQ_ROUTER),
        m_backend_(m_context, ZMQ_DEALER)
  {
    set_log_fn([](const char* arg) { klog().t(arg); });

    m_public_ .bind(REP_ADDRESS);
    m_backend_.bind(BACKEND_ADDRESS);

    m_future = std::async(std::launch::async, [this] { zmq::proxy(m_public_, m_backend_); });
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
        m_clients.at(broker_peer)->send_ipc_message(deserialize<ipc_payload_t::PLATFORM>(args).get());
      break;
      case SYSTEM_EVENTS__PLATFORM_EVENT:
      {
        auto out = deserialize<ipc_payload_t::IPC_MSG>(args);
        klog().t("Sending IPC message: {}", to_string_max(out.msg->to_string(), 650));

        IPCHandlerInterface* client = m_clients.at(find_peer(out.platform, true));

        klog().t("Client address is {}", client->get_addr());

        client->send_ipc_message(out.get());
      break;
      }
      case SYSTEM_EVENTS__IPC_REQUEST:
        if (auto peer = find_peer(args.front(), true); !peer.empty())
        {
          klog().d("Sending KIQ message with {} to {}", args.front(), peer);
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
            platform, info, to_info_type(platform, type), ""));
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
    m_clients.emplace(kai_peer,    new botbroker_handler{config::Process::kai_address(),    m_context, kai_peer, this, true});

    m_clients.at(broker_peer)->send_ipc_message(std::make_unique<status_check>());
    m_clients.at(sentnl_peer)->send_ipc_message(std::make_unique<status_check>());
    m_clients.at(kai_peer   )->send_ipc_message(std::make_unique<status_check>());

    for (const auto& peer : ipc_peers)
      m_daemon.add_observer(peer, [&peer] { klog().e("Heartbeat timed out for {}", peer); });
    m_daemon.reset();
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
