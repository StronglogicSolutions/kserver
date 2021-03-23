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

static bool shouldRepost(const std::string& s)
{
  return (
    s == "1"    ||
    s == "true" ||
    s == "t"
  );
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

std::string savePlatformEnvFile(const PlatformPost& post)
{
  const std::string directory_name{post.time + post.id + post.pid};
  const std::string filename      {directory_name};

  FileUtils::createTaskDirectory(directory_name);

  return FileUtils::saveEnvFile(
    FileUtils::createEnvFile(
    std::unordered_map<std::string, std::string>{
      {"content", post.content},
      {"urls",    post.urls}
    }
  ), filename);
}

bool populatePlatformPost(PlatformPost& post)
{
  const std::string env_path{"data/" + post.time + post.id + post.pid + "/v.env"};
  const std::vector<std::string> post_values = FileUtils::readEnvValues(env_path, PLATFORM_ENV_KEYS);
  if (post_values.size() == 2)
  {
    post.content = post_values.at(0);
    post.urls    = post_values.at(1);
    return true;
  }

  return false;
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


Scheduler::Scheduler()
: m_kdb(Database::KDB{})
{}

Scheduler::Scheduler(Database::KDB&& kdb)
: m_kdb(std::move(kdb))
{}

Scheduler::Scheduler(ScheduleEventCallback fn)
: m_event_callback(fn), m_kdb(Database::KDB{})
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
 * parsePlatformPosts
 *
 * @param   [in] {QueryValues} r-value reference to a QueryValues object
 * @returns [out] {std::vector<PlatformPost>}
 */
std::vector<PlatformPost> Scheduler::parsePlatformPosts(QueryValues&& result) {
  std::vector<PlatformPost> posts{};
  posts.reserve(result.size() / 5);
  std::string pid, o_pid, id, time, repost, name, method;

  for (const auto& v : result) {
         if (v.first == "platform_post.pid"  ) { pid = v.second; }
    else if (v.first == "platform_post.o_pid"  ) { o_pid = v.second; }
    else if (v.first == "platform_post.unique_id" ) { id = v.second; }
    else if (v.first == "platform_post.time" ) { time = v.second; }
    else if (v.first == "platform_post.repost" ) { repost = v.second; }
    else if (v.first == "platform.name" ) { name = v.second; }
    else if (v.first == "platform.method" ) { method = v.second; }

    if (!pid.empty() && !o_pid.empty() && !id.empty() && !time.empty() && !repost.empty() && !name.empty() && !method.empty()) {
      PlatformPost post{};
      post.pid = pid;
      post.o_pid = o_pid;
      post.id = id;
      post.time = time;
      post.repost = repost;
      post.name = name;
      post.method = method;

      posts.emplace_back(std::move(post));

      pid   .clear();
      o_pid .clear();
      id    .clear();
      time  .clear();
      repost.clear();
      name  .clear();
      method.clear();
    }
  }

  return posts;
}

/**
 * fetchTasks
 *
 * Fetch scheduled tasks which are intended to only be run once
 *
 * @return [out] {std::vector<Task>} A vector of Task objects
 */
std::vector<Task> Scheduler::fetchTasks() {
  const std::string past_15_minute_timestamp{UNIXTIME_NOW + " - 900"};
  const std::string current_timestamp{UNIXTIME_NOW};
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
        CompBetweenFilter{                            // filter
          "time",                                     // field of comparison
          past_15_minute_timestamp,                   // min range
          current_timestamp                           // max range
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
 * @brief
 *
 * @param pid
 * @return std::vector<std::string>
 */
std::vector<std::string> Scheduler::fetchRepostIDs(const std::string& pid)
{
  std::vector<std::string> pids{};

  auto result = m_kdb.select(
    "platform_repost",
    {
      "r_pid"
    },
    QueryFilter{
      {"pid", pid}
    }
  );

  for (const auto& value : result)
  {
    if (value.first == "r_pid")
      pids.emplace_back(value.second);
  }

  return pids;
}

bool Scheduler::postAlreadyExists(const PlatformPost& post)
{
  return !(m_kdb.select(
    "platform_post",
    {"id"},
    QueryFilter{{"pid", post.pid},
    {"unique_id", post.id}}
  ).empty());
}

bool Scheduler::updatePostStatus(const PlatformPost& post, const std::string& status)
{
  return (!m_kdb.update(
    "platform_post",
    {"status"},
    {status},
    QueryFilter{{"pid", post.pid},
    {"unique_id", post.id}},
    "id"
  ).empty());
}
/**
 * savePlatformPost
 *
 * @param   [in]  {PlatformPost} post
 * @param   [in]  {std::string}  status
 * @returns [out] {bool}
 */
bool Scheduler::savePlatformPost(PlatformPost post, const std::string& status) {

  if (postAlreadyExists(post))
    return updatePostStatus(post, status);

  auto insert_id = m_kdb.insert(
    "platform_post",
    {"pid", "unique_id", "time", "o_pid", "status", "repost"},
    {post.pid, post.id, post.time, post.o_pid, status, post.repost},
    "id"
  );

  savePlatformEnvFile(post);

  bool result = (!insert_id.empty());

  if (result && shouldRepost(post.repost) && (post.o_pid == constants::NO_ORIGIN_PLATFORM_EXISTS))
    for (const auto& platform_id : fetchRepostIDs(post.pid))
      savePlatformPost(
        PlatformPost{
          .pid     = platform_id,
          .o_pid   = post.pid,
          .id      = post.id,
          .time    = post.time,
          .content = post.content,
          .urls    = post.urls,
          .repost  = post.repost
        },
        constants::PLATFORM_POST_INCOMPLETE
     );

  return result;
}

/**
 * savePlatformPost
 *
 * @param payload
 * @return true
 * @return false
 */
bool Scheduler::savePlatformPost(std::vector<std::string> payload) {
  if (payload.size() < constants::PLATFORM_MINIMUM_PAYLOAD_SIZE)
      return false;

  const std::string& name = payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX);
  const std::string& id   = payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
  const std::string& time = payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX);
  const std::string& platform_id = getPlatformID(name);

  if (platform_id.empty())
    return false;

  if (isProcessingPlatform())
  {
    auto it = m_platform_map.find({platform_id, id});
    if (it != m_platform_map.end())
      it->second = PlatformPostState::SUCCESS;
  }

  return savePlatformPost(PlatformPost{
    .pid     = platform_id,
    .o_pid   = constants::NO_ORIGIN_PLATFORM_EXISTS,
    .id      = id  .empty() ? StringUtils::generate_uuid_string()   : id,
    .time    = time.empty() ? std::to_string(TimeUtils::unixtime()) : time,
    .content = payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX),
    .urls    = payload.at(constants::PLATFORM_PAYLOAD_URL_INDEX),
    .repost  = payload.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX),
    .name    = name
  });
}

/**
 * @brief Get the Platform I D object
 *
 * @param mask
 * @return std::string
 */
std::string Scheduler::getPlatformID(uint32_t mask) {
  auto app_info = ProcessExecutor::getAppInfo(mask);
  if (!app_info.name.empty()) {
    auto result = m_kdb.select(
      "platform",
      {
        "id"
      },
      QueryFilter{
      {"name", app_info.name}
      }
    );
    for (const auto& value : result)
      if (value.first == "id")
        return value.second;
  }
  return "";
}

std::string Scheduler::getPlatformID(const std::string& name) {
  if (!name.empty()) {
    auto result = m_kdb.select(
      "platform",
      {
        "id"
      },
      QueryFilter{
      {"name", name}
      }
    );
    for (const auto& value : result)
      if (value.first == "id")
        return value.second;
  }
  return "";
}

std::vector<PlatformPost> Scheduler::fetchPendingPlatformPosts()
{
  return parsePlatformPosts(
    m_kdb.selectSimpleJoin(
      "platform_post",
      {"platform_post.pid", "platform_post.o_pid", "platform_post.unique_id", "platform_post.time", "platform.name", "platform_post.repost", "platform.method"},
      QueryFilter{
        {"platform_post.status", constants::PLATFORM_POST_INCOMPLETE},
        {"platform_post.repost", constants::SHOULD_REPOST}
      },
      Join{
        .table      = "platform",
        .field      = "id",
        .join_table = "platform_post",
        .join_field = "pid",
        .type       =  JoinType::INNER
      }
    )
  );
}

std::vector<std::string> Scheduler::platformToPayload(PlatformPost& platform)
{
  std::vector<std::string> payload{};
  payload.resize(8);
  payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX) = platform.name;
  payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX)       = platform.id;
  payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX)     = platform.time;
  payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX)  = platform.content;
  payload.at(constants::PLATFORM_PAYLOAD_URL_INDEX)      = platform.urls; // concatenated string
  payload.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX)   = platform.repost;
  payload.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX)   = platform.method;

  return payload;
}
/**
 * @brief
 *
 */
void Scheduler::processPlatform()
{
  if (isProcessingPlatform())
  {
    KLOG("Platform requests are still being processed");
    return;
  }

  for (auto&& platform_post : fetchPendingPlatformPosts())
  {
    if (populatePlatformPost(platform_post))
    {
      m_platform_map.insert({
        {platform_post.pid, platform_post.id}, PlatformPostState::PROCESSING
      });

      m_event_callback(
        ALL_CLIENTS,
        SYSTEM_EVENTS__PLATFORM_POST_REQUESTED,
        platformToPayload(platform_post)
      );

    }
    else
      ELOG("Failed to retrieve values for {} platform post with id {}",
        platform_post.name,
        platform_post.id
      );
  }
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
 * @brief isProcessingPlatform
 *
 * @return true
 * @return false
 */
bool Scheduler::isProcessingPlatform()
{
  for (const auto& platform_request : m_platform_map)
  {
    if (platform_request.second == PlatformPostState::PROCESSING)
      return true;
  }
  return false;
}

/**
 * @brief onPlatformError
 *
 * @param [in] {std::vector<std::string>} payload
 */
void Scheduler::onPlatformError(const std::vector<std::string>& payload)
{
  const std::string& name = payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX);
  const std::string& id   = payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
  const std::string& platform_id = getPlatformID(name);

  if (isProcessingPlatform())
  {
    auto it = m_platform_map.find({platform_id, id});
    if (it != m_platform_map.end())
      it->second = PlatformPostState::FAILURE;
  }

  PlatformPost post{};
  post.name = name;
  post.id   = id;
  post.pid  = platform_id;

  updatePostStatus(post, PLATFORM_STATUS_FAILURE);
}
