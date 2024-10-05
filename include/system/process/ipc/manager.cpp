#include "manager.hpp"
#include "system/process/executor/task_handlers/task.hpp"
#include <logger.hpp>

namespace kiq
{
  using namespace kiq::log;

  static const char* broker_peer = "botbroker";
  static const char* sentnl_peer = "sentinel";
  static const char* kai_peer    = "kai";

  static const std::array<std::string_view, 3> ipc_peers
  {
    broker_peer,
    sentnl_peer,
    kai_peer
  };

  static const std::map<std::string_view, std::string> ipc_addresses{
    { broker_peer, config::Process::broker_address() },
    { sentnl_peer, config::Process::sentnl_address() },
    { kai_peer   , config::Process::kai_address   () }
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
  //---------------------------------------------------------------------
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

    klog().d("Processing message of type {}",       constants::IPC_MESSAGE_NAMES.at(type)                 );
    klog().d("View message: {}",                    to_string_max                  (msg->to_string(), 650));
    klog().d("For event handler {} with {} frames", IPC_MESSAGE_NAMES.at           (msg->type()           ),
                                                                                    msg->data().size()    );

  }
  //---------------------------------------------------------------------
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

  //---------------------------------------------------------------------
  IPCManager::IPCManager()
      : m_req_ready(true),
        m_context(1)
  {
    set_log_fn([](const char* arg) { klog().t(arg); });
    klog().i("Starting IPC manager");
    start();
  }
  //---------------------------------------------------------------------
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
  //---------------------------------------------------------------------
  bool
  IPCManager::ReceiveEvent(int32_t event, const std::vector<std::string>& args)
  {
    klog().i("Processing IPC message for event {}", event);

    ipc_event out_event;

    switch (event)
    {
      case SYSTEM_EVENTS__PLATFORM_POST_REQUESTED:
        out_event.msg  = deserialize<ipc_payload_t::PLATFORM>(args).get();
        out_event.peer = broker_peer;
      break;
      case SYSTEM_EVENTS__PLATFORM_EVENT:
      {
        auto out       = deserialize<ipc_payload_t::IPC_MSG>(args);
        out_event.peer = find_peer(out.platform, true);
        out_event.msg  = out.get();
      break;
      }
      case SYSTEM_EVENTS__IPC_REQUEST:
        if (const auto peer = find_peer(args.front(), true); !peer.empty())
        {
          out_event.msg  = std::make_unique<kiq_message>(args.front());
          out_event.peer = peer;
        }
        else
          klog().w("Ignoring IPC request from unknown peer: {}", peer);
      break;
      case SYSTEM_EVENTS__PLATFORM_INFO_REQUEST:
        if (const auto peer = find_peer(args.front()); !peer.empty())
        {
          const auto& platform = args.front();
          const auto& info     = args.at(1);
          const auto& type     = args.at(2);
          out_event.peer       = peer;
          out_event.msg        = std::make_unique<platform_info>(platform, info, to_info_type(platform, type), "");
        }
      default: break;
    }

    if (out_event.msg)
    {
      klog().t("Queueing {} for {}", constants::IPC_MESSAGE_NAMES.at(out_event.msg->type()), out_event.peer);

      m_queue.emplace_back(std::move(out_event));

      return true;
    }

    return false;
  }
  //---------------------------------------------------------------------
  void
  IPCManager::start()
  {
    m_public_  = zmq::socket_t{m_context, ZMQ_ROUTER};
    m_backend_ = zmq::socket_t{m_context, ZMQ_DEALER};
    m_control_ = zmq::socket_t{m_context, ZMQ_PULL};

    klog().d("Binding public, backend, and ipc control sockets");

    m_public_ .bind(REP_ADDRESS);
    m_backend_.bind(BACKEND_ADDRESS);
    m_control_.bind(CONTROL_ADDRESS);

    m_future = std::async(std::launch::async, [this] { zmq::proxy_steerable(m_public_, m_backend_, nullptr, m_control_); });

    m_workers.emplace_back(IPCWorker{m_context, "Worker 1", &m_clients});
    m_workers.back().start();

    for (const auto& peer : ipc_peers)
    {
      const auto& client = m_clients.emplace(peer,
        new botbroker_handler{ipc_addresses.at(peer),
                              m_context,
                              peer,
                              this,
                              true});

      client.first->second->send_ipc_message(std::make_unique<status_check>());
      add_observer(peer);
    }

    klog().d("IPC Workers and clients initialized. Resetting heartbeat daemon");
    m_daemon.reset();
  }
  //---------------------------------------------------------------------
  void
  IPCManager::process_message(u_ipc_msg_ptr msg)
  {
    log_message(msg.get());
    m_dispatch_table[msg->type()](std::move(msg));
  }
  //---------------------------------------------------------------------
  void
  IPCManager::add_observer(std::string_view peer)
  {
    m_daemon.add_observer(peer, [this, peer]
      {
        klog().e("Heartbeat timed out for {}", peer);
        const auto it = m_clients.find(peer);
        if (it == m_clients.end())
        {
          klog().w("IPC manager does not have an IPC worker for {}", peer);
          return;
        }

        klog().d("{} reconnecting to {}", peer, it->second->get_addr());

        static_cast<botbroker_handler*>(it->second)->reconnect();

        if (++m_timeouts > 1 && m_clients.size() > 1) // TODO: should depend on # of previously connected clients
        {
          klog().e("{} timeouts reached. Replacing back-end worker.", m_timeouts);

          evt::instance()(ALL_CLIENTS, SYSTEM_EVENTS__IPC_RECONNECT_REQUEST, {}); // Must be handled on original thread
          m_timeouts = 0;
        }

        std::thread{[this, peer]
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(5000));
          add_observer(peer);
        }}.detach();
      });
      m_daemon.validate(peer);
  }
  //---------------------------------------------------------------------
  void
  IPCManager::on_heartbeat(std::string_view peer)
  {
    if (!m_daemon.has_observer(peer))
      add_observer(peer);

    if (!m_daemon.validate(peer))
      klog().w("Couldn't validate heartbeat for {}", peer);
  }
  //----------------------------------------------------------------------
  void
  IPCManager::reconnect()
  {
    klog().w("IPC Reconnect request received. Destroying clients and workers");

    for (auto it = m_clients.begin(); it != m_clients.end();)
    {
      MessageHandlerInterface* handler_ptr = it->second;
      delete handler_ptr;
      it = m_clients.erase(it);
    }

    m_clients.clear();
    m_workers.clear();

    zmq::socket_t control{m_context, ZMQ_PUSH}; // Terminate proxy
    control.connect(CONTROL_ADDRESS);
    control.send(zmq::str_buffer("TERMINATE"));
    control.close();

    if (m_future.valid())
      m_future.wait();

    m_control_.close();
    m_backend_.close();
    m_public_. close();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    klog().w("Clients and workers destroyed. Restarting.");

    start();
  }

  //---------------------------------------------------------------------
  void
  IPCManager::run(bool must_reconnect)
  {
    static const auto limit = 10;
                 auto i     =  0;

    if (must_reconnect)
    {
      klog().d("IPCManager must shutdown, re-initialize and re-connect sockets");
      reconnect();
      return;
    }

    while (!m_queue.empty() && ++i < limit)
    {
      auto&& out = m_queue.front();
      if (m_daemon.has_observer(out.peer))
      {
        klog().t("Sending {} the following IPC message: {}", out.peer, to_string_max(out.msg->to_string(), 650));
        m_clients.at(out.peer)->send_ipc_message(std::move(out.msg));
        m_queue.pop_front();
      }
    }
  }
} // ns kiq
