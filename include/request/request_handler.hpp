#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <codec/kmessage_generated.h>
#include <database/DatabaseConnection.h>
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
#include <iostream>
#include <map>
#include <mutex>
#include <request/task_handlers/instagram.hpp>
#include <server/types.hpp>
#include <string>
#include <system/cron.hpp>
#include <thread>
#include <utility>
#include <vector>

namespace Request {

enum DevTest { Schedule = 1, ExecuteTask = 2 };

using namespace KData;

flatbuffers::FlatBufferBuilder builder(1024);

auto KLOG = KLogger::GetInstance() -> get_logger();

/**
 * RequestHandler
 *
 * Handles incoming requests coming from the KY_GUI Application
 */
class RequestHandler {
 public:
  /**
   * RequestHandler()
   * @constructor
   *
   * Loads configuration and instantiates a DatabaseConfiguration object
   *
   */
  RequestHandler() : m_executor(nullptr) {
    if (!ConfigParser::initConfig()) {
      KLOG->info("RequestHandler::RequestHandler() - Unable to load config");
      return;
    }

    m_credentials = {.user = ConfigParser::getDBUser(),
                     .password = ConfigParser::getDBPass(),
                     .name = ConfigParser::getDBName()};

    DatabaseConfiguration configuration{
        .credentials = m_credentials, .address = "127.0.0.1", .port = "5432"};

    m_connection = DatabaseConnection{};
    m_connection.setConfig(configuration);
    KLOG->info("RequestHandler::RequestHandler - set database credentials");
  }

  /**
   * @constructor
   *
   * The move constructor
   */
  RequestHandler(RequestHandler &&r)
      : m_executor(r.m_executor),
        m_scheduler(r.m_scheduler),
        m_connection(r.m_connection),
        m_credentials(r.m_credentials) {
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
        m_connection(r.m_connection),
        m_credentials(r.m_credentials) {}

  /**
   * @operator
   *
   * The copy assignment operator
   */
  RequestHandler &operator=(const RequestHandler &handler) {
    this->m_executor = nullptr;
    this->m_scheduler = nullptr;
    this->m_connection = handler.m_connection;
    this->m_credentials = handler.m_credentials;
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
      m_executor = handler.m_executor;
      m_scheduler = handler.m_scheduler;
      handler.m_executor = nullptr;
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
      KLOG->info(
          "RequestHandler::~RequestHandler() - Waiting for maintenance worker "
          "to complete");
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
  void initialize(
      std::function<void(std::string, int, std::string, int)> event_callback_fn,
      std::function<void(int, int, std::vector<std::string>)>
          system_callback_fn,
      std::function<void(int, std::vector<Scheduler::Task>)> task_callback_fn) {
    m_executor = new Executor::ProcessExecutor();
    m_scheduler = getScheduler();
    m_executor->setEventCallback([this](std::string result, int mask,
                                        std::string request_id,
                                        int client_socket_fd) {
      onProcessComplete(result, mask, request_id, client_socket_fd);
    });
    m_system_callback_fn = system_callback_fn;
    m_event_callback_fn = event_callback_fn;
    m_task_callback_fn = task_callback_fn;

    // Begin maintenance loop to process scheduled tasks as they become ready
    m_maintenance_worker =
        std::thread(std::bind(&RequestHandler::maintenanceLoop, this));
    maintenance_loop_condition.notify_one();
    KLOG->info("RequestHandler::initialize() - Initialization complete");
  }

  void setHandlingData(bool is_handling) {
    handling_data = is_handling;
    if (!is_handling) {
      maintenance_loop_condition.notify_one();
    }
  }

  Scheduler::Scheduler* getScheduler() {
    return new Scheduler::Scheduler{
        [this](std::string id, int client_socket_fd, int event, std::vector<std::string> args) {
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
    KLOG->info(
        "RequestHandler::maintenanceLoop() - Beginning maintenance loop");
    for (;;) {
      std::unique_lock<std::mutex> lock(m_mutex);
      maintenance_loop_condition.wait(lock,
                                      [this]() { return !handling_data; });
      KLOG->info("RequestHandler::maintenanceLoop() - condition met");
      int client_socket_fd = -1;
      std::vector<Scheduler::Task> tasks = m_scheduler->fetchTasks();
      if (!tasks.empty()) {
        std::string scheduled_times{"Scheduled time(s): "};
        KLOG->info("There are tasks to be reviewed");
        for (const auto &task : tasks) {
          auto formatted_time = TimeUtils::format_timestamp(task.datetime);
          std::cout << formatted_time << std::endl;
          scheduled_times.append(formatted_time);
          scheduled_times += " ";
          KLOG->info(
              "Task info: Time: {} - Mask: {}\n Args: {}\n {}\n. Excluded: Execution "
              "Flags",
              formatted_time, std::to_string(task.execution_mask),
              task.file ? "hasFile(s)" : "", task.envfile);
        }
        std::string tasks_message = std::to_string(tasks.size());
        tasks_message += " tasks need to be executed";
        m_system_callback_fn(client_socket_fd,
                             SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
                             {tasks_message, scheduled_times});

        auto it = m_tasks_map.find(client_socket_fd);
        if (it == m_tasks_map.end()) {
          m_tasks_map.insert(std::pair<int, std::vector<Scheduler::Task>>(
              client_socket_fd, tasks));
        } else {
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());
        }
        KLOG->info(
            "RequestHandler::maintenanceLoop() - KServer has {} {} pending "
            "execution",
            m_tasks_map.at(client_socket_fd).size(),
            m_tasks_map.at(client_socket_fd).size() == 1 ? "task" : "task");
      } else {
        KLOG->info(
            "RequestHandler::maintenanceLoop() - There are currently no tasks "
            "ready for execution");
        m_system_callback_fn(
            client_socket_fd, SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,
            {"There are currently no tasks ready for execution"});
      }
      if (!m_tasks_map.empty()) {
        if (!handlePendingTasks()) {
          KLOG->error(
            "RequestHandler::maintenanceLoop() - ERROR handling pending tasks");
        }
      }
      System::Cron<System::SingleJob> cron{};
      std::string jobs = cron.listJobs();
      if (!jobs.empty()) {
        KLOG->info(
            "RequestHandler::maintenanceLoop() - Cron - There are currently no "
            "jobs");
      } else {
        KLOG->info(
            "RequestHandler::maintenanceLoop() - Cron - There are currently "
            "the following cron jobs: \n {}",
            jobs);
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
    KLOG->info("RequestHandler::maintenanceLoop() - Running scheduled tasks");
    Scheduler::Scheduler* scheduler = getScheduler();
    if (!m_tasks_map.empty()) {
      Executor::ProcessExecutor executor{};
      bool is_scheduled_task = true;
      executor.setEventCallback(
          [this, is_scheduled_task](std::string result, int mask, std::string id, int client_socket_fd) {
            onProcessComplete(result, mask, id, client_socket_fd, is_scheduled_task);
          });

      std::vector<std::future<void>> futures{};
      futures.reserve(m_tasks_map.size() * m_tasks_map.begin()->second.size());
      for (const auto& client_tasks : m_tasks_map) {
        if (!client_tasks.second.empty()) {
          for (const auto& task : client_tasks.second) {
            futures.push_back(std::async(std::launch::deferred,
              &Executor::ProcessExecutor::executeTask, std::ref(executor), client_tasks.first, task));
          }
        }
      }
      for (auto &future : futures) {
        future.get();
      }
    }
    delete scheduler;
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
      KLOG->info("RequestHandler:: Handling schedule request");
      if (argv.empty()) {
        KLOG->info(
            "RequestHandler::Scheduler - Can't handle a task with no "
            "arguments");
        return "";
      }
      auto mask = argv.at(argv.size() - 2);
      auto kdb = Database::KDB();

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
        if (name == "Instagram") {
          KLOG->info("RequestHandler - New Instagram Task requested");
          Scheduler::Task task = Task::IGTaskHandler::prepareTask(argv, uuid);
          auto num = task.files.size();
          auto file_index = 0;
          for (const auto &file_info : task.files) {
            std::cout << "task file: " << file_info.first << std::endl;
            std::vector<std::string> callback_args{file_info.first,
                                                   file_info.second, uuid};
            if (file_index == task.files.size() - 1) {
              callback_args.push_back("final file");
            }
            m_system_callback_fn(client_socket_fd, SYSTEM_EVENTS__FILE_UPDATE,
                                 callback_args);
          }

          if (task.validate()) {
            KLOG->info("Sending task request to Scheduler");
            auto id = m_scheduler->schedule(task);
            if(!id.empty()) {
              m_system_callback_fn(client_socket_fd, SYSTEM_EVENTS__SCHEDULER_SUCCESS, {
                uuid, id, std::to_string(num)
              });
              return "Operation succeeded";
            }
          }
        }
      }
      return std::string{"Operation failed"};
    }
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
      std::vector<Scheduler::Task> tasks = m_scheduler->fetchTasks();
      if (!tasks.empty()) {
        KLOG->info("There are tasks to be reviewed");
        for (const auto &task : tasks) {
          KLOG->info(
              "RequestHandler:: OPERATION HANDLER - Task info: {} - Mask: {}\n "
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
          m_tasks_map.insert(std::pair<int, std::vector<Scheduler::Task>>(
              client_socket_fd, tasks));
        } else {
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());
        }
        KLOG->info(
            "RequestHandler:: OPERATION HANDLER - {} currently has {} tasks "
            "pending execution",
            client_socket_fd, m_tasks_map.at(client_socket_fd).size());
      } else {
        KLOG->info("There are currently no tasks ready for execution");
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
    // TODO: We need to fix the DB query so it is orderable and groupable
    DatabaseQuery select_query{.table = "apps",
                               .fields = {"name", "path", "data", "mask"},
                               .type = QueryType::SELECT,
                               .values = {}};
    QueryResult result = m_connection.query(select_query);
    std::map<int, std::string> command_map{};
    std::vector<std::string> names{};
    std::vector<int> masks{};
    std::string name{""};
    for (const auto &row : result.values) {
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
    QueryResult result = m_connection.query(
        DatabaseQuery{.table = "apps",
                      .fields = {"path"},
                      .type = QueryType::SELECT,
                      .values = {},
                      .filter = QueryFilter{std::make_pair(
                          std::string("mask"), std::to_string(mask))}});

    for (const auto &row : result.values) {
      KLOG->info("Field: {}, Value: {}", row.first, row.second);
      m_executor->request(row.second, mask, client_socket_fd, request_id, {}, Executor::ExecutionRequestType::IMMEDIATE);
    }
    std::string info_string{
        "RequestHandler:: PROCESS RUNNER - Process execution requested for "
        "applications matching the mask "};
    info_string += std::to_string(mask);
    m_system_callback_fn(client_socket_fd,
                         SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
                         {info_string, request_id});
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
                         int client_socket_fd, bool scheduled_task = false) {
    KLOG->info(
        "RequestHandler::onProcessComplete() - Process complete notification "
        "for client {}'s request {}",
        client_socket_fd, id);
    m_event_callback_fn(value, mask, id, client_socket_fd);
    if (scheduled_task) {
      KLOG->info(
        "RequestHandler::onScheduledTaskComplete() - Task complete "
        "notification "
        "for client {}'s task {}",
        client_socket_fd, id);

      std::map<int, std::vector<Scheduler::Task>>::iterator it =
          m_tasks_map.find(client_socket_fd);
      if (it != m_tasks_map.end()) {
        auto task_it = std::find_if(
            it->second.begin(), it->second.end(),
            [id](Scheduler::Task task) { return task.id == std::stoi(id); });
        if (task_it != it->second.end()) {
          KLOG->info(
              "RequestHandler::onProcessComplete() - removing completed "
              "task from memory");
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
   * TODO: We need to move away from sending process execution results via the scheduler's callback, and only use this to inform of scheduling events. Process execution results should come from the ProcessExecutor and its respective callback.
   */

  void onSchedulerEvent(std::string id, int client_socket_fd, int event, std::vector<std::string> args = {}) {
    m_system_callback_fn(client_socket_fd, event, args);
  }

  /**
   * callback functions
   */
  std::function<void(std::string, int, std::string, int)> m_event_callback_fn;
  std::function<void(int, int, std::vector<std::string>)> m_system_callback_fn;
  std::function<void(int, std::vector<Scheduler::Task>)> m_task_callback_fn;

  std::map<int, std::vector<Scheduler::Task>> m_tasks_map;
  std::mutex m_mutex;
  std::condition_variable maintenance_loop_condition;
  std::atomic<bool> handling_data;

  Executor::ProcessExecutor* m_executor;
  Scheduler::Scheduler* m_scheduler;
  DatabaseConnection m_connection;
  DatabaseCredentials m_credentials;
  std::thread m_maintenance_worker;
};
}  // namespace Request
#endif  // __REQUEST_HANDLER_HPP__
