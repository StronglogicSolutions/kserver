#ifndef __IPC_MANAGER_HPP__
#define __IPC_MANAGER_HPP__

#include <thread>
#include "log/logger.h"
#include "system/process/ipc/client/client.hpp"

class Worker {
 public:
  Worker()
  : m_is_running(false) {}

  virtual void start() {
    m_is_running = true;
    m_thread = std::thread(Worker::run, this);
  }

  static void run(void* worker) {
    static_cast<Worker*>(worker)->loop();
  }

  void stop() {
    m_is_running = false;
    if (m_thread.joinable()) {
      m_thread.join();
    }
  }

 bool        m_is_running;
 uint32_t    m_loops;

 protected:
  virtual void loop() = 0;
 private:
  std::thread m_thread;
};


class IPCManager : public Worker {
public:

IPCManager() {}

void process(std::string message, int32_t fd) {
  KLOG("Received processing request");

  if (m_clients.find(fd) == m_clients.end()) {
    m_clients.insert({fd, IPCClient{28473}});
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

#endif // __IPC_MANAGER_HPP__
