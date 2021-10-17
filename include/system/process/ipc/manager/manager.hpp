#pragma once

#include <thread>
#include <deque>
#include <condition_variable>
#include <mutex>

#include "log/logger.h"
#include "system/process/ipc/ipc.hpp"
#include "system/process/ipc/client/client.hpp"
#include "interface/worker_interface.hpp"
#include "server/types.hpp"

static const uint32_t KSERVER_IPC_DEFAULT_PORT{28473};

using SystemCallback_fn_ptr = std::function<void(int, std::vector<std::string>)>;

class IPCManager : public Worker {
using u_ipc_msg_ptr = ipc_message::u_ipc_msg_ptr;
public:

IPCManager(SystemCallback_fn_ptr system_event_fn)
: m_system_event_fn{system_event_fn},
  m_req_ready(true)
{}

void process(std::string message, int32_t fd) {
  KLOG("Received outgoing IPC request");

  if (m_clients.empty())
    m_clients.insert({ALL_CLIENTS, IPCClient{KSERVER_IPC_DEFAULT_PORT}});

  m_clients.at(ALL_CLIENTS).Enqueue(std::move(std::make_unique<kiq_message>(message)));

  return;
}

bool ReceiveEvent(int32_t event, const std::vector<std::string> args)
{
  KLOG("Processing IPC message for event {}", event);

  if (m_clients.empty())
    m_clients.insert({ALL_CLIENTS, IPCClient{KSERVER_IPC_DEFAULT_PORT}});

  if (event == SYSTEM_EVENTS__PLATFORM_POST_REQUESTED)
    m_clients.at(ALL_CLIENTS).Enqueue(std::move(std::make_unique<platform_message>(
      args.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_ID_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_USER_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_URL_INDEX),
      args.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX) == "y",
      args.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX)
    )));

  return true;
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
  if (!m_incoming_queue.empty())
  {
    std::deque<u_ipc_msg_ptr>::iterator it = m_incoming_queue.begin();

    while (it != m_incoming_queue.end())
    {
      std::vector<std::string> payload{};
      const uint8_t            message_type = it->get()->type();

      switch (message_type)
      {
        case (constants::IPC_PLATFORM_TYPE):
        {
          platform_message*        message = static_cast<platform_message*>(it->get());
          payload.reserve(7);
          payload.emplace_back(message->platform());
          payload.emplace_back(message->id());
          payload.emplace_back(message->user());
          payload.emplace_back(""); // time?
          payload.emplace_back(message->content());
          payload.emplace_back(message->urls());
          payload.emplace_back(std::to_string(message->repost()));
          payload.emplace_back(message->args());
          m_system_event_fn(SYSTEM_EVENTS__PLATFORM_NEW_POST, payload);
        }
        break;

        case (constants::IPC_PLATFORM_ERROR):
        {
          platform_error*     error_message = static_cast<platform_error*>(it->get());
          payload.resize(5);
          payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX) = error_message->name();
          payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX)       = error_message->id();
          payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX)     = error_message->user();
          payload.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX)    = error_message->error();

          m_system_event_fn(SYSTEM_EVENTS__PLATFORM_ERROR, payload);
        }
        break;
      }
      it = m_incoming_queue.erase(it);
    }
  }
}

private:
virtual void loop() override {
  while (m_is_running) {
    for (auto&&[fd, client] : m_clients) {
      try {
        const uint8_t mask = client.Poll();
        if (HasRequest(mask) && client.ReceiveIPCMessage(false))
        {
          client.ReplyIPC();
        }

        if (HasReply(mask))
        {
          client.ReceiveIPCMessage();
        }
      }
      catch (const std::exception& e)
      {
        ELOG("Caught IPC exception: {}\nResetting client socket", e.what());
        client.ResetSocket();
      }

      std::vector<u_ipc_msg_ptr> messages = client.GetMessages();

      m_incoming_queue.insert(
        m_incoming_queue.end(),
        std::make_move_iterator(messages.begin()),
        std::make_move_iterator(messages.end())
      );

      client.ProcessQueue();

      HandleClientMessages();
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
};
