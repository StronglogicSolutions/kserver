#pragma once

#include <thread>

class Worker {
public:
  Worker()
  : m_is_running(false) {}

  virtual void start()
  {
    m_is_running = true;
    m_thread = std::thread(Worker::run, this);
  }

  static void run(void* worker)
  {
    static_cast<Worker*>(worker)->loop();
  }

  void stop()
  {
    m_is_running = false;
    if (m_thread.joinable())
      m_thread.join();
  }

 bool        m_is_running;
 uint32_t    m_loops;

 protected:
  virtual void loop() = 0;
 private:
  std::thread m_thread;
};
