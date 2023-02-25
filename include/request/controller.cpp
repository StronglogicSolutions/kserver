#include "controller.hpp"
#include "common/time.hpp"
#include "kproto/types.hpp"

namespace kiq
{
static flatbuffers::FlatBufferBuilder builder(1024);
//----------------------------------------------------------------------------------
Controller::Controller()
: m_active(true),
  m_executor(nullptr),
  m_scheduler(
    [this](const int32_t&                  client_socket_fd,
           const int32_t&                  event,
           const std::vector<std::string>& args)
  {
    onSchedulerEvent(client_socket_fd, event, args);
  }),
  m_ps_exec_count(0),
  m_client_rq_count(0),
  m_system_rq_count(0),
  m_err_count(0)
{}
//----------------------------------------------------------------------------------
Controller::Controller(Controller &&r)
: m_active(r.m_active),
  m_executor(r.m_executor),
  m_scheduler(nullptr)
{
  r.m_executor = nullptr;
}
//----------------------------------------------------------------------------------
Controller::Controller(const Controller &r)
: m_active(r.m_active),
  m_executor(nullptr),
  m_scheduler(nullptr)
{}
//----------------------------------------------------------------------------------
Controller& Controller::operator=(const Controller &handler)
{
  this->m_executor  = nullptr;
  return *this;
}
//----------------------------------------------------------------------------------
Controller& Controller::operator=(Controller&& handler)
{
  if (&handler != this)
  {
    delete m_executor;
    m_executor          = handler.m_executor;
    m_active            = handler.m_active;
    handler.m_executor  = nullptr;
  }

  return *this;
}
//----------------------------------------------------------------------------------
Controller::~Controller()
{
  if (m_executor != nullptr)
    delete m_executor;

  if (m_maintenance_worker.joinable())
  {
    KLOG("Waiting for maintenance worker to complete");
    m_maintenance_worker.join();
  }

  if (m_sentinel_future.valid())
    m_sentinel_future.wait();
}
//----------------------------------------------------------------------------------
void Controller::Initialize(ProcessCallbackFn process_callback_fn,
                            SystemCallbackFn  system_callback_fn,
                            StatusCallbackFn  status_callback_fn,
                            ClientValidateFn  validate_client_fn)
{
  const bool scheduled_task{true};
  m_executor = new ProcessExecutor();
  m_executor->setEventCallback(
    [this, scheduled_task](const std::string&  result,
                            const int32_t&     mask,
                            const std::string& id,
                            const int32_t&     client_socket_fd,
                            const bool&        error)
    {
      onProcessComplete(result, mask, id, client_socket_fd, error, scheduled_task);
    }
  );

  m_process_event      = process_callback_fn;
  m_system_event       = system_callback_fn;
  m_server_status      = status_callback_fn;
  m_validate_client    = validate_client_fn;
  m_maintenance_worker = std::thread(std::bind(&Controller::InfiniteLoop, this));

  if (m_sentinel_future.valid())
    m_sentinel_future.wait();

  m_sentinel_future = std::async(std::launch::deferred, [this] {
    if (m_timer.expired())
    {
      while (m_active)
      {
        if (m_timer.expired())
        {
          const auto message = "Controller's worker seems deadlocked";
          VLOG(message);
          SystemUtils::SendMail(config::Email::notification(), message);
          m_active = false;
        }
          std::this_thread::sleep_for(std::chrono::minutes(1));
      }
    }
  });

  SetWait(false);
  KLOG("Initialization complete");
}
//----------------------------------------------------------------------------------
void Controller::Shutdown()
{
  m_active = false;
}
//----------------------------------------------------------------------------------
void Controller::SetWait(const bool& wait)
{
  {
    std::lock_guard<std::mutex> lock{m_mutex};
    m_wait.store(wait);
  }
  m_condition.notify_one();
}
//----------------------------------------------------------------------------------
void Controller::InfiniteLoop()
{
  static const bool    autostart{true};
  static const int32_t client_fd{ALL_CLIENTS};
  static       Timer   timer{Timer::TEN_MINUTES, autostart};

  KLOG("Worker starting");

  while (m_active)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this]() { return !m_wait; });

    std::vector<Task> tasks = m_scheduler.fetchTasks();
    for (auto&& recurring_task : m_scheduler.fetchRecurringTasks())
      tasks.emplace_back(recurring_task);

    if (!tasks.empty())
    {
      std::string scheduled_times{"Scheduled time(s): "};
      KLOG("{} tasks found", tasks.size());

      for (const auto& task : tasks)
      {
        const auto formatted_time = TimeUtils::FormatTimestamp(task.datetime);
        scheduled_times += formatted_time + " ";
        KLOG("Task - Time: {} - Mask: {}\nEnv: {}", formatted_time, task.execution_mask, task.envfile);
      }

      m_system_event(client_fd, SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
        {std::to_string(tasks.size()) + " tasks need to be executed", scheduled_times});

      if (auto it = m_tasks_map.find(client_fd); it == m_tasks_map.end())
        m_tasks_map.insert(std::pair<int, std::vector<Task>>(client_fd, tasks));
      else
        it->second.insert(it->second.end(), tasks.begin(), tasks.end());

      KLOG("Tasks pending execution: ", m_tasks_map.at(client_fd).size());
    }

    HandlePendingTasks();

    if (timer.expired())
    {
      VLOG(Status());
      timer.reset();
      m_validate_client();
    }

    m_scheduler.ProcessIPC();
    m_scheduler.ResolvePending();
    m_condition.wait_for(lock, std::chrono::milliseconds(5000));
  }

  SystemUtils::SendMail(config::System::admin(), "Worker loop has ended");
}
//----------------------------------------------------------------------------------
void Controller::HandlePendingTasks()
{
  try
  {
    for (const auto& client_tasks : m_tasks_map)
      for (const auto& task : client_tasks.second)
        m_executor->executeTask(client_tasks.first, task);
    m_tasks_map.clear();
  }
  catch(const std::exception& e)
  {
    const auto error = fmt::format("Exception caught while executing process: {}", e.what());
    ELOG(error);
    SystemUtils::SendMail(config::System::admin(), error);
  }
}
//----------------------------------------------------------------------------------
void Controller::operator()(const KOperation&               op,
                            const std::vector<std::string>& argv,
                            const int32_t&                  client_fd,
                            const std::string&              uuid)
{
  if (op != "Schedule" || argv.empty()) return;

  const auto mask = argv.at(TaskIndexes::MASK);
  const auto application = ProcessExecutor::GetAppInfo(std::stoi(mask));

  KLOG("Handling schedule request application {} with mask {}", application.name, mask);

  if (application.is_valid())
  {
    Task task{};
    if (application.name == Name::INSTAGRAM)
    {
      KLOG("Instagram Task requested");
      IGTaskHandler ig_task_handler{};
      ig_task_handler.prepareTask(argv, uuid, &task);
    }
    else
    {
      KLOG("Generic Task requested");
      GenericTaskHandler generic_task_handler{};
      generic_task_handler.prepareTask(argv, uuid, &task);
    }

    for (const auto &file_info : task.files)
    {
      KLOG("Task file: {}", file_info.first);
      std::vector<std::string> callback_args{file_info.first, file_info.second, uuid};

      if (task.files.size() == 1) callback_args.push_back("final file");

      m_system_event(client_fd, SYSTEM_EVENTS__FILE_UPDATE, callback_args);
    }

    if (task.validate())
    {
      KLOG("Sending task request to Scheduler");

      auto id = m_scheduler.schedule(task);
      if (!id.empty())
      {
        std::vector<std::string> callback_args{};
        callback_args.reserve((5 + task.files.size()));
        callback_args.insert(callback_args.end(), {
          uuid, id,                            // UUID and database ID
          std::to_string(task.execution_mask), // Application mask
          FileUtils::ReadEnvFile(task.envfile),// Environment file
          std::to_string(task.files.size())    // File number
        });

        for (auto&& file : task.files) callback_args.emplace_back(file.first);

        m_system_event(client_fd, SYSTEM_EVENTS__SCHEDULER_SUCCESS, callback_args);
      }
      else
        KLOG("Task with UUID {} was validated, but scheduling failed", uuid);
    }
    else
      KLOG("Task with UUID {} was processed, but did not pass validation", uuid);
  }
  else
    KLOG("Task scheduling failed to match app to mask {}", mask);
}
//----------------------------------------------------------------------------------
std::vector<KApplication> Controller::CreateSession()
{
  uint8_t                   i{};
  KApplication              command{};
  std::vector<KApplication> commands{};
  const auto                name{"apps"};
  const Fields              fields{"name", "path", "data", "mask"};

  for (const auto& row : m_kdb.select(name, fields, QueryFilter{}))
  {
    if (row.first == "name")
      command.name = row.second;
      else
      if (row.first == "mask")
      command.mask = row.second;
      else
      if (row.first == "path")
      command.path = row.second;
      else
      if (row.first == "data")
      command.data = row.second;

    if (i == 3)
    {
      i = 0;
      commands.push_back(command);
    }
    else
      i++;
  }
  return commands;
}
//----------------------------------------------------------------------------------
void Controller::Execute(const uint32_t&    mask,
                         const std::string& request_id,
                         const int32_t&     client_socket_fd)
{
  KLOG("Execute request received.\nMask: {}  ID: {}", mask, request_id);
  const std::string              name{"apps"};
  const std::vector<std::string> fields{"path"};
  const QueryFilter              filter = CreateFilter("mask", std::to_string(mask));

  ProcessExecutor executor{};
  executor.setEventCallback([this](const std::string& result,
                                   int                mask,
                                   const std::string& request_id,
                                   int                client_socket_fd,
                                   bool               error)
  {
    onProcessComplete(result, mask, request_id, client_socket_fd, error);
  });

  for (const auto& row : m_kdb.select(name, fields, filter))
  {
    m_system_event(client_socket_fd,
                   SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
                   {"PROCESS RUNNER - Process execution requested for applications with mask " + std::to_string(mask),
                   request_id});

    executor.request(row.second, mask, client_socket_fd, request_id, {}, constants::IMMEDIATE_REQUEST);
  }
}
//----------------------------------------------------------------------------------
void Controller::ProcessSystemEvent(const int32_t&                    event,
                                      const std::vector<std::string>& payload,
                                      const int32_t&                  id)
{
  VLOG("Processing event {}", event);
  switch (event)
  {
    case (SYSTEM_EVENTS__PLATFORM_REQUEST):
      m_scheduler.OnPlatformRequest(payload);
    break;
    case (SYSTEM_EVENTS__PLATFORM_NEW_POST):
      m_scheduler.SavePlatformPost(payload);
    break;
    case (SYSTEM_EVENTS__PLATFORM_ERROR):
      m_scheduler.OnPlatformError(payload);
    break;
    case (SYSTEM_EVENTS__PROCESS_COMPLETE):
    {
      const std::string output =           payload.at(EVENT_PROCESS_OUTPUT_INDEX);
      const int32_t     mask   = std::stoi(payload.at(EVENT_PROCESS_MASK_INDEX));
      m_scheduler.OnProcessOutput(output, mask, id);
    }
    break;
  }
  m_system_rq_count++;
}
//----------------------------------------------------------------------------------
void Controller::ProcessClientRequest(const int32_t&     client_fd,
                                      const std::string& message)
{
  using namespace Request;
  using Payload = std::vector<std::string>;
  auto ReadTask = [](const Task& task, Payload& payload)
  {
    payload.emplace_back(               task.id());
    payload.emplace_back(               task.name);
    payload.emplace_back(               task.datetime);
    payload.emplace_back(               task.execution_flags);
    payload.emplace_back(std::to_string(task.completed));
    payload.emplace_back(std::to_string(task.recurring));
    payload.emplace_back(std::to_string(task.notify));
    payload.emplace_back(               task.runtime);
    payload.emplace_back(               task.filesToString());
  };

  Payload     args = GetArgs(message);
  RequestType type = int_to_request_type(std::stoi(args.at(Request::REQUEST_TYPE_INDEX)));

  VLOG("Controller processing client request of type {}", request_type_to_string(type));

  switch (type)
  {
    case(RequestType::GET_APPLICATION):
    {
      KApplication application = Registrar::args_to_application(args);
        m_system_event(client_fd,
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
        m_system_event(client_fd, SYSTEM_EVENTS__REGISTRAR_SUCCESS,
          DataUtils::VAbsorb(std::move(application.vector()), std::move(id))) :
        m_system_event(client_fd, SYSTEM_EVENTS__REGISTRAR_FAIL, application.vector());
      break;
    }

    case (RequestType::REMOVE_APPLICATION):
    {
      KApplication application = Registrar::args_to_application(args);
      auto         name        = m_registrar.remove(application);
      (!name.empty()) ?
        m_system_event(client_fd, SYSTEM_EVENTS__REGISTRAR_SUCCESS,
          DataUtils::VAbsorb(std::move(application.vector()),  std::move(std::string{"Application was deleted"}))) :
        m_system_event(client_fd, SYSTEM_EVENTS__REGISTRAR_FAIL,
          DataUtils::VAbsorb(std::move(application.vector()), std::move(std::string{"Failed to delete application"})));
      break;
    }

    case (RequestType::UPDATE_APPLICATION):
      KLOG("Must implement UPDATE_APPLICATION");
    break;

    case (RequestType::FETCH_SCHEDULE):
    {
      std::vector<Task>    tasks;
      try
      {
        tasks             = m_scheduler.fetchAllTasks();
      }
      catch(const std::exception& e)
      {
        ELOG("Exception thrown from while fetching tasks: {}", e.what());
      }

      const auto           size              = tasks.size();
      Payload              payload{"Schedule"};
      KLOG("Processing schedule fetch request");
      KLOG("Fetched {} scheduled tasks for client", size);

      try
      {
        for (const auto& task : tasks) ReadTask(task, payload);
      }
      catch(const std::exception& e)
      {
        ELOG("Exception thrown from while reading tasks: {}", e.what());
      }
      try
      {
        m_system_event(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, payload);
        m_system_event(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, {"Schedule end", std::to_string(size)});
      }
      catch(const std::exception& e)
      {
        ELOG("Exception thrown passing tasks to system callback: {}", e.what());
      }
    }
    break;
    case (RequestType::UPDATE_SCHEDULE):
    {
      Task               recvd_task   = args_to_task(args);
      const std::string& task_id      = recvd_task.id();
      bool               save_success = m_scheduler.update(recvd_task);
      bool               env_updated  = m_scheduler.updateEnvfile(task_id, recvd_task.envfile);

      m_system_event(client_fd, SYSTEM_EVENTS__SCHEDULER_UPDATE, {task_id, (save_success) ? "Success" : "Failure"});

      if (!env_updated)
        KLOG("Failed to update envfile while handling update for task {}", task_id);
    }
    break;
    case (RequestType::FETCH_SCHEDULE_TOKENS):
    {
      auto id = args.at(constants::PAYLOAD_ID_INDEX);
      Task task = m_scheduler.GetTask(id);
      if (task.validate())
        m_system_event(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS,
          DataUtils::VAbsorb(std::move(FileUtils::ReadFlagTokens(task.envfile, task.execution_flags)),
                                    std::move(id)));
      break;
    }

    case (RequestType::FETCH_TASK_DATA):
    {
      Payload payload{};
      const std::vector<Task>  tasks = m_scheduler.fetchTasks(
        args.at(constants::FETCH_TASK_MASK_INDEX),
        args.at(constants::FETCH_TASK_DATE_RANGE_INDEX),
        args.at(constants::FETCH_TASK_ROW_COUNT_INDEX),
        args.at(constants::FETCH_TASK_MAX_ID_INDEX),
        args.at(constants::FETCH_TASK_ORDER_INDEX));

      payload.emplace_back(std::to_string(tasks.size()));

      for (auto&& task : tasks)
      {
        payload.emplace_back(task.id());
        for (const auto& flag : FileUtils::ExtractFlagTokens(task.execution_flags))
        {
          payload.emplace_back(flag);
          payload.emplace_back(FileUtils::ReadEnvToken(task.envfile, flag));
        }
      }

      m_system_event(client_fd, SYSTEM_EVENTS__TASK_DATA,       payload);
      m_system_event(client_fd, SYSTEM_EVENTS__TASK_DATA_FINAL, {});
      break;
    }

    case (RequestType::TRIGGER_CREATE):
    {
      Payload    event_args{};
      int32_t    event_type{};
      if (m_scheduler.addTrigger(args))
      {
        event_args.insert(event_args.end(), {"Trigger Created", args.at(1), args.at(2)});
        event_type = SYSTEM_EVENTS__TRIGGER_ADD_SUCCESS;
      }
      else
      {
        event_args.emplace_back("Failed to create trigger");
        event_type = SYSTEM_EVENTS__TRIGGER_ADD_FAIL;
      }
      m_system_event(client_fd, event_type, event_args);
      break;
    }

    case (TASK_FLAGS):
      m_system_event(client_fd, SYSTEM_EVENTS__TASK_FETCH_FLAGS,
        DataUtils::VAbsorb(std::move(m_scheduler.getFlags(args.at(1))), std::move(args.at(1))));
    break;

    case (FETCH_FILE):
    {
      if (const auto files = m_scheduler.getFiles(Payload{args.begin() + 1, args.end()}); !files.empty())
        m_system_event(client_fd, SYSTEM_EVENTS__FILES_SEND, FileMetaData::MetaDataToPayload(files));
    }
    break;

    case (FETCH_FILE_ACK):
      m_system_event(client_fd, SYSTEM_EVENTS__FILES_SEND_ACK, {});
    break;

    case (FETCH_FILE_READY):
      m_system_event(client_fd, SYSTEM_EVENTS__FILES_SEND_READY, {});
    break;

    case (FETCH_TERM_HITS):
    {
      Payload event_args{};
      for (const auto& term_data : m_scheduler.FetchTermEvents())
        event_args.emplace_back(term_data.to_JSON());
      m_system_event(client_fd, SYSTEM_EVENTS__TERM_HITS, event_args);
    }
    break;
    case (EXECUTE):
      Execute(std::stoi(args.at(1)), args.at(2), client_fd);
    break;
    case(FETCH_POSTS):
      m_scheduler.FetchPosts();
    break;
    case(UPDATE_POST):
      m_scheduler.SavePlatformPost({args.begin() + 1, args.end()});
    break;
    case(KIQ_STATUS):
      m_system_event(client_fd, SYSTEM_EVENTS__STATUS_REPORT, { Status() });
    break;
    case (RequestType::UNKNOWN):
      [[ fallthrough ]];
    default:
      ELOG("Controller could not process unknown client request: {}", type);
  }
  m_client_rq_count++;
}
//----------------------------------------------------------------------------------
void Controller::onProcessComplete(const std::string& value,
                                   const int32_t&     mask,
                                   const std::string& id,
                                   const int32_t&     client_socket_fd,
                                   const bool&        error,
                                   const bool&        scheduled_task)
{
  using TaskVectorMap = std::map<int, std::vector<Task>>;
  using TaskVector    = std::vector<Task>;

  KLOG("Process complete notification for client {}'s request {}",
    client_socket_fd, id);

  m_process_event( // Inform system of process result
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
        [id](Task task) { return task.task_id == std::stoi(id); }
      );

      if (task_it != it->second.end())
      {
        uint8_t status{};

        if (error)
        {
          status = (task_it->completed == Completed::FAILED) ? Completed::RETRY_FAIL :
                                                               Completed::FAILED;
          KLOG("Sending email to administrator about failed task.\nNew Status: {}", Completed::STRINGS[status]);
          SystemUtils::SendMail(config::Email::notification(), Messages::TASK_ERROR_EMAIL + value);
          m_err_count++;
        }
        else
        {
          status = task_it->recurring ? Completed::SCHEDULED :
                                        Completed::SUCCESS;
          if (!m_scheduler.processTriggers(&*task_it))
            KLOG("Error occurred processing triggers for task {} with mask {}", task_it->id(), task_it->execution_mask);
          m_ps_exec_count++;
        }

        task_it->completed = status;                            // Update status
        m_scheduler.updateStatus(&*task_it, value);             // Failed tasks will re-run once more
        m_executor->saveResult(mask, 1, TimeUtils::UnixTime()); // Save execution result

        if (!error && task_it->recurring) m_scheduler.updateRecurring(&*task_it);

        if (task_it->notify)                                             // Send email notification
        {
          KLOG("Task notification enabled - emailing result to administrator");
          std::string email_string{};
          email_string.reserve(value.size() + 84);
          email_string += task_it->toString();
          email_string += error ? "\nError" : "\n";
          email_string += value;

          SystemUtils::SendMail(config::Email::notification(), email_string);
        }
      }
    }
  }
}
//----------------------------------------------------------------------------------
void Controller::onSchedulerEvent(const int32_t&                  client_socket_fd,
                                  const int32_t&                  event,
                                  const std::vector<std::string>& args)
{
  m_system_event(client_socket_fd, event, args);
}
//----------------------------------------------------------------------------------
std::string Controller::Status() const
{
  return m_server_status() + "\n\n" + fmt::format("Controller Status Update\nProcesses Executed: {}\nClient Requests: {}\nSystem Requests: {}\nErrors: {}",
    m_ps_exec_count, m_client_rq_count, m_system_rq_count, m_err_count) + "\n\n" + m_scheduler.Status();
}

}  // ns kiq
