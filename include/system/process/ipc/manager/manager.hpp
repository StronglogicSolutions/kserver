#ifndef __IPC_MANAGER_HPP__
#define __IPC_MANAGER_HPP__

#include <thread>
#include <log/logger.h>
#include <system/process/ipc/client/client.hpp>

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

void process(std::string message) {
  KLOG("Received processing request");

  if (m_clients.empty()) {
    m_clients.push_back(IPCClient{28473});
  }

  m_clients.front().SendMessage(message);

  return;
}

private:

virtual void loop() override {
  while (m_is_running) {
    for (IPCClient& client : m_clients) {
      if (client.Poll()) {
        if (client.ReceiveMessage()) {
          client.ProcessMessage();
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  }
}

std::vector<IPCClient> m_clients;

};

#endif // __IPC_MANAGER_HPP__
