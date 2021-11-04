#include "scheduler.hpp"
#include "executor/task_handlers/generic.hpp"
#include "ipc/ipc.hpp"

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
TaskWrapper args_to_task(std::vector<std::string> args) {
  TaskWrapper wrapper{};
  Task        task;
  if (args.size() == constants::PAYLOAD_SIZE) {
    auto mask = getAppMask(args.at(constants::PAYLOAD_NAME_INDEX));

    if (mask != NO_APP_MASK) {
      task.id              = std::stoi(args.at(constants::PAYLOAD_ID_INDEX));
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
Task Scheduler::parseTask(QueryValues&& result) {
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
      task.id                  = std::stoi(v.second);
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
 * @param  [in]  {QueryValues}        result  The DB query result consisting of string tuples
 * @return [out] {std::vector<Task>}          A vector of Task objects
 *
 * TODO: Remove "parse_files" boolean after refactoring `fetchTasks` (non-recurring tasks method) to use joins
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
          .id               = id,
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

std::vector<Task> Scheduler::fetchTasks() {
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
      }
    )
  );
  // TODO: Convert above to a JOIN query, and remove this code below
  for (auto& task : tasks)
    for (const auto& file : getFiles(std::to_string(task.id)))
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
  using SelectJoinFilters = std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>;
  return parseTasks(
    m_kdb.selectJoin<SelectJoinFilters>(
      "schedule", {                                                     // table
        Field::ID,                                            // fields
        Field::TIME,
        Field::MASK,
        Field::FLAGS,
        Field::ENVFILE,
        Field::COMPLETED,
        Field::RECURRING,
        Field::NOTIFY,
        "recurring.time",                                                                      // TODO: Improve constants
        "(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files" // TODO: replace with grouping
      }, SelectJoinFilters{
        CompFilter{                                                     // filter
          Field::RECURRING,                                             // field
          "0",                                                          // value
          "<>"                                                          // comparator
        },
        CompFilter{                                                     // filter
          UNIXTIME_NOW,                                                          // field
          "recurring.time + (SELECT get_recurring_seconds(schedule.recurring))", // value (from function)
          ">"                                                                    // comparator
        },
        MultiOptionFilter{                            // filter
          "completed",                                // field of comparison
          "IN", {                                     // comparison type
            Completed::STRINGS[Completed::SCHEDULED], // set of values for comparison
            Completed::STRINGS[Completed::FAILED]
          }
        }
      },
      Joins{
        Join{
          .table      ="recurring",                                     // table to join
          .field      ="sid",                                           // field to join on
          .join_table ="schedule",                                      // table to join to
          .join_field ="id",                                            // field to join to
          .type       = JoinType::INNER                                 // type of join
        },
        Join{
          .table      ="file",
          .field      ="sid",
          .join_table ="schedule",
          .join_field ="id",
          .type       = JoinType::OUTER
        }
      }
    ),
    true, // ⬅ Parse files
    true  // ⬅ Is recurring task
  );
}

/**
 * @brief
 *
 * @return std::vector<Task>
 */
std::vector<Task> Scheduler::fetchAllTasks() {
  return parseTasks(
    m_kdb.selectJoin<QueryFilter>(
      "schedule",
      {
        Field::ID,                                            // fields
        Field::TIME,
        Field::MASK,
        Field::FLAGS,
        Field::ENVFILE,
        Field::COMPLETED,
        Field::RECURRING,
        Field::NOTIFY,
        "(SELECT  string_agg(file.name, ' ') FROM file WHERE file.sid = schedule.id) as files" // TODO: replace with grouping
      },
      {},
      Joins{
        Join{
            .table      ="file",
            .field      ="sid",
            .join_table ="schedule",
            .join_field ="id",
            .type       = JoinType::OUTER
          }
      }
    ),
    true
  );
}

/**
 * getTask
 *
 * get one task by ID
 *
 * @deprecated   NOT SAFE FOR USE
 * @param  [in]  {std::string}  id  The task ID
 * @return [out] {Task}             A task
 */
Task Scheduler::getTask(const std::string& id)
{
  Task task = parseTask(m_kdb.select(
    "schedule", {       // table
      Field::MASK,    Field::FLAGS, // fields
      Field::ENVFILE, Field::TIME,
      Field::COMPLETED
      }, {
        CreateFilter("id", id)      // filter
      }
  ));

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
 * getTask
 *
 * get one task by ID
 *
 * @deprecated   NOT SAFE FOR USE
 *
 * @param  [in]  {int}  id  The task ID
 * @return [out] {Task}     A task
 */
Task Scheduler::getTask(int id) {
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
  const QueryFilter filter = CreateFilter("id", std::to_string(task->id));
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
    if (!HasRecurring(task.id))
      m_kdb.insert("recurring", {"sid", "time"}, {
        std::to_string(task.id),
        std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))});
    else
    m_kdb.update("recurring", {"time"},
      {std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))},
      CreateFilter("sid", std::to_string(task.id)));

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
    }, CreateFilter("id", std::to_string(task.id)),
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
  return !m_kdb.update("recurring", {"time"}, {task->datetime}, CreateFilter("sid", std::to_string(task->id)), "id").empty();
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
  const auto  initiating_task = getTask(applications.first);
  const auto  responding_task = getTask(applications.second);
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

    std::vector<JSONItem> items{};

    for (size_t i = 1; i < (event.payload.size() - 1); i += 2)
      items.emplace_back(JSONItem{.type = event.payload[i], .value = event.payload[i + 1]});

    std::string message{"KIQ is now tracking the following terms:"};

    for (const auto& item : items)
    {
      const auto user       = ReadEnvToken(initiating_task.envfile, constants::USER_KEY);
      const auto known_term = m_research_manager.TermHasHits(item.value);
      const auto term_info  = m_research_manager.RecordTermEvent(item, user, initiating_application);
      if (known_term)
        m_research_manager.AnalyzeTermHit(item.value, term_info.id);
      message               += '\n' + term_info.ToString();
    }

    m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__KIQ_IPC_MESSAGE,
      {CreateOperation("ipc", {constants::TELEGRAM_COMMAND, message, ""})});
  }
}

/**
 * PostExecWait
 */
template <typename T>
void Scheduler::PostExecWait(const int32_t& i, const T& r_)
{
  using ExecPair = std::pair<int32_t, std::vector<int32_t>>;
  int32_t r;
  if constexpr(std::is_integral<T>::value)
    r = r_;
  else
    r = std::stoi(r_);

  auto& map = m_postexec_waiting;
  const auto HasKey = [&map](const int32_t& k) -> bool { return map.find(k) != map.end(); };

  if (!HasKey(i))
    map.insert(ExecPair(i, {}));
  map[i].emplace_back(r);
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
          outgoing_event.payload.emplace_back(FileUtils::ReadEnvToken(getTask(id).envfile, constants::HEADER_KEY));
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
        {
          const auto ProcessResearch = [this, id](const ProcessEventData event) -> void
          { /* 2. Analyze(KNLP)
             * 3. Record DB and Term hits?
             * 4. Sentences from previous hits
             * 5. Analyze (KNLP)
             * 6. Analyze-comparison (KNLP)
             * 7. Compare interests (TODO)
             * 8. Discover memetic origin
             * 9. Discover memetic propagators*/
            KApplication app = ProcessExecutor::GetAppInfo(-1, "KNLP");
            if (!app.is_valid())
            {
               ELOG("No KNLP app for language processing");
               return;
            }

            Task task = GenericTaskHandler::Create(
              app.mask, event.payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX));

            const auto task_id = schedule(task);

            if (task_id.size())
            {
              KLOG("Research triggered scheduling of task {}", task_id);
              PostExecWait(id, task_id);
            }
            else
              ELOG("Failed to schedule {} task", app.name);
          };

          ProcessResearch(outgoing_event);
        }
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
      KLOG("Task {} triggered scheduling of new task with ID {}", task_ptr->id, task.id);

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
    for (const auto& task : responding_tasks)
      if (task == id)
        return initiator;
  return INVALID_ID;
}
