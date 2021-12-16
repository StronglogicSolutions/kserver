#include "scheduler.hpp"

namespace kiq {

TaskWrapper* FindNode(const TaskWrapper* node, const int32_t& id)
{
  for (auto next = node->child; next;)
    if (next->id == id)  return next;
    else                 next = next->child;
  throw std::invalid_argument{"Node not found"};
}

[[ maybe_unused ]]
bool HasPendingTasks(TaskWrapper* root)
{
  for (auto node = root; node; node = node->child)
    if (!node->complete) return true;
  return false;
};

TaskWrapper* FindParent(const TaskWrapper* node, const int32_t& mask)
{
  auto parent = node->parent;
  while (parent && parent->task.execution_mask != mask)
    parent = parent->parent;
  return parent;
}

TaskWrapper* FindMasterRoot(const TaskWrapper* ptr)
{
  auto root = ptr->parent;
  while (root->parent)
    root = root->parent;
  return root;
}

bool AllTasksComplete (const Scheduler::PostExecMap& map)
{
  for (const auto& [parent_id, task] : map)
    if (!task.second.complete) return false;
  return true;
}

uint32_t getAppMask(std::string name)
{
        auto db = Database::KDB{};
  const auto value_field{"mask"};
  for (const auto& row : db.select("apps", {value_field}, CreateFilter("name", name)))
    if (row.first == value_field)
      return std::stoi(row.second);
  return std::numeric_limits<uint32_t>::max();
}

/**
 * getIntervalSeconds
 *
 * Helper function returns the number of seconds equivalent to a recurring interval
 *
 * @param  [in]  {uint32_t}  The integer value representing a recurring interval
 * @return [out] {uint32_t}  The number of seconds equivalent to that interval
 */
const uint32_t getIntervalSeconds(uint32_t interval) {
  switch(interval) {
    case Constants::Recurring::HOURLY:
      return 3600;
    case Constants::Recurring::DAILY:
      return 86400;
    case Constants::Recurring::MONTHLY:
      return 86400 * 30;
    case Constants::Recurring::YEARLY:
      return 86400 * 365;
    default:
      return 0;
  }
}

/**
 * @brief
 *
 * @param args
 * @return Task
 */
Task args_to_task(std::vector<std::string> args)
{
  Task task;
  if (args.size() == constants::PAYLOAD_SIZE)
  {
    auto mask = getAppMask(args.at(constants::PAYLOAD_NAME_INDEX));
    if (mask != NO_APP_MASK)
    {
      task.task_id         = std::stoi(args.at(constants::PAYLOAD_ID_INDEX));
      task.execution_mask  = mask;
      task.datetime        = args.at(constants::PAYLOAD_TIME_INDEX);
      task.execution_flags = args.at(constants::PAYLOAD_FLAGS_INDEX);
      task.completed       = std::stoi(args.at(constants::PAYLOAD_COMPLETED_INDEX));
      task.recurring       = std::stoi(args.at(constants::PAYLOAD_RECURRING_INDEX));
      task.notify          = args.at(constants::PAYLOAD_NOTIFY_INDEX).compare("1") == 0;
      task.runtime         = StripSQuotes(args.at(constants::PAYLOAD_RUNTIME_INDEX));
      // task.filenames = args.at(constants::PAYLOAD_ID_INDEX;
      task.envfile         = args.at(constants::PAYLOAD_ENVFILE_INDEX);
      KLOG("Can't parse files from schedule payload. Must be implemented");
    }
  }
  return task;
}

bool IsRecurringTask(const Task& task)
{
  return static_cast<uint8_t>(task.recurring) > Constants::Recurring::NO;
}
} // ns kiq
