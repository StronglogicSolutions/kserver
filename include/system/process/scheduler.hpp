#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#include "log/logger.h"
#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "result_parser.hpp"

#define NO_COMPLETED_VALUE 99

const char TIMESTAMP_TIME_AS_TODAY[]{
            "(extract(epoch from (TIMESTAMPTZ 'today')) + "\
            "3600 * extract(hour from(to_timestamp(schedule.time))) + "\
            "60 * extract(minute from(to_timestamp(schedule.time))) + "\
            "extract(second from (to_timestamp(schedule.time))))"};

const std::string UNIXTIME_NOW{
  "extract(epoch from (now()))::int"
};

 /**
  * TODO: This should be moved elsewhere. Perhaps the Registrar
  */
uint32_t getAppMask(std::string name);

using ScheduleEventCallback =
    std::function<void(int32_t, int32_t, const std::vector<std::string>&)>;

class DeferInterface {
 public:
  virtual std::string schedule(Task task) = 0;
  virtual ~DeferInterface() {}
};

class CalendarManagerInterface {
 public:
  virtual std::vector<Task> fetchTasks() = 0;
  virtual ~CalendarManagerInterface() {}
};

/**
 * getIntervalSeconds
 *
 * Helper function returns the number of seconds equivalent to a recurring interval
 *
 * @param  [in]  {uint32_t}  The integer value representing a recurring interval
 * @return [out] {uint32_t}  The number of seconds equivalent to that interval
 */
const uint32_t getIntervalSeconds(uint32_t interval);

std::string savePlatformEnvFile(const PlatformPost& post);

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

bool populatePlatformPost(PlatformPost& post);
/**
 * @brief
 *
 * @param args
 * @return Task
 */
Task args_to_task(std::vector<std::string> args);

/**
 * Scheduler
 *
 * @class
 *
 */
class Scheduler : public DeferInterface, CalendarManagerInterface {
public:
        Scheduler();
        Scheduler(Database::KDB&& kdb);
        Scheduler(ScheduleEventCallback fn);

virtual ~Scheduler() override;

virtual std::string               schedule(Task task) override;

        Task                      parseTask(QueryValues&& result);
        std::vector<Task>         parseTasks(QueryValues&& result,
                                             bool          parse_files = false,
                                             bool          is_recurring = false);
        std::vector<PlatformPost> parsePlatformPosts(QueryValues&& result);

virtual std::vector<Task>         fetchTasks() override;
        std::vector<Task>         fetchRecurringTasks();
        std::vector<Task>         fetchAllTasks();
        std::vector<std::string>  fetchRepostIDs(const std::string& pid);
        std::vector<PlatformPost> fetchPendingPlatformPosts();

        Task                      getTask(std::string id);
        Task                      getTask(int id);
        std::vector<std::string>  getFiles(std::string sid);
        std::string               getPlatformID(uint32_t mask);
        std::string               getPlatformID(const std::string& name);

        bool                      update(Task task);
        bool                      updateStatus(Task* task, const std::string& output = "");
        bool                      updateRecurring(Task* task);
        bool                      savePlatformPost(PlatformPost       post,
                                                   const std::string& status = constants::PLATFORM_POST_COMPLETE);
        bool                      savePlatformPost(std::vector<std::string> payload);
        void                      onPlatformError(const std::vector<std::string>& payload);
        bool                      updatePostStatus(const PlatformPost& post, const std::string& status);

        void                      processPlatform();
        bool                      handleProcessOutput(const std::string& output, const int32_t mask);
        bool                      isProcessingPlatform();
        bool                      postAlreadyExists(const PlatformPost& post);
        std::vector<std::string>  platformToPayload(PlatformPost& platform);
        static bool               isKIQProcess(uint32_t mask);

private:
ScheduleEventCallback   m_event_callback;
Database::KDB           m_kdb;
ResultProcessor         m_result_processor;
PlatformRequestMap      m_platform_map;
};
