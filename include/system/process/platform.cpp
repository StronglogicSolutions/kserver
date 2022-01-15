#include "platform.hpp"

namespace kiq {
/**
 * @brief Construct a new Platform:: Platform object
 *
 * @param fn
 */
Platform::Platform(SystemEventcallback fn)
: m_event_callback(fn)
{}

/**
 * @brief
 *
 * @param post
 * @return std::string
 */
std::string SavePlatformEnvFile(const PlatformPost& post)
{
  using namespace FileUtils;
  using Map = std::unordered_map<std::string, std::string>;
  const std::string directory_name{post.time + post.id + post.pid};
  const std::string filename      {directory_name};

  CreateTaskDirectory(directory_name);

  return SaveEnvFile(CreateEnvFile(
    Map{{"content", post.content},
        {"urls",    post.urls},
        {"args",    post.args}}), filename);
}

/**
 * @brief
 *
 * @param post
 * @return true
 * @return false
 */
static bool PopulatePlatformPost(PlatformPost& post)
{
  using namespace FileUtils;
  const std::string              env_path    = "data/" + post.time + post.id + post.pid + "/v.env";
  const std::vector<std::string> post_values = ReadEnvValues(env_path, PLATFORM_ENV_KEYS);
  const size_t                   size        = post_values.size();
  const bool                     has_args    = (size == 3);
  if (size > 1)
  {
    post.content = post_values.at(constants::PLATFORM_POST_CONTENT_INDEX);
    post.urls    = post_values.at(constants::PLATFORM_POST_URL_INDEX);
    post.args    = (has_args) ? CreateOperation("Bot", GetJSONArray(ReadEnvToken(env_path, "args", true))) : "";
    return true;
  }

  return false;
}

static bool MustRepost(const std::string& s)
{
  return (
    s == "1"    ||
    s == "true" ||
    s == "t"
  );
}

/**
 * @brief
 *
 * @param pid
 * @return std::vector<std::string>
 */
std::vector<std::string> Platform::FetchRepostIDs(const std::string& pid)
{
  std::vector<std::string> pids{};
  for (const auto& value : m_db.select("platform_repost", {"r_pid"}, CreateFilter("pid", pid)))
    if (value.first == "r_pid")
      pids.emplace_back(value.second);
  return pids;
}

/**
 * @brief
 *
 * @param post
 * @return true
 * @return false
 */
bool Platform::PostAlreadyExists(const PlatformPost& post)
{
  return !(m_db.selectSimpleJoin(
    "platform_post",
    {"platform_post.id"},
    CreateFilter("platform_post.pid", post.pid, "platform_user.name", post.user, "platform_post.unique_id", post.id),
      Join{
        .table = "platform_user",
        .field = "id",
        .join_table = "platform_post",
        .join_field = "uid",
        .type = JoinType::INNER
      }
  ).empty());
}

/**
 * @brief
 *
 * @param post
 * @param status
 * @return true
 * @return false
 */
bool Platform::UpdatePostStatus(const PlatformPost& post, const std::string& status)
{
  if (post.id.empty() || post.user.empty())
    return false;

  return (!m_db.update(
    "platform_post",
    {"status"},
    {status},
    CreateFilter("pid", post.pid, "unique_id", post.id, "uid", GetUID(post.pid, post.user)),
    "id"
  ).empty());
}

/**
 * @brief Create a Affiliate Post object
 *
 * @param name
 * @param type
 * @param post
 * @return PlatformPost
 */
PlatformPost MakeAffiliatePost(const std::string user, const std::string type, const PlatformPost& post)
{
  static const char DO_NOT_REPOST[] = {"false"};
  const auto createAffiliateContent = [&user, &type, &post]() {
    const std::string affiliate_content = FileUtils::ReadFile(config::Platform::affiliate_content(type));
    return affiliate_content + '\n' + post.content;
  };

  PlatformPost affiliate_post{};
  affiliate_post.id      = StringUtils::GenerateUUIDString();
  affiliate_post.pid     = post.pid;
  affiliate_post.o_pid   = post.pid;
  affiliate_post.user    = user;
  affiliate_post.time    = post.time;
  affiliate_post.urls    = post.urls;
  affiliate_post.method  = post.method;
  affiliate_post.name    = post.name; // TODO: recent addition -> appropriate?
  affiliate_post.args    = post.args;
  affiliate_post.repost  = DO_NOT_REPOST;
  affiliate_post.content = createAffiliateContent();

  return affiliate_post;
}

/**
 * @brief Create a Affiliate Posts object
 *
 * @param post
 * @return const std::vector<PlatformPost>
 */
const std::vector<PlatformPost> Platform::MakeAffiliatePosts(const PlatformPost& post)
{
  std::vector<PlatformPost> affiliate_posts{};

  QueryValues result = m_db.selectSimpleJoin(
    "platform_user",
    {
      "platform_affiliate_user.a_uid",
      "platform_user.type"
    },
    CreateFilter("platform_user.name", post.user, "platform_user.pid",  post.pid),
    Join{
      .table      = "platform_affiliate_user",
      .field      = "uid",
      .join_table = "platform_user",
      .join_field = "id",
      .type       =  JoinType::INNER
    }
  );

  std::string name, type;

  for (const auto& value : result)
  {
    if (value.first == "platform_affiliate_user.a_uid")
    {
      for (const auto& af_value : m_db.select("platform_user", {"name"}, CreateFilter("id", value.second)))
        if (af_value.first == "name")
          name = af_value.second;
    }
    else
    if (value.first == "platform_user.type")
      type = value.second;

    if (!name.empty() && !type.empty())
    {
      const PlatformPost affiliate_post = MakeAffiliatePost(name, type, post);
      if (affiliate_post.is_valid())
        affiliate_posts.emplace_back(affiliate_post);
    }
  }

  return affiliate_posts;
}

/**
 * SavePlatformPost
 *
 * Notes:
 * 1. If post already exists, update it
 * 2. If new post, save particulars to a unique environment file
 * 3. If repostable, save the post for assigned platforms
 * 4. If reposting platforms have affiliate users, save posts for them too
 * 5. If original platform has affiliate users, save posts for them too.
 *
 * TODO: Affiliate User Concerns:
 *  - What do we do about differing usernames? In most cases we could use the DEFAULT_USER
 *  - Otherwise, we should use a [ user : user ] mapping between platforms.
 *
 * @param   [in]  {PlatformPost} post
 * @param   [in]  {std::string}  status
 * @returns [out] {bool}
 */
bool Platform::SavePlatformPost(PlatformPost post, const std::string& status)
{
  auto GetValidUser = [this](auto o_pid, auto name) { return (GetPlatform(o_pid) == "TW Search") ? config::Platform::default_user() : name; };

  if (PostAlreadyExists(post)) return UpdatePostStatus(post, status);

  KLOG("Saving platform post:\n{}", post.ToString());

  std::string uid = GetUID(post.pid, post.user);
  if (uid.empty())
    uid = AddUser(post.pid, post.user);

  const auto id = m_db.insert("platform_post", {"pid",   "unique_id", "time", "o_pid",   "uid", "status", "repost"},
                                               {post.pid, post.id, post.time, post.o_pid, uid, status, post.repost}, "id");
  bool result = (!id.empty());

  if (result && MustRepost(post.repost) && (post.o_pid == constants::NO_ORIGIN_PLATFORM_EXISTS))
  {
    for (const auto& platform_id : FetchRepostIDs(post.pid))
    {
      const PlatformPost repost{
        .pid     = platform_id,
        .o_pid   = post.pid,
        .id      = post.id,
        .user    = GetValidUser(post.pid, post.name),
        .time    = post.time,
        .content = post.content,
        .urls    = post.urls,
        .repost  = post.repost,
        .name    = post.name,
        .args    = ToJSONArray({post.args}),
        .method  = post.method
      };
      SavePlatformPost(repost, constants::PLATFORM_POST_INCOMPLETE);

      for (const auto& af_post : MakeAffiliatePosts(repost))
        SavePlatformPost(af_post, constants::PLATFORM_POST_INCOMPLETE);
    }

    for (const auto& af_post : MakeAffiliatePosts(post))
      SavePlatformPost(af_post, constants::PLATFORM_POST_INCOMPLETE);
  }

  return (result) ? !(SavePlatformEnvFile(post).empty()) :
                    false;
}

/**
 * SavePlatformPost
 *
 * @param payload
 * @return true
 * @return false
 */
bool Platform::SavePlatformPost(std::vector<std::string> payload)
{
  if (payload.size() < constants::PLATFORM_MINIMUM_PAYLOAD_SIZE)
      return false;

  const std::string& name        = payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX);
  const std::string& id          = payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
  const std::string& user        = payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX);
  const std::string& time        = payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX);
  const std::string& content     = payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX);
  const std::string& urls        = payload.at(constants::PLATFORM_PAYLOAD_URL_INDEX);
  const std::string& repost      = payload.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX);
  const std::string& method      = payload.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX);
  const std::string& platform_id = GetPlatformID(name);
  const std::string& args        = payload.size() > 8 ?
                                     payload.at(constants::PLATFORM_PAYLOAD_ARGS_INDEX) :
                                     "";

  if (platform_id.empty())
    return false;

  if (IsProcessingPlatform())
  {
    auto it = m_platform_map.find({platform_id, id});
    if (it != m_platform_map.end())
      it->second = PlatformPostState::SUCCESS;
  }

  return SavePlatformPost(PlatformPost{
    .pid     = platform_id,
    .o_pid   = constants::NO_ORIGIN_PLATFORM_EXISTS,
    .id      = id  .empty() ? StringUtils::GenerateUUIDString()   : id,
    .user    = user,
    .time    = time.empty() ? std::to_string(TimeUtils::UnixTime()) : time,
    .content = content,
    .urls    = urls,
    .repost  = repost,
    .name    = name,
    .args    = args,
    .method  = method
  });
}

/**
 * @brief Get the Platform ID object
 *
 * @param mask
 * @return std::string
 */
std::string Platform::GetPlatformID(uint32_t mask) {
  auto app_info = ProcessExecutor::GetAppInfo(mask);
  if (!app_info.name.empty()) {
    for (const auto& value : m_db.select("platform", {"id"}, CreateFilter("name", app_info.name)))
      if (value.first == "id")
        return value.second;
  }
  return "";
}

/**
 * @brief
 *
 * @param name
 * @return std::string
 */
std::string Platform::GetPlatformID(const std::string& name) {
  if (!name.empty()) {
    for (const auto& value : m_db.select("platform", {"id"}, CreateFilter("name", name)))
      if (value.first == "id")
        return value.second;
  }
  return "";
}

/**
 * ParsePlatformPosts
 *
 * @param   [in] {QueryValues} r-value reference to a QueryValues object
 * @returns [out] {std::vector<PlatformPost>}
 */
std::vector<PlatformPost> Platform::ParsePlatformPosts(QueryValues&& result) {
  std::vector<PlatformPost> posts{};
  posts.reserve(result.size() / 5);
  std::string pid, o_pid, id, time, repost, name, method, uid;

  for (const auto& v : result) {
         if (v.first == "platform_post.pid")       { pid    = v.second;  }
    else if (v.first == "platform_post.o_pid")     { o_pid  = v.second;  }
    else if (v.first == "platform_post.unique_id") { id     = v.second;  }
    else if (v.first == "platform_post.time")      { time   = v.second;  }
    else if (v.first == "platform_post.repost")    { repost = v.second;  }
    else if (v.first == "platform.name")           { name   = v.second;  }
    else if (v.first == "platform.method")         { method = v.second;  }
    else if (v.first == "platform_post.uid")       { uid    = v.second;  }

    if (DataUtils::NoEmptyArgs(pid, o_pid, id, time, repost, name, method, uid))
    {
      PlatformPost post{};
      post.pid    = pid;
      post.o_pid  = o_pid;
      post.id     = id;
      post.user   = GetUser(uid);
      post.time   = time;
      post.repost = repost;
      post.name   = name;
      post.method = method;
      posts.emplace_back(std::move(post));
      DataUtils::ClearArgs(pid, o_pid, id, time, repost, name, method, uid);
    }
  }

  return posts;
}

/**
 * @brief
 *
 * @return std::vector<PlatformPost>
 */
std::vector<PlatformPost> Platform::FetchPendingPlatformPosts()
{
  static const auto        table{"platform_post"};
  static const Fields      fields{"platform_post.pid", "platform_post.o_pid", "platform_post.unique_id",
                                  "platform_post.time", "platform.name", "platform_post.repost",
                                  "platform.method", "platform_post.uid"};
  static const QueryFilter filter = CreateFilter("platform_post.status", constants::PLATFORM_POST_INCOMPLETE);
  static const Join        join{.table      = "platform",
                                .field      = "id",
                                .join_table = "platform_post",
                                .join_field = "pid",
                                .type       =  JoinType::INNER};

  return ParsePlatformPosts(m_db.selectSimpleJoin(table, fields, filter, join));
}

/**
 * @brief IsProcessingPlatform
 *
 * @return true
 * @return false
 */
bool Platform::IsProcessingPlatform()
{
  for (const auto& platform_request : m_platform_map)
  {
    if (platform_request.second == PlatformPostState::PROCESSING)
      return true;
  }
  return false;
}

/**
 * @brief OnPlatformError
 *
 * @param [in] {std::vector<std::string>} payload
 */
void Platform::OnPlatformError(const std::vector<std::string>& payload)
{
  const std::string& name        = payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX);
  const std::string& id          = payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
  const std::string& user        = payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX);
  const std::string& platform_id = GetPlatformID(name);
  const std::string& error       = payload.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX);

  ELOG("Platform error received.\nError message: {}", error);
  SystemUtils::SendMail(config::Email::admin(), error);

  if (!id.empty())
  {
    if (IsProcessingPlatform())
    {
      auto it = m_platform_map.find({platform_id, id});
      if (it != m_platform_map.end())
        it->second = PlatformPostState::FAILURE;
    }

    PlatformPost post{};
    post.name = name;
    post.id   = id;
    post.pid  = platform_id;
    post.user = user;

    UpdatePostStatus(post, PLATFORM_STATUS_FAILURE);
  }
  else
    ELOG("Platform error had no associated post", error);
}

/**
 * @brief
 *
 */
void Platform::ProcessPlatform()
{
  if (IsProcessingPlatform()) return KLOG("Platform requests are still being processed");

  for (auto&& platform_post : FetchPendingPlatformPosts())
  {
    if (PopulatePlatformPost(platform_post))
    {
      KLOG("Processing platform post\n Platform: {}\nID: {}\nUser: {}\nContent: {}",
            platform_post.name, platform_post.id, platform_post.user, platform_post.content);

      m_platform_map.insert({{platform_post.pid, platform_post.id}, PlatformPostState::PROCESSING});
      m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__PLATFORM_POST_REQUESTED, platform_post.GetPayload());
    }
    else
      ELOG("Failed to retrieve values for {} platform post with id {}", platform_post.name, platform_post.id);
  }
}

/**
 * @brief
 *
 * @param pid
 * @param username
 * @return true
 * @return false
 */
bool Platform::UserExists(const std::string& pid, const std::string& name)
{
  return !m_db.select("platform_user", {"id"}, CreateFilter("pid", pid, "name", name)).empty();
}

/**
 * @brief
 *
 * @param pid
 * @param username
 * @param type
 * @return std::string
 */
std::string Platform::AddUser(const std::string& pid, const std::string& name, const std::string& type)
{
  return m_db.insert("platform_user", {"pid", "name", "type"}, {pid, name, type}, "id");
}

/**
 * @brief
 *
 * @param pid
 * @param username
 * @return std::string
 */
std::string Platform::GetUID(const std::string& pid, const std::string& name)
{
  for (const auto& v : m_db.select("platform_user", {"id"}, CreateFilter("pid", pid, "name", name)))
    if (v.first == "id")
      return v.second;
  return "";
}


/**
 * @brief
 *
 * @param pid
 * @param uid
 * @return std::string
 */
std::string Platform::GetUser(const std::string& uid, const std::string& pid, bool use_default)
{
  auto        table  = "platform_user";
  Fields      fields = {"name"};
  QueryFilter filter;

  if (uid.size())
    filter = CreateFilter("id", uid);
  else
  if (use_default && pid.size())
  {
    filter = CreateFilter("pid", pid, "type", "official");
  }
  else
    return "";

  for (const auto& v : m_db.select(table, fields, filter))
    if (v.first == "name")
      return v.second;

  return "";
}

std::string Platform::GetPlatform(const std::string& pid)
{
  for (const auto& value : m_db.select("platform", {"name"}, CreateFilter("id", pid)))
    if (value.first == "name")
      return value.second;
  return "";
}
} // ns kiq