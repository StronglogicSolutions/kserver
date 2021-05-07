#include "scheduler.hpp"


uint32_t getAppMask(std::string name) {
  const std::string filter_field{"name"};
  const std::string value_field{"mask"};

  QueryValues values = Database::KDB{}.select(
    "apps",
    {
      value_field
    },
    {
      {filter_field, name}
    }
  );

  for (const auto &pair : values) {
    auto key = pair.first;
    auto value = pair.second;

    if (key == value_field)
      return std::stoi(value);
  }

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
Task args_to_task(std::vector<std::string> args) {
  Task task{};

  if (args.size() == constants::PAYLOAD_SIZE) {
    auto mask = getAppMask(args.at(constants::PAYLOAD_NAME_INDEX));

    if (mask != NO_APP_MASK) {
      task.id              = std::stoi(args.at(constants::PAYLOAD_ID_INDEX));
      task.execution_mask  = mask;
      task.datetime        = args.at(constants::PAYLOAD_TIME_INDEX);
      task.execution_flags = args.at(constants::PAYLOAD_FLAGS_INDEX);
      task.completed       = args.at(constants::PAYLOAD_COMPLETED_INDEX).compare("1") == 0;
      task.recurring       = std::stoi(args.at(constants::PAYLOAD_RECURRING_INDEX));
      task.notify          = args.at(constants::PAYLOAD_NOTIFY_INDEX).compare("1") == 0;
      task.runtime         = stripSQuotes(args.at(constants::PAYLOAD_RUNTIME_INDEX));
      // task.filenames = args.at(constants::PAYLOAD_ID_INDEX;
      KLOG("Can't parse files from schedule payload. Must be implemented");
    }
  }
  return task;
}

bool IsRecurringTask(const Task& task)
{
  return static_cast<uint8_t>(task.recurring) > Constants::Recurring::NO;
}

Scheduler::Scheduler(Database::KDB&& kdb)
: m_kdb(std::move(kdb)),
  m_platform(nullptr)
{
  KLOG("Scheduler instantiated for testing");
}

Scheduler::Scheduler(SystemEventcallback fn)
: m_event_callback(fn),
  m_kdb(Database::KDB{}),
  m_platform(fn)
{}

Scheduler::~Scheduler() {
  KLOG("Scheduler destroyed");
}

/**
 * schedule
 *
 * Schedule a task
 *
 * @param  [in]  {Task}        task  The task to schedule
 * @return [out] {std::string}       The string value ID representing the Task in the database
 *                                   or an empty string if the insert failed
 */
std::string Scheduler::schedule(Task task) {
  if (task.validate()) {
    try {
      std::string id =
        m_kdb.insert("schedule", {              // table
          "time",                               // fields
          "mask",
          "flags",
          "envfile",
          "recurring",
          "notify"}, {
          task.datetime,                        // values
          std::to_string(task.execution_mask),
          task.execution_flags,
          task.envfile,
          std::to_string(task.recurring),
          std::to_string(task.notify)},
          "id");                                // return
      if (!id.empty()) {
        KLOG("Request to schedule task was accepted\nID {}", id);
        for (const auto& file : task.files) {
          KLOG("Recording file in DB: {}", file.first);
          m_kdb.insert("file", {                  // table
            "name",                               // fields
            "sid"
          }, {
            file.first,                           // values
            id
          });
        }
        if (task.recurring &&
          !m_kdb.insert("recurring", {
            "sid",
            "time"
          }, {
            id,
            std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))
          })) {
            ELOG(
              "Recurring task was scheduled, but there was an error adding"\
              "its entry to the recurring table");
        }
      }
      return id;                                  // schedule id
    } catch (const pqxx::sql_error &e) {
      ELOG("Insert query failed: {}", e.what());
    } catch (const std::exception &e) {
      ELOG("Insert query failed: {}", e.what());
    }
  }
  return "";
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
  for (const auto& v : result) {
    if (v.first == Field::MASK)
      {
        task.execution_mask      = std::stoi(v.second);
      }
else if (v.first == Field::FLAGS)
      { task.execution_flags     = v.second;                  }
else if (v.first == Field::ENVFILE)
      { task.envfile             = v.second;                  }
else if (v.first == Field::TIME)
      { task.datetime            = v.second;                  }
else if (v.first == Field::ID)
      { task.id                  = std::stoi(v.second);       }
else if (v.first == Field::COMPLETED)
      { task.completed           = std::stoi(v.second);       }
else if (v.first == Field::RECURRING)
      { task.recurring           = std::stoi(v.second);       }
else if (v.first == Field::NOTIFY)
      { task.notify              = v.second.compare("t") == 0;}
  }
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
        id > 0           && recurring > -1 && notify > -1      ) {

      if (parse_files && !checked_for_files) {
        checked_for_files = true;
        continue;
      } else {
        if (recurring != Constants::Recurring::NO)
        {
          if (recurring == Constants::Recurring::HOURLY)
          {
            time = std::to_string(std::stoi(time) + getIntervalSeconds(recurring));
          }
          else
          {
            time = TimeUtils::time_as_today(time);
          }
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
          .runtime          = FileUtils::readRunArgs(envfile),                      // ⬅ set in environment.hpp
          .filenames        = StringUtils::split(filenames, ' ')
        });
        id                  = 0;
        recurring           = -1;
        notify              = -1;
        completed           = NO_COMPLETED_VALUE;
        checked_for_files   = false;
        filenames.clear();
        envfile.clear();
        flags.clear();
        time.clear();
        mask.clear();
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
  for (auto& task : tasks) {
    task.filenames = getFiles(std::to_string(task.id));
  }

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
Task Scheduler::getTask(std::string id) {
  Task task = parseTask(m_kdb.select(
    "schedule", {       // table
      Field::MASK,    Field::FLAGS, // fields
      Field::ENVFILE, Field::TIME,
      Field::COMPLETED
      }, {
        {"id", id}      // filter
      }
  ));

  task.filenames = getFiles(id);

  return task;
}

/**
 * @brief Get the Files object
 *
 * @param sid
 * @return std::vector<std::string>
 */
std::vector<std::string> Scheduler::getFiles(std::string sid) {
  std::vector<std::string> file_names{};

  QueryValues result = m_kdb.select(
    "file", {        // table
      "name",        // fields
      }, {
        {"sid", sid} // filter
      }
  );

  for (const auto& v : result) if (v.first == "name")
                                  file_names.push_back(v.second);
  return file_names;
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
  Task task =  parseTask(m_kdb.select(                  // SELECT
    "schedule", {                                       // table
      Field::MASK,    Field::FLAGS, // fields
      Field::ENVFILE, Field::TIME,
      Field::COMPLETED
    }, QueryFilter{
      {"id", std::to_string(id)}                        // filter
    }
  ));

  task.filenames = getFiles(std::to_string(id));        // files

  return task;
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
  return (!m_kdb.update(              // UPDATE
    "schedule",                       // table
    {
      "completed"                     // field
    },
    {
      std::to_string(task->completed) // value
    },
    QueryFilter{
      {"id", std::to_string(task->id)}// filter
    },
    "id"                              // returning value
  )
  .empty());                          // not empty = success
}

  /**
 * update
 *
 * @param   [in] {Task}   task  The task to update
 * @return [out] {bool}         Whether the UPDATE query was successful
 */
bool Scheduler::update(Task task) {
  if (IsRecurringTask(task))
    m_kdb.update(
      "recurring", {"time"},
      {std::to_string(std::stoi(task.datetime) - getIntervalSeconds(task.recurring))},
      {{"sid", std::to_string(task.id)}}
    );

  KLOG("Runtime flags cannot be updated. Must be implemented");
  // TODO: implement writing of R_FLAGS to envfile
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
    }, QueryFilter{
      {"id", std::to_string(task.id)} // filter
    },
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
  return !m_kdb.update(                // UPDATE
    "recurring", {                      // table
      "time"                      // field
    }, {
      task->datetime  // value
    }, QueryFilter{
      {"sid", std::to_string(task->id)} // filter
    },
    "id"                               // returning value
  )
  .empty();                            // not empty = success
}

/**
 * @brief
 *
 * @param mask
 * @return true
 * @return falses
 */
bool Scheduler::isKIQProcess(uint32_t mask) {
  return ProcessExecutor::getAppInfo(mask).is_kiq;
}

/**
 * @brief
 *
 * @param output
 * @param mask
 * @return true
 * @return false
 */
bool Scheduler::handleProcessOutput(const std::string& output, const int32_t mask) {
  ProcessParseResult result = m_result_processor.process(output, ProcessExecutor::getAppInfo(mask));

  if (!result.data.empty())
  {
    for (auto&& outgoing_event : result.data)
      if (outgoing_event.event == SYSTEM_EVENTS__PLATFORM_NEW_POST)
        m_event_callback(ALL_CLIENTS, outgoing_event.event, outgoing_event.payload);
      else
        ELOG("Result processor returned unknown event with code {}", outgoing_event.event);
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
 * @brief
 *
 */
void Scheduler::processPlatform()
{
  m_platform.processPlatform();
}

bool Scheduler::processTriggers(Task* task)
{
  const auto get_application = [](int mask) {
    return get_app_info(mask);
  };

  Task              new_task = Task::clone_basic(*task);
  const std::string uuid = StringUtils::generate_uuid_string();
  std::string       environment_file{};

  for (const auto& token : FileUtils::extractFlagTokens(task->execution_flags))
  {
    environment_file +=
      constants::PARAM_KEY_MAP.at(token)            + "=\"" +
      FileUtils::readEnvToken(task->envfile, token) + '\"'  + ARGUMENT_SEPARATOR + '\n';
  }

  KApplication application = get_application(task->execution_mask);

  if (application.name == "ig_dm")
  {
    new_task.execution_flags += " --direct_message=$DIRECT_MESSAGE";
    environment_file +=
      "DIRECT_MESSAGE=\"" + FileUtils::readEnvToken(task->envfile, "REQUESTED_BY_PHRASE") +
      '\"' + ARGUMENT_SEPARATOR + '\n';
  }

  new_task.envfile = FileUtils::saveEnvFile(environment_file, uuid);
  const std::string new_task_id = schedule(new_task);

  return !(new_task_id.empty());
}