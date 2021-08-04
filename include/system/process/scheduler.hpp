#pragma once

#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#include "log/logger.h"
#include "database/kdb.hpp"
#include "executor/task_handlers/task.hpp"
#include "result_parser.hpp"
#include "platform.hpp"
#include "trigger.hpp"

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

/**
 * @brief
 *
 * @param args
 * @return TaskWrapper
 */
TaskWrapper args_to_task(std::vector<std::string> args);

/**
 * Scheduler
 *
 * @class
 *
 */
class Scheduler : public DeferInterface, CalendarManagerInterface {
public:
        // Scheduler();
        Scheduler(Database::KDB&& kdb);
        Scheduler(SystemEventcallback fn);

virtual ~Scheduler() override;

virtual std::string               schedule(Task task) override;

        Task                      parseTask(QueryValues&& result);
        std::vector<Task>         parseTasks(QueryValues&& result,
                                             bool          parse_files = false,
                                             bool          is_recurring = false);

virtual std::vector<Task>         fetchTasks() override;
        std::vector<Task>         fetchRecurringTasks();
        std::vector<Task>         fetchAllTasks();
        std::vector<std::string>  fetchRepostIDs(const std::string& pid);

        Task                      getTask(std::string id);
        Task                      getTask(int id);
        std::vector<FileMetaData> getFiles(const std::string& sid, const std::string& type = "");

        bool                      update(Task task);
        bool                      updateStatus(Task* task, const std::string& output = "");
        bool                      updateRecurring(Task* task);
        bool                      updateEnvfile(const std::string& id, const std::string& env);

        bool                      handleProcessOutput(const std::string& output, const int32_t mask);
        static bool               isKIQProcess(uint32_t mask);

        void                      processPlatform();
        bool                      savePlatformPost(std::vector<std::string> payload);
        void                      onPlatformError(const std::vector<std::string>& payload);
        bool                      processTriggers(Task*              task);
        bool                      addTrigger(const std::vector<std::string>& payload);

        template <typename T>
        std::vector<std::string>  getFlags(const T& mask);

private:
SystemEventcallback m_event_callback;
Database::KDB       m_kdb;
ResultProcessor     m_result_processor;
Platform            m_platform;
Trigger             m_trigger;

};
