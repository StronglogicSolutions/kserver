#pragma once

#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "executor/executor.hpp"
#include "server/types.hpp"
#include <logger.hpp>

namespace kiq {

using namespace log;
using namespace constants;
using post_t  = PlatformPost;
using posts_t = std::vector<PlatformPost>;


struct TaskParserInterface
{
  using arg_map_t = std::map<std::string, std::function<void(std::string)>>;
  struct Content
  {
    std::string description;
    std::string header;
    std::string hashtags;
    std::string link_bio;
    std::string requested_by;
    std::string requested_by_phrase;
    std::string promote_share;

    std::string value() const;
  };

public:

  virtual ~TaskParserInterface() = default;
  virtual void parse(const std::string& flag, const std::string& arg) = 0;
  void add_file(const std::string& file);
  PlatformPost get();

protected:
  PlatformPost post_;
  Content      content_;
  arg_map_t    arg_map;
};

struct TaskParser : public TaskParserInterface
{

  TaskParser(const Task& task, std::string_view pid);
  void parse(const std::string& flag, const std::string& arg) final;
};
//-------------------------------------------------------------------------------------
std::string SavePlatformEnvFile (const post_t& post);
std::string SavePlatformEnv     (const post_t& post);
bool        PopulatePost        (PlatformPost& post);
bool        PopulatePlatformPost(PlatformPost& post);
bool        MustRepost          (const std::string& s);
//-------------------------------------------------------------------------------------
static const std::vector<std::string> PLATFORM_KEYS{
  "pid"
  "o_pid"
  "id"
  "time"
  "content"
  "urls"
  "repost"
};

static const std::vector<std::string> PLATFORM_ENV_KEYS{
  "content",
  "urls",
  "args"
};

static const std::unordered_map<std::string, std::string> PlatformMaster
{
  {"TW Research", "Twitter"},
  {"TW Search",   "Twitter"},
};

static const std::string GetBasePlatform(const std::string& name)
{
  return PlatformMaster.at(name);
}

static bool IsTwitterPlatform(const std::string& name)
{
  return (PlatformMaster.at(name) == "Twitter");
}

static bool HasBasePlatform(const std::string& name)
{
  try
  {
    auto base = GetBasePlatform(name);
    return (base.size());
  }
  catch (const std::exception& e)
  {
    klog().i("Exception thrown retrieving base platform {}. Exception: {}", name, e.what());
    return false;
  }
}

using platform_key_t = std::pair<std::string, std::string>;
class Platform
{
public:
  explicit Platform(SystemEventcallback fn);
  bool                      SavePlatformPost         (post_t, const std::string& status = constants::PLATFORM_POST_COMPLETE) const;
  bool                      SavePlatformPost         (std::vector<std::string>);
  void                      OnPlatformError          (const std::vector<std::string>&);
  void                      ProcessPlatform          ();
  posts_t                   Fetch                    (bool pending = false)                                                  const;
  void                      FetchPosts               ()                                                                      const;
  std::string               Status                   ()                                                                      const;
  std::string               GetPlatform              (const std::string&)                                                    const;
  std::string               GetPlatformID            (const std::string&)                                                    const;
  std::string               GetPlatformID            (uint32_t)                                                              const;
  std::string               GetUser                  (const std::string&, const std::string& pid = "",
                                                      bool  use_default = false)                                             const;
  PlatformPost              to_post                  (const Task& task)                                                      const;

private:
  std::string               GetUID                   (const std::string&, const std::string&)                                const;
  void                      print_pending            ()                                                                      const;
  std::vector<std::string>  FetchRepostIDs           (const std::string&)                                                    const;
  posts_t                   FetchPendingPlatformPosts()                                                                      const;
  posts_t                   ParsePlatformPosts       (QueryValues&&)                                                         const;
  const posts_t             MakeAffiliatePosts       (const post_t&)                                                         const;
  bool                      Update                   (const post_t&, const std::string&)                                     const;
  bool                      PostAlreadyExists        (const post_t&)                                                         const;
  bool                      IsProcessingPlatform     ();
  bool                      UserExists               (const std::string&, const std::string&)                                const;
  std::string               AddUser                  (const std::string&, const std::string&,
                                                      const std::string& type = "default")                                   const;
  bool                      insert_or_update         (const post_t&);
  void                      fail_pending_posts       ();
  bool                      complete_post            (const post_t&);

  Database::KDB       m_db;
  PlatformRequestMap  m_platform_map;
  SystemEventcallback m_event_callback;
  unsigned int        m_pending{0};
  unsigned int        m_errors {0};
  unsigned int        m_posted {0};
};
} // ns kiq
