#include "controller.hpp"
#include "common/time.hpp"
#include "kproto/types.hpp"
#include "server/event_handler.hpp"
#include <logger.hpp>

namespace kiq
{
using Payload = std::vector<std::string>;
static flatbuffers::FlatBufferBuilder builder(1024);
using namespace kiq::log;
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
    klog().i("Waiting for maintenance worker to complete");
    m_maintenance_worker.join();
  }

  if (m_sentinel_future.valid())
    m_sentinel_future.wait();
}
//----------------------------------------------------------------------------------
void Controller::Initialize(ProcessCallbackFn process_callback_fn,
                            StatusCallbackFn  status_callback_fn,
                            ClientValidateFn  validate_client_fn)
{
  const bool scheduled_task{true};
  m_executor = new ProcessExecutor();
  m_executor->setEventCallback(
    [this, scheduled_task](const std::string& result,
                           const int32_t&     mask,
                           const std::string& id,
                           const int32_t&     fd,
                           const bool&        error)
    {
      onProcessComplete(result, mask, id, fd, error, scheduled_task);
    }
  );

  m_process_event      = process_callback_fn;
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
          klog().t(message);
          SystemUtils::SendMail(config::Email::notification(), message);
          m_active = false;
        }
        std::this_thread::sleep_for(std::chrono::minutes(1));
      }
    }
  });

  SetWait(false);
  klog().i("Initialization complete");
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

  klog().i("Worker starting");

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
      klog().i("{} tasks found", tasks.size());

      for (const auto& task : tasks)
      {
        const auto formatted_time = TimeUtils::FormatTimestamp(task.datetime);
        scheduled_times += formatted_time + " ";
        klog().i("Task - Time: {} - Mask: {}\nEnv: {}", formatted_time, task.mask, task.env);
      }

      evt::instance()(client_fd, SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
        {std::to_string(tasks.size()) + " tasks need to be executed", scheduled_times});

      if (auto it = m_tasks_map.find(client_fd); it == m_tasks_map.end())
        m_tasks_map.insert(std::pair<int, std::vector<Task>>(client_fd, tasks));
      else
        it->second.insert(it->second.end(), tasks.begin(), tasks.end());

      klog().i("{} Tasks pending execution: ", m_tasks_map.at(client_fd).size());
    }

    HandlePendingTasks();

    if (timer.expired())
    {
      klog().t("{}", Status());
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
  // try
  // {
    for (const auto& client_tasks : m_tasks_map)
      for (const auto& task : client_tasks.second)
        m_executor->executeTask(client_tasks.first, task);
    m_tasks_map.clear();
  // }
  // catch(const std::exception& e)
  // {
  //   const auto error = fmt::format("Exception caught while executing process: {}", e.what());
  //   klog().e(error.c_str());
  //   SystemUtils::SendMail(config::System::admin(), error);
  // }
}
//----------------------------------------------------------------------------------
void Controller::operator()(const KOperation&               op,
                            const std::vector<std::string>& argv,
                            const int32_t&                  client_fd,
                            const std::string&              uuid)
{
  using namespace FileUtils;

  if (op != "Schedule" || argv.empty()) return;

  const auto mask = argv.at(TaskIndexes::MASK);
  const auto application = ProcessExecutor::GetAppInfo(std::stoi(mask));
  klog().i("Handling schedule request application {} with mask {}", application.name, mask);

  if (application.is_valid())
  {
    Task tk;
    if (application.name == Name::INSTAGRAM)
    {
      klog().i("Instagram Task requested");
      IGTaskHandler ig_task_handler{};
      ig_task_handler.prepareTask(argv, uuid, &tk);
    }
    else
    {
      klog().i("Generic Task requested");
      GenericTaskHandler generic_task_handler{};
      generic_task_handler.prepareTask(argv, uuid, &tk);
    }

    for (const auto& info : tk.files)
    {
      klog().i("Task file: {}", info.first);
      Payload args{info.first, info.second, uuid};

      if (tk.files.size() == 1) args.push_back("final file");

      evt::instance()(client_fd, SYSTEM_EVENTS__FILE_UPDATE, args);
    }

    if (tk.validate())
    {
      klog().i("Sending task request to Scheduler");

      auto id = m_scheduler.schedule(tk);
      if (!id.empty())
      {
        Payload args;
        args.insert(args.end(), {uuid, id,std::to_string(tk.mask), ReadEnvFile(tk.env), std::to_string(tk.files.size())});

        for (auto&& file : tk.files)
          args.push_back(file.first);

        evt::instance()(client_fd, SYSTEM_EVENTS__SCHEDULER_SUCCESS, args);
      }
      else
        klog().i("Task with UUID {} was validated, but scheduling failed", uuid);
    }
    else
      klog().i("Task with UUID {} was processed, but did not pass validation", uuid);
  }
  else
    klog().i("Task scheduling failed to match app to mask {}", mask);
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
  klog().i("Execute request received.\nMask: {}  ID: {}", mask, request_id);
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
    evt::instance()(client_socket_fd,
                   SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
                   {"PROCESS RUNNER - Process execution requested for applications with mask " + std::to_string(mask),
                   request_id});

    executor.request(row.second, mask, client_socket_fd, request_id, {}, constants::IMMEDIATE_REQUEST);
  }
}
//----------------------------------------------------------------------------------
void Controller::ProcessSystemEvent(const int32_t&                  event,
                                    const std::vector<std::string>& payload,
                                    const int32_t&                  id)
{
  klog().t("Processing event {}", event);
  switch (event)
  {
    case (SYSTEM_EVENTS__PLATFORM_REQUEST):
      m_scheduler.OnPlatformRequest(payload);
    break;
    case (SYSTEM_EVENTS__PLATFORM_NEW_POST):
      m_scheduler.SavePlatformPost<std::vector<std::string>>(payload);
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
  auto ReadTask = [](const Task& tk, Payload& payload)
  {
    payload.emplace_back(               tk.id());
    payload.emplace_back(               tk.name);
    payload.emplace_back(               tk.datetime);
    payload.emplace_back(               tk.flags);
    payload.emplace_back(std::to_string(tk.completed));
    payload.emplace_back(std::to_string(tk.recurring));
    payload.emplace_back(std::to_string(tk.notify));
    payload.emplace_back(               tk.runtime);
    payload.emplace_back(               tk.filesToString());
  };

  Payload     args = GetArgs(message);
  RequestType type = int_to_request_type(std::stoi(args.at(Request::REQUEST_TYPE_INDEX)));

  klog().t("Controller processing client request of type {}", request_type_to_string(type));

  switch (type)
  {
    case(RequestType::GET_APPLICATION):
    {
      KApplication application = Registrar::args_to_application(args);
        evt::instance()(client_fd,
          (m_registrar.find(Registrar::args_to_application(args))) ? SYSTEM_EVENTS__REGISTRAR_SUCCESS :
                                                                     SYSTEM_EVENTS__REGISTRAR_FAIL,
          application.vector());
      break;
    }

    case (RequestType::REGISTER_APPLICATION):
    {
      KApplication application = Registrar::args_to_application(args);
      auto         id          = m_registrar.add(application);
      (!id.empty()) ?
        evt::instance()(client_fd, SYSTEM_EVENTS__REGISTRAR_SUCCESS,
          DataUtils::VAbsorb(std::move(application.vector()), std::move(id))) :
        evt::instance()(client_fd, SYSTEM_EVENTS__REGISTRAR_FAIL, application.vector());
      break;
    }

    case (RequestType::REMOVE_APPLICATION):
    {
      KApplication application = Registrar::args_to_application(args);
      auto         name        = m_registrar.remove(application);
      (!name.empty()) ?
        evt::instance()(client_fd, SYSTEM_EVENTS__REGISTRAR_SUCCESS,
          DataUtils::VAbsorb(std::move(application.vector()),  std::move(std::string{"Application was deleted"}))) :
        evt::instance()(client_fd, SYSTEM_EVENTS__REGISTRAR_FAIL,
          DataUtils::VAbsorb(std::move(application.vector()), std::move(std::string{"Failed to delete application"})));
      break;
    }

    case (RequestType::UPDATE_APPLICATION):
      klog().i("Must implement UPDATE_APPLICATION");
    break;

    case (RequestType::FETCH_SCHEDULE):
    {
      try
      {
        std::vector<Task>    tasks             = m_scheduler.fetchAllTasks();
        const auto           size              = tasks.size();
        Payload              payload{"Schedule"};
        klog().i("Processing schedule fetch request");
        klog().i("Fetched {} scheduled tasks for client", size);
        for (const auto& tk : tasks)
          ReadTask(tk, payload);
        evt::instance()(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, payload);
        evt::instance()(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH, {"Schedule end", std::to_string(size)});
      }
      catch(const std::exception& e)
      {
        klog().e("Exception thrown from while fetching tasks: {}", e.what());
      }
    }
    break;
    case (RequestType::UPDATE_SCHEDULE):
    {
      Task               recvd_task   = args_to_task(args);
      const std::string& task_id      = recvd_task.id();
      bool               save_success = m_scheduler.update(recvd_task);
      bool               env_updated  = m_scheduler.updateEnvfile(task_id, recvd_task.env);

      evt::instance()(client_fd, SYSTEM_EVENTS__SCHEDULER_UPDATE, {task_id, (save_success) ? "Success" : "Failure"});

      if (!env_updated)
        klog().i("Failed to update envfile while handling update for task {}", task_id);
    }
    break;
    case (RequestType::FETCH_SCHEDULE_TOKENS):
    {
      auto id = args.at(constants::PAYLOAD_ID_INDEX);
      Task task = m_scheduler.GetTask(id);
      if (task.validate())
        evt::instance()(client_fd, SYSTEM_EVENTS__SCHEDULER_FETCH_TOKENS,
          DataUtils::VAbsorb(std::move(FileUtils::ReadFlagTokens(task.env, task.flags)),
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

      for (auto&& tk : tasks)
      {
        payload.emplace_back(tk.id());
        for (const auto& flag : FileUtils::ExtractFlagTokens(tk.flags))
        {
          payload.emplace_back(flag);
          payload.emplace_back(FileUtils::ReadEnvToken(tk.env, flag));
        }
      }

      evt::instance()(client_fd, SYSTEM_EVENTS__TASK_DATA,       payload);
      evt::instance()(client_fd, SYSTEM_EVENTS__TASK_DATA_FINAL, {});
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
      evt::instance()(client_fd, event_type, event_args);
      break;
    }

    case (TASK_FLAGS):
      evt::instance()(client_fd, SYSTEM_EVENTS__TASK_FETCH_FLAGS,
        DataUtils::VAbsorb(std::move(m_scheduler.getFlags(args.at(1))), std::move(args.at(1))));
    break;

    case (FETCH_FILE):
    {
      if (const auto files = m_scheduler.getFiles(Payload{args.begin() + 1, args.end()}); !files.empty())
        evt::instance()(client_fd, SYSTEM_EVENTS__FILES_SEND, FileMetaData::MetaDataToPayload(files));
    }
    break;

    case (FETCH_FILE_ACK):
      evt::instance()(client_fd, SYSTEM_EVENTS__FILES_SEND_ACK, {});
    break;

    case (FETCH_FILE_READY):
      evt::instance()(client_fd, SYSTEM_EVENTS__FILES_SEND_READY, {});
    break;

    case (FETCH_TERM_HITS):
    {
      Payload event_args{};
      for (const auto& term_data : m_scheduler.FetchTermEvents())
        event_args.emplace_back(term_data.to_JSON());
      evt::instance()(client_fd, SYSTEM_EVENTS__TERM_HITS, event_args);
    }
    break;
    case (CONVERT_TASK):
      m_scheduler.SavePlatformPost<std::string>(args.at(constants::CONVERT_TASK_DATA_INDEX));
    break;
    case (EXECUTE):
      Execute(std::stoi(args.at(1)), args.at(2), client_fd);
    break;
    case(FETCH_POSTS):
      m_scheduler.FetchPosts();
    break;
    case(UPDATE_POST):
      m_scheduler.SavePlatformPost<Payload>({args.begin() + 1, args.end()});
    break;
    case(KIQ_STATUS):
      evt::instance()(client_fd, SYSTEM_EVENTS__STATUS_REPORT, { Status() });
    break;
    case (RequestType::UNKNOWN):
      [[ fallthrough ]];
    default:
      klog().e("Controller could not process unknown client request: {}", type);
  }
  m_client_rq_count++;
}
//----------------------------------------------------------------------------------
void Controller::onProcessComplete(const std::string& out,
                                   const int32_t&     mask,
                                   const std::string& id,
                                   const int32_t&     fd,
                                   const bool&        err,
                                   const bool&        scheduled)
{
  using TaskVectorMap = std::map<int, std::vector<Task>>;
  using TaskVector    = std::vector<Task>;
  using namespace Completed;

  auto find_task = [id](const auto tk) { return tk.task_id == std::stoi(id); };

  klog().i("Process complete notification for client {}'s request {}", fd, id);
  m_process_event(out, mask, id, fd, err);

  if (!scheduled)
    return;

  (err) ? klog().e("Error was returned") : klog().i("Task {} for client {} OK", id, fd);

  const auto it = m_tasks_map.find(fd);
  if (it == m_tasks_map.end())
    return;

  auto tk_it = std::find_if(it->second.begin(), it->second.end(), find_task);
  if (tk_it == it->second.end())
    return;

  if (err)
  {
    tk_it->completed = (tk_it->completed == FAILED) ? RETRY_FAIL : FAILED;
    klog().i("Sending email to administrator about failed task.\nNew Status: {}", STRINGS[tk_it->completed]);
    SystemUtils::SendMail(config::Email::notification(), Messages::TASK_ERROR_EMAIL + out);
    m_err_count++;
  }
  else
  {
    tk_it->completed = tk_it->recurring ? SCHEDULED : SUCCESS;
    if (!m_scheduler.processTriggers(&*tk_it))
      klog().i("Error occurred processing triggers for task {} with mask {}", tk_it->id(), tk_it->mask);
    m_ps_exec_count++;
  }

  m_scheduler.updateStatus(&*tk_it, out);                        // Failed tasks will re-run once more
  m_executor->saveResult(mask, 1, TimeUtils::UnixTime());

  if (!err && tk_it->recurring)
    m_scheduler.updateRecurring(&*tk_it);

  if (tk_it->notify)
  {
    klog().i("Task notification enabled - emailing result to administrator");
    std::string email_s = tk_it->toString();
    email_s += err ? "\nError" : "\n";
    email_s += out;
    SystemUtils::SendMail(config::Email::notification(), email_s);
  }
}
//----------------------------------------------------------------------------------
void Controller::onSchedulerEvent(const int32_t&                  client_socket_fd,
                                  const int32_t&                  event,
                                  const std::vector<std::string>& args)
{
  evt::instance()(client_socket_fd, event, args);
}
//----------------------------------------------------------------------------------
std::string Controller::Status() const
{
  return m_server_status() + "\n\n" + fmt::format("Controller Status Update\nProcesses Executed: {}\nClient Requests: {}\nSystem Requests: {}\nErrors: {}",
    m_ps_exec_count, m_client_rq_count, m_system_rq_count, m_err_count) + "\n\n" + m_scheduler.Status();
}

}  // ns kiq
