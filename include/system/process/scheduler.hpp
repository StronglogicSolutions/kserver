#pragma once

#include <type_traits>
#include <vector>
#include <deque>

#include "log/logger.h"
#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "result_parser.hpp"
#include "platform.hpp"
#include "trigger.hpp"
#include "research_manager.hpp"

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

static int8_t IG_FEED_IDX{0x00};
static int8_t YT_FEED_IDX{0x01};
static int8_t TW_FEED_IDX{0x02};
static int8_t TW_SEARCH_IDX{0x03};
static int8_t TW_RESEARCH_IDX{0x04};
static int8_t KNLP_IDX{0x05};
static const std::string NLP_APP{"KNLP"};
static const std::string TW_RESEARCH_APP{"TW Research"};
static const char* REQUIRED_APPLICATIONS[]{
  "IG Feed",
  "YT Feed",
  "TW Feed",
  "TW Search",
  TW_RESEARCH_APP.c_str(),
  NLP_APP        .c_str()
};

static const int8_t  REQUIRED_APPLICATION_NUM{6};
static const int32_t INVALID_ID = std::numeric_limits<int32_t>::max();

class DeferInterface
{
 public:
  virtual std::string schedule(Task task) = 0;
  virtual ~DeferInterface() {}
};

class CalendarManagerInterface
{
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

/**
 * @brief
 *
 * @param args
 * @return Task
 */
Task args_to_task(std::vector<std::string> args);

class ResearchManager;
/**
 * Scheduler
 *
 * @class
 *
 */
struct TaskWrapper
{
  int32_t          id;
  bool             complete;
  TaskWrapper*     parent;
  TaskWrapper*     child;
  ProcessEventData event;

  void SetEvent(ProcessEventData&& event_)
  {
    event = event_;
  }
};

class Scheduler : public DeferInterface, CalendarManagerInterface
{
public:
using PostExecDuo     = std::pair<int32_t, int32_t>;
using PostExecQueue   = std::deque<int32_t>;
using PostExecTuple   = std::pair<int32_t, TaskWrapper>;
using PostExecMap     = std::unordered_map<int32_t, PostExecTuple>;
using PostExecLists   = std::unordered_map<int32_t, TaskWrapper*>;
using ApplicationInfo = std::pair<int32_t, std::string>;
using ApplicationMap  = std::unordered_map<int32_t, std::string>;
using TermEvents      = std::vector<ResearchManager::TermEvent>;



        Scheduler(Database::KDB&& kdb);
        Scheduler(SystemEventcallback fn);

virtual ~Scheduler() override;

virtual std::string               schedule(Task task) override;

        Task                      parseTask(QueryValues&& result);
        std::vector<Task>         parseTasks(QueryValues&& result,
                                             bool          parse_files = false,
                                             bool          is_recurring = false);

virtual std::vector<Task>         fetchTasks() override;
        std::vector<Task>         fetchTasks(const std::string& mask,        const std::string& date_range = "0TO0",
                                             const std::string& count = "0", const std::string& limit = "0",
                                             const std::string& order = "asc");
        std::vector<Task>         fetchRecurringTasks();
        std::vector<Task>         fetchAllTasks();
        std::vector<std::string>  fetchRepostIDs(const std::string& pid);

        Task                      GetTask(const std::string& id);
        Task                      GetTask(int id);
        std::vector<FileMetaData> getFiles(const std::string& sid, const std::string& type = "");
        std::vector<FileMetaData> getFiles(const std::vector<std::string>& sids, const std::string& type = "");
        template <typename T>
        bool                      HasRecurring(const T& id);
        bool                      update(Task task);
        bool                      updateStatus(Task* task, const std::string& output = "");
        bool                      updateRecurring(Task* task);
        bool                      updateEnvfile(const std::string& id, const std::string& env);

        bool                      handleProcessOutput(const std::string& output, const int32_t mask, int32_t id);
        static bool               isKIQProcess(uint32_t mask);

        void                      processPlatform();
        bool                      savePlatformPost(std::vector<std::string> payload);
        void                      onPlatformError(const std::vector<std::string>& payload);
        bool                      processTriggers(Task*              task);
        bool                      addTrigger(const std::vector<std::string>& payload);
        int32_t                   FindPostExec(const int32_t& id);
        TermEvents                FetchTermEvents() const;
        void                      ResolvePending(const bool& check_timer = true);

        template <typename T>
        std::vector<std::string>  getFlags(const T& mask);

private:
        void                      PostExecWork(ProcessEventData&& event, Scheduler::PostExecDuo applications);
        template <typename T = int32_t>
        void                      PostExecWait(const int32_t& i, const T& r);
        template <typename T = int32_t>
        void                      ProcessResearch(const T& id, const std::string& data, const std::string& application_name);
        void                      SetIPCCommand(const uint8_t& command);
        bool                      IPCNotPending() const;

SystemEventcallback m_event_callback;
Database::KDB       m_kdb;
ResultProcessor     m_result_processor;
Platform            m_platform;
Trigger             m_trigger;
PostExecLists       m_postexec_lists;      // -> These two need to be converted to a single class
PostExecMap         m_postexec_map;        // -> where the root has access to a map of all the  task lists
ApplicationMap      m_app_map;
ResearchManager     m_research_manager;
std::string         m_message_buffer;
uint8_t             m_ipc_command;

};
