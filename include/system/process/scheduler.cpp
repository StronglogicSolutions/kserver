#include "scheduler.hpp"
#include "executor/task_handlers/generic.hpp"
#include "ipc/ipc.hpp"

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
  static const std::string  table {"apps"};
  static const Fields       fields{"mask", "name"};
  static const QueryFilter  filter{};
  Scheduler::ApplicationMap map   {};
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
  m_research_manager(&m_kdb, &m_platform),
  m_ipc_command(constants::NO_COMMAND_INDEX)
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
  static const Fields fields{Field::ID, Field::MASK, Field::FLAGS, Field::ENVFILE, Field::TIME,Field::COMPLETED};

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
                          Field::MASK,      Field::FLAGS,
                          Field::ENVFILE,   Field::TIME,
                          Field::COMPLETED, Field::ID },
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
void Scheduler::PostExecWork(ProcessEventData&& event, Scheduler::PostExecDuo applications)
{
  using namespace FileUtils;
  const auto& map          = m_postexec_map;
  const auto& lists        = m_postexec_lists;
  const bool& immediately  = false;
  const auto  CompleteTask = [&map, &lists](const int32_t& id) -> void
  {
    auto pid  = map.at(id).first;
    auto node = FindNode(lists.at(pid), id);
    node->complete = true;
  };
  const auto SequenceTasks = [this](const std::vector<TaskParams>& v)  -> void
  {
    int32_t id = v.front().id;
    for (const auto& params : v)
      id = CreateChild(id, params.data, params.name, params.args);
  };
  const auto FindRoot      = [&map, &lists](const int32_t& id) -> TaskWrapper* { return lists.at(map.at(id).first); };
  const auto FindTask      = [&map](const int32_t& id)         -> Task         { return map.at(id).second.task; };
  const auto GetTokens     = [](const auto& payload)           -> std::vector<JSONItem>
  {
    std::vector<JSONItem> tokens{};
    if (payload.size())
      for (size_t i = 1; i < (payload.size() - 1); i += 2)
        tokens.emplace_back(JSONItem{payload[i], payload[i + 1]});
    return tokens;
  };
  const auto PerformAnalysis = [this, &event, &GetTokens](const auto& root, const auto& child, const auto& subchild)
  {
    /****************************************************
     *     NOTE: store tokens for comparison            *
     *     NEXT: IMPLEMENT SOLUTION                     *
     ****************************************************
     ** 1. Collect all tokens from both tasks          **
     ** 2. Perform Word association analysis on tokens **
     ** 3. Retrieve texts from both tasks              **
     ** 4. Perform sentiment analysis on both tasks    **
     ** 5. Perform subject analysis on both tasks      **
     ** 6. Evaluate congruence:                        **
     **   - supporting same ideas                      **
     **   - organizational interests                   **
     ** 7. Trends analysis                             **
     **   - Comparse each task's trend timeline        **
     **   - Identify disrupting word                   **
     **   - Email / IPC notify admin                   **
     ****************************************************
     ****************************************************/
    using Emotion   = EmotionResultParser::Emotion<EmotionResultParser::Emotions>;
    using Sentiment = SentimentResultParser::Sentiment;
    using Terms = std::vector<JSONItem>;
    KLOG("Performing final analysis on research triggered by {}", root.id);
    const auto        root_data      = root.event.payload;                                 // Terms Payload
    const auto        ner_parent     = *(FindParent(&child,        FindMask(NER_APP)));
    const TaskWrapper sub_c_emo_tk   = *(FindParent(&subchild,     FindMask(EMOTION_APP)));
    const TaskWrapper child_c_emo_tk = *(FindParent(&sub_c_emo_tk, FindMask(EMOTION_APP)));
    const auto        sub_c_emo_data = sub_c_emo_tk.event.payload;                         // Emotion Payload
    const auto        child_emo_data = child_c_emo_tk.event.payload;                       // Emotion Payload
    const auto        sub_c_sts_data = subchild.event.payload;                             // Sentiment Payload
    const auto        child_sts_data = child.event.payload;                                // Sentiment Payload
    const Terms       terms_data     = GetTokens(ner_parent.event.payload);                // Terms
    const Emotion     child_emo      = Emotion::Create(child_emo_data);                    // Emotions
    const Emotion     sub_c_emo      = Emotion::Create(sub_c_emo_data);                    // Emotions
    const Sentiment   child_sts      = Sentiment::Create(child_sts_data);                  // Sentiment
    const Sentiment   sub_c_sts      = Sentiment::Create(sub_c_sts_data);                  // Sentiment
  };

  const auto& init_id   = applications.first;
  const auto& resp_id   = applications.second;
  const auto  init_task = GetTask(init_id);
  const auto  resp_task = GetTask(resp_id);
  const auto& init_mask = init_task.execution_mask;
  const auto& resp_mask = resp_task.execution_mask;
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

  if (initiating_application == TW_RESEARCH_APP && responding_application == NER_APP)
  {
    using JSONItem  = NERResultParser::NLPItem;
    static const std::string IPC_Message_Header{"KIQ is now tracking the following terms:"};

    if (m_message_buffer.empty())
      m_message_buffer += IPC_Message_Header;

    auto items = GetTokens(event.payload);
    for (auto&& item : items)
    {
      const auto user       = init_task.GetToken(constants::USER_KEY);
      const auto term_hits  = m_research_manager.GetTermHits(item.value);
      const auto known_term = term_hits.size();
      const auto term_info  = m_research_manager.RecordTermEvent(std::move(item), user, initiating_application, resp_task);
      if (known_term)
        for (auto&& hit : term_hits)
          CreateChild(resp_id, GetTask(hit.sid).GetToken(constants::DESCRIPTION_KEY), NER_APP, {"entity"});     // 1. Analyze NER
      else
      if (term_info.valid())
        m_message_buffer += '\n' + term_info.ToString();
    }

    if (IPCNotPending())
      SetIPCCommand(constants::TELEGRAM_COMMAND_INDEX);
  }
  else
  if (initiating_application == NER_APP && responding_application == NER_APP)                                  // 2. Analyze Emotion
    SequenceTasks({{init_id, resp_task.GetToken(constants::DESCRIPTION_KEY), EMOTION_APP, {"emotion"}},
                   {         init_task.GetToken(constants::DESCRIPTION_KEY), EMOTION_APP, {"emotion"}}});
  else
  if (initiating_application == EMOTION_APP && responding_application == EMOTION_APP)                          // 3. Analyze Sentiment
    SequenceTasks({{init_id, FindTask(resp_id).GetToken(constants::DESCRIPTION_KEY), SENTIMENT_APP, {"sentiment"}},
                   {         FindTask(init_id).GetToken(constants::DESCRIPTION_KEY), SENTIMENT_APP, {"sentiment"}}});
  else
  if (initiating_application == SENTIMENT_APP && responding_application == SENTIMENT_APP)                      // 4. Final analysis
  {
    const auto init_task = map.at(init_id).second;
    const auto resp_task = map.at(resp_id).second;
    const auto init_root = FindRoot(init_id);
    const auto resp_root = FindRoot(resp_id);
    if (init_root == resp_root && m_app_map.at(init_root->task.execution_mask) == TW_RESEARCH_APP)
      PerformAnalysis(*(init_root), init_task, resp_task);
    else
      KLOG("All tasks originating from {} have completed", init_root->id);
  };

  m_postexec_map.at(resp_id).second.SetEvent(std::move(event));
  CompleteTask(resp_id);

  if (AllTasksComplete(m_postexec_map))
    ResolvePending(immediately);
}

/**
 * PostExecWait
 */
template <typename T>
void Scheduler::PostExecWait(const int32_t& i, const T& r_)
{
  static const bool always_complete{true};
  int32_t r;
  if constexpr(std::is_integral<T>::value)
    r = r_;
  else
    r = std::stoi(r_);

  auto& lists = m_postexec_lists;
  auto& map   = m_postexec_map;

  const auto HasKey  = [&lists]            (const int32_t& k) -> bool { return lists.find(k) != lists.end(); };
  const auto AddRoot = [&lists, &map, this](const int32_t& k) -> void
  {
    map  .insert({k, PostExecTuple{k, TaskWrapper{GetTask(k), always_complete}}});
    lists.insert({k, &(map.at(k).second)});
  };
  const auto AddNode = [&lists, &map, this](const int32_t& p, const int32_t& v) -> void
  {
    map  .insert({v, PostExecTuple{p, TaskWrapper{GetTask(v)}}});

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
            PostExecWork(std::move(outgoing_event), PostExecDuo{parent_id, id});
        }
        break;
        case (SYSTEM_EVENTS__PROCESS_RESEARCH):
          CreateChild(id, outgoing_event.payload[constants::PLATFORM_PAYLOAD_CONTENT_INDEX], NER_APP, {"entity"});
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

int32_t Scheduler::FindPostExec(const int32_t& id)
{
  for (const auto& [initiator, task_wrapper] : m_postexec_map)
    if (task_wrapper.second.id == id)
      return task_wrapper.first;
  return INVALID_ID;
}

Scheduler::TermEvents Scheduler::FetchTermEvents() const
{
  return m_research_manager.GetAllTermEvents();
}

void Scheduler::SetIPCCommand(const uint8_t& command)
{
  m_ipc_command = command;
  StartTimer();
}

bool Scheduler::IPCNotPending() const
{
  return (m_ipc_command == constants::NO_COMMAND_INDEX || !TimerActive());
}

void Scheduler::ResolvePending(const bool& check_timer)
{
  if (!TimerActive() || (check_timer && !TimerExpired())) return;

  KLOG("Resolving pending IPC message");
  const auto payload = {CreateOperation("ipc", {constants::IPC_COMMANDS[m_ipc_command], m_message_buffer, ""})};

  m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__KIQ_IPC_MESSAGE, payload);
  m_message_buffer.clear();
  m_postexec_map  .clear();
  m_postexec_lists.clear();
  SetIPCCommand(constants::NO_COMMAND_INDEX);
  StopTimer();
}

template <typename T, typename S>
int32_t Scheduler::CreateChild(const T& id, const std::string& data, const S& application_name, const std::vector<std::string>& args)
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
    const auto new_task_id = schedule(GenericTaskHandler::Create(app.mask, data, "", "", args));
    if (new_task_id.size())
    {
      PostExecWait(task_id, new_task_id);
      KLOG("{} scheduled as a child of {}", new_task_id, task_id);
      return std::stoi(new_task_id);
    }
    else
      ELOG("Failed to schedule {} task", app.name);
  }
  else
    ELOG("Application \"{}\" not found", application_name);

  return INVALID_ID;
};

template int32_t Scheduler::CreateChild(const uint32_t& id, const std::string& data, const std::string& application_name, const std::vector<std::string>& args);
template int32_t Scheduler::CreateChild(const std::string& id, const std::string& data, const std::string& application_name, const std::vector<std::string>& args);

int32_t Scheduler::FindMask(const std::string& application_name)
{
  auto it = std::find_if(m_app_map.begin(), m_app_map.end(),
    [&application_name](const ApplicationInfo& a) { return a.second == application_name; });
  if (it != m_app_map.end())  return it->first;
  return INVALID_MASK;
}