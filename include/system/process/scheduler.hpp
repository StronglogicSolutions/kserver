#pragma once

#include <type_traits>
#include <vector>
#include <deque>
#include <unordered_set>
#include "log/logger.h"
#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "result_parser.hpp"
#include "platform.hpp"
#include "trigger.hpp"
#include "research_manager.hpp"

#define NO_COMPLETED_VALUE 99

namespace kiq {
static const char* TIMESTAMP_TIME_AS_TODAY{"(extract(epoch from (TIMESTAMPTZ 'today')) + "\
                                           "3600 * extract(hour from(to_timestamp(schedule.time))) + "\
                                           "60 * extract(minute from(to_timestamp(schedule.time))) + "\
                                           "extract(second from (to_timestamp(schedule.time))))"};
static const char* UNIXTIME_NOW           {"extract(epoch from (now()))::int"};

class ResearchManager;

struct ResearchPoll
{
int32_t     task_id;
std::string term;

bool operator==(const ResearchPoll& p) const
{
  return ((task_id == p.task_id) && (term == p.term));
}
};

struct PollHash
{
  std::size_t operator() (const ResearchPoll& p) const
  {
    return std::hash<int>()(p.task_id) * 31 + std::hash<int>()(static_cast<int32_t>(p.term.front()));
  }
};

struct TaskParams
{
TaskParams(const int32_t& id_, const std::string& data_, const std::string& name_, const std::vector<std::string>& args_)
: id(id_),
  data(data_),
  name(name_),
  args(args_)
{}

  TaskParams(const std::string& data_, const std::string& name_, const std::vector<std::string>& args_)
: id(0),
  data(data_),
  name(name_),
  args(args_)
{}

int32_t id;
std::string data;
std::string name;
std::vector<std::string> args;
};

static const int32_t INVALID_ID   = std::numeric_limits<int32_t>::max();
static const int32_t INVALID_MASK = std::numeric_limits<int32_t>::max();

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
 * Scheduler
 *
 * @class
 *
 */
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
using TermEvents      = std::vector<TermEvent>;

        Scheduler(Database::KDB&& kdb);
        Scheduler(SystemEventcallback fn);

virtual ~Scheduler() override;

virtual std::string               schedule(Task task) override;
        std::string               ScheduleIPC(const std::vector<std::string>& v);
        void                      ProcessIPC();

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

        bool                      OnProcessOutput(const std::string& output, const int32_t mask, int32_t id);
        static bool               isKIQProcess(uint32_t mask);

        bool                      SavePlatformPost(std::vector<std::string> payload);
        void                      OnPlatformError(const std::vector<std::string>& payload);
        void                      OnPlatformRequest(const std::vector<std::string>& payload);
        bool                      processTriggers(Task*              task);
        bool                      addTrigger(const std::vector<std::string>& payload);
        int32_t                   FindPostExec(const int32_t& id);
        TermEvents                FetchTermEvents() const;
        void                      ResolvePending(const bool& check_timer = true);

        template <typename T>
        std::vector<std::string>  getFlags(const T& mask);

private:
        int32_t                   FindMask(const std::string& application_name);
        void                      PostExecWork(ProcessEventData&& event, Scheduler::PostExecDuo applications);
        template <typename T = int32_t>
        void                      PostExecWait(const int32_t& i, const T& r);
        template <typename T = int32_t, typename S = std::string>
        int32_t                   CreateChild(const T& id, const std::string& data, const S& application_name, const std::vector<std::string>& args = {});
        void                      SetIPCCommand(const uint8_t& command);
        IPCSendEvent              MakeIPCEvent(int32_t event, TGCommand command, const std::string& data, const std::string& arg = "");
        bool                      IPCNotPending() const;
        void                      SendIPCRequest(const std::string& id, const std::string& pid, const std::string& command, const std::string& data, const std::string& time);
        bool                      IPCResponseReceived() const;
        bool                      OnIPCReceived(const std::string& id);

using MessageQueue  = std::deque<IPCSendEvent>;
using DispatchedIPC = std::unordered_map<std::string, PlatformIPC>;
using ResearchPolls = std::unordered_set<ResearchPoll, PollHash>;
using TermIDs       = std::unordered_set<int32_t>;

SystemEventcallback m_event_callback;
Database::KDB       m_kdb;
ResultProcessor     m_result_processor;
Platform            m_platform;
Trigger             m_trigger;
PostExecLists       m_postexec_lists;     // -> These two need to be converted to a single class
PostExecMap         m_postexec_map;       // -> where the root has access to a map of all the  task lists
ApplicationMap      m_app_map;
ResearchManager     m_research_manager;
MessageQueue        m_message_queue;
uint8_t             m_ipc_command;
DispatchedIPC       m_dispatched_ipc;
ResearchPolls       m_research_polls;     // -> should become responsibility of Research Manager (along with much more)
TermIDs             m_term_ids;           // -> same as above
};

bool           HasPendingTasks(TaskWrapper* root);
bool           AllTasksComplete (const Scheduler::PostExecMap& map);
uint32_t       getAppMask(std::string name);
const uint32_t GetIntervalSeconds(uint32_t interval);
Task           args_to_task(std::vector<std::string> args);
bool           IsRecurringTask(const Task& task);
uint32_t       getAppMask(std::string name);
} // ns kiq
