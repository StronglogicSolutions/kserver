#include <task/task_queue.hpp>

#include <iostream>

/**
 * @global
 * {int} num_threads A rough estimate as to the number of threads we can run
 * concurrently
 */
int num_threads = std::thread::hardware_concurrency();

/**
 * @constructor
 * Nothing fancy
 */
TaskQueue::TaskQueue()
  : m_active(true) {}
/**
 * @destructor
 * Make sure all worker threads detach or join before destroying TaskQueue
 * instance
 */
TaskQueue::~TaskQueue() {
  m_active = false;
  detachThreads();
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
void TaskQueue::pushToQueue(std::function<void()> fn) {
  std::unique_lock<std::mutex> lock(m_mutex_lock);  // obtain mutex
  m_task_queue.push(fn);                            // add work to queue
  lock.unlock();
  pool_condition.notify_one();  // one worker can begin waiting to perform work
}

/**
 * workerLoop
 *
 * The loop is the essential lifecycle of the worker
 * @method
 */
void TaskQueue::workerLoop() {
  std::function<void()> fn;
  while (m_active) {
    {  // encapsulate atomic management of queue
      std::unique_lock<std::mutex> lock(m_mutex_lock);  // obtain mutex
      pool_condition
          .wait(  // condition: not accepting tasks or queue is not empty
              lock,
              [this]() { return !accepting_tasks || !m_task_queue.empty() || !m_active; });
      std::cout << "Wait condition met" << std::endl;
      if (!m_active) {
        // Destructor was called - exit
        break;
      }
      if (!accepting_tasks && m_task_queue.empty()) {
        // If the queue is empty, it's safe to begin accepting tasks
        accepting_tasks = true;
        continue;
      }
      std::cout << "Taking task" << std::endl;
      fn = m_task_queue.front();  // obtain task from FIFO container
      m_task_queue.pop();
      accepting_tasks = true;  // begin accepting before lock expires
    }                          // queue management complete (lock expires)
    fn();                      // work
  }
}

/**
 * deployWorkers
 *
 * Procures workers and sets them into existing by executing the workerLoop
 * function
 * @method
 */
void TaskQueue::deployWorkers() {
  for (int i = 0; i < (num_threads - 1); i++) {
    m_thread_pool.push_back(std::thread([this]() { workerLoop(); }));
  }
  // TODO: mutex may not be necessary, as accepting_tasks is atomic
  std::unique_lock<std::mutex> lock(m_mutex_lock);  // obtain mutex
  accepting_tasks = false;  // allow pool wait condition to be met
  lock.unlock();
  // when we send the notification immediately, the consumer will try to get the
  // lock , so unlock asap
  pool_condition.notify_all();
}  // lock expires

/**
 * initialize
 *
 * To be called after an instance of TaskQueue is created.
 * @method
 */
void TaskQueue::initialize() { deployWorkers(); }

/**
 * detachThreads
 * TODO: change to "joinThreads" or "finishThreads"
 * Allows threads to terminate.
 * @method
 * @cleanup
 */
void TaskQueue::detachThreads() {
  pool_condition.notify_all();
  for (std::thread& t : m_thread_pool) {
    if (t.joinable()) {
      t.join();
    }
  }
}

