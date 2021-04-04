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
  "username",
  "content",
  "urls"
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
bool                      updatePostStatus(const PlatformPost& post, const std::string& status);
bool                      postAlreadyExists(const PlatformPost& post);
bool                      isProcessingPlatform();

// Members
Database::KDB       m_kdb;
PlatformRequestMap  m_platform_map;
SystemEventcallback m_event_callback;
};
