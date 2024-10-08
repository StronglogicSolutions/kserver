#include "common/time.hpp"
#include "scheduler.hpp"
#include "executor/task_handlers/generic.hpp"
#include "server/event_handler.hpp"
#include <logger.hpp>

namespace kiq
{
using namespace kiq::log;

static Timer timer;
static const bool        IMMEDIATELY          = false;
static const size_t      QUEUE_LIMIT          = 0x05;
static const std::string IPC_MESSAGE_HEADER   = "KIQ is now tracking the following terms\n";
static const uint32_t    PLATFORM_REQUEST     = SYSTEM_EVENTS__PLATFORM_POST_REQUESTED;
static const Fields      DEFAULT_TASK_FIELDS  = {Field::ID, Field::TIME, Field::MASK, Field::FLAGS,
                                                 Field::ENVFILE, Field::COMPLETED, Field::NOTIFY, Field::RECURRING};
static const std::string FILEQUERY            = {"(SELECT string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files"};
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

IPCMessage::payload_t
IPCMessage::get_payload() const
{
  return m_args;
}
//------------------------
IPCMessage::payload_t
IPCMessage::platform_payload(std::function<std::string(std::string)> get_user) const
{
  const auto dest = option();
  klog().d("Dest: {}", dest);
  const auto args = CreateOperation("bot",
    { command(), data(), config::Platform::default_user(), dest.empty() ? config::Process::tg_dest() : dest }
  );
  return {
    type_name(), platform(), p_uuid(), get_user(platform()), time(), data(), "", "false", "bot", args, std::to_string(get_ipc_cmd_code(command()))
  };
}
//------------------------
void
IPCMessage::set_validate(bool validate)
{
  m_validate = validate;
}
//------------------------
std::string
IPCMessage::type_name() const
{
  return IPC_MESSAGE_NAMES.at(m_type);
}
//------------------------
uint8_t
IPCMessage::type() const
{
  return m_type;
}
//------------------------
std::string
IPCMessage::platform() const
{
  return m_args.at(1);
}
//------------------------
std::string
IPCMessage::data() const
{
  return m_args.at(2);
}
//------------------------
std::string
IPCMessage::command() const
{
  const auto cmd = m_args.at(3);
  if (cmd.empty())
    return std::to_string(std::numeric_limits<uint32_t>::max());
  return cmd;
}
//------------------------
std::string
IPCMessage::cmd_code_string() const
{
  return std::to_string(IPC_CMD_CODES.at(command()));
}
//------------------------
std::string
IPCMessage::time() const
{
  if (size() > 4)
    if (const auto value = m_args.at(4); !value.empty())
      return value;

  return TimeUtils::Now();
}
//------------------------
std::string
IPCMessage::recurring() const
{
  if (size() > 5)
    if (const auto& recur = m_args.at(5); !recur.empty())
      return recur;

  return std::to_string(Constants::Recurring::NO);
}
//------------------------
std::string
IPCMessage::status() const
{
  if (size() > 6)
    return m_args.at(6);
  return "";
}
//------------------------
std::string
IPCMessage::id() const
{
  if (size() > 7)
    return m_args.at(7);
  return "";
}
//------------------------
std::string
IPCMessage::p_uuid() const
{
  if (size() > 8)
  {
    if (const auto& uuid = m_args.at(8); !uuid.empty())
      return uuid;
  }
  return StringUtils::GenerateUUIDString();
}
//------------------------
std::string
IPCMessage::option() const
{
  if (size() > 9)
    return m_args.at(9);
  return "";
}
//------------------------

size_t
IPCMessage::size() const
{
  return m_args.size();
}

//----------------------------------------------------------------------------------------------------------------
IPCSendEvent Scheduler::MakeIPCEvent(int32_t event, TGCommand command, const std::string& data, const std::string& arg)
{
  using namespace DataUtils;
  auto EventPost = [](auto pid, auto user, auto cmd, auto data = "", auto arg = "")
  {
    return PlatformPost{
        pid,
        constants::NO_ORIGIN_PLATFORM_EXISTS,
        StringUtils::GenerateUUIDString(),
        user,
        TimeUtils::Now(),
        data,
        "",
        "",
        "Telegram",
        arg,
        "bot",
        cmd};
  };
        auto cmd_s   = std::to_string(static_cast<uint8_t>(command));
  const auto pid     = m_platform.GetPlatformID("Telegram");
  const auto user    = m_platform.GetUser("", pid, true);
  const auto ipc_evt = EventPost(pid, user, cmd_s, data, arg);
  switch (event)
  {
    case (SYSTEM_EVENTS__KIQ_IPC_MESSAGE):
    case (SYSTEM_EVENTS__PLATFORM_POST_REQUESTED):
      return IPCSendEvent{event, ipc_evt.GetPayload()};
    default:
      return IPCSendEvent{};
  }
}
//----------------------------------------------------------------------
using DateRange = std::pair<std::string, std::string>;
const auto GetDateRange = [](const std::string& date_range_s) -> DateRange
{
  static const std::string MAX_INT = std::to_string(std::numeric_limits<int32_t>::max());
  if (date_range_s.front() == '0')
    return DateRange{"0", MAX_INT};

  const auto pos = date_range_s.find_first_of("TO");
  return DateRange{date_range_s.substr(0, (date_range_s.size() - pos - 2)), date_range_s.substr(pos + 1)};
};
//----------------------------------------------------------------------
Scheduler::Scheduler(Database::KDB&& kdb)
    : m_kdb(std::move(kdb)),
      m_platform(nullptr),
      m_trigger(nullptr),
      m_research_manager(&m_kdb, &m_platform, [this](const auto& name)
                          { return FindMask(name); }),
      m_research_polls{}
{
  klog().i("Scheduler instantiated for testing");
}
//----------------------------------------------------------------------------------------------------------------
static Scheduler::ApplicationMap FetchApplicationMap(Database::KDB& db)
{
  static const std::string  table {"apps"};
  static const Fields       fields{"mask", "name"};
  Scheduler::ApplicationMap map   {};

  for (const auto& row : db.select(table, fields))
    map.insert({ std::stoi(row.at("mask")), row.at("name")});

  return map;
}
//----------------------------------------------------------------------------------------------------------------
Scheduler::Scheduler(SystemEventcallback fn)
    : m_event_callback(fn),
      m_kdb(Database::KDB{}),
      m_platform(fn),
      m_trigger(&m_kdb),
      m_app_map(FetchApplicationMap(m_kdb)),
      m_research_manager(&m_kdb, &m_platform, [this](const auto &name)
                          { return FindMask(name); }),
      m_ipc_command(constants::NO_COMMAND_INDEX)
{
  const auto AppExists = [this](const std::string& name) -> bool
  {
    auto it = std::find_if(m_app_map.begin(), m_app_map.end(),
                            [name](const ApplicationInfo& info)
                            { return info.second == name; });
    auto found = it != m_app_map.end();
    return found;
  };

  try
  {
    for (int i = 0; i < REQUIRED_APPLICATION_NUM; i++)
      if (!AppExists(REQUIRED_APPLICATIONS[i]))
        throw std::runtime_error{("Required application was missing. {}", REQUIRED_APPLICATIONS[i])};
  }
  catch (const std::exception &e)
  {
    klog().e(e.what());
    throw;
  }
}
//----------------------------------------------------------------------------------------------------------------
Scheduler::~Scheduler()
{
  klog().i("Scheduler destroyed");
}
//----------------------------------------------------------------------------------------------------------------
template <typename T>
bool Scheduler::HasRecurring(const T& id)
{
  std::string task_id;
  if constexpr (std::is_integral<T>::value)
    task_id = std::to_string(id);
  else
    task_id = id;

  return !(m_kdb.select("recurring", {"id"}, CreateFilter("sid", task_id)).empty());
}
//----------------------------------------------------------------------------------------------------------------
template bool Scheduler::HasRecurring(const std::string& id);
template bool Scheduler::HasRecurring(const int32_t &id);
//----------------------------------------------------------------------------------------------------------------
std::string Scheduler::schedule(Task task)
{
  const auto IsImageExtension = [](const std::string& ext) -> bool
  {
    return ((ext == "jpg") || (ext == "jpeg") || (ext == "png") || (ext == "gif") || (ext == "bmp") || (ext == "svg"));
  };
  const auto IsVideoExtension = [](const std::string& ext) -> bool
  {
    return ((ext == "mp4") || (ext == "avi") || (ext == "mkv") || (ext == "webm"));
  };
  const auto GetFileType = [IsImageExtension, IsVideoExtension](const std::string& filename) -> const std::string
  {
    std::string extension = filename.substr(filename.find_last_of('.') + 1);
    const std::string ext = StringUtils::ToLower(extension);
    if (IsImageExtension(ext))
      return "image";
    if (IsVideoExtension(ext))
      return "video";
    return "unknown";
  };

  std::string  id;
  const auto   table {"schedule"};
  const Fields fields{"time", "mask", "flags", "envfile", "recurring", "notify"};
  const Values values{task.datetime, std::to_string(task.mask), task.flags,
                      task.env,  std::to_string(task.recurring),      std::to_string(task.notify)};

  if (task.validate())
  {
    try
    {
      if (id = m_kdb.insert(table, fields, values, "id"); !id.empty())
      {
        klog().i("Request to schedule task was accepted\nID {}", id);

        for (const auto file : task.files)
        {
          const Fields f_fields = {"name", "sid", "type"};
          const Values f_values = {file.first, id, GetFileType(file.first)};
          klog().i("Recording file in DB: {}", file.first);
          m_kdb.insert("file", f_fields, f_values);
        }

        if (task.recurring)
        {
          const auto recurring = std::to_string(std::stoi(task.datetime) - GetIntervalSeconds(task.recurring));
          m_kdb.insert("recurring", {"sid", "time"}, {id, recurring});
        }
      }
      else
        klog().e("Failed to insert task into database: {}", task.toString());
    }
    catch (const pqxx::sql_error &e)
    {
      klog().e("Insert query failed: {}", e.what());
    }
    catch (const std::exception &e)
    {
      klog().e("Insert query failed: {}", e.what());
    }
  }
  else
    klog().e("Failed to validate task: {}", task.toString());

  return id;
}
//----------------------------------------------------------------------------------------------------------------
Task Scheduler::parseTask(QueryValues&& result, bool parse_files, bool is_recurring)
{
  Task task;
  int  notify{-1};
  bool checked_for_files{false};
  std::string TIME_FIELD = (is_recurring) ? Field::REC_TIME : Field::TIME;

  for (const auto &row : result)
  {
    task.mask      = std::stoi(row.at(Field::MASK));
    task.flags     = row.at(Field::FLAGS);
    task.env       = row.at(Field::ENVFILE);
    task.datetime  = row.at(TIME_FIELD);
    task.task_id   = std::stoi(row.at(Field::ID));
    task.completed = std::stoi(row.at(Field::COMPLETED));
    task.recurring = std::stoi(row.at(Field::RECURRING));
    notify         = row.at(Field::NOTIFY) == "t";

      // for (const auto& filename : StringUtils::Split(v.at(FILEQUERY), ' '))
      //   TODO: get data
      // checked_for_files = true;

    if (task.recurring != Constants::Recurring::NO)
      task.datetime = TimeUtils::time_as_today(task.datetime);
    task.runtime  = FileUtils::ReadRunArgs(task.env);
    task.name     = m_app_map.at(task.mask);
    task.notify   = notify == 1;

    if (task.filenames.empty())
    {
      for (const auto& file : getFiles({task.id()}))
      {
        task.filenames.push_back(file.name);
        task.files.push_back({file.name, ""});
      }
    }
  }

  return task;
}
//----------------------------------------------------------------------------------------------------------------
std::vector<Task> Scheduler::parseTasks(QueryValues&& result, bool parse_files, bool is_recurring, bool original_time)
{
  auto has_rec = [](const auto& row) { return row.find(Field::REC_TIME) != row.end(); };
  int               recurring = -1;
  std::string       TIME_FIELD, env, time, mask, filenames;
  std::vector<Task> tasks;

  for (const auto &row : result)
  {
    recurring = std::stoi(row.at(Field::RECURRING));
    env       = row.at(Field::ENVFILE);
    mask      = row.at(Field::MASK);
    time      = row.at( (original_time) ? Field::TIME : (recurring && has_rec(row)) ? Field::REC_TIME : Field::TIME);
    filenames = row.at(FILEQUERY);

    if (!original_time && recurring)
      time = TimeUtils::time_as_today(time);

    tasks.push_back(Task{
      .mask      = std::stoi(mask),
      .datetime  = time,
      .file      = true,
      .files     = {},
      .env       = env,
      .flags     = row.at(Field::FLAGS),
      .task_id   = std::stoi(row.at(Field::ID)),
      .completed = std::stoi(row.at(Field::COMPLETED)),
      .recurring = std::stoi(row.at(Field::RECURRING)),
      .notify    = row.at(Field::NOTIFY) == "t",
      .runtime   = FileUtils::ReadRunArgs(env), // ⬅ set from DB?
      .filenames = StringUtils::Split(filenames, ' '),
      .name      = m_app_map.at(std::stoi(mask))});
  }

  return tasks;
}
//----------------------------------------------------------------------------------------------------------------
using tasks_t = std::vector<Task>;
tasks_t
Scheduler::fetchTasks(const std::string& mask,
                      const std::string& range,
                      const std::string& count,
                      const std::string& max_id, // TODO: use boolean to alternate date_range or id_range
                      const std::string& order)
{
  using Filter_t = std::vector<std::variant<CompBetweenFilter, QueryFilter>>;
  static const Fields fields{Field::ID,      Field::TIME,      Field::MASK,   Field::FLAGS,
                             Field::ENVFILE, Field::COMPLETED, Field::NOTIFY, Field::RECURRING};
  const auto date_range   = GetDateRange(range);
  const auto order_filter = OrderFilter{Field::ID, order};
  const auto limit_filter = LimitFilter{count};
  const Filter_t filters  = {CompBetweenFilter{Field::TIME, date_range.first, date_range.second},
                            CreateFilter(Field::MASK, mask)};

  return parseTasks((m_kdb.selectMultiFilter("schedule", fields, filters, order_filter, limit_filter)));
}
//----------------------------------------------------------------------------------------------------------------
std::vector<Task> Scheduler::fetchTasks()
{
  using Filters_t = std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>;
  const auto table   = "schedule";
  const auto fields  = DEFAULT_TASK_FIELDS;
  const auto filters = Filters_t{CompFilter{"recurring", "0", "="}, CompFilter{UNIXTIME_NOW, Field::TIME, ">"},
                                  MultiOptionFilter{"completed", "IN", {Completed::STRINGS[Completed::SCHEDULED], Completed::STRINGS[Completed::FAILED]}}};

  auto tasks = parseTasks(m_kdb.selectMultiFilter(table, fields, filters, OrderFilter{Field::ID, "ASC"}));
  for (auto &&task : tasks)
    for (const auto &file : getFiles(task.id()))
      task.filenames.emplace_back(file.name);
  return tasks;
}
//----------------------------------------------------------------------------------------------------------------
std::vector<Task> Scheduler::fetchRecurringTasks()
{
  using Filters = std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>;
  static const bool parse_files{true};
  static const bool recurring  {true};
  static const Fields fields{Field::ID,      Field::TIME,      Field::MASK, Field::FLAGS,
                             Field::ENVFILE, Field::COMPLETED, Field::RECURRING,
                             Field::NOTIFY, "recurring.time", FILEQUERY};
  static const Filters filters{
      CompFilter{Field::RECURRING, "0", "<>"},
      CompFilter{UNIXTIME_NOW, "recurring.time", ">"},
      MultiOptionFilter{"completed", "IN", {Completed::STRINGS[Completed::SCHEDULED], Completed::STRINGS[Completed::FAILED]}}};
  static const Joins joins{{"recurring", "sid", "schedule", "id", JoinType::INNER},
                            {"file", "sid", "schedule", "id", JoinType::OUTER}};

  return parseTasks(m_kdb.selectJoin<Filters>(
                        "schedule", fields, filters, joins, OrderFilter{Field::ID, "ASC"}),
                    parse_files, recurring);
}
//----------------------------------------------------------------------------------------------------------------
std::vector<Task> Scheduler::fetchAllTasks(bool original_time)
{
  static const Fields fields  {Field::ID,        Field::TIME,      Field::MASK, Field::FLAGS, Field::ENVFILE,
                               Field::COMPLETED, Field::RECURRING, Field::NOTIFY, FILEQUERY};
  static const Joins joins{{"file", "sid", "schedule", "id", JoinType::OUTER}};

  return parseTasks(m_kdb.selectJoin<QueryFilter>("schedule", fields, {}, joins), true, original_time);
}
//----------------------------------------------------------------------------------------------------------------
Task Scheduler::GetTask(const std::string& id)
{
  static const Fields fields  {Field::ID,        Field::TIME,      Field::MASK, Field::FLAGS, Field::ENVFILE,
                               Field::COMPLETED, Field::RECURRING, Field::NOTIFY, FILEQUERY};
  static const Joins joins{{"file", "sid", "schedule", "id", JoinType::OUTER}};

  return parseTask(m_kdb.selectJoin<QueryFilter>("schedule", fields, {CreateFilter("schedule.id", id)}, joins), true);
}
//----------------------------------------------------------------------------------------------------------------
std::vector<FileMetaData> Scheduler::getFiles(const std::vector<std::string>& sids, const std::string& type)
{
  std::vector<FileMetaData> files;
  files.reserve(sids.size());

  for (const auto& sid : sids)
  {
    const QueryFilter filter = (type.empty()) ? CreateFilter("sid", sid) : CreateFilter("sid", sid, "type", type);
    FileMetaData file;

    for (const auto& row : m_kdb.select("file", {"id", "name", "type"}, filter))
    {
      file.id      = row.at("id");
      file.name    = row.at("name");
      file.type    = row.at("type");
      file.task_id = sid;
      files.push_back(file);
      file.clear();
    }
  }

  return files;
}
//----------------------------------------------------------------------------------------------------------------
std::vector<FileMetaData> Scheduler::getFiles(const std::string& sid, const std::string& type)
{
  const QueryFilter filter = (type.empty()) ? CreateFilter("sid", sid) : CreateFilter("sid", sid, "type", type);

  std::vector<FileMetaData> files;
  FileMetaData              file;

  for (const auto &row : m_kdb.select("file", {"id", "name", "type"}, filter))
  {
    file.id      = row.at("id");
    file.name    = row.at("name");
    file.type    = row.at("type");
    file.task_id = sid;
    files.push_back(file);
    file.clear();
  }

  return files;
}
//----------------------------------------------------------------------------------------------------------------
[[deprecated]] Task Scheduler::GetTask(int id)
{
  Task task = parseTask(m_kdb.select("schedule", {Field::MASK, Field::FLAGS, Field::ENVFILE, Field::TIME,
                                                  Field::COMPLETED, Field::ID},
                                     CreateFilter("id", std::to_string(id))));

  for (const auto &file : getFiles(std::to_string(id))) // files
    task.filenames.push_back(file.name);

  return task;
}
//----------------------------------------------------------------------------------------------------------------
template std::vector<std::string> Scheduler::getFlags(const std::string& mask);
template std::vector<std::string> Scheduler::getFlags(const uint32_t&    mask);
//----------------------------------------------------------------------------------------------------------------
template <typename T>
std::vector<std::string> Scheduler::getFlags(const T& mask)
{
  std::string filter_mask;
  if constexpr (std::is_same_v<std::string, T>)
    filter_mask = mask;
  else
  if constexpr (std::is_integral<T>::value)
    filter_mask = std::to_string(mask);

  const auto& rows = m_kdb.select("schedule", {Field::FLAGS}, CreateFilter("mask", filter_mask), 1);
  if (!rows.empty())
    return StringUtils::Split(rows.front().at(Field::FLAGS), ' ');

  klog().e("No task exists with that mask");
  return {};
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::updateStatus(Task* task, const std::string& output)
{
  static const std::string table     = "schedule";
  static const Fields      fields    = {"completed"};
         const Values      values    = {std::to_string(task->completed)};
         const QueryFilter filter    = CreateFilter("id", task->id());
         const std::string returning = "id";
  return (!m_kdb.update(table, fields, values, filter, returning).empty());
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::update(Task task)
{
  if (IsRecurringTask(task))
    if (!HasRecurring(task.task_id))
      m_kdb.insert("recurring", {"sid", "time"}, {task.id(), std::to_string(std::stoi(task.datetime) - GetIntervalSeconds(task.recurring))});
    else
      m_kdb.update("recurring", {"time"},
                    {std::to_string(std::stoi(task.datetime) - GetIntervalSeconds(task.recurring))},
                    CreateFilter("sid", task.id()));

  return !m_kdb.update("schedule", {"mask", "time", "flags", "completed", "recurring", "notify", "runtime"},
                        {std::to_string(task.mask), task.datetime, task.flags, std::to_string(task.completed),
                        std::to_string(task.recurring), std::to_string(task.notify), task.runtime},
                        CreateFilter("id", task.id()),
                        "id")
              .empty();
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::updateRecurring(Task* task)
{
  auto time = std::to_string(std::stoi(task->datetime) + GetIntervalSeconds(task->recurring));
  klog().i("{} task {} scheduled for {}", Constants::Recurring::names[task->recurring], task->id(), time);
  return !m_kdb.update("recurring", {"time"}, {time}, CreateFilter("sid", task->id()), "id").empty();
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::updateEnvfile(const std::string& id, const std::string& env)
{
  const auto rows = m_kdb.select("schedule", {"envfile"}, CreateFilter("id", id));
  if (!rows.empty())
    return false;

  FileUtils::SaveFile(env, rows.front().at("envfile"));
  return true;
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::isKIQProcess(uint32_t mask)
{
  return ProcessExecutor::GetAppInfo(mask).is_kiq;
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::PostExecWork(ProcessEventData &&event, Scheduler::PostExecDuo applications)
{
  using namespace FileUtils;
  static const IPCSendEvent IPC_QUEUE_HEADER_DATA = MakeIPCEvent(
      SYSTEM_EVENTS__PLATFORM_POST_REQUESTED,
      TGCommand::message,
      IPC_MESSAGE_HEADER,
      CreateOperation("Bot", {config::Process::tg_dest()}));
  const auto &map = m_postexec_map;
  const auto &lists = m_postexec_lists;
  auto InitQueue = [this]()
  { m_message_queue.clear(); m_message_queue.emplace_back(IPC_QUEUE_HEADER_DATA); };
  auto NotScheduled = [this](auto id)
  { return m_postexec_map.find(std::stoi(id)) == m_postexec_map.end(); };
  auto CompleteTask = [&map, &lists](const int32_t &id)
  {
    auto pid = map.at(id).first;
    auto node = FindNode(lists.at(pid), id);
    node->complete = true;
  };
  auto SequenceTasks = [this](const std::vector<TaskParams> &v, bool enforce_unique = true)
  {
    const auto& map = m_postexec_map;
    int32_t      id = v.front().id;
    for (const auto &prms : v)
    {
      if (enforce_unique)
      {
        const auto  mask  = FindMask(prms.name);
        const auto& data  = prms.data;
        const auto  match = std::find_if(map.begin(), map.end(), [&data, &mask](auto tup)
        {
          TaskWrapper wrap = tup.second.second;
          const auto& task = wrap.task;
          return (task.mask == mask && task.GetToken(constants::DESCRIPTION_KEY) == data);
        });
        if (match != map.end())
          continue;
      }

      id = CreateChild(id, prms.data, prms.name, prms.args);
    }
  };
  auto FindRoot   = [&map, &lists](int32_t id)   { return lists.at(map.at(id).first); };
  auto FindTask   = [&map]        (int32_t id)   { return map.at(id).second.task; };
  auto GetAppName = [this]        (int32_t mask) { return m_app_map.at(mask); };
  auto Sanitize   = []            (auto &item)   { item.value = StringUtils::RemoveTags(item.value); };
  auto Finalize   = [&map, this, CompleteTask](int32_t id)
  {
    CompleteTask(id);
    if (AllTasksComplete(map))
    {
      if (m_message_queue.size() < 2)
      {
        klog().i("Research results: no actions");
        m_message_queue.clear();
      }
      klog().i("Resolving pending IPC from Finalize()");
      ResolvePending(IMMEDIATELY);
    }
  };
  auto GetTokens = [](const auto &p)
  {
    std::vector<JSONItem> v{};
    for (size_t i = 1; i < (p.size() - 1); i += 2)
      v.emplace_back(JSONItem{p[i], p[i + 1]});
    return v;
  };
  auto OnTermEvent = [this](const TermEvent& term_info)
  {
    if (const auto tid = std::stoi(term_info.tid); m_term_ids.find(tid) == m_term_ids.end())
    {
      m_message_queue.front().append_msg(term_info.to_str());
      m_term_ids.insert(tid);
    }
  };
  auto AnalyzeTW = [this, &event, &GetTokens](const auto& root, const auto& child, const auto& subchild)
  {
    auto QueueFull  = [this]                 { return m_message_queue.size() > QUEUE_LIMIT; };
    auto PollExists = [this](const auto &id) { return m_research_polls.find(id) != m_research_polls.end(); };

    if (QueueFull())
      return klog().d("Outbound IPC queue is full");

    const auto event = PLATFORM_REQUEST;
    for (const auto &request : m_research_manager.AnalyzeTW(root, child, subchild))
    {
      if (!PollExists(root.id))
      {
        klog().d("Adding poll for {}", root.id);
        std::string dest = config::Process::tg_dest();
        m_message_queue.emplace_back(MakeIPCEvent(event, TGCommand::message, request.data, CreateOperation("Bot", {dest})));
        m_message_queue.emplace_back(MakeIPCEvent(event, TGCommand::poll, request.title, CreateOperation("Bot", {dest, "High", "Some", "Little", "None"})));
        auto uuid = m_message_queue.back().data.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
        m_research_manager.AddMLInput(uuid, TWResearchInputs{request.emotion, request.sentiment});
        m_research_polls.insert(root.id);
        return; // Limit to one
      }
    }
  };

  const auto& init_id   = applications.first;
  const auto& resp_id   = applications.second;
  const auto  init_task = FindTask(init_id);
  const auto  resp_task = FindTask(resp_id);
  const auto& init_mask = init_task.mask;
  const auto& resp_mask = resp_task.mask;
        auto& t_wrapper = m_postexec_map.at(resp_id).second;

  if (event.payload.empty())
    return Finalize(resp_id);

  try
  {
    assert(m_app_map.at(init_mask).size() && m_app_map.at(resp_mask).size());
    t_wrapper.SetEvent(std::move(event));
  }
  catch (const std::exception &e)
  {
    klog().e("Unknown application cannot be processed for post execution work. Exception: {}", e.what());
    return;
  }

  const auto& initiating_application = GetAppName(init_mask);
  const auto& responding_application = GetAppName(resp_mask);

  if (initiating_application == TW_RESEARCH_APP && responding_application == NER_APP)
  {
    if (m_message_queue.empty())
      InitQueue();

    const auto time = FindMasterRoot(&t_wrapper)->event.payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX);
    for (auto&& item : GetTokens(t_wrapper.event.payload))
    {
      if (!VerifyTerm(item.value))
        continue;

      Sanitize(item);
      const auto root       = FindMasterRoot(&t_wrapper)->task;
      const auto user       = init_task.GetToken(constants::USER_KEY);
      const auto term_hits  = m_research_manager.GetTermHits(item.value);
      const auto term_event = m_research_manager.RecordTermEvent(std::move(item), user, initiating_application, root, time);
      if (term_hits.size())
        for (auto&& hit : term_hits)
          if (!(hit.sid.empty()) && NotScheduled(hit.sid))
            CreateChild(resp_id, GetTask(hit.sid).GetToken(constants::DESCRIPTION_KEY),
                        NER_APP, {"entity"});
          else if (term_event.valid())
            OnTermEvent(term_event);
    }

    if (IPCNotPending())
      SetIPCCommand(constants::TELEGRAM_COMMAND_INDEX);
  }
  else
  if (initiating_application == NER_APP && responding_application == NER_APP)
    SequenceTasks({{init_id, init_task.GetToken(constants::DESCRIPTION_KEY), EMOTION_APP, {"emotion"}},
                   {         resp_task.GetToken(constants::DESCRIPTION_KEY), EMOTION_APP, {"emotion"}}});
  else
  if (initiating_application == EMOTION_APP && responding_application == EMOTION_APP)
    SequenceTasks({{init_id, init_task.GetToken(constants::DESCRIPTION_KEY), SENTIMENT_APP, {"sentiment"}},
                   {         resp_task.GetToken(constants::DESCRIPTION_KEY), SENTIMENT_APP, {"sentiment"}}});
  else
  if (initiating_application == SENTIMENT_APP && responding_application == SENTIMENT_APP)
  {
    const auto init_node = map.at(init_id).second;
    const auto resp_node = map.at(resp_id).second;
    const auto init_root = FindMasterRoot(&init_node);
    const auto resp_root = FindMasterRoot(&resp_node);
    if (init_root == resp_root && GetAppName(init_root->task.mask) == TW_RESEARCH_APP)
      AnalyzeTW(*(init_root), init_node, resp_node);
    else
      klog().i("All tasks originating from {} have completed", init_root->id);
  };

  Finalize(resp_id);
}
//----------------------------------------------------------------------------------------------------------------
template <typename T>
void Scheduler::PostExecWait(const int32_t& i, const T& r_)
{
  static const bool always_complete{true};
  int32_t r;
  if constexpr (std::is_integral<T>::value)
    r = r_;
  else
    r = std::stoi(r_);

  auto& lists = m_postexec_lists;
  auto& map   = m_postexec_map;

  const auto HasKey  = [&lists]            (auto k) { return lists.find(k) != lists.end(); };
  const auto AddRoot = [&lists, &map, this](auto k)
  {
    map  .insert({k, PostExecTuple{k, TaskWrapper{GetTask(k), always_complete}}});
    lists.insert({k, &(map.at(k).second)});
  };
  const auto AddNode = [&lists, &map, this](auto p, auto v)
  {
    map.insert({v, PostExecTuple{p, TaskWrapper{GetTask(v)}}});

    TaskWrapper* inserted_ptr = &(map.at(v).second);
    TaskWrapper* root         = lists.at(p);
    TaskWrapper* next         = root->child;
    TaskWrapper* parent       = root;

    while (next)
    {
      parent = next;
      next   = next->child;
    }
    parent->child        = inserted_ptr;
    inserted_ptr->parent = parent;
  };

  if (!HasKey(i))
    AddRoot(i);
  AddNode(i, r);
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::OnProcessOutput(const std::string& output, const int32_t mask, const int32_t id)
{
  auto GetValidArgument = [this](const auto &id)
  {
    const auto value = FileUtils::ReadEnvToken(GetTask(id).env, constants::HEADER_KEY);
    return (value != constants::GENERIC_HEADER) ? value : "";
  };

  ProcessParseResult result = m_result_processor.process(output, ProcessExecutor::GetAppInfo(mask));

  for (auto &&outgoing_event : result.events)
    switch (outgoing_event.code)
    {
    case (SYSTEM_EVENTS__PLATFORM_NEW_POST):
      outgoing_event.payload.emplace_back(GetValidArgument(id));
      m_event_callback(ALL_CLIENTS, outgoing_event.code, outgoing_event.payload);
      break;
    case (SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT):
    {
      auto parent_id = FindPostExec(id);
      if (parent_id != INVALID_ID)
        PostExecWork(std::move(outgoing_event), PostExecDuo{parent_id, id});
    }
    break;
    case (SYSTEM_EVENTS__PROCESS_RESEARCH): // TODO: We need to parse date
      CreateChild(id, outgoing_event.payload[constants::PLATFORM_PAYLOAD_CONTENT_INDEX], NER_APP, {"entity"});
      m_postexec_map.at(id).second.SetEvent(std::move(outgoing_event));
      break;
    default:
      klog().e("Result processor returned unknown event with code {}", outgoing_event.code);
    }

  return !(result.events.empty());
}
//----------------------------------------------------------------------------------------------------------------
template <typename T>
bool Scheduler::SavePlatformPost(const T& data)
{
  if constexpr (std::is_same_v<T, std::vector<std::string>>)
  {
    const auto rc = m_platform.SavePlatformPost(data);
    const auto ev = (rc) ? SYSTEM_EVENTS__PLATFORM_CREATED : SYSTEM_EVENTS__PLATFORM_ERROR;
    evt::instance()(ev, data);
    return rc;
  }
  else
  if constexpr (std::is_same_v<T, std::string>)
  {
    if (const auto post = m_platform.to_post(GetTask(data)); post.is_valid() &&
        m_platform.SavePlatformPost(post, constants::PLATFORM_POST_COMPLETE))
    {
      evt::instance()(SYSTEM_EVENTS__PLATFORM_CREATED, post.GetPayload());
      return true;
    }
    evt::instance()(SYSTEM_EVENTS__PLATFORM_ERROR, { "Failed to create post from task", data });
  }
  return false;
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::OnPlatformRequest(const std::vector<std::string> &payload)
{
  auto DefaultTGOP = []
  { return CreateOperation("Bot", {config::Process::tg_dest()}); };
  auto GetMLData = [this] { return m_research_manager.GetMLData(); };
  static const auto plat_req = SYSTEM_EVENTS__PLATFORM_POST_REQUESTED;
  const auto& platform = payload[constants::PLATFORM_REQUEST_PLATFORM_INDEX];
  const auto& id       = payload[constants::PLATFORM_REQUEST_ID_INDEX];
  const auto& user     = payload[constants::PLATFORM_REQUEST_USER_INDEX];
  const auto& message  = payload[constants::PLATFORM_REQUEST_MESSAGE_INDEX];
  const auto& args     = payload[constants::PLATFORM_REQUEST_ARGS_INDEX];

  klog().i("Platform request from {}", platform, message, args);
  for (const auto& arg : payload)
    klog().d("{}", arg);
    // klog().i("Platform request from {}.\nMessage: {}\nArgs: {}", platform, message, args);

  if (message == REQUEST_SCHEDULE_POLL_STOP) // platform data command time recurring status id p_uuid
    ScheduleIPC(IPCMessage{"", platform, args, message, "", "", "", id});
  else
  if (message == REQUEST_PROCESS_POLL_RESULT)
  {
    if (!OnIPCReceived(id))
      return klog().e("Unable to match unknown IPC response {} from {}", id, platform);

    const auto result = m_result_processor.process(args, PlatformIPC{platform, TGCommand::poll_result, id});
    for (const ProcessEventData &event : result.events)
      switch (event.code)
      {
        case (SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT):
          klog().i("Finalizing research data for ML input");
          m_research_manager.FinalizeMLInputs(GetUUID(m_dispatched_ipc.at(id).id), event.payload);
        break;
        default:
          klog().e("Unable to complete processing result from {} IPC request: Unknown event with code {}", platform, event.code);
      }

    if (IPCResponseReceived() && m_research_manager.MLInputReady())
    {
      m_research_manager.GenerateMLData();
      m_message_queue.emplace_back(MakeIPCEvent(plat_req, TGCommand::message, GetMLData(), DefaultTGOP()));
      schedule(GenericTaskHandler::Create(FindMask("Kneural"), "", "", "", {"--input=" + GetMLData()}));
      ResolvePending(IMMEDIATELY);
    }
  }
  else
  if (message == REQUEST_GENERATE_AI)
    if (platform == "kai")
      ScheduleIPC(IPCMessage{std::to_string(constants::IPC_PLATFORM_TYPE), platform, args, message, "", "", "", id});
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::OnPlatformError(const std::vector<std::string>& payload)
{ // TODO: Check ID and resolve pending IPC failures
  m_platform.OnPlatformError(payload);
  OnIPCReceived(payload.at(constants::PLATFORM_ERROR_ID_INDEX));
}

bool Scheduler::processTriggers(Task* task_ptr)
{
  bool processed_triggers = true;

  for (const auto &task : m_trigger.process(task_ptr))
    if (schedule(task).empty())
      processed_triggers = false;
    else
      klog().i("Task {} triggered scheduling of new task with ID {}", task_ptr->id(), task.id());

  return processed_triggers;
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::addTrigger(const std::vector<std::string>& payload)
{
  if (!payload.empty())
  {
    TriggerConfig      config{};
    const int32_t      TRIGGER_MAP_NUM_INDEX = 5;
    const int32_t      mask                  = std::stoi(payload.at(1));
    const int32_t      trigger_mask          = std::stoi(payload.at(2));
    const int32_t      map_num               = std::stoi(payload.at(5));
    const int32_t      config_num            = std::stoi(payload.at(6));
    const KApplication app                   = ProcessExecutor::GetAppInfo(mask);
    const KApplication trigger_app           = ProcessExecutor::GetAppInfo(trigger_mask);

    if (app.is_valid() && trigger_app.is_valid())
    {
      config.token_name  = payload.at(3);
      config.token_value = payload.at(4);

      for (int i = 0; i < map_num; i++)
        config.info.map.insert({payload.at((TRIGGER_MAP_NUM_INDEX + i + 1)),
                                payload.at((TRIGGER_MAP_NUM_INDEX + i + 2))});

      for (int i = 0; i < config_num; i++)
        config.info.config_info_v.emplace_back(ParamConfigInfo{
          .token_name     = payload.at((TRIGGER_MAP_NUM_INDEX + map_num + i + 1)),
          .config_section = payload.at((TRIGGER_MAP_NUM_INDEX + map_num + i + 2)),
          .config_name    = payload.at((TRIGGER_MAP_NUM_INDEX + map_num + i + 3))});

      return m_trigger.add(config);
    }
  }

  return false;
}
//----------------------------------------------------------------------------------------------------------------
int32_t Scheduler::FindPostExec(const int32_t& id)
{
  for (const auto& [_, task_wrapper] : m_postexec_map)
    if (task_wrapper.second.id == id)
      return task_wrapper.first;
  return INVALID_ID;
}
//----------------------------------------------------------------------------------------------------------------
Scheduler::TermEvents Scheduler::FetchTermEvents() const
{
  return m_research_manager.GetAllTermEvents();
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::SetIPCCommand(const uint8_t& command)
{
  m_ipc_command = command;
  timer.reset();
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::IPCNotPending() const
{
  return (m_ipc_command == constants::NO_COMMAND_INDEX || !timer.active());
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::ResolvePending(const bool& check_timer)
{
  if (check_timer && (!timer.active() || !timer.expired()))
    return;

  klog().i("Resolving pending IPC messages");
  for (auto&& buffer = m_message_queue.begin(); buffer != m_message_queue.end(); buffer++)
  {
    m_tx_ipc++;
    const auto ipc_event = *(buffer);
    m_event_callback(ALL_CLIENTS, ipc_event.event, ipc_event.data);
  }

  m_postexec_tasks += m_postexec_map.size();
  m_message_queue .clear();
  m_postexec_map  .clear();
  m_postexec_lists.clear();
  SetIPCCommand(constants::NO_COMMAND_INDEX);
  timer.stop();
}
//----------------------------------------------------------------------------------------------------------------
template <typename T, typename S>
int32_t Scheduler::CreateChild(const T&                        id,
                               const std::string&              data,
                               const S&                        application_name,
                               const std::vector<std::string>& args)
{ /* 4. Sentences from previous hits
    * 5. Analyze (KNLP)
    * 6. Analyze-comparison (KNLP)
    * 7. Compare interests (TODO)
    * 8. Discover memetic origin
    * 9. Discover memetic propagators*/
  int32_t task_id;
  if constexpr (std::is_integral<T>::value)
    task_id = id;
  else
    task_id = std::stoi(id);

  KApplication app = ProcessExecutor::GetAppInfo(-1, application_name);
  if (app.is_valid())
  {
    if (const auto new_task_id = schedule(GenericTaskHandler::Create(app.mask, data, "", "", args)); new_task_id.size())
    {
      PostExecWait(task_id, new_task_id);
      klog().i("{} scheduled as a child of {}", new_task_id, task_id);
      return std::stoi(new_task_id);
    }
    else
      klog().e("Failed to schedule {} task", app.name);
  }
  else
    klog().e("Application not found: {}", application_name);

  return INVALID_ID;
};
//----------------------------------------------------------------------------------------------------------------
template int32_t Scheduler::CreateChild(const uint32_t&, const std::string&, const std::string&, const std::vector<std::string>&);
template int32_t Scheduler::CreateChild(const std::string&, const std::string&, const std::string&, const std::vector<std::string>&);
//----------------------------------------------------------------------------------------------------------------
int32_t Scheduler::FindMask(const std::string& application_name)
{
  for (auto it = m_app_map.begin(); it != m_app_map.end(); it++)
    if (it->second == application_name)
      return it->first;
  return INVALID_MASK;
}
//----------------------------------------------------------------------------------------------------------------
std::string Scheduler::ScheduleIPC(const IPCMessage& msg)
{
  static const Fields fields   = { "pid", "command", "data", "time", "p_uuid", "recurring", "type" };
         const Values values   = { m_platform.GetPlatformID(msg.platform()), msg.command(), msg.data(),
                                   msg.time(), msg.p_uuid(), msg.recurring(), std::to_string(msg.type()) };

  klog().i("Scheduling IPC. ID {} for platform {} with command {} at {}",
    msg.p_uuid(), msg.platform(), msg.command(), msg.time());

  const auto id = m_kdb.insert("ipc", fields, values, "id");

  if (id.empty())
    klog().e("Error scheduling IPC: SQL INSERT failed.");
  else
    SavePlatformEnvFile(PlatformPost::Dummy(id, msg.time(), msg.option())); // TODO: make env procedure more generic

  return id;
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::ProcessIPC()
{
  using namespace DataUtils;
  static const auto   table  = "ipc";
  static const Fields fields = { "id", "pid", "command", "data", "time", "p_uuid", "type", "recurring" };

  if (!IPCResponseReceived())
    return;

  const auto filter = QueryComparisonFilter{{"time", "<", TimeUtils::Now()}};
  const auto query  = m_kdb.selectMultiFilter<QueryComparisonFilter, QueryFilter>(
                        table, fields,
                        { filter, CreateFilter("status", "0") });

  for (const auto &row : query)
  {
    const auto recur = row.at("recurring");
    const auto id    = row.at("id");
    const auto time  = row.at("time");
    SendIPCRequest({
      row.at("type"),
      m_platform.GetPlatform(row.at("pid")),
      row.at("data"),
      row.at("command"),
      time,
      recur,
      "0",
      id,
      row.at("p_uuid"),
      GetArgsOperation(GetEnvPath(id, time))});

    if (const auto recurring = std::stoi(recur); recurring)
      m_kdb.update("ipc",
        { "time", "status" },
        { std::to_string(std::stoul(time) + GetIntervalSeconds(recurring)), "0" },
        QueryFilter{"id", id});
  }

  m_platform.ProcessPlatform();
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::UpdateIPC(const std::string& id, int status = 1)
{
   return !m_kdb.update("ipc",
   { "status" }, { std::to_string(status) },
    CreateFilter("id", id)).empty();
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::SendIPCRequest(const IPCMessage& msg)
{
  auto get_user = [this](auto p) -> std::string { return GetPlatformUser(p); };
  using namespace StringUtils;

  const auto payload = msg.platform_payload(get_user);

  m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__PLATFORM_EVENT, payload);

  if (msg.type())
    UpdateIPC(msg.id()); // TODO: Add to dispatch and record receipt of IPC_OK
  else
    m_dispatched_ipc.insert({
      msg.p_uuid(), PlatformIPC{msg.platform(), GetIPCCommand(msg.cmd_code_string()), msg.id()}
    });
  klog().d("Sent IPC with ID {} command {} and data {}", msg.id(), msg.command(), msg.data());

  m_tx_ipc++;
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::IPCResponseReceived() const
{
  for (const auto &[id, request] : m_dispatched_ipc)
    if (!request.complete)
      return false;
  return true;
}
//----------------------------------------------------------------------------------------------------------------
bool Scheduler::OnIPCReceived(const std::string& uuid)
{
  auto UpdateStatus = [this](auto id)
  { m_kdb.update("ipc", {"status"}, {"1"}, CreateFilter("id", id)); };
  auto it = m_dispatched_ipc.find(uuid);
  if (it == m_dispatched_ipc.end())
    return false;
  it->second.complete = true;
  UpdateStatus(it->second.id);
  return true;
}
//----------------------------------------------------------------------------------------------------------------
std::string Scheduler::Status() const
{
  return m_platform.Status() + "\n\n" + fmt::format(
    "Scheduler Status\nMessages sent: {}\nMessage queue: {}\nDispatched requests: {}\nPostExec Tasks: {}",
    m_tx_ipc, m_message_queue.size(), m_dispatched_ipc.size(), m_postexec_tasks);
}
//----------------------------------------------------------------------------------------------------------------
std::string Scheduler::GetUUID(const std::string& id) const
{
  const auto &rows = m_kdb.select("ipc", {"p_uuid"}, QueryFilter{"id", id});
  if (!rows.empty())
    return rows.front().at("p_uuid");

  return "";
}
//----------------------------------------------------------------------------------------------------------------
void Scheduler::FetchPosts()
{
  m_platform.FetchPosts();
}
//----------------------------------------------------------------------------------------------------------------
std::string Scheduler::GetPlatformUser(std::string_view platform) const
{
  return m_platform.GetUser("", m_platform.GetPlatformID(platform.data()));
}
//----------------------------------------------------------------------------------------------------------------
template bool Scheduler::SavePlatformPost(const std::vector<std::string>&);
template bool Scheduler::SavePlatformPost(const std::string&);
} // ns kiq
