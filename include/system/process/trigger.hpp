#pragma once

#include "log/logger.h"
#include "executor/task_handlers/task.hpp"
#include "database/kdb.hpp"
#include "executor/environment.hpp"
#include <unordered_map>

namespace kiq {
using TriggerPair = std::pair<std::string, std::string>;
using TriggerMap = std::unordered_map<std::string, std::string>;

struct ParamConfigInfo
{
  std::string token_name;
  std::string config_section;
  std::string config_name;
};

struct TriggerParamInfo
{
TriggerMap                   map;
std::vector<ParamConfigInfo> config_info_v;
};

struct TriggerConfig
{
  KApplication     application;
  TriggerParamInfo info;
  std::string      token_name;
  std::string      token_value;
  int32_t          mask;

  bool ready()
  {
    return (!(info.map.empty()) || !(info.config_info_v.empty())) && !(application.path.empty());
  }
};

class Trigger
{
public:
Trigger(Database::KDB* db_ptr)
: m_db(db_ptr) {}

std::vector<Task> process(Task* task);
bool              add(TriggerConfig config);
bool              remove(const std::string& tid);

private:
Database::KDB*      m_db;
};
} // ns kiq