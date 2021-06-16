#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

/**
 * TaskQueue
 *
 * A task queue employs a pool of worker threads who can execute arbitrary tasks
 * @class
 */
class TaskQueue {
 public:
 /**
 * @constructor
 */
  TaskQueue();
/**
 * @destructor
 */
  ~TaskQueue();

/** PUBLIC METHODS **/

/**
 * initialize
 *
 * To be called after an instance of TaskQueue is created.
 */
  void initialize();

/**
 * pushToQueue
 *
 * Add a task to the queue
 *
 * @method
 * @param[in] {std::function<void()>} A fully encapsulated template function
 * with its own internal state
 */
  void pushToQueue(std::function<void()> fn);

 private:

/** PRIVATE METHODS **/
/**
 * workerLoop
 *
 * The loop is the essential lifecycle of the worker
 * @method
 */
  void workerLoop();
/**
 * deployWorkers
 *
 * Procures workers and sets them into existing by executing the workerLoop
 * function
 * @method
 */
  void deployWorkers();
/**
 * detachThreads
 *
 * Allows threads to terminate.
 * @method
 * @cleanup
 */
  void joinThreads();

  std::queue<std::function<void()>> m_task_queue;
  std::vector<std::thread>          m_thread_pool;
  std::mutex                        m_mutex_lock;
  std::condition_variable           pool_condition;
  std::atomic<bool>                 accepting_tasks;
  bool                              m_active;
  int32_t                           m_num_threads;
};
