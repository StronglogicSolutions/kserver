#pragma once

#include "log/logger.h"
#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "executor/executor.hpp"
#include "server/types.hpp"

namespace kiq {
std::string SavePlatformEnv(const PlatformPost& post);
bool        PopulatePost(PlatformPost& post);

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
    KLOG("Exception thrown retrieving base platform {}. Exception: {}", name, e.what());
    return false;
  }
}

using platform_key_t = std::pair<std::string, std::string>;
class Platform
{
public:
explicit Platform(SystemEventcallback fn);
bool                      SavePlatformPost(std::vector<std::string> payload);
void                      OnPlatformError(const std::vector<std::string>& payload);
void                      ProcessPlatform();
std::vector<PlatformPost> Fetch(bool pending = false) const;
void                      FetchPosts() const;
std::string               Status() const;
std::string               GetPlatform(const std::string& pid);
std::string               GetPlatformID(const std::string& name);
std::string               GetPlatformID(uint32_t mask);
std::string               GetUser  (const std::string& uid, const std::string& pid = "", bool use_default = false) const;

private:
std::vector<std::string>  FetchRepostIDs(const std::string& pid);
std::vector<PlatformPost> FetchPendingPlatformPosts() const;
std::vector<PlatformPost> ParsePlatformPosts(QueryValues&& result) const;
bool                      SavePlatformPost(PlatformPost       post,
                                           const std::string& status = constants::PLATFORM_POST_COMPLETE);
const std::vector<PlatformPost> MakeAffiliatePosts(const PlatformPost& post);
bool                      Update(const PlatformPost& post, const std::string& status);
bool                      PostAlreadyExists(const PlatformPost& post);
bool                      IsProcessingPlatform();
bool                      UserExists(const std::string& pid, const std::string& name);
std::string               AddUser(const std::string& pid, const std::string& name, const std::string& type
= "default");
std::string               GetUID(const std::string& pid, const std::string& name);

// Members
Database::KDB       m_db;
PlatformRequestMap  m_platform_map;
SystemEventcallback m_event_callback;
unsigned int        m_pending{0};
unsigned int        m_errors {0};
unsigned int        m_posted {0};
};
} // ns kiq
