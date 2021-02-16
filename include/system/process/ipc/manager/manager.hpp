#pragma once

#include <thread>
#include "log/logger.h"
#include "system/process/ipc/client/client.hpp"
#include "interface/worker_interface.hpp"

static const uint32_t KSERVER_IPC_DEFAULT_PORT{28473};

class IPCManager : public Worker {
public:

IPCManager() {}

void process(std::string message, int32_t fd) {
  KLOG("Received outgoing IPC request");

  if (m_clients.find(fd) == m_clients.end()) {
    m_clients.insert({fd, IPCClient{KSERVER_IPC_DEFAULT_PORT}});
  }

  m_clients.at(fd).SendMessage(message);

  return;
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

private:
virtual void loop() override {
  while (m_is_running) {
    for (auto&&[fd, client] : m_clients) {
      if (client.Poll() && client.ReceiveMessage())
        client.ProcessMessage();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  }
}

std::unordered_map<int32_t, IPCClient> m_clients;

};
