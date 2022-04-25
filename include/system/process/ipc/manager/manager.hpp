#pragma once

#include <thread>
#include <deque>
#include <condition_variable>
#include <mutex>

#include "log/logger.h"
#include "interface/worker_interface.hpp"
#include "server/types.hpp"
#include "system/process/ipc/ipc.hpp"
#include "system/process/ipc/client/client.hpp"

namespace kiq {
using SystemCallback_fn_ptr = std::function<void(int32_t, const std::vector<std::string>&)>;
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
static const uint32_t DEFAULT_PORT{static_cast<uint32_t>(std::stoul(config::Process::ipc_port()))};

static auto NOOP = [] { (void)(0); };

class IPCManager : public Worker {
public:

IPCManager(SystemCallback_fn_ptr system_event_fn)
: m_system_event_fn(system_event_fn),
  m_req_ready(true)
{
  m_clients.insert(std::pair<int32_t, IPCClient>{ALL_CLIENTS, IPCClient{DEFAULT_PORT}});
  m_clients.at(ALL_CLIENTS).KeepAlive();
}

void process(std::string message, int32_t fd)
{
  KLOG("Received outgoing IPC request");
  m_clients.at(ALL_CLIENTS).Enqueue(std::move(std::make_unique<kiq_message>(message)));
}

bool ReceiveEvent(int32_t event, const std::vector<std::string> args)
{
  auto Deserialize = [](auto args)
  {
    return std::make_unique<platform_message>(
      args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_USER_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_URL_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
      std::stoi(args.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)),
      args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX));
  };

  KLOG("Processing IPC message for event {}", event);
  bool received{true};

  if (event == SYSTEM_EVENTS__PLATFORM_POST_REQUESTED ||
      event == SYSTEM_EVENTS__PLATFORM_EVENT)
    m_clients.at(ALL_CLIENTS).Enqueue(std::move(Deserialize(args)));
  else
    received = false;

  return received;
}

void close(int32_t fd) {
  std::unordered_map<int32_t, IPCClient>::iterator it = m_clients.find(fd);

  if (it != m_clients.end())
  {
    IPCClient& client = it->second;
    client.Shutdown();
    m_clients.erase(it);
  }
}

void HandleClientMessages()
{
  using Payload = std::vector<std::string>;
  auto GetPayload = [](platform_message* message) -> Payload
  {
    return Payload{message->platform(), message->id(),   message->user(), "",
                   message->content(),  message->urls(), std::to_string(message->repost()), message->args()};
  };

  auto GetError = [](platform_error* message)     -> Payload
  {
    return Payload{message->name(), message->id(), message->user(), message->error(), ""};
  };

  auto GetRequest = [](platform_request* message) -> Payload
  {
    return Payload{message->platform(), message->id(),   message->user(),
                   message->content(),  message->args()};
  };
  auto GetInfo = [](platform_info* message)       -> Payload
  {
    return Payload{message->platform(), message->type(), message->info()};
  };

  if (m_incoming_queue.size())
  {
    std::deque<u_ipc_msg_ptr>::iterator it = m_incoming_queue.begin();
    while (it != m_incoming_queue.end())
    {
      std::vector<std::string> payload{};
      const uint8_t            message_type = it->get()->type();
      switch (message_type)
      {
        case (::constants::IPC_PLATFORM_TYPE):
          m_system_event_fn(SYSTEM_EVENTS__PLATFORM_NEW_POST, GetPayload(static_cast<platform_message*>(it->get())));
        break;
        case (::constants::IPC_PLATFORM_REQUEST):
          m_system_event_fn(SYSTEM_EVENTS__PLATFORM_REQUEST, GetRequest(static_cast<platform_request*>(it->get())));
        break;
        case (::constants::IPC_PLATFORM_INFO):
          m_system_event_fn(SYSTEM_EVENTS__PLATFORM_INFO, GetInfo(static_cast<platform_info*>(it->get())));
        break;
        case (::constants::IPC_PLATFORM_ERROR):
          m_system_event_fn(SYSTEM_EVENTS__PLATFORM_ERROR, GetError(static_cast<platform_error*>(it->get())));
        break;
        break;
        case (::constants::IPC_KEEPALIVE_TYPE):
          m_daemon.reset();
        break;
        case (::constants::IPC_OK_TYPE):
          NOOP();
        break;
        default:
          ELOG("Failed to handle unknown IPC message");
      }
      it = m_incoming_queue.erase(it);
    }
  }
}

private:
virtual void loop() override
{
  while (m_is_running)
  {
    for (auto&& [fd, client] : m_clients)
    {
      try
      {
        const uint8_t mask = client.Poll();

        if (HasRequest(mask) && client.ReceiveIPCMessage(false))
          client.ReplyIPC();

        if (HasReply(mask))
          client.ReceiveIPCMessage();
      }
      catch (const std::exception& e)
      {
        ELOG("Caught IPC exception: {}\nResetting client socket", e.what());
        client.ResetSocket(false);
      }

      std::vector<u_ipc_msg_ptr> messages = client.GetMessages();

      m_incoming_queue.insert(m_incoming_queue.end(), std::make_move_iterator(messages.begin()),
                                                      std::make_move_iterator(messages.end())  );
      if (client.HasOutbound() && !m_daemon.active())
        m_daemon.reset();

      client.ProcessQueue();
    }

    HandleClientMessages();

    if (!m_daemon.validate())
    {
      m_daemon.stop();
      ELOG("IPC session is unreliable");
      m_clients.at(ALL_CLIENTS).ResetSocket();
      m_clients.at(ALL_CLIENTS).KeepAlive();
    }

    std::unique_lock<std::mutex> lock{m_mutex};
    m_condition.wait_for(lock, std::chrono::milliseconds(300));
  }
}

std::unordered_map<int32_t, IPCClient> m_clients;
SystemCallback_fn_ptr                  m_system_event_fn;
std::deque<u_ipc_msg_ptr>              m_incoming_queue;
std::mutex                             m_mutex;
std::condition_variable                m_condition;
bool                                   m_req_ready;
session_daemon                         m_daemon;
};
} // ns kiq
