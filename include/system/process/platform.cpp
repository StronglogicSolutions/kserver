#include "platform.hpp"
#include <logger.hpp>

namespace kiq {

using namespace log;
//-------------------------------------------------------------------------------------
//----------------------------------PLATFORM-------------------------------------------
//-------------------------------------------------------------------------------------
Platform::Platform(SystemEventcallback fn)
: m_event_callback(fn)
{}
//-------------------------------------------------------------------------------------
std::vector<std::string> Platform::FetchRepostIDs(const std::string& pid) const
{
  std::vector<std::string> pids{};
  for (const auto& value : m_db.select("platform_repost", {"r_pid"}, CreateFilter("pid", pid)))
    if (value.first == "r_pid")
      pids.emplace_back(value.second);
  return pids;
}
//-------------------------------------------------------------------------------------
bool Platform::PostAlreadyExists(const post_t& post) const
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
//-------------------------------------------------------------------------------------
bool Platform::Update(const post_t& post, const std::string& status) const
{
  if (post.id.empty() || post.user.empty())
    return false;

  const auto id = m_db.update("platform_post", {"status"}, {status},
    CreateFilter("pid", post.pid, "unique_id", post.id, "uid", GetUID(post.pid, post.user)), "id");

  m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__PLATFORM_UPDATE, post.GetPayload());
  return !id.empty();
}
//-------------------------------------------------------------------------------------
static
PlatformPost MakeAffiliatePost(const std::string user, const std::string type, const post_t& post)
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
  affiliate_post.name    = post.name;
  affiliate_post.args    = post.args;
  affiliate_post.repost  = DO_NOT_REPOST;
  affiliate_post.content = createAffiliateContent();

  return affiliate_post;
}
//-------------------------------------------------------------------------------------
const std::vector<PlatformPost> Platform::MakeAffiliatePosts(const post_t& post) const
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
//-------------------------------------------------------------------------------------
 /* Notes:
 * 1. If post already exists, update it
 * 2. If new post, save particulars to a unique environment file
 * 3. If repostable, save the post for assigned platforms
 * 4. If reposting platforms have affiliate users, save posts for them too
 * 5. If original platform has affiliate users, save posts for them too.             */
//-------------------------------------------------------------------------------------
bool Platform::SavePlatformPost(post_t post, const std::string& status) const
{
  using filter_t  = std::pair<std::string, std::string>;
  using filters_t = std::vector<filter_t>;
  auto GetFilters   = [this](auto pid, auto rpid) -> filters_t
  {
    filters_t   result;
    std::string type, value;
    for (const auto& r : m_db.select("platform_filter", {"value", "type"}, CreateFilter("pid", pid, "rpid", rpid)))
    {
      if (r.first == "type" ) type  = r.second;
      else
      if (r.first == "value") value = r.second;

      if (DataUtils::NoEmptyArgs(type, value))
      {
        result.push_back({type, value});
        DataUtils::ClearArgs(type, value);
      }
    }

    return result;
  };
  auto ValidateRepost = [this](auto filters, auto post)
  {
    auto IsTrue = [](auto s) { return (s == "true" || s == "t" || s == "1"); };
    bool result = filters.empty();
    for (const auto& [type, value] : filters)
    {
      if (type == "user")
        result = (post.user == value);
      if (type == "default")
        result = IsTrue(value);
      if (result)
        break;
    }
    return result;
  };
  auto GetValidUser = [this](auto o_pid, auto name) { return (GetPlatform(o_pid) == "TW Search") ? config::Platform::default_user() : name; };

  if (PostAlreadyExists(post))
  {
    post.status = (post.status.empty()) ? status : post.status;
    return Update(post, post.status);
  }

  klog().i("Saving platform post on platform {} by user {}", post.pid, post.user);

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
      if (!ValidateRepost(GetFilters(post.pid, platform_id), post))
        continue;

      const PlatformPost repost{
        platform_id,                        // pid
        post.pid,                           // o_pid
        post.id,                            // id
        GetValidUser(post.pid, post.user),  // user
        post.time,                          // time
        post.content,                       // content
        post.urls,                          // urls
        post.repost,                        // repost
        post.name,                          // name
        ToJSONArray({post.args}),           // args
        post.method};                       // method

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
  PlatformPost post = PlatformPost::FromPayload(payload);
  post.pid = GetPlatformID(post.name);
  if (!complete_post(post))
    klog().w("Failed to update platform post in processing queue. Ignore if new post.");

  return SavePlatformPost(post);
}
//-------------------------------------------------------------------------------------
std::string Platform::GetPlatformID(uint32_t mask) const
{
  auto app_info = ProcessExecutor::GetAppInfo(mask);
  if (!app_info.name.empty()) {
    for (const auto& value : m_db.select("platform", {"id"}, CreateFilter("name", app_info.name)))
      if (value.first == "id")
        return value.second;
  }
  return "";
}
//-------------------------------------------------------------------------------------
std::string Platform::GetPlatformID(const std::string& name) const
{
  if (!name.empty())
    for (const auto& value : m_db.select("platform", {"id"}, CreateFilter("name", name)))
      if (value.first == "id")
        return value.second;
  return "";
}
//-------------------------------------------------------------------------------------
std::vector<PlatformPost> Platform::ParsePlatformPosts(QueryValues&& result) const
{
  static const size_t       post_size = 10;
  std::vector<PlatformPost> posts;
  std::map<std::string, std::string> map;
  for (const auto& v : result)
  {
    map[v.first] = v.second;
    if (map.size() == post_size)
    {
      PlatformPost post;
      post.pid    = map["platform_post.pid"];
      post.o_pid  = map["platform_post.o_pid"];
      post.id     = map["platform_post.unique_id"];
      post.user   = map["platform_user.name"];
      post.time   = map["platform_post.time"];
      post.repost = map["platform_post.repost"];
      post.name   = map["platform.name"];
      post.method = map["platform.method"];
      post.status = map["platform_post.status"];

      if (!PopulatePlatformPost(post))
      {
        klog().e("Failed to retrieve values for {} platform post with id {}", post.name, post.id);
        continue;
      }

      posts.emplace_back(std::move(post));
      map.clear();
    }
  }

  return posts;
}
//-------------------------------------------------------------------------------------
std::vector<PlatformPost> Platform::FetchPendingPlatformPosts() const
{
  return Fetch(true);
}
//-------------------------------------------------------------------------------------
bool Platform::IsProcessingPlatform()
{
  for (const auto& platform_request : m_platform_map)
    if (platform_request.second.second == PlatformPostState::PROCESSING)
      return true;
  return false;
}
//-------------------------------------------------------------------------------------
void Platform::OnPlatformError(const std::vector<std::string>& payload)
{
  const std::string& name        = payload.at(constants::PLATFORM_PAYLOAD_PLATFORM_INDEX);
  const std::string& id          = payload.at(constants::PLATFORM_PAYLOAD_ID_INDEX);
  const std::string& user        = payload.at(constants::PLATFORM_PAYLOAD_USER_INDEX);
  const std::string& platform_id = GetPlatformID(name);
  const std::string& error       = payload.at(constants::PLATFORM_PAYLOAD_ERROR_INDEX);

  klog().e("{} platform error received for {}.\nError message: {}", name, id, error);
  SystemUtils::SendMail(config::Email::admin(), error);

  if (!id.empty())
  {
    if (IsProcessingPlatform())
    {
      auto it = m_platform_map.find({platform_id, id});
      if (it != m_platform_map.end())
      {
        it->second.second = PlatformPostState::FAILURE;
        m_errors++;
        m_pending--;
      }
    }

    PlatformPost post;
    post.name = name;
    post.id   = id;
    post.pid  = platform_id;
    post.user = user;

    Update(post, PLATFORM_STATUS_FAILURE);
  }
  else
    klog().e("Platform error had no associated post", error);
}
//-------------------------------------------------------------------------------------
void
Platform::ProcessPlatform()
{
  static Timer timer{Timer::TEN_MINUTES};

  if (!timer.active())
    timer.reset();
  else
  if (timer.active() && timer.expired())
  {
    fail_pending_posts();
    timer.stop();
  }

  if (IsProcessingPlatform())
    return;
    // return print_pending();

  for (const auto& post : FetchPendingPlatformPosts())
  {
    klog().i("Processing post {} for {} by {}", post.name, post.id, post.user);
    if (insert_or_update(post))
    {
      m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__PLATFORM_POST_REQUESTED, post.GetPayload());
      m_pending++;
    }
  }
}
//--------------------------------------------------------------------------------------
void
Platform::print_pending() const
{
  auto get_pending_reqs = [this]
  {
    std::string s;
    for (const auto& [post, status] : m_platform_map)
      s += '\n' + post.second + " for " + GetPlatform(post.first) +
      ". Status: "                      + GetPostStatus(status.second);
    return s;
  };
  klog().i("Platform requests are still being processed: {}", get_pending_reqs());
}
//--------------------------------------------------------------------------------------
bool Platform::insert_or_update(const post_t& p)
{
  const platform_key_t key{p.pid, p.id};
  if (auto it = m_platform_map.find(key); it == m_platform_map.end())
  {
    m_platform_map[key] = {p, PlatformPostState::PROCESSING};
    return true;
  }
  else
  if (it->second.second == PlatformPostState::PROCESSING)
  {
    klog().w("Platform is already processing {} for {}", p.id, p.name);
    return false;
  }
  else
  if (it->second.second == PlatformPostState::FAILURE)
  {
    if (it->second.first.retry)
    {
      klog().w("Failed post has already had a retry. Will not update");
      return false;
    }
    klog().d("Post previously failed. Retrying");
    it->second.second == PlatformPostState::PROCESSING;
    it->second.first.retry = true;
    return true;
  }

  return true;
}
//-------------------------------------------------------------------------------------
void Platform::fail_pending_posts()
{
  for (auto&& post : m_platform_map)
    if (post.second.second == PlatformPostState::PROCESSING)
    {
      klog().e("{} post {} failed", post.second.first.name, post.second.first.id);
      post.second.second = PlatformPostState::FAILURE;
      Update(post.second.first, PLATFORM_STATUS_FAILURE);
      m_errors++;
      m_pending--;
    }
}
//-------------------------------------------------------------------------------------
bool Platform::complete_post(const post_t& post)
{
    klog().i("Attempting to complete PID {} and ID {}", post.pid, post.id);
    if (auto it = m_platform_map.find({post.pid, post.id}); it != m_platform_map.end())
    {
      if (it->second.second != PlatformPostState::SUCCESS)
      {
        m_posted++;
        m_pending--;
      }

      klog().i("Completed {} platform request {}", post.name, post.id);
      m_platform_map.erase(it);
      return true;
  }
  return false;
}
//-------------------------------------------------------------------------------------
bool Platform::UserExists(const std::string& pid, const std::string& name) const
{
  return !m_db.select("platform_user", {"id"}, CreateFilter("pid", pid, "name", name)).empty();
}
//-------------------------------------------------------------------------------------
std::string Platform::AddUser(const std::string& pid, const std::string& name, const std::string& type) const
{
  return m_db.insert("platform_user", {"pid", "name", "type"}, {pid, name, type}, "id");
}
//-------------------------------------------------------------------------------------
std::string Platform::GetUID(const std::string& pid, const std::string& name) const
{
  for (const auto& v : m_db.select("platform_user", {"id"}, CreateFilter("pid", pid, "name", name)))
    if (v.first == "id")
      return v.second;
  return "";
}
//-------------------------------------------------------------------------------------
std::string Platform::GetUser(const std::string& uid, const std::string& pid, bool use_default) const
{
  static const auto   table  = "platform_user";
  static const Fields fields = {"name"};
  QueryFilter         filter;

  if (uid.size())
    filter = CreateFilter("id", uid);
  else
  if (use_default && pid.size())
    filter = CreateFilter("pid", pid, "type", "official");
  else
    return "";

  for (const auto& v : m_db.select(table, fields, filter))
    if (v.first == "name")
      return v.second;

  return "";
}
//-------------------------------------------------------------------------------------
std::string Platform::GetPlatform(const std::string& pid) const
{
  for (const auto& value : m_db.select("platform", {"name"}, CreateFilter("id", pid)))
    if (value.first == "name")
      return value.second;
  return "";
}
//-------------------------------------------------------------------------------------
std::string Platform::Status() const
{
  return fmt::format("Platform Status: Pending {} Complete {} Errors {}", m_pending, m_posted, m_errors);
}
//-------------------------------------------------------------------------------------
std::vector<PlatformPost> Platform::Fetch(bool pending) const
{
  return ParsePlatformPosts(m_db.selectJoin("platform_post", {"platform_post.pid", "platform_post.o_pid", "platform_post.unique_id",
                                                              "platform_post.time", "platform.name", "platform_post.repost", "platform.method", "platform_post.uid", "platform_post.status", "platform_user.name"},
    (pending) ? CreateFilter("platform_post.status", constants::PLATFORM_POST_INCOMPLETE) : QueryFilter{},
    Joins{{"platform", "id", "platform_post", "pid",      JoinType::INNER},
          {"platform_user", "id", "platform_post", "uid", JoinType::OUTER}}));
}
//-------------------------------------------------------------------------------------
void Platform::FetchPosts() const
{
  static bool              not_only_pending = false;
  std::vector<std::string> payload;
  for (auto& post : Fetch(not_only_pending))
  {
    PopulatePlatformPost(post);
    const auto post_payload = post.GetPayload();
    payload.insert(payload.end(), post_payload.begin(), post_payload.end());
  }

  m_event_callback(ALL_CLIENTS, SYSTEM_EVENTS__PLATFORM_FETCH_POSTS , payload);
}

//----------------------------------------------------------------------------------------------
PlatformPost Platform::to_post(const Task& task) const
{
  //-----------------------------
  TaskParser parser{task, GetPlatformID(task.mask)};
  for (const auto& flag : exec_flags_to_vector(task.flags))
    parser.parse(flag, task.GetToken(flag));

  for (const auto& file : task.filenames)
    parser.add_file(file);

  return parser.get();
}

} // ns kiq
