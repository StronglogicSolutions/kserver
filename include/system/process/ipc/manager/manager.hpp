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
using u_ipc_msg_ptr         = ipc_message::u_ipc_msg_ptr;
using SystemCallback_fn_ptr = std::function<void(int32_t, const std::vector<std::string>&)>;
using SystemDispatch_t      = std::function<void(std::deque<u_ipc_msg_ptr>::const_iterator)>;
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
      args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX),       args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_USER_INDEX),           args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_URL_INDEX),            args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
      std::stoi(args.at(constants::PLATFORM_PAYLOAD_CMD_INDEX)), args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX));
  };

  KLOG("Processing IPC message for event {}", event);
  bool received{true};

  if (event == SYSTEM_EVENTS__PLATFORM_POST_REQUESTED || event == SYSTEM_EVENTS__PLATFORM_EVENT)
    m_clients.at(ALL_CLIENTS).Enqueue(std::move(Deserialize(args)));
  else
    received = false;

  return received;
}

void close(int32_t fd)
{
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
  using ipc_msg_it_t = std::deque<u_ipc_msg_ptr>::iterator;
  using Payload      = std::vector<std::string>;

  auto GetPayload = [](auto m) { return Payload{m->platform(), m->id(),   m->user(), "", m->content(), m->urls(), std::to_string(m->repost()), m->args()}; };
  auto GetInfo    = [](auto m) { return Payload{m->platform(), m->type(), m->info()};                                                                      };
  auto GetError   = [](auto m) { return Payload{m->name(),     m->id(),   m->user(), m->error(), ""};                                                      };
  auto GetRequest = [](auto m) { return Payload{m->platform(), m->id(),   m->user(), m->content(), m->args()};                                             };

  std::map<uint8_t, SystemDispatch_t> dispatch_table{
  {::constants::IPC_PLATFORM_TYPE   , [&, this](auto it) { m_system_event_fn(::constants::IPC_PLATFORM_TYPE,    GetPayload(static_cast<platform_message*>(it->get()))); }},
  {::constants::IPC_PLATFORM_REQUEST, [&, this](auto it) { m_system_event_fn(::constants::IPC_PLATFORM_REQUEST, GetRequest(static_cast<platform_request*>(it->get()))); }},
  {::constants::IPC_PLATFORM_INFO   , [&, this](auto it) { m_system_event_fn(::constants::IPC_PLATFORM_INFO,    GetInfo   (static_cast<platform_info*>   (it->get()))); }},
  {::constants::IPC_PLATFORM_ERROR  , [&, this](auto it) { m_system_event_fn(::constants::IPC_PLATFORM_ERROR,   GetError  (static_cast<platform_error*>  (it->get()))); }},
  {::constants::IPC_KEEPALIVE_TYPE  , [&, this](auto it) { m_daemon.reset();                                                                                            }},
  {::constants::IPC_OK_TYPE         , [&, this](auto it) { NOOP();                                                                                                      }}};

  for (ipc_msg_it_t it = m_incoming_queue.begin(); it != m_incoming_queue.end();)
  {
    dispatch_table[it->get()->type()](it);
    it = m_incoming_queue.erase(it);
  }
}

private:
virtual void loop() override
{
  uint8_t mask;
  while (m_is_running)
  {
    for (auto&& [fd, client] : m_clients)
    {
      try
      {
        {
          std::unique_lock<std::mutex> lock{m_mutex};
          mask = client.Poll();
        }

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
      m_clients.at(ALL_CLIENTS).ResetSocket(false);
      m_clients.at(ALL_CLIENTS).KeepAlive();
    }
  }
}

std::unordered_map<int32_t, IPCClient> m_clients;
SystemCallback_fn_ptr                  m_system_event_fn;
std::deque<u_ipc_msg_ptr>              m_incoming_queue;
std::mutex                             m_mutex;
bool                                   m_req_ready;
session_daemon                         m_daemon;
};
} // ns kiq
