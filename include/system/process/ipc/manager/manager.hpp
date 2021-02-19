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
: m_system_event_fn{system_event_fn}
{}

void process(std::string message, int32_t fd) {
  KLOG("Received outgoing IPC request");

  if (m_clients.find(fd) == m_clients.end()) {
    m_clients.insert({fd, IPCClient{KSERVER_IPC_DEFAULT_PORT}});
  }

  m_clients.at(fd).SendMessage(message);

  return;
}

bool ReceiveEvent(int32_t event, const std::vector<std::string> args)
{
  return false;
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
      if (it->get()->type() == constants::IPC_PLATFORM_TYPE)
      {
        platform_message* message = static_cast<platform_message*>(it->get());

        std::vector<std::string> payload{};
        payload.reserve(4);
        payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX)   = message->name();
        payload.at(constants::PLATFORM_PAYLOAD_O_PLATFORM_INDEX) = "";
        payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX)         = message->id();
        payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX)       = "";
        payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX)    = message->content();
        payload.at(constants::PLATFORM_PAYLOAD_URL_INDEX)        = message->urls();

        m_system_event_fn(SYSTEM_EVENTS__PLATFORM_NEW_POST, payload);

        it = m_incoming_queue.erase(it);
      }
    }
  }
}

private:
virtual void loop() override {
  while (m_is_running) {
    for (auto&&[fd, client] : m_clients) {
      if (client.Poll() && client.ReceiveIPCMessage())
      {
        client.ProcessMessage();

        std::vector<u_ipc_msg_ptr> messages = client.GetMessages();

        m_incoming_queue.insert(
          m_incoming_queue.end(),
          std::make_move_iterator(messages.begin()),
          std::make_move_iterator(messages.end())
        );
      }
    }

    std::unique_lock<std::mutex> lock{m_mutex};
    m_condition.wait_for(lock, std::chrono::milliseconds(10000));

  }
}

std::unordered_map<int32_t, IPCClient> m_clients;
SystemCallback_fn_ptr                  m_system_event_fn;
std::deque<u_ipc_msg_ptr>              m_incoming_queue;
std::mutex                             m_mutex;
std::condition_variable                m_condition;
};
