#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <codec/kmessage_generated.h>
#include <log/logger.h>
#include <stdlib.h>

#include <atomic>
#include <chrono>
#include <codec/util.hpp>
#include <condition_variable>
#include <config/config_parser.hpp>
#include <database/kdb.hpp>
#include <executor/executor.hpp>
#include <executor/scheduler.hpp>
#include <executor/task_handlers/instagram.hpp>
#include <executor/task_handlers/task.hpp>
#include <iostream>
#include <map>
#include <mutex>
#include <executor/task_handlers/task.hpp>
#include <executor/task_handlers/instagram.hpp>
#include <executor/task_handlers/generic.hpp>
#include <server/types.hpp>
#include <string>
#include <system/cron.hpp>
#include <thread>
#include <utility>
#include <vector>

#define OPERATION_SUCCESS "Operation succeeded"
#define OPERATION_FAIL "Operation failed"

namespace Request {

enum DevTest {
  Schedule = 1,
  ExecuteTask = 2
};

using namespace KData;
using namespace Executor;

flatbuffers::FlatBufferBuilder builder(1024);

/**
 * RequestHandler
 *
 * Handles incoming requests coming from the KY_GUI Application
 */
class RequestHandler {

 using EventCallbackFn  = std::function<void(std::string, int, std::string, int, bool)>;
 using SystemCallbackFn = std::function<void(int, int, std::vector<std::string>)>;
 using TaskCallbackFn   = std::function<void(int, std::vector<Task>)>;

 public:
  /**
   * RequestHandler()
   * @constructor
   *
   * Loads configuration and instantiates a DatabaseConfiguration object
   *
   */
  RequestHandler() : m_executor(nullptr), m_active(true) {}

  /**
   * @constructor
   *
   * The move constructor
   */
  RequestHandler(RequestHandler &&r)
      : m_executor(r.m_executor),
        m_scheduler(r.m_scheduler),
        m_active(r.m_active) {
    r.m_executor = nullptr;
  }

  /**
   * @constructor
   *
   * The copy constructor
   */
  RequestHandler(const RequestHandler &r)
      : m_executor(nullptr),  // We do not copy the Executor
        m_scheduler(nullptr),
        m_active(r.m_active) {}

  /**
   * @operator
   *
   * The copy assignment operator
   */
  RequestHandler &operator=(const RequestHandler &handler) {
    this->m_executor  = nullptr;
    this->m_scheduler = nullptr;
    return *this;
  }

  /**
   * @operator
   *
   * The move assignment operator
   */
  RequestHandler &operator=(RequestHandler &&handler) {
    if (&handler != this) {
      delete m_executor;
      delete m_scheduler;
      m_executor          = handler.m_executor;
      m_scheduler         = handler.m_scheduler;
      m_active            = handler.m_active;
      handler.m_executor  = nullptr;
      handler.m_scheduler = nullptr;
    }
    return *this;
  }

  /**
   * @destructor
   *
   * Deletes the process executor and ensures that the maintenance worker thread
   * completes
   */
  ~RequestHandler() {
    if (m_executor != nullptr) {
      delete m_executor;
      delete m_scheduler;
    }
    //    if (m_maintenance_worker.valid()) {
    if (m_maintenance_worker.joinable()) {
      KLOG("Waiting for maintenance worker to complete");
      m_maintenance_worker.join();
    }
  }

  /**
   * initialize
   *
   * Initializes the RequestHandler with callbacks for sending system events and
   * process execution results. Instantiates a ProcessExecutor and provides a
   * callback. Starts a thread to perform work on the maintenance loop
   *
   */
  void initialize(EventCallbackFn event_callback_fn,
                  SystemCallbackFn system_callback_fn,
                  TaskCallbackFn task_callback_fn) {
    m_executor = new Executor::ProcessExecutor();
    m_scheduler = getScheduler();
    m_executor->setEventCallback([this](std::string result,
                                        int mask,
                                        std::string request_id,
                                        int client_socket_fd,
                                        bool error) {
      onProcessComplete(result, mask, request_id, client_socket_fd, error);
    });
    m_system_callback_fn  = system_callback_fn;
    m_event_callback_fn   = event_callback_fn;
    m_task_callback_fn    = task_callback_fn;

    setHandlingData(false);
    // Begin maintenance loop to process scheduled tasks as they become ready
    m_maintenance_worker =
        std::thread(std::bind(&RequestHandler::maintenanceLoop, this));
    maintenance_loop_condition.notify_one();
    KLOG("Initialization complete");
  }

  void shutdown() {
    m_active = false;
  }

  /**
   * setHandlingData
   *
   * Sets the `is_handling` class member
   *
   * @param[in] {bool} is_handling    Whether the system is currently receiving packets of data
   */
  void setHandlingData(bool is_handling) {
    handling_data = is_handling;
    if (!is_handling) {
      maintenance_loop_condition.notify_one();
    }
  }

  /**
   * getScheduler
   *
   * @returns [out] {Scheduler*}  A pointer to a new instance of Scheduler
   */
  Scheduler::Scheduler* getScheduler() {
    return new Scheduler::Scheduler{[this](std::string id, int client_socket_fd,
                                           int event,
                                           std::vector<std::string> args) {
      onSchedulerEvent(id, client_socket_fd, event, args);
    }};
  }

  /**
   * maintenanceLoop
   *
   * Maintenance work performed on a separate thread. Includs checking for
   * scheduled tasks, invoking the process executor and delegating work to the
   * system cron.
   */
  void maintenanceLoop() {
    KLOG("Beginning maintenance loop");
    while (m_active) {
      std::unique_lock<std::mutex> lock(m_mutex);
      maintenance_loop_condition.wait(lock,
                                      [this]() { return !handling_data; });
      int client_socket_fd = -1;

      std::vector<Task> tasks = m_scheduler->fetchTasks();                // Normal tasks
      for (auto&& recurring_task : m_scheduler->fetchRecurringTasks()) {  // Recurring tasks
        tasks.emplace_back(recurring_task);
      }

      if (!tasks.empty()) {
        std::string scheduled_times{"Scheduled time(s): "};
        KLOG("Scheduled tasks found: {}", tasks.size());
        for (const auto &task : tasks) {
          auto formatted_time = TimeUtils::format_timestamp(task.datetime);
          scheduled_times.append(formatted_time);
          scheduled_times += " ";
          KLOG(
            "Task info: Time: {} - Mask: {}\n Args: {}\n {}\n. Excluded: "
            "Execution Flags",
            formatted_time, std::to_string(task.execution_mask),
            task.file ? "hasFile(s)" : "", task.envfile
          );
        }
        std::string tasks_message = std::to_string(tasks.size());
        tasks_message += " tasks need to be executed";
        m_system_callback_fn(client_socket_fd,                      // Client
                             SYSTEM_EVENTS__SCHEDULED_TASKS_READY,  // Event
                             {tasks_message, scheduled_times});     // Args

        auto it = m_tasks_map.find(client_socket_fd);
        if (it == m_tasks_map.end()) {
          m_tasks_map.insert(std::pair<int, std::vector<Task>>(
              client_socket_fd, tasks));
        } else {
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());
        }
        KLOG(
            "KServer has {} {} pending execution",
            m_tasks_map.at(client_socket_fd).size(),
            m_tasks_map.at(client_socket_fd).size() == 1 ? "task" : "task");
      } else {
        KLOG("No tasks ready for execution");
        m_system_callback_fn(client_socket_fd,                      // Client
          SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,                      // Events
          {"There are currently no tasks ready for execution"});    // Args
      }
      if (!m_tasks_map.empty()) {
        if (!handlePendingTasks()) {
          ELOG("ERROR handling pending tasks");
        }
      }
      System::Cron<System::SingleJob> cron{};
      std::string jobs = cron.listJobs();
      if (jobs.empty()) {
        KLOG("Cron - There are currently the following cron jobs: \n {}", jobs);
      }
      std::this_thread::sleep_for(std::chrono::seconds(30));
    }
  }

  /**
   * handlePendingTasks
   *
   * Iterates pending tasks and requests their execution
   */
  bool handlePendingTasks() {
    if (!m_tasks_map.empty()) {
      Executor::ProcessExecutor executor{};
      bool is_scheduled_task = true;
      executor.setEventCallback(
        [this, is_scheduled_task](std::string result, int mask,
                                  std::string id, int client_socket_fd,
                                  bool error) {
          onProcessComplete(result, mask, id, client_socket_fd, error,
                            is_scheduled_task);
        });

      std::vector<std::future<void>> futures{};
      futures.reserve(m_tasks_map.size() * m_tasks_map.begin()->second.size());
      for (const auto &client_tasks : m_tasks_map) {
        if (!client_tasks.second.empty()) {
          for (const auto &task : client_tasks.second) {
            futures.push_back(std::async(
              std::launch::deferred, &Executor::ProcessExecutor::executeTask,
              std::ref(executor), client_tasks.first, task));
          }
        }
      }
      for (auto &future : futures) {
        future.get();
      }
    }
    return true;
  }

  /**
   * \b SCHEDULE \b TASK
   *
   * \overload
   * @request
   *
   * Processes requests to schedule a task
   *
   * @param[in] {KOperation} `op` The operation requested
   * @param[in] {std::vector<std::string>} `argv` The task
   * @param[in] {int} `client_socket_fd` The client socket file descriptor
   * @param[in] {std::string> `uuid` The unique universal identifier to
   * distinguish the task
   */
  std::string operator()(KOperation op, std::vector<std::string> argv,
                         int client_socket_fd, std::string uuid) {
    if (op == "Schedule") {
      if (argv.empty()) {
        KLOG("Can't handle a task with no arguments");
        return "";
      }
      auto mask = argv.at(TaskIndexes::MASK);
      auto kdb = Database::KDB();

      KLOG("Handling schedule request for process matching mask {}", mask);

      QueryValues result =
          kdb.select("apps", {"name", "path"}, {{"mask", mask}});
      std::string name{};
      std::string path{};
      for (const auto &value : result) {
        if (value.first == "name") {
          name += value.second;
          continue;
        }
        if (value.first == "path") {
          path += value.second;
          continue;
        }
      }
      if (!path.empty() && !name.empty()) {
        Task task{};
        if (name == Name::INSTAGRAM) {
          KLOG("New Instagram Task requested");
          IGTaskHandler ig_task_handler{};
          ig_task_handler.prepareTask(argv, uuid, &task);
        } else { // assume Generic Task
          KLOG("New Generic Task requested");
          GenericTaskHandler generic_task_handler{};
          generic_task_handler.prepareTask(argv, uuid, &task);
        }
        if (&task != nullptr) {
          auto file_index = 0;
          for (const auto &file_info : task.files) {
            KLOG("task file: {}", file_info.first);
            std::vector<std::string> callback_args{file_info.first,
                                                   file_info.second, uuid};
            if (file_index == task.files.size() - 1) {
              callback_args.push_back("final file");
            }
            m_system_callback_fn(client_socket_fd, SYSTEM_EVENTS__FILE_UPDATE,
                                 callback_args);
          }
          if (task.validate()) {
            KLOG("Sending task request to Scheduler");
            auto id = m_scheduler->schedule(task);
            if (!id.empty()) {
              // Task was scheduled. Prepare a vector with info about the task.
              std::vector<std::string> callback_args{};
              callback_args.reserve((5 + task.files.size()));
              callback_args.insert(callback_args.end(), {
                uuid, id,                            // UUID and database ID
                std::to_string(task.execution_mask), // Application mask
                FileUtils::readEnvFile(task.envfile),// Environment file
                std::to_string(task.files.size())    // File number
              });
              for (auto&& file : task.files) {
                callback_args.emplace_back(file.first); // Add the filenames
              }
              m_system_callback_fn(
                  client_socket_fd, SYSTEM_EVENTS__SCHEDULER_SUCCESS,
                  callback_args
                );
              return OPERATION_SUCCESS;
            } else {
              KLOG("Task with UUID {} was validated, but scheduling failed", uuid);
              return OPERATION_FAIL;
            }
          } else {
            KLOG("Task with UUID {} was processed, but did not pass validation", uuid);
            return OPERATION_FAIL;
          }
        }
      }
      KLOG("Task scheduling failed: Unable to find an application matching mask {}", mask);
    }
    return OPERATION_FAIL;
  }

  /**
   * \b EXECUTE \b SCHEDULED \b TASK
   *
   * \overload
   *
   * @request
   * @development
   *
   * Calls the scheduler to fetch tasks which need to be executed soon and
   * returns them to the KServer. These tasks can be performed iteratively on a
   * message loop, to ensure each execution completes before another one begins.
   *
   *
   * @param[in] {int} `client_socket_fd` The client socket file descriptor
   * @param[in] {KOperation} `op` The operation requested
   * @param[in] {DevTest} `test` An enum value representing the type of
   * operation to perform
   *
   */
  void operator()(int client_socket_fd, KOperation op, DevTest test) {
    if (strcmp(op.c_str(), "Test") == 0 && test == DevTest::Schedule) {
      std::vector<Task> tasks = m_scheduler->fetchTasks();
      if (!tasks.empty()) {
        KLOG("There are tasks to be reviewed");
        for (const auto &task : tasks) {
          KLOG("Task info: {} - Mask: {}\n "
              "Args: {}\n {}\n. Excluded: Execution "
              "Flags",
              task.datetime, std::to_string(task.execution_mask),
              task.file ? "hasFile(s)" : "", task.envfile);
        }
        std::string tasks_message = std::to_string(tasks.size());
        tasks_message += " tasks scheduled to run in the next 24 hours";
        m_system_callback_fn(client_socket_fd,
                             SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
                             {tasks_message});
        // We need to inform the client of available tasks, and let them decide
        // if a task should be executed.
        m_executor->executeTask(client_socket_fd, tasks.at(0));
        auto it = m_tasks_map.find(client_socket_fd);
        if (it == m_tasks_map.end()) {
          m_tasks_map.insert(std::pair<int, std::vector<Task>>(
              client_socket_fd, tasks));
        } else {
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());
        }
        KLOG("{} currently has {} tasks pending execution",
            client_socket_fd, m_tasks_map.at(client_socket_fd).size());
      } else {
        KLOG("There are currently no tasks ready for execution");

        m_system_callback_fn(
            client_socket_fd, SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,
            {"There are currently no tasks ready for execution"});
      }
    }
  }

  /**
   * \b FETCH \b PROCESSES
   *
   * \overload
   *
   * @request
   *
   * Fetches the available applications that can be requested for execution by
   * clients using KY_GUI
   *
   * @param[in] {KOperation} `op` The operation requested
   *
   */
  std::map<int, std::string> operator()(KOperation op) {
    auto kdb = Database::KDB{};
    QueryValues result =
      kdb.select(
        "apps",                             // Table
        {"name", "path", "data", "mask"},   // Fields
        QueryFilter{}
      );
    std::map<int, std::string> command_map{};
    std::vector<std::string> names{};
    std::vector<int> masks{};
    for (const auto& row : result) {
      if (row.first == "name") {
        names.push_back(row.second);
      } else if (row.first == "mask") {
        masks.push_back(stoi(row.second));
      }
    }
    if (masks.size() == names.size()) {
      for (int i = 0; i < masks.size(); i++) {
        command_map.emplace(masks.at(i), names.at(i));
      }
    }
    return command_map;
  }
  /**
   * \b EXECUTE \b PROCESS
   *
   * \overload
   *
   * @request
   *
   * Calls on the ProcessExecutor and requests that it execute an application
   * whose mask value matches those contained within the value passed as a
   * parameter
   *
   * @param[in] {uint32_t} `mask` The requested mask
   * @param[in] {int} `client_socket_fd` The file descriptor of the client
   * making the request
   */
  void operator()(uint32_t mask, std::string request_id, int client_socket_fd) {
    auto kdb = Database::KDB{};
    QueryValues result = kdb.select(    // Select
      "apps", {"path"}, QueryFilter{    // Path from apps
        {"mask", std::to_string(mask)}  // Filter by mask
      }
    );

    for (const auto &row : result) {
      m_executor->request(row.second, mask, client_socket_fd, request_id, {},
                          Executor::ExecutionRequestType::IMMEDIATE);
      std::string info_string{
          "PROCESS RUNNER - Process execution requested for "
          "applications matching the mask "};
      info_string += std::to_string(mask);
      m_system_callback_fn(client_socket_fd,
                           SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
                           {info_string, request_id});
    }
  }

 private:
  /**
   * onProcessComplete
   *
   * The callback function called by the ProcessExecutor after completing a
   * process
   *
   * @param[in] <std::string> `value`      The stdout from value from the
   * executed process
   * @param[in] <int> `mask`               The bitmask associated with the
   * process
   * @param[in] <std::string> `id`         The request ID for the process
   * @param[in] <int> `client_socket_fd`   The file descriptor for the client
   * who made the request
   *
   */
  void onProcessComplete(std::string value, int mask, std::string id,
                         int client_socket_fd, bool error,
                         bool scheduled_task = false) {
    KLOG("Process complete notification for client {}'s request {}",
        client_socket_fd, id);
    m_event_callback_fn( // Inform system of process result
      value,
      mask,
      id,
      client_socket_fd,
      error
    );
    if (scheduled_task) { // If it was a scheduled task, we need to update task map held in memory
      std::vector<Task>::iterator task_it; // Declare task iterator
      KLOG("Task complete notification for client {}'s task {}{}",
        client_socket_fd, id, error ? "\nERROR WAS RETURNED" : ""
      );

      std::map<int, std::vector<Task>>::iterator it = // Find map iterator for client ID
        m_tasks_map.find(client_socket_fd);
      if (it != m_tasks_map.end()) { // Found client's tasks
        task_it = std::find_if(      // Find specific task
          it->second.begin(), it->second.end(),
          [id](Task task) {
            return task.id == std::stoi(id);
          }
        );
        if (task_it != it->second.end()) {
          uint8_t status{Scheduler::Completed::SUCCESS};

          // Error handling
          if (error) { // Email if it failed
            SystemUtils::sendMail(
              ConfigParser::Email::notification(),
              std::string{Scheduler::Messages::TASK_ERROR_EMAIL + value},
              ConfigParser::Email::admin()
            );
            status = task_it->completed == Scheduler::Completed::FAILED ? // If failed previously
              Scheduler::Completed::RETRY_FAIL : // Retry failed
              Scheduler::Completed::FAILED;      // Allow retry
            KLOG("Sending email to administrator about failed task.\nNew "
                "Status: {}",
                Scheduler::Completed::STRINGS[status]);
          }
          // Update status
          task_it->completed = status;
          m_scheduler->updateStatus(&*task_it); // Failed tasks will re-run once more

          if (!error && task_it->recurring) { // Only update if no error occurred
            task_it->datetime = std::to_string(TimeUtils::unixtime());
            m_scheduler->updateRecurring(&*task_it); // Latest time
            KLOG("Task {} was a recurring task scheduled to run {}",
              task_it->id,
              Executor::Constants::Recurring::names[task_it->recurring]
            );
          }

          if (task_it->notify) { // Task notification enabled - send result by email
            KLOG("Task notification enabled - emailing result to administrator");
            std::string email_string{};
            email_string.reserve(value.size() + 84);
            email_string += task_it->toString();
            email_string += error ? "\nError" : "\n";
            email_string += value;

            SystemUtils::sendMail(
              ConfigParser::Email::notification(),
              email_string,
              ConfigParser::Email::admin()
            );
          }
          // Remove task - failed tasks will be fetched once more by the maintenance worker
          KLOG("removing completed task from memory");
          it->second.erase(task_it);
        }
      }
    }
  }
  /**
   * onScheduledTaskComplete
   *
   * @request
   *
   * The callback function called by the Executor::Scheduler after a scheduled
   * task completes
   *
   * @param[in] <std::string> `value`             The stdout from value from the
   * executed process
   * @param[in] <int>         `mask`              The bitmask associated with
   * the process
   * @param[in] <std::string> `id`                The task ID as it is tracked
   * in the DB
   * @param[in] <int>         `event`             The type of event
   *
   * @param[in] <int>         `client_socket_fd`  The file descriptor of the
   * client requesting the task
   *
   * TODO: We need to move away from sending process execution results via the
   * scheduler's callback, and only use this to inform of scheduling events.
   * Process execution results should come from the ProcessExecutor and its
   * respective callback.
   */

  void onSchedulerEvent(std::string id,
                        int client_socket_fd,
                        int event,
                        std::vector<std::string> args = {}) {
    m_system_callback_fn(client_socket_fd, event, args);
  }

  // Callbacks
  EventCallbackFn                   m_event_callback_fn;
  SystemCallbackFn                  m_system_callback_fn;
  TaskCallbackFn                    m_task_callback_fn;
  // Data
  std::map<int, std::vector<Task>>  m_tasks_map;
  std::mutex                        m_mutex;
  std::condition_variable           maintenance_loop_condition;
  std::atomic<bool>                 handling_data;
  bool                              m_active;
  // Workers
  Executor::ProcessExecutor*        m_executor;
  Scheduler::Scheduler*             m_scheduler;
  std::thread                       m_maintenance_worker;
};
}  // namespace Request
#endif  // __REQUEST_HANDLER_HPP__
