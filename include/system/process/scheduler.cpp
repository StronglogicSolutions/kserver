#include "scheduler.hpp"
#include "executor/task_handlers/generic.hpp"
#include "ipc/ipc.hpp"

using  TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using  Duration  = std::chrono::seconds;

static TimePoint time_point;
static bool      timer_active;

static bool TimerExpired()
{
  static const uint32_t  TEN_MINUTES = 600;
         const TimePoint now         = std::chrono::system_clock::now();
         const int64_t   elapsed = std::chrono::duration_cast<Duration>(now - time_point).count();
  return (elapsed > TEN_MINUTES)  ;
}

static void StartTimer()
{
  time_point   = std::chrono::system_clock::now();
  timer_active = true;
}

static void StopTimer()
{
  timer_active = false;
}

static bool TimerActive()
{
  return timer_active;
}

uint32_t getAppMask(std::string name)
{
        auto db = Database::KDB{};
  const auto value_field{"mask"};

  for (const auto& row : db.select("apps", {value_field}, CreateFilter("name", name)))
    if (row.first == value_field)
      return std::stoi(row.second);
  return std::numeric_limits<uint32_t>::max();
}

/**
 * getIntervalSeconds
 *
 * Helper function returns the number of seconds equivalent to a recurring interval
 *
 * @param  [in]  {uint32_t}  The integer value representing a recurring interval
 * @return [out] {uint32_t}  The number of seconds equivalent to that interval
 */
const uint32_t getIntervalSeconds(uint32_t interval) {
  switch(interval) {
    case Constants::Recurring::HOURLY:
      return 3600;
    case Constants::Recurring::DAILY:
      return 86400;
    case Constants::Recurring::MONTHLY:
      return 86400 * 30;
    case Constants::Recurring::YEARLY:
      return 86400 * 365;
    default:
      return 0;
  }
}

/**
 * @brief
 *
 * @param args
 * @return Task
 */
TaskWrapper args_to_task(std::vector<std::string> args)
{
  TaskWrapper wrapper{};
  Task        task;
  if (args.size() == constants::PAYLOAD_SIZE)
  {
    auto mask = getAppMask(args.at(constants::PAYLOAD_NAME_INDEX));

    if (mask != NO_APP_MASK)
    {
      task.task_id         = std::stoi(args.at(constants::PAYLOAD_ID_INDEX));
      task.execution_mask  = mask;
      task.datetime        = args.at(constants::PAYLOAD_TIME_INDEX);
      task.execution_flags = args.at(constants::PAYLOAD_FLAGS_INDEX);
      task.completed       = std::stoi(args.at(constants::PAYLOAD_COMPLETED_INDEX));
      task.recurring       = std::stoi(args.at(constants::PAYLOAD_RECURRING_INDEX));
      task.notify          = args.at(constants::PAYLOAD_NOTIFY_INDEX).compare("1") == 0;
      task.runtime         = StripSQuotes(args.at(constants::PAYLOAD_RUNTIME_INDEX));
      // task.filenames = args.at(constants::PAYLOAD_ID_INDEX;
      wrapper.envfile      = args.at(constants::PAYLOAD_ENVFILE_INDEX);
      KLOG("Can't parse files from schedule payload. Must be implemented");
    }
  }
  wrapper.task = task;
  return wrapper;
}

bool IsRecurringTask(const Task& task)
{
  return static_cast<uint8_t>(task.recurring) > Constants::Recurring::NO;
}

Scheduler::Scheduler(Database::KDB&& kdb)
: m_kdb(std::move(kdb)),
  m_platform(nullptr),
  m_trigger(nullptr),
  m_research_manager(&m_kdb, &m_platform)
{
  KLOG("Scheduler instantiated for testing");
}

static Scheduler::ApplicationMap FetchApplicationMap(Database::KDB& db)
{
  static const std::string  table{"apps"};
  static const Fields       fields = {"mask", "name"};
  static const QueryFilter  filter{};
  Scheduler::ApplicationMap map{};
  std::string               mask, name;

  for (const auto& value : db.select(table, fields, filter))
  {
    if (value.first == "mask")
      mask = value.second;
    else
    if (value.first == "name")
      name = value.second;

    if (DataUtils::NoEmptyArgs(mask, name))
    {
      map.insert({std::stoi(mask), name});
      DataUtils::ClearArgs(mask, name);
    }
  }
  return map;
}

Scheduler::Scheduler(SystemEventcallback fn)
: m_event_callback(fn),
  m_kdb(Database::KDB{}),
  m_platform(fn),
  m_trigger(&m_kdb),
  m_app_map(FetchApplicationMap(m_kdb)),
  m_research_manager(&m_kdb, &m_platform)
{
  const auto AppExists = [this](const std::string& name) -> bool
  {
    auto it    = std::find_if(m_app_map.begin(), m_app_map.end(),
      [name](const ApplicationInfo& info) { return info.second == name;});
    auto found = it != m_app_map.end();
    return found;
  };

  try
  {
    for (int i = 0; i < REQUIRED_APPLICATION_NUM; i++)
      if (!AppExists(REQUIRED_APPLICATIONS[i]))
        throw std::runtime_error{("Required application was missing. {}", REQUIRED_APPLICATIONS[i])};
  }
  catch(const std::exception& e)
  {
    ELOG(e.what());
    throw;
  }
}

Scheduler::~Scheduler()
{
  KLOG("Scheduler destroyed");
}

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

template bool Scheduler::HasRecurring(const std::string& id);
template bool Scheduler::HasRecurring(const int32_t&     id);

/**
 * schedule
 *
 * Schedule a task
 *
 * @param  [in]  {Task}        task  The task to schedule
 * @return [out] {std::string}       The string value ID representing the Task in the database
 *                                   or an empty string if the insert failed
 */
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
    const std::string ext       = StringUtils::ToLower(extension);
    if (IsImageExtension(ext))
      return "image";
    if (IsVideoExtension(ext))
      return "video";
    return "unknown";
  };

  std::string  id;
  const auto   table {"schedule"};
  const Fields fields{"time", "mask", "flags", "envfile", "recurring", "notify"};
  const Values values{task.datetime, std::to_string(task.execution_mask), task.execution_flags,
                      task.envfile, std::to_string(task.recurring), std::to_string(task.notify)};

  if (task.validate())
  {
    try
    {
      id = m_kdb.insert(table, fields, values, "id");
      if (!id.empty())
      {
        KLOG("Request to schedule task was accepted\nID {}", id);

        for (const auto file : task.files)
        {
          const Fields f_fields = {"name","sid","type"};
          const Values f_values = {file.first, id, GetFileType(file.first)};
          KLOG("Recording file in DB: {}", file.first);
          m_kdb.insert("file", f_fields, f_values);
        }

        if (task.recurring)
        {
          const auto recurring = std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring));
          m_kdb.insert("recurring", {"sid", "time"}, {id, recurring});
        }
      }
    }
    catch (const pqxx::sql_error &e)
    {
      ELOG("Insert query failed: {}", e.what());
    }
    catch (const std::exception &e)
    {
      ELOG("Insert query failed: {}", e.what());
    }
  }
  return id;
}

/**
 * parseTask
 *
 * Parses a QueryValues data structure (vector of string tuples) and returns a Task object
 *
 * @param  [in]  {QueryValues}  result  The DB query result consisting of string tuples
 * @return [out] {Task}                 The task
 */
Task Scheduler::parseTask(QueryValues&& result)
{
  Task task{};
  for (const auto& v : result)
    if (v.first == Field::MASK)
      task.execution_mask      = std::stoi(v.second);
    else
    if (v.first == Field::FLAGS)
      task.execution_flags     = v.second;
    else
    if (v.first == Field::ENVFILE)
      task.envfile             = v.second;
    else
    if (v.first == Field::TIME)
      task.datetime            = v.second;
    else
    if (v.first == Field::ID)
      task.task_id             = std::stoi(v.second);
    else
    if (v.first == Field::COMPLETED)
      task.completed           = std::stoi(v.second);
    else
    if (v.first == Field::RECURRING)
      task.recurring           = std::stoi(v.second);
    else
    if (v.first == Field::NOTIFY)
      task.notify              = v.second.compare("t") == 0;

  return task;
}

/**
 * parseTasks
 *
 * Parses a QueryValues data structure (vector of string tuples) and returns a vector of Task objects
 *
 * @param  [in]  {QueryValues}        The DB query result consisting of string tuples
 * @return [out] {std::vector<Task>}  A vector of Task objects
 */
std::vector<Task> Scheduler::parseTasks(QueryValues&& result, bool parse_files, bool is_recurring)
{
  const std::string files_field{"(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files"};
  int id{}, completed{NO_COMPLETED_VALUE}, recurring{-1}, notify{-1};
  std::string mask, flags, envfile, time, filenames;
  std::vector<Task> tasks;
  bool checked_for_files{false};
  std::string TIME_FIELD = (is_recurring) ?
                              Field::REC_TIME : Field::TIME;

  for (const auto& v : result) {
    if      (v.first == Field::MASK     ) { mask      = v.second;                   }
    else if (v.first == Field::FLAGS    ) { flags     = v.second;                   }
    else if (v.first == Field::ENVFILE  ) { envfile   = v.second;                   }
    else if (v.first == TIME_FIELD      ) { time      = v.second;                   }
    else if (v.first == Field::ID       ) { id        = std::stoi(v.second);        }
    else if (v.first == Field::COMPLETED) { completed = std::stoi(v.second);        }
    else if (v.first == Field::RECURRING) { recurring = std::stoi(v.second);        }
    else if (v.first == Field::NOTIFY   ) { notify    = v.second.compare("t") == 0; }
    else if (v.first == files_field     ) { filenames = v.second;
                                            checked_for_files = true;               }

    if (!envfile.empty() && !flags.empty() && !time.empty() &&
        !mask.empty()    && completed != NO_COMPLETED_VALUE &&
        id > 0           && recurring > -1 && notify > -1      )
    {

      if (parse_files && !checked_for_files)
      {
        checked_for_files = true;
        continue;
      }
      else
      {
        if (recurring != Constants::Recurring::NO)
        {
          if (recurring == Constants::Recurring::HOURLY)
            time = std::to_string(std::stoi(time) + getIntervalSeconds(recurring));
          else
            time = TimeUtils::time_as_today(time);
        }

        tasks.push_back(Task{
          .execution_mask   = std::stoi(mask),
          .datetime         = time,
          .file             = true,
          .files            = {},
          .envfile          = envfile,
          .execution_flags  = flags,
          .task_id          = id,
          .completed        = completed,
          .recurring        = recurring,
          .notify           = (notify == 1),
          .runtime          = FileUtils::ReadRunArgs(envfile), // ⬅ set from DB?
          .filenames        = StringUtils::Split(filenames, ' ')});
        id                  = 0;
        recurring           = -1;
        notify              = -1;
        completed           = NO_COMPLETED_VALUE;
        checked_for_files   = false;
        DataUtils::ClearArgs(filenames, envfile, flags, time, mask);
      }
    }
  }
  return tasks;
}

/**
 * fetchTasks
 *
 * Fetch scheduled tasks which are intended to only be run once
 *
 * @return [out] {std::vector<Task>} A vector of Task objects
 */
std::vector<Task> Scheduler::fetchTasks(const std::string& mask, const std::string& date_range_s, const std::string& count, const std::string& limit, const std::string& order)
{
  using DateRange = std::pair<std::string, std::string>;
  static const std::string MAX_INT = std::to_string(std::numeric_limits<int32_t>::max());
  const auto GetDateRange = [](const std::string& date_range_s) -> DateRange
  {
    if (date_range_s.front() == '0')
      return DateRange{"0", MAX_INT};

    const auto split_idx = date_range_s.find_first_of("TO");
    return DateRange{date_range_s.substr(0, (date_range_s.size() - split_idx - 2)),
                     date_range_s.substr(split_idx + 1)};

  };

  const DateRange date_range   = GetDateRange(date_range_s);
  const auto      result_limit = (limit == "0") ? MAX_INT : limit; // TODO: ID limit - needs to be < or >
  const auto      row_order    = order;

  auto tasks = m_kdb.selectMultiFilter<CompBetweenFilter, QueryFilter>(
      "schedule", {                                   // table
        Field::ID,
        Field::TIME,
        Field::MASK,
        Field::FLAGS,
        Field::ENVFILE,
        Field::COMPLETED,
        Field::NOTIFY,
        Field::RECURRING
      }, std::vector<std::variant<CompBetweenFilter, QueryFilter>>{
        CompBetweenFilter{                                   // filter TODO: add id < or > result_limit
          .field = Field::TIME,
          .a     = date_range.first,
          .b     = date_range.second
        },
        CreateFilter(Field::MASK, mask)},
        OrderFilter{
          .field = Field::ID,
          .order = row_order
        },
        LimitFilter{
          .count = count
        });
  return parseTasks(std::move(tasks));
}

std::vector<Task> Scheduler::fetchTasks()
{
  std::vector<Task> tasks = parseTasks(
    m_kdb.selectMultiFilter<CompFilter, CompBetweenFilter, MultiOptionFilter>(
      "schedule", {                                   // table
        Field::ID,
        Field::TIME,
        Field::MASK,
        Field::FLAGS,
        Field::ENVFILE,
        Field::COMPLETED,
        Field::NOTIFY,
        Field::RECURRING
      }, std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>{
        CompFilter{                                   // filter
          "recurring",                                // field of comparison
          "0",                                        // value for comparison
          "="                                         // comparator
        },
        CompFilter{                                   // filter
          UNIXTIME_NOW,                               // value A
          Field::TIME,                                // value B
          ">"                                         // comparison
        },
        MultiOptionFilter{                            // filter
          "completed",                                // field of comparison
          "IN", {                                     // comparison type
            Completed::STRINGS[Completed::SCHEDULED], // set of values for comparison
            Completed::STRINGS[Completed::FAILED]
          }
        }
      },
      OrderFilter{Field::ID, "ASC"}
    )
  );
  // TODO: Convert above to a JOIN query, and remove this code below
  for (auto& task : tasks)
    for (const auto& file : getFiles(task.id()))
      task.filenames.emplace_back(file.name);

  return tasks;
}

/**
 * fetchRecurringTasks
 *
 * Fetch scheduled tasks that are set to be performed on a recurring basis
 *
 * @return [out] {std::vector<Task>} A vector of Task objects
 */
std::vector<Task> Scheduler::fetchRecurringTasks() {
  using Filters = std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>;
  static const bool    parse_files{true};
  static const bool    recurring{true};
  static const char*   sub_q{"(SELECT  string_agg(file.name, ' ') "\
                             "FROM file WHERE file.sid = schedule.id) as files"};
  static const Fields  fields{Field::ID, Field::TIME, Field::MASK, Field::FLAGS,
                              Field::ENVFILE, Field::COMPLETED, Field::RECURRING,
                              Field::NOTIFY, "recurring.time", sub_q};
  static const Filters filters{
    CompFilter{Field::RECURRING, "0", "<>"},
    CompFilter{UNIXTIME_NOW, "recurring.time + (SELECT get_recurring_seconds(schedule.recurring))", ">"},
    MultiOptionFilter{"completed", "IN",
      {Completed::STRINGS[Completed::SCHEDULED], Completed::STRINGS[Completed::FAILED]}}};
  static const Joins   joins{{"recurring", "sid", "schedule", "id", JoinType::INNER},
                             {"file", "sid", "schedule", "id",      JoinType::OUTER}};

  return parseTasks(m_kdb.selectJoin<Filters>(
    "schedule", fields, filters, joins, OrderFilter{Field::ID, "ASC"}), parse_files, recurring);
}

/**
 * @brief
 *
 * @return std::vector<Task>
 */
std::vector<Task> Scheduler::fetchAllTasks()
{
  static const char*  FILEQUERY{"(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files"};
  static const Fields fields{Field::ID, Field::TIME, Field::MASK, Field::FLAGS, Field::ENVFILE,
                             Field::COMPLETED,Field::RECURRING,Field::NOTIFY, FILEQUERY};
  static const Joins  joins{{ "file", "sid", "schedule", "id", JoinType::OUTER}};

  return parseTasks(m_kdb.selectJoin<QueryFilter>("schedule", fields, {}, joins), true);
}

/**
 * GetTask
 *
 * get one task by ID
 *
 * @deprecated   NOT SAFE FOR USE
 * @param  [in]  {std::string}  id  The task ID
 * @return [out] {Task}             A task
 */
Task Scheduler::GetTask(const std::string& id)
{
  static const Fields fields{Field::MASK, Field::FLAGS, Field::ENVFILE, Field::TIME,Field::COMPLETED};

  Task task = parseTask(m_kdb.select("schedule", fields, {CreateFilter("id", id)}));

  for (const auto& file : getFiles(id))
    task.filenames.push_back(file.name);

  return task;
}


/**
 * @brief Get the Files object
 *
 * @param sid
 * @return std::vector<std::string>
 */
std::vector<FileMetaData> Scheduler::getFiles(const std::vector<std::string>& sids, const std::string& type)
{
  std::vector<FileMetaData> files{};
  files.reserve(sids.size());

  for (const auto& sid : sids)
  {
    const QueryFilter filter = (type.empty()) ? CreateFilter("sid", sid) : CreateFilter("sid", sid, "type", type);
    FileMetaData      file{};

    for (const auto& v : m_kdb.select("file", {"id", "name", "type"}, filter))
    {
      if (v.first == "id")
        file.id = v.second;
      else
      if (v.first == "name")
        file.name = v.second;
      else
      if (v.first == "type")
        file.type = v.second;

      if (file.complete())
      {
        file.task_id = sid;
        files.push_back(file);
        file.clear();
      }
    }
  }

  return files;
}

std::vector<FileMetaData> Scheduler::getFiles(const std::string& sid, const std::string& type)
{
  std::vector<FileMetaData> files{};
  const QueryFilter         filter = (type.empty()) ?
                                      CreateFilter("sid", sid) :
                                      CreateFilter("sid", sid, "type", type);

  QueryValues result = m_kdb.select("file", {"id", "name", "type"}, filter);

  FileMetaData file{};

  for (const auto& v : result)
  {
    if (v.first == "id")
      file.id = v.second;
    else
    if (v.first == "name")
      file.name = v.second;
    else
    if (v.first == "type")
      file.type = v.second;

    if (file.complete())
    {
      file.task_id = sid;
      files.push_back(file);
      file.clear();
    }
  }

  return files;
}
/**
 * GetTask
 *
 * get one task by ID
 *
 * @deprecated   NOT SAFE FOR USE
 *
 * @param  [in]  {int}  id  The task ID
 * @return [out] {Task}     A task
 */
Task Scheduler::GetTask(int id) {
  Task task =  parseTask(m_kdb.select("schedule", {
                          Field::MASK,    Field::FLAGS,
                          Field::ENVFILE, Field::TIME,
                          Field::COMPLETED},
                          CreateFilter("id", std::to_string(id))));

  for (const auto& file : getFiles(std::to_string(id)))                 // files
    task.filenames.push_back(file.name);

  return task;
}

template std::vector<std::string>  Scheduler::getFlags(const std::string& mask);
template std::vector<std::string>  Scheduler::getFlags(const uint32_t& mask);

template <typename T>
std::vector<std::string> Scheduler::getFlags(const T& mask)
{
  std::string filter_mask;
  if constexpr (std::is_same_v<std::string, T>)
    filter_mask = mask;
  else
  if constexpr (std::is_integral<T>::value)
    filter_mask =  std::to_string(mask);

  for (const auto& row : m_kdb.select("schedule", {Field::FLAGS}, CreateFilter("mask", filter_mask), 1))
    if (row.first == Field::FLAGS)
      return StringUtils::Split(row.second, ' ');

  ELOG("No task exists with that mask");
  return {};
}
/**
 * updateStatus
 *
 * update the completion status of a task
 *
 * @param   [in] {Task}   task  The task to update
 * @return [out] {bool}         Whether the UPDATE query was successful
 */
bool Scheduler::updateStatus(Task* task, const std::string& output) {
  const std::string table = "schedule";
  const Fields      fields = {"completed"};
  const Values      values = {std::to_string(task->completed)};
  const QueryFilter filter = CreateFilter("id", task->id());
  const std::string returning = "id";
  return (!m_kdb.update(table, fields, values, filter, returning).empty());
}

  /**
 * update
 *
 * @param   [in] {Task}   task  The task to update
 * @return [out] {bool}         Whether the UPDATE query was successful
 */
bool Scheduler::update(Task task)
{
  if (IsRecurringTask(task))
    if (!HasRecurring(task.task_id))
      m_kdb.insert("recurring", {"sid", "time"}, {
        task.id(),
        std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))});
    else
    m_kdb.update("recurring", {"time"},
      {std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))},
      CreateFilter("sid", task.id()));

  return !m_kdb.update(                // UPDATE
    "schedule", {                      // table
          "mask",
          "time",
          "flags",
          "completed",
          "recurring",
          "notify",
          "runtime"
    }, {
      std::to_string(task.execution_mask),
      task.datetime,
      task.execution_flags,
      std::to_string(task.completed),
      std::to_string(task.recurring),
      std::to_string(task.notify),
      task.runtime
    }, CreateFilter("id", task.id()),
    "id"                               // returning value
  )
  .empty();                            // not empty = success
}

/**
 * updateRecurring
 *
 * update the last time a recurring task was run
 *
 * @param   [in] {Task}   task  The task to update
 * @return [out] {bool}         Whether the UPDATE query was successful
 */
bool Scheduler::updateRecurring(Task* task) {
  return !m_kdb.update("recurring", {"time"}, {task->datetime}, CreateFilter("sid", task->id()), "id").empty();
}

/**
 * @brief
 *
 * @param id
 * @param env
 * @return [out] {bool}
 */
bool Scheduler::updateEnvfile(const std::string& id, const std::string& env)
{
  for (const auto& value : m_kdb.select("schedule", {"envfile"}, CreateFilter("id", id)))
    if (value.first == "envfile")
    {
      FileUtils::SaveFile(env, value.second);
      return true;
    }
  return false;
}

/**
 * isKIQProcess
 *
 * @param   [in] <uint32_t>
 * @returns [out] {bool}
 */
bool Scheduler::isKIQProcess(uint32_t mask)
{
  return ProcessExecutor::GetAppInfo(mask).is_kiq;
}

/**
 * PostExecWork
 */
void Scheduler::PostExecWork(ProcessEventData event, Scheduler::PostExecDuo applications)
{
  using namespace FileUtils;
  const auto  TaskFn       = [this](const int32_t& id) { return GetTask(id); };
  const auto& map          = m_postexec_waiting;
  const bool& immediately  = false;
  const auto  HasPayload   = [](const ProcessEventData& event) { return (event.payload.size()); };
  const auto  LastExecPair = [&map, &TaskFn](const int32_t& a, const int32_t&b, const int32_t& mask)
  {
    const auto GetMask = [&TaskFn](int32_t id) { return TaskFn(id).execution_mask; };

    bool found{false};

    if (map.find(a) != map.end())
    {
      const auto& v = map.at(a);
      for (auto i = 0; i < v.size(); i++)
      {
        if (found && GetMask(v.at(i).back()) == mask) // TODO: rigorous testing required
          return false;

        auto q = v.at(i);
        if (q.size() && q.back() == b)
          found = true;
      }
    }
    return found;
  };

  const auto AddPostExec = [this](const std::string& id, const std::string& application_name) -> void
  {
    ProcessResearch(id, FileUtils::ReadEnvToken(GetTask(id).envfile, constants::DESCRIPTION_KEY), application_name);
  };

  const auto& init_id         = applications.first;
  const auto& resp_id         = applications.second;
  const auto  initiating_task = GetTask(init_id);
  const auto  responding_task = GetTask(resp_id);
  const auto& init_mask       = initiating_task.execution_mask;
  const auto& resp_mask       = responding_task.execution_mask;
  try
  {
    assert(m_app_map.at(init_mask).size() && m_app_map.at(resp_mask).size());
  }
  catch(const std::exception& e)
  {
    ELOG("Unknown application cannot be processed for post execution work. Exception: {}", e.what());
    return;
  }

  const auto& initiating_application = m_app_map.at(init_mask);
  const auto& responding_application = m_app_map.at(resp_mask);

  if (initiating_application == "TW Research" && responding_application == "KNLP")
  {
    using JSONItem  = KNLPResultParser::NLPItem;
    static const std::string IPC_Message_Header{"KIQ is now tracking the following terms:"};
    std::vector<JSONItem>    items{};

    if (HasPayload(event))
      for (size_t i = 1; i < (event.payload.size() - 1); i += 2)
        items.emplace_back(JSONItem{.type = event.payload[i], .value = event.payload[i + 1]});

    if (m_message_buffer.empty())
      m_message_buffer += IPC_Message_Header;

    for (auto&& item : items)
    {
      const auto user       = ReadEnvToken(initiating_task.envfile, constants::USER_KEY);
      const auto term_hits  = m_research_manager.GetTermHits(item.value);
      const auto known_term = term_hits.size();
      const auto term_info  = m_research_manager.RecordTermEvent(std::move(item), user, initiating_application, responding_task);
      if (known_term)
        for (const auto& hit : term_hits)
          (void)0;
          // m_research_manager.AnalyzeTermHit(hit, GetTask(hit.sid));
      else
      if (term_info.valid())
        m_message_buffer += '\n' + term_info.ToString();
    }

    if (IPCNotPending())
      SetIPCCommand(constants::TELEGRAM_COMMAND_INDEX);

    if (LastExecPair(init_id, resp_id, responding_task.execution_mask))
      ResolvePending(immediately);
  }
}

/**
 * PostExecWait
 */
template <typename T>
void Scheduler::PostExecWait(const int32_t& i, const T& r_)
{
  using ExecPair = std::pair<int32_t, std::vector<PostExecQueue>>;
  int32_t r;
  if constexpr(std::is_integral<T>::value)
    r = r_;
  else
    r = std::stoi(r_);

  auto& map = m_postexec_waiting;
  const auto HasKey = [&map](const int32_t& k) -> bool { return map.find(k) != map.end(); };

  if (!HasKey(i))
    map.insert(ExecPair(i, {}));
  map[i].emplace_back(PostExecQueue{r});
}

/**
 * @brief
 *
 * @param output
 * @param mask
 * @return true
 * @return false
 */

bool Scheduler::handleProcessOutput(const std::string& output, const int32_t mask, const int32_t id)
{
  ProcessParseResult result = m_result_processor.process(output, ProcessExecutor::GetAppInfo(mask));

  if (!result.data.empty())
  {
    for (auto&& outgoing_event : result.data)
      switch (outgoing_event.event)
      {
        case (SYSTEM_EVENTS__PLATFORM_NEW_POST):
          outgoing_event.payload.emplace_back(FileUtils::ReadEnvToken(GetTask(id).envfile, constants::HEADER_KEY));
          m_event_callback(ALL_CLIENTS, outgoing_event.event, outgoing_event.payload);
        break;
        case (SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT):
        {
          auto parent_id = FindPostExec(id);
          if (parent_id != INVALID_ID)
            PostExecWork(outgoing_event, PostExecDuo{parent_id, id});
        }
        break;
        case (SYSTEM_EVENTS__PROCESS_RESEARCH):
          ProcessResearch(id, outgoing_event.payload[constants::PLATFORM_PAYLOAD_CONTENT_INDEX], "KNLP");
        break;
        default:
          ELOG("Result processor returned unknown event with code {}", outgoing_event.event);
      }
    return true;
  }
  return false;
}

/**
 * @brief
 *
 * @param payload
 * @return true
 * @return false
 */
bool Scheduler::savePlatformPost(std::vector<std::string> payload)
{
  return m_platform.savePlatformPost(payload);
}

/**
 * @brief
 *
 * @param payload
 */
void Scheduler::onPlatformError(const std::vector<std::string>& payload)
{
  m_platform.onPlatformError(payload);
}

/**
 * @brief processPlatform
 *
 */
void Scheduler::processPlatform()
{
  m_platform.processPlatform();
}

bool Scheduler::processTriggers(Task* task_ptr)
{
  bool                    processed_triggers{true};
  const std::vector<Task> tasks = m_trigger.process(task_ptr);

  for (const auto& task : tasks)
  {
    const std::string task_id = schedule(task);
    if (task_id.empty())
      processed_triggers = false;
    else
      KLOG("Task {} triggered scheduling of new task with ID {}", task_ptr->id(), task.id());

  }

  return processed_triggers;
}

/**
 * @brief
 *
 * @param payload
 * @return true
 * @return false
 */
bool Scheduler::addTrigger(const std::vector<std::string>& payload)
{
  if (!payload.empty())
  {
    TriggerConfig       config{};
    const int32_t       TRIGGER_MAP_NUM_INDEX = 5;
    const int32_t       mask                  = std::stoi(payload.at(1));
    const int32_t       trigger_mask          = std::stoi(payload.at(2));
    const int32_t       map_num               = std::stoi(payload.at(5));
    const int32_t       config_num            = std::stoi(payload.at(6));
    const KApplication  app                   = ProcessExecutor::GetAppInfo(mask);
    const KApplication  trigger_app           = ProcessExecutor::GetAppInfo(trigger_mask);

    if (app.is_valid() && trigger_app.is_valid())
    {
      config.token_name  = payload.at(3);
      config.token_value = payload.at(4);

      for (int i = 0; i < map_num; i++)
        config.info.map.insert({
          payload.at((TRIGGER_MAP_NUM_INDEX + i + 1)),
          payload.at((TRIGGER_MAP_NUM_INDEX + i + 2))});

      for (int i = 0; i < config_num; i++)
        config.info.config_info_v.emplace_back(
          ParamConfigInfo{
            .token_name     = payload.at((TRIGGER_MAP_NUM_INDEX + map_num + i + 1)),
            .config_section = payload.at((TRIGGER_MAP_NUM_INDEX + map_num + i + 2)),
            .config_name    = payload.at((TRIGGER_MAP_NUM_INDEX + map_num + i + 3))});

      return m_trigger.add(config);
    }
  }

  return false;
}

// TODO: This should persist in DB or file system
int32_t Scheduler::FindPostExec(const int32_t& id)
{
  auto not_found = m_postexec_waiting.end();
  for (const auto& [initiator, responding_tasks] : m_postexec_waiting)
    for (const auto& queue : responding_tasks)
      for (const auto& task : queue)
        if (task == id)
          return initiator;
  return INVALID_ID;
}

Scheduler::TermEvents Scheduler::FetchTermEvents() const
{
  return m_research_manager.GetAllTermEvents();
}

void Scheduler::SetIPCCommand(const uint8_t& command)
{
  if (m_ipc_command == constants::NO_COMMAND_INDEX)
  {
    m_ipc_command = command;
    StartTimer();
  }
  else
    ELOG("Cannot replace current pending IPC command: {}", constants::IPC_COMMANDS[m_ipc_command]);
}

bool Scheduler::IPCNotPending() const
{
  return (m_ipc_command == constants::NO_COMMAND_INDEX);
}

void Scheduler::ResolvePending(const bool& check_timer)
{
  if (!TimerActive() || (check_timer && !TimerExpired())) return;

  KLOG("Resolving pending IPC message");
  const auto payload = {CreateOperation("ipc", {constants::IPC_COMMANDS[m_ipc_command], m_message_buffer, ""})};

  m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__KIQ_IPC_MESSAGE, payload);
  m_message_buffer  .clear();
  m_postexec_waiting.clear();
  SetIPCCommand(constants::NO_COMMAND_INDEX);
  StopTimer();
}

template <typename T>
void Scheduler::ProcessResearch(const T& id, const std::string& data, const std::string& application_name)
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
  if (!app.is_valid())
  {
    ELOG("No KNLP app for language processing");
    return;
  }

  Task task = GenericTaskHandler::Create(app.mask, data);

  const auto new_task_id = schedule(task);

  if (new_task_id.size())
  {
    KLOG("Research triggered scheduling of task {}", new_task_id);
    PostExecWait(task_id, new_task_id);
  }
  else
    ELOG("Failed to schedule {} task", app.name);
};
