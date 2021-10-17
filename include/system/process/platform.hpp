#pragma once
#include "log/logger.h"
#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "executor/executor.hpp"
#include "server/types.hpp"

std::string savePlatformEnvFile(const PlatformPost& post);
bool        populatePlatformPost(PlatformPost& post);

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

class Platform
{
public:
explicit Platform(SystemEventcallback fn);
bool                      savePlatformPost(std::vector<std::string> payload);
void                      onPlatformError(const std::vector<std::string>& payload);
void                      processPlatform();

private:
std::string               getPlatformID(uint32_t mask);
std::string               getPlatformID(const std::string& name);
std::vector<std::string>  fetchRepostIDs(const std::string& pid);
std::vector<PlatformPost> fetchPendingPlatformPosts();
std::vector<PlatformPost> parsePlatformPosts(QueryValues&& result);
std::vector<std::string>  platformToPayload(PlatformPost& platform);
bool                      savePlatformPost(PlatformPost       post,
                                           const std::string& status = constants::PLATFORM_POST_COMPLETE);
const std::vector<PlatformPost> createAffiliatePosts(const PlatformPost& post);
bool                      updatePostStatus(const PlatformPost& post, const std::string& status);
bool                      postAlreadyExists(const PlatformPost& post);
bool                      isProcessingPlatform();
bool                      userExists(const std::string& pid, const std::string& name);
std::string               addUser(const std::string& pid, const std::string& name, const std::string& type
= "default");
std::string               getUserID(const std::string& pid, const std::string& name);
std::string               getUser  (const std::string& uid);

// Members
Database::KDB       m_db;
PlatformRequestMap  m_platform_map;
SystemEventcallback m_event_callback;
};
