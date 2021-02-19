#pragma once

#include <thread>
#include <deque>
#include "log/logger.h"
#include "system/process/ipc/ipc.hpp"
#include "system/process/ipc/client/client.hpp"
#include "interface/worker_interface.hpp"
#include "server/types.hpp"

static const uint32_t KSERVER_IPC_DEFAULT_PORT{28473};

using SystemCallback_fn_ptr = std::function<void(int, std::vector<std::string>)>;

class IPCManager : public Worker {
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
  while (!m_incoming_queue.empty())
  {
    ipc_message message = m_incoming_queue.front();
    // TODO: process
    if (!message.data().empty())
    {
      std::vector<std::string> payload{message.string()};
      m_system_event_fn(SYSTEM_EVENTS__PLATFORM_NEW_POST, payload);
    }
    m_incoming_queue.pop_front();
  }
}

private:
virtual void loop() override {
  while (m_is_running) {
    for (auto&&[fd, client] : m_clients) {
      if (client.Poll() && client.ReceiveMessage())
      {
        client.ProcessMessage();
        std::vector<ipc_message> messages = client.GetMessages();
        m_incoming_queue.insert(
          m_incoming_queue.end(),
          std::make_move_iterator(messages.begin()),
          std::make_move_iterator(messages.end())
        );
      }

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  }
}

std::unordered_map<int32_t, IPCClient> m_clients;
SystemCallback_fn_ptr                  m_system_event_fn;
std::deque<ipc_message>                m_incoming_queue;

};
