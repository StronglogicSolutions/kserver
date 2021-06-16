#include <task/task_queue.hpp>

#include <iostream>

/**
 * @constructor
 * Nothing fancy
 */
TaskQueue::TaskQueue()
: m_active(true),
  m_num_threads(std::thread::hardware_concurrency() / 2) {}

/**
 * @destructor
 * Make sure all worker threads detach or join before destroying TaskQueue
 * instance
 */
TaskQueue::~TaskQueue()
{
  m_active = false;
  joinThreads();
}

/**
 * pushToQueue
 *
 * Add a task to the queue
 *
 * @method
 * @param[in] {std::function<void()>} A fully encapsulated template function
 * with its own internal state
 */
void TaskQueue::pushToQueue(std::function<void()> fn)
{
  std::unique_lock<std::mutex> lock(m_mutex_lock);  // obtain mutex
  m_task_queue.push(fn);                            // add work to queue
  lock.unlock();
  pool_condition.notify_one();                      // one worker can wait for work
}

/**
 * workerLoop
 *
 * The loop is the essential lifecycle of the worker
 * @method
 */
void TaskQueue::workerLoop()
{
  std::function<void()> fn;
  while (m_active)
  {
    {
      std::unique_lock<std::mutex> lock(m_mutex_lock);
      pool_condition.wait(lock,
        [this]() { return !accepting_tasks || !m_task_queue.empty() || !m_active; });

      if (!m_active)
        break;                                           // Destructor called, exit

      if (!accepting_tasks && m_task_queue.empty())
      {
        accepting_tasks.store(true);                     // Queue empty ∴ safe accept tasks
        continue;
      }

      fn = m_task_queue.front(); // Take work
      m_task_queue.pop();
      accepting_tasks.store(true);
    }
    fn();                                            // Work
  }
}

/**
 * deployWorkers
 *
 * Procures workers and sets them into existing by executing the workerLoop
 * function
 * @method
 */
void TaskQueue::deployWorkers()
{
  for (uint8_t i = 0; i < m_num_threads; i++)
    m_thread_pool.push_back(std::thread{&TaskQueue::workerLoop, this});

  std::unique_lock<std::mutex> lock{m_mutex_lock};
  accepting_tasks.store(false);
  lock.unlock();
  pool_condition.notify_all();
}

/**
 * initialize
 *
 * To be called after an instance of TaskQueue is created.
 * @method
 */
void TaskQueue::initialize()
{
  if (m_num_threads < 2)
    m_num_threads = 2;
  deployWorkers();
}

/**
 * detachThreads
 * Allows threads to terminate.
 * @method
 * @cleanup
 */
void TaskQueue::joinThreads()
{
  pool_condition.notify_all();
  for (std::thread& t : m_thread_pool) {
    if (t.joinable()) {
      t.join();
    }
  }
}

