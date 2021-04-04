#include "platform.hpp"

Platform::Platform(SystemEventcallback fn)
: m_event_callback(fn)
{}

std::string savePlatformEnvFile(const PlatformPost& post)
{
  const std::string directory_name{post.time + post.id + post.pid};
  const std::string filename      {directory_name};

  FileUtils::createTaskDirectory(directory_name);

  return FileUtils::saveEnvFile(
    FileUtils::createEnvFile(
    std::unordered_map<std::string, std::string>{
      {"username", post.user},
      {"content", post.content},
      {"urls",    post.urls}
    }
  ), filename);
}

bool populatePlatformPost(PlatformPost& post)
{
  const std::string env_path{"data/" + post.time + post.id + post.pid + "/v.env"};
  const std::vector<std::string> post_values = FileUtils::readEnvValues(env_path, PLATFORM_ENV_KEYS);
  if (post_values.size() == 3)
  {
    post.user    = post_values.at(constants::PLATFORM_POST_USER_INDEX);
    post.content = post_values.at(constants::PLATFORM_POST_CONTENT_INDEX);
    post.urls    = post_values.at(constants::PLATFORM_POST_URL_INDEX);
    return true;
  }

  return false;
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
 * @brief
 *
 * @param pid
 * @return std::vector<std::string>
 */
std::vector<std::string> Platform::fetchRepostIDs(const std::string& pid)
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

bool Platform::postAlreadyExists(const PlatformPost& post)
{
  return !(m_kdb.select(
    "platform_post",
    {"id"},
    QueryFilter{{"pid", post.pid},
    {"unique_id", post.id}}
  ).empty());
}

bool Platform::updatePostStatus(const PlatformPost& post, const std::string& status)
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

PlatformPost createAffiliatePost(const std::string name, const std::string type, const PlatformPost& post)
{
  const auto createAffiliateContent = [&name, &type, &post]() {
    const auto& original_content = post.content;
    (void)(original_content);
    return "Placeholder content";
  };

  PlatformPost affiliate_post{};
  if (type == "placeholder")
  {
    affiliate_post.pid     = post.pid;
    affiliate_post.content = createAffiliateContent();
  }

  return affiliate_post;
}

const std::vector<PlatformPost> createAffiliatePosts(const PlatformPost& post)
{
  std::vector<PlatformPost> affiliate_posts{};

  Database::KDB kdb{};

  QueryValues result = kdb.selectSimpleJoin(
    "platform_user pu",
    {
      "pau.a_uid",
      "pu.type"
    },
    QueryFilter{
      {"pu.name", post.user},
      {"pu.pid", post.pid}
    },
    Join{
      .table = "platform_affiliate_user pau",
      .field      = "id",
      .join_table = "pu",
      .join_field = "pid",
      .type       =  JoinType::INNER
    }
  );

  std::string name{};
  std::string type{};

  for (const auto& value : result)
  {
    if (value.first == "pau.a_uid")
      for (const auto& af_value : kdb.select("platform_user", {"name"}, QueryFilter{{"id", value.second}}))
      {
        if (af_value.first == "name")
          name = af_value.second;
      }
    else
    if (value.first == "pu.type")
      type = value.second;

    if (!name.empty() && !type.empty())
    {
      const PlatformPost affiliate_post = createAffiliatePost(name, type, post);
      if (affiliate_post.is_valid())
        affiliate_posts.emplace_back(affiliate_post);
    }
  }

  return affiliate_posts;
}

/**
 * savePlatformPost
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
bool Platform::savePlatformPost(PlatformPost post, const std::string& status) {

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
  {
    for (const auto& platform_id : fetchRepostIDs(post.pid))
    {
      PlatformPost repost{
        .pid     = platform_id,
        .o_pid   = post.pid,
        .id      = post.id,
        .user    = post.user,
        .time    = post.time,
        .content = post.content,
        .urls    = post.urls,
        .repost  = post.repost
      };
      savePlatformPost(repost, constants::PLATFORM_POST_INCOMPLETE);

      for (const auto& affiliate_post : createAffiliatePosts(repost))
        savePlatformPost(affiliate_post, constants::PLATFORM_POST_INCOMPLETE);
    }

    for (const auto& affiliate_post : createAffiliatePosts(post))
      savePlatformPost(affiliate_post, constants::PLATFORM_POST_INCOMPLETE);
    }

  return result;
}

/**
 * savePlatformPost
 *
 * @param payload
 * @return true
 * @return false
 */
bool Platform::savePlatformPost(std::vector<std::string> payload) {
  if (payload.size() < constants::PLATFORM_MINIMUM_PAYLOAD_SIZE)
      return false;

  const std::string& name = payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX);
  const std::string& id   = payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
  const std::string& user = payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX);
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
    .user    = user,
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
std::string Platform::getPlatformID(uint32_t mask) {
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

std::string Platform::getPlatformID(const std::string& name) {
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

/**
 * parsePlatformPosts
 *
 * @param   [in] {QueryValues} r-value reference to a QueryValues object
 * @returns [out] {std::vector<PlatformPost>}
 */
std::vector<PlatformPost> Platform::parsePlatformPosts(QueryValues&& result) {
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

std::vector<PlatformPost> Platform::fetchPendingPlatformPosts()
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

std::vector<std::string> Platform::platformToPayload(PlatformPost& platform)
{
  std::vector<std::string> payload{};
  payload.resize(8);
  payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX) = platform.name;
  payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX)       = platform.id;
  payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX)     = platform.user;
  payload.at(constants::PLATFORM_PAYLOAD_TIME_INDEX)     = platform.time;
  payload.at(constants::PLATFORM_PAYLOAD_CONTENT_INDEX)  = platform.content;
  payload.at(constants::PLATFORM_PAYLOAD_URL_INDEX)      = platform.urls; // concatenated string
  payload.at(constants::PLATFORM_PAYLOAD_REPOST_INDEX)   = platform.repost;
  payload.at(constants::PLATFORM_PAYLOAD_METHOD_INDEX)   = platform.method;

  return payload;
}

/**
 * @brief isProcessingPlatform
 *
 * @return true
 * @return false
 */
bool Platform::isProcessingPlatform()
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
void Platform::onPlatformError(const std::vector<std::string>& payload)
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

/**
 * @brief
 *
 */
void Platform::processPlatform()
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
