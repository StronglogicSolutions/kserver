#pragma once

#include <stdlib.h>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>
#include <chrono>
#include <condition_variable>

#include "common/util.hpp"
#include "log/logger.h"
#include "codec/kmessage_generated.h"
#include "config/config_parser.hpp"
#include "database/kdb.hpp"
#include "system/process/executor/executor.hpp"
#include "system/process/scheduler.hpp"
#include "system/process/registrar.hpp"
#include "system/process/executor/task_handlers/instagram.hpp"
#include "system/process/executor/task_handlers/generic.hpp"
#include "server/types.hpp"
#include "system/cron.hpp"
#include "types.hpp"

#define OPERATION_SUCCESS "Operation succeeded"
#define OPERATION_FAIL    "Operation failed"

namespace Request {

enum DevTest {
  Schedule = 1,
  ExecuteTask = 2
};

using namespace KData;

static flatbuffers::FlatBufferBuilder builder(1024);


/**
 * Controller
 *
 * Handles incoming requests coming from the KY_GUI Application
 */
class Controller {

 using EventCallbackFn  = std::function<void(std::string, int, std::string, int, bool)>;
 using SystemCallbackFn = std::function<void(int, int, std::vector<std::string>)>;
 using TaskCallbackFn   = std::function<void(int, std::vector<Task>)>;

 public:
  /**
   * Controller()
   * @constructor
   *
   * Loads configuration and instantiates a DatabaseConfiguration object
   *
   */
  Controller()
  : m_active(true),
    m_executor(nullptr),
    m_scheduler(getScheduler())
  {}

  /**
   * @constructor
   *
   * The move constructor
   */
  Controller(Controller &&r)
  : m_active(r.m_active),
    m_executor(r.m_executor),
    m_scheduler(nullptr)
  {
    r.m_executor = nullptr;
  }

  /**
   * @constructor
   *
   * The copy constructor
   */
  Controller(const Controller &r)
  : m_active(r.m_active),
    m_executor(nullptr),
    m_scheduler(nullptr)
  {}

  /**
   * @operator
   *
   * The copy assignment operator
   */
  Controller &operator=(const Controller &handler) {
    this->m_executor  = nullptr;
    return *this;
  }

  /**
   * @operator
   *
   * The move assignment operator
   */
  Controller &operator=(Controller &&handler) {
    if (&handler != this) {
      delete m_executor;
      m_executor          = handler.m_executor;
      m_active            = handler.m_active;
      handler.m_executor  = nullptr;
    }
    return *this;
  }

  /**
   * @destructor
   *
   * Deletes the process executor and ensures that the maintenance worker thread
   * completes
   */
  ~Controller() {
    if (m_executor != nullptr)
      delete m_executor;

    if (m_maintenance_worker.joinable()) {
      KLOG("Waiting for maintenance worker to complete");
      m_maintenance_worker.join();
    }
  }

  /**
   * initialize
   *
   * Initializes the Controller with callbacks for sending system events and
   * process execution results. Instantiates a ProcessExecutor and provides a
   * callback. Starts a thread to perform work on the maintenance loop
   *
   */
  void initialize(EventCallbackFn event_callback_fn,
                  SystemCallbackFn system_callback_fn,
                  TaskCallbackFn task_callback_fn) {
    const bool scheduled_task{true};
    m_executor = new ProcessExecutor();
    m_executor->setEventCallback(
      [this, scheduled_task](const std::string& result,
                             const int          mask,
                             const std::string& id,
                             const int          client_socket_fd,
                             const bool         error)
      {
        onProcessComplete(result, mask, id, client_socket_fd, error, scheduled_task);
      }
    );

    m_system_callback_fn  = system_callback_fn;
    m_event_callback_fn   = event_callback_fn;
    m_task_callback_fn    = task_callback_fn;

    setHandlingData(false);

    // Begin maintenance loop to process scheduled tasks as they become ready
    m_maintenance_worker =
        std::thread(std::bind(&Controller::maintenanceLoop, this));
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

    if (!is_handling)
      maintenance_loop_condition.notify_one();
  }

  /**
   * getScheduler
   *
   * @returns [out] {Scheduler}  New instance of Scheduler
   */
  Scheduler getScheduler() {
    return Scheduler{
      [this](int32_t                         client_socket_fd,
             int32_t                         event,
             const std::vector<std::string>& args)
    {
      onSchedulerEvent(client_socket_fd, event, args);
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
        [this]() { return !handling_data; }
      );

      int client_socket_fd = -1;

      std::vector<Task> tasks = m_scheduler.fetchTasks();
      for (auto&& recurring_task : m_scheduler.fetchRecurringTasks())
        tasks.emplace_back(recurring_task);

      if (!tasks.empty()) {
        std::string scheduled_times{"Scheduled time(s): "};
        KLOG("{} tasks found", tasks.size());
        for (const auto &task : tasks) {
          auto formatted_time = TimeUtils::FormatTimestamp(task.datetime);
          scheduled_times += formatted_time + " ";
          KLOG("Task info: Time: {} - Mask: {}\n Args: {}\n {}",
            formatted_time, std::to_string(task.execution_mask),
            task.file ? "hasFile(s)" : "", task.envfile
          );
        }

        m_system_callback_fn(
          client_socket_fd,
          SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
          {std::to_string(tasks.size()) + " tasks need to be executed", scheduled_times}
        );

        auto it = m_tasks_map.find(client_socket_fd);
        if (it == m_tasks_map.end())
          m_tasks_map.insert(std::pair<int, std::vector<Task>>(client_socket_fd, tasks));
        else
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());

        KLOG("KServer has {} {} pending execution",
          m_tasks_map.at(client_socket_fd).size(),
          m_tasks_map.at(client_socket_fd).size() == 1 ? "task" : "task");
      }

      if (!m_tasks_map.empty())
        handlePendingTasks();

      m_scheduler.processPlatform();
      maintenance_loop_condition.wait_for(lock, std::chrono::milliseconds(20000));
    }
  }

  /**
   * handlePendingTasks
   *
   * Iterates pending tasks and requests their execution
   */
  void handlePendingTasks() {
    if (!m_tasks_map.empty()) {
      std::vector<std::future<void>> futures{};
      futures.reserve(m_tasks_map.size() * m_tasks_map.begin()->second.size());

      for (const auto &client_tasks : m_tasks_map)
        if (!client_tasks.second.empty())
          for (const auto &task : client_tasks.second)
            futures.push_back(std::async(
              std::launch::deferred, &ProcessExecutor::executeTask,
              std::ref(*(m_executor)), client_tasks.first, task));

      for (auto& future : futures)
        future.get();
    }
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
        uint8_t file_index = 0;
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
          auto id = m_scheduler.schedule(task);
          if (!id.empty()) {
            // Task was scheduled. Prepare a vector with info about the task.
            std::vector<std::string> callback_args{};
            callback_args.reserve((5 + task.files.size()));
            callback_args.insert(callback_args.end(), {
              uuid, id,                            // UUID and database ID
              std::to_string(task.execution_mask), // Application mask
              FileUtils::ReadEnvFile(task.envfile),// Environment file
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
      KLOG("Task scheduling failed: Unable to find an application matching mask {}", mask);
    }
    return OPERATION_FAIL;
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
  std::vector<KApplication> operator()(KOperation op) {
    std::vector<KApplication>      commands{};
    const std::string              name{"apps"};
    const std::vector<std::string> fields{"name", "path", "data", "mask"};
    const QueryFilter              filter{};

    if (op.compare("Start") == 0) {
      uint8_t arg_idx{};
      KApplication command{};

      for (const auto& row : m_kdb.select(name, fields, filter)) {
        if (row.first == "name") {
          command.name = row.second;
        } else if (row.first == "mask") {
          command.mask = row.second;
        } else if (row.first == "path") {
          command.path = row.second;
        } else if (row.first == "data") {
          command.data = row.second;
        }
        if (arg_idx == 3) {
          arg_idx = 0;
          commands.push_back(command);
        } else {
          arg_idx++;
        }
      }
    }
    return commands;
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
    const std::string              name{"apps"};
    const std::vector<std::string> fields{"path"};
    const QueryFilter              filter{{"mask", std::to_string(mask)}};

    ProcessExecutor executor{};
    executor.setEventCallback([this](std::string result,
                                     int         mask,
                                     std::string request_id,
                                     int         client_socket_fd,
                                     bool        error)
    {
      onProcessComplete(result, mask, request_id, client_socket_fd, error);
    });

    for (const auto &row : m_kdb.select(name, fields, filter)) {
      m_system_callback_fn(client_socket_fd,
                           SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
                           {"PROCESS RUNNER - Process execution requested for applications with mask " + std::to_string(mask),
                           request_id});

      executor.request(row.second, mask, client_socket_fd, request_id, {},
                          constants::IMMEDIATE_REQUEST);
    }
  }

/**
 * @brief process_system_events
 *
 * @param [in] {int32_t}                  event
 * @param [in] {std::vector<std::string>> payload
 */
  void process_system_event(const int32_t event, const std::vector<std::string> payload)
  {
    switch (event)
    {
      case (SYSTEM_EVENTS__PLATFORM_NEW_POST):
        m_scheduler.savePlatformPost(payload);
        break;

      case (SYSTEM_EVENTS__PLATFORM_ERROR):
        m_scheduler.onPlatformError(payload);
        break;

      case (SYSTEM_EVENTS__PROCESS_COMPLETE):
      {
        const std::string output =           payload.at(EVENT_PROCESS_OUTPUT_INDEX);
        const int32_t     mask   = std::stoi(payload.at(EVENT_PROCESS_MASK_INDEX));
        m_scheduler.handleProcessOutput(output, mask);
      }
      break;
    }
  }

  /**
   * process_client_request
   *
   * @param [in] {int32_t}     client_fd The client socket file descripqqqQQQr
   * @param [in] {std::string} message
   */
  void process_client_request(int32_t client_fd, const std::string& message) {
    std::vector<std::string> args = GetArgs(message);
    RequestType type = int_to_request_type(std::stoi(args.at(Request::REQUEST_TYPE_INDEX)));

    switch (type)
    {
      case(RequestType::GET_APPLICATION):
      {
        KApplication application = Registrar::args_to_application(args);
          m_system_callback_fn(client_fd,
            (m_registrar.find(Registrar::args_to_application(args))) ?
              SYSTEM_EVENTS__REGISTRAR_SUCCESS :
              SYSTEM_EVENTS__REGISTRAR_FAIL,
            application.vector());
        break;
      }

      case (RequestType::REGISTER_APPLICATION):
      {
        KApplication application = Registrar::args_to_application(args);
        auto         id          = m_registrar.add(application);
        (!id.empty()) ?
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__REGISTRAR_SUCCESS,
            DataUtils::vector_absorb(std::move(application.vector()), std::move(id))) :
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__REGISTRAR_FAIL, application.vector());
        break;
      }

      case (RequestType::REMOVE_APPLICATION):
      {
        KApplication application = Registrar::args_to_application(args);
        auto         name        = m_registrar.remove(application);
        (!name.empty()) ?
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__REGISTRAR_SUCCESS,
            DataUtils::vector_absorb(std::move(application.vector()),  std::move(std::string{"Application was deleted"}))) :
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__REGISTRAR_FAIL,
            DataUtils::vector_absorb(std::move(application.vector()), std::move(std::string{"Failed to delete application"})));
        break;
      }

      case (RequestType::UPDATE_APPLICATION):
      {
        KLOG("Must implement UPDATE_APPLICATION");
        break;
      }

      case (RequestType::FETCH_SCHEDULE):
      {
        KLOG("Processing schedule fetch request");
        const uint8_t AVERAGE_TASK_SIZE = 9;
        uint8_t       i{0};
        uint8_t       TASKS_PER_EVENT{4};
        std::vector<Task> tasks = m_scheduler.fetchAllTasks();
        std::vector<std::string> payload{};
        payload.reserve((tasks.size() * AVERAGE_TASK_SIZE) + 2);
        payload.emplace_back("Schedule");

        for (const auto& task : tasks)
        {
          KApplication app = m_executor->getAppInfo(task.execution_mask);
          payload.emplace_back(std::to_string(task.id));
          payload.emplace_back(               app.name);
          payload.emplace_back(               task.datetime);
          payload.emplace_back(               task.execution_flags);
          payload.emplace_back(std::to_string(task.completed));
          payload.emplace_back(std::to_string(task.recurring));
          payload.emplace_back(std::to_string(task.notify));
          payload.emplace_back(               task.runtime);
          payload.emplace_back(               task.filesToString());
          if (!(++i % TASKS_PER_EVENT))
          {
            m_system_callback_fn(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, payload);
            payload.clear();
            payload.emplace_back("Schedule more");
          }
        }

        if (!payload.empty())
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, payload);

        const auto size = tasks.size();
        KLOG("Fetched {} scheduled tasks for client", size);

        m_system_callback_fn(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, {"Schedule end", std::to_string(size)});

        break;
      }

      case (RequestType::UPDATE_SCHEDULE):
      {
        // TODO: Not getting completed/status value
        TaskWrapper        task_wrapper = args_to_task(args);
        const std::string& task_id      = std::to_string(task_wrapper.task.id);
        bool               save_success = m_scheduler.update(task_wrapper.task);
        bool               env_updated  = m_scheduler.updateEnvfile(task_id, task_wrapper.envfile);

        m_system_callback_fn(client_fd, SYSTEM_EVENTS__SCHEDULER_UPDATE, {task_id, (save_success) ? "Success" : "Failure"});

        if (!env_updated)
          KLOG("Failed to update envfile while handling update for task {}", task_id);
        // TODO: Consolidate update event to include information about saving environment file
        break;
      }

      case (RequestType::FETCH_SCHEDULE_TOKENS):
      {
        auto id = args.at(constants::PAYLOAD_ID_INDEX);
        Task task = m_scheduler.getTask(id);
        if (task.validate())
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS,
            DataUtils::vector_absorb(std::move(FileUtils::ReadFlagTokens(task.envfile, task.execution_flags)),
                                     std::move(id)));
        break;
      }

      case (RequestType::FETCH_TASK_DATA):
      {
        std::vector<std::string> payload{};
        const std::vector<Task>  tasks =
          m_scheduler.fetchTasks(args.at(constants::FETCH_TASK_MASK_INDEX),
                                 args.at(constants::FETCH_TASK_DATE_RANGE_INDEX),
                                 args.at(constants::FETCH_TASK_ROW_COUNT_INDEX),
                                 args.at(constants::FETCH_TASK_MAX_ID_INDEX),
                                 args.at(constants::FETCH_TASK_ORDER_INDEX));

        payload.emplace_back(std::to_string(tasks.size()));

        for (auto&& task : tasks)
        {
          payload.emplace_back(std::to_string(task.id));
          for (const auto& flag : FileUtils::ExtractFlagTokens(task.execution_flags))
          {
            payload.emplace_back(flag);
            payload.emplace_back(FileUtils::ReadEnvToken(task.envfile, flag));
          }
        }

        m_system_callback_fn(client_fd, SYSTEM_EVENTS__TASK_DATA,       payload);
        m_system_callback_fn(client_fd, SYSTEM_EVENTS__TASK_DATA_FINAL, {});
        break;
      }

      case (RequestType::TRIGGER_CREATE):
      {
        auto result = m_scheduler.addTrigger(args);
        std::vector<std::string> event_args{}; int32_t event_type{};
        if (result)
        {
          event_args.insert(event_args.end(), {"Trigger Created", args.at(1), args.at(2)});
          event_type = SYSTEM_EVENTS__TRIGGER_ADD_SUCCESS;
        }
        else
        {
          event_args.emplace_back("Failed to create trigger");
          event_type = SYSTEM_EVENTS__TRIGGER_ADD_FAIL;
        }
        m_system_callback_fn(client_fd, event_type, event_args);
        break;
      }

      case (TASK_FLAGS):
        m_system_callback_fn(client_fd, SYSTEM_EVENTS__TASK_FETCH_FLAGS,
          DataUtils::vector_absorb(std::move(m_scheduler.getFlags(args.at(1))),
                                   std::move(args.at(1))));
        break;

      case (FETCH_FILE):
      {
        auto files = m_scheduler.getFiles(std::vector<std::string>{args.begin() + 1, args.end()});
        if (!files.empty())
          m_system_callback_fn(client_fd, SYSTEM_EVENTS__FILES_SEND, FileMetaData::MetaDataToPayload(files));
        break;
      }

      case (FETCH_FILE_ACK):
        m_system_callback_fn(client_fd, SYSTEM_EVENTS__FILES_SEND_ACK, {});
        break;

      case (FETCH_FILE_READY):
        m_system_callback_fn(client_fd, SYSTEM_EVENTS__FILES_SEND_READY, {});
        break;
      case (RequestType::UNKNOWN):
      default:
        ELOG("Controller could not process unknown client request: {}", type);
        break;
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
   *                                       executed process
   * @param[in] <int> `mask`               The bitmask associated with the
   *                                       process
   * @param[in] <std::string> `id`         The request ID for the process
   * @param[in] <int> `client_socket_fd`   The file descriptor for the client
   * who made the request
   *
   */
  void onProcessComplete(const std::string& value,
                         const int          mask,
                         const std::string& id,
                         const int          client_socket_fd,
                         const bool         error,
                         const bool         scheduled_task = false)
  {
    using TaskVectorMap = std::map<int, std::vector<Task>>;
    using TaskVector    = std::vector<Task>;

    KLOG("Process complete notification for client {}'s request {}",
      client_socket_fd, id);

    m_event_callback_fn( // Inform system of process result
      value,
      mask,
      id,
      client_socket_fd,
      error
    );

    if (scheduled_task) // If it was a scheduled task, we need to update task map held in memory
    {
      TaskVector::iterator task_it;

      KLOG("Task complete notification for client {}'s task {}{}",
        client_socket_fd, id, error ? "\nERROR WAS RETURNED" : ""
      );

      TaskVectorMap::iterator it = m_tasks_map.find(client_socket_fd); // Find client's tasks

      if (it != m_tasks_map.end())
      {                                   // Find task
        task_it = std::find_if(it->second.begin(), it->second.end(),
          [id](Task task) { return task.id == std::stoi(id); }
        );

        if (task_it != it->second.end())
        {
          uint8_t status{};

          if (error)
          {
            status = (task_it->completed == Completed::FAILED) ?
              Completed::RETRY_FAIL :                           // No retry
              Completed::FAILED;                                // Retry

            KLOG("Sending email to administrator about failed task.\nNew "
                "Status: {}",
                Completed::STRINGS[status]);

            SystemUtils::SendMail(                                         // Email error to notification recipient
              ConfigParser::Email::notification(),
              std::string{Messages::TASK_ERROR_EMAIL + value},
              ConfigParser::Email::admin()
            );
          }
          else
          {
            status = task_it->recurring ?
              Completed::SCHEDULED :
              Completed::SUCCESS;
            if (!m_scheduler.processTriggers(&*task_it))
              KLOG("Error occurred processing triggers for task {} with mask {}", task_it->id, task_it->execution_mask);
          }

          task_it->completed = status;                            // Update status
          m_scheduler.updateStatus(&*task_it, value);             // Failed tasks will re-run once more
          m_executor->saveResult(mask, 1, TimeUtils::UnixTime()); // Save execution result

          if (!error && task_it->recurring)                       // If no error, update last execution time
          {
            KLOG("Task {} will be scheduled for {}",
              task_it->id, TimeUtils::FormatTimestamp(task_it->datetime));

            m_scheduler.updateRecurring(&*task_it); // Latest time
            KLOG("Task {} was a recurring task scheduled to run {}",
              task_it->id, Constants::Recurring::names[task_it->recurring]);
          }

          if (task_it->notify)                                             // Send email notification
          {
            KLOG("Task notification enabled - emailing result to administrator");
            std::string email_string{};
            email_string.reserve(value.size() + 84);
            email_string += task_it->toString();
            email_string += error ? "\nError" : "\n";
            email_string += value;

            SystemUtils::SendMail(
              ConfigParser::Email::notification(),
              email_string,
              ConfigParser::Email::admin()
            );
          }

          KLOG("removing completed task from memory");
          it->second.erase(task_it);
          if (it->second.empty())
            m_tasks_map.erase(it);
        }
      }
    }
  }

  /**
   * onScheduledTaskComplete
   *
   * @request
   *
   * The callback function called by the Scheduler after a scheduled
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

  void onSchedulerEvent(int32_t client_socket_fd,
                        int32_t event,
                        const std::vector<std::string>& args = {})
  {
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
  Registrar::Registrar              m_registrar;
  ProcessExecutor*                  m_executor;
  Scheduler                         m_scheduler;
  std::thread                       m_maintenance_worker;
  Database::KDB                     m_kdb;
};
}  // namespace Request
