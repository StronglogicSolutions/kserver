#include "executor/task_handlers/task.hpp"

namespace kiq {

TaskWrapper* FindNode(const TaskWrapper* node, const int32_t& id);
TaskWrapper* FindParent(const TaskWrapper* node, const int32_t& mask);
TaskWrapper* FindMasterRoot(const TaskWrapper* ptr);

} // ns kiq