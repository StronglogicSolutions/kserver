#include "executor.hpp"
#include <logger.hpp>

namespace kiq {
namespace constants {
const uint8_t IMMEDIATE_REQUEST = 0;
const uint8_t SCHEDULED_REQUEST = 1;
const uint8_t RECURRING_REQUEST = 2;
} // namespace constants
//------------------------------------------------------------------------------
const char* findWorkDir(std::string_view path)
{
  return path.substr(0, path.find_last_of("/")).data();
}
//------------------------------------------------------------------------------
using namespace kiq::log;
//------------------------------------------------------------------------------
ProcessResult run_(std::string_view path, std::vector<std::string> argv)
{
  std::vector<std::string> v{path.data()};
  v.insert(v.end(), std::make_move_iterator(argv.begin()), std::make_move_iterator(argv.end()));
  return qx(v, findWorkDir(path));
}
//------------------------------------------------------------------------------
ProcessDaemon::ProcessDaemon(std::string_view path, std::vector<std::string> argv)
: m_path(std::move(path)), m_argv(std::move(argv)) {}
//------------------------------------------------------------------------------
ProcessResult ProcessDaemon::run()
{
  std::future<ProcessResult> result_future =
      std::async(std::launch::async, &run_, m_path, m_argv);
  return result_future.get();
}
//------------------------------------------------------------------------------
ProcessExecutor::ProcessExecutor(const ProcessExecutor &e)
: m_callback(e.m_callback), m_tracked_callback(e.m_tracked_callback) {}
//------------------------------------------------------------------------------
ProcessExecutor::ProcessExecutor(ProcessExecutor &&e)
: m_callback(e.m_callback), m_tracked_callback(e.m_tracked_callback)
{
  e.m_callback         = nullptr;
  e.m_tracked_callback = nullptr;
}
//------------------------------------------------------------------------------
ProcessExecutor& ProcessExecutor::operator=(const ProcessExecutor &e)
{
  this->m_callback         = nullptr;
  this->m_tracked_callback = nullptr;
  this->m_callback         = e.m_callback;
  this->m_tracked_callback = e.m_tracked_callback;
  return *this;
};
//------------------------------------------------------------------------------
ProcessExecutor& ProcessExecutor::operator=(ProcessExecutor &&e)
{
  if (&e != this) {
    m_callback           = e.m_callback;
    m_tracked_callback   = e.m_tracked_callback;
    e.m_callback         = nullptr;
    e.m_tracked_callback = nullptr;
  }
  return *this;
}
//------------------------------------------------------------------------------
void ProcessExecutor::setEventCallback(ProcessEventCallback f)
{
  m_callback = f;
}
//------------------------------------------------------------------------------
void ProcessExecutor::setEventCallback(TrackedEventCallback f)
{
  m_tracked_callback = f;
}
//------------------------------------------------------------------------------
void ProcessExecutor::notifyProcessEvent(std::string std_out,
                                         int         mask,
                                         int         client_socket_fd,
                                         bool error)
{
  m_callback(std_out, mask, client_socket_fd, error);
}
//------------------------------------------------------------------------------
void ProcessExecutor::notifyTrackedProcessEvent(std::string std_out, int mask,
                                        std::string id, int client_socket_fd,
                                        bool error)
{
  m_tracked_callback(std_out, mask, id, client_socket_fd, error);
}
//------------------------------------------------------------------------------
void ProcessExecutor::request(std::string_view         path,
                              int                      mask,
                              int                      client_socket_fd,
                              std::vector<std::string> argv)
{
  if (path[0])
  {
    ProcessDaemon *pd_ptr = new ProcessDaemon(path, argv);
    auto result = pd_ptr->run();
    if (!result.output.empty()) {
      notifyProcessEvent(result.output, mask, client_socket_fd, result.error);
    }
    delete pd_ptr;
  }
}
//------------------------------------------------------------------------------
void ProcessExecutor::request(std::string_view         path,
                              int                      mask,
                              int                      client_socket_fd,
                              std::string              id,
                              std::vector<std::string> argv,
                              uint8_t                  type)
{
  if (path[0])
  {
    ProcessDaemon* pd_ptr = new ProcessDaemon(path, argv);
    ProcessResult  result = pd_ptr->run();

    notifyTrackedProcessEvent(result.output, mask, id, client_socket_fd, result.error);
    if (!result.error && type != constants::IMMEDIATE_REQUEST)
    {
      Database::KDB kdb;
      auto COMPLETED =
        type == constants::RECURRING_REQUEST ?
          Completed::STRINGS[Completed::SCHEDULED] :
          Completed::STRINGS[Completed::SUCCESS];
      std::string result = kdb.update("schedule", {"completed"}, {COMPLETED}, CreateFilter("id", id), "id");
      klog().i("Updated task {} to reflect its completion", result);
    }

    delete pd_ptr;
  }
}

template <typename T>
bool ProcessExecutor::saveResult(uint32_t mask, T status, uint32_t time) {
  try {
    auto app_info = ProcessExecutor::GetAppInfo(mask);

    if (!app_info.id.empty()) {
      Database::KDB kdb{};

      std::string id =
        kdb.insert("process_result", {        // table
          "aid",                              // fields
          "time",
          "status"}, {
          app_info.id,                        // values
          std::to_string(time),
          std::to_string(status)},
          "id");                             // return
      if (!id.empty()) {
        klog().i("Recorded process {} with result of {} at {}",
          app_info.name,
          Completed::NAMES[status],
          TimeUtils::FormatTimestamp(time)
        );

        return true;
      }
    }
  } catch (const pqxx::sql_error &e) {
    klog().e("Insert query failed: {}", e.what());
  } catch (const std::exception &e) {
    klog().e("Insert query failed: {}", e.what());
  }

  return false;
}

template bool ProcessExecutor::saveResult(uint32_t, int,     uint32_t);
template bool ProcessExecutor::saveResult(uint32_t, uint8_t, uint32_t);
template bool ProcessExecutor::saveResult(uint32_t, char,    uint32_t);

void ProcessExecutor::executeTask(int client_socket_fd, Task task)
{
  klog().i("Executing task {}", task.task_id);

  Environment environment{};
  environment.setTask(task);

  if (environment.prepareRuntime())
  {
    ExecutionState exec_state = environment.get();
    request(
      exec_state.path,
      task.mask,
      client_socket_fd,
      task.id(),
      exec_state.argv,
      (task.recurring) ?
        constants::RECURRING_REQUEST :
        constants::SCHEDULED_REQUEST
    );
  } //
}
//------------------------------------------------------------------------------
template <typename T>
KApplication ProcessExecutor::GetAppInfo(const int32_t& mask, const T& name)
{
  if (mask == -1 && name.empty())
    throw std::invalid_argument{"Must provide mask or name"};

  Database::KDB kdb;
  KApplication  app;
  Fields        fields{"id", "path", "data", "name", "internal", "mask"};
  QueryFilter   filter{};

  if (mask > -1)
    filter.Add("mask", std::to_string(mask));
  if (name.size())
    filter.Add("name", name);

  QueryValues values = kdb.select("apps", fields, filter);

  for (const auto &value_pair : values)
  {
    if (value_pair.first == "path")
      app.path   = value_pair.second;
    else
    if (value_pair.first == "data")
      app.data   = value_pair.second;
    else
    if (value_pair.first == "name")
      app.name   = value_pair.second;
    else
    if (value_pair.first == "id")
      app.id     = value_pair.second;
    else
    if (value_pair.first == "mask")
      app.mask   = value_pair.second;
    else
    if (value_pair.first == "internal")
      app.is_kiq = (value_pair.second == "t") ? true : false;
  }

  return app;
}
//------------------------------------------------------------------------------
template KApplication ProcessExecutor::GetAppInfo(const int32_t& mask = -1, const std::string& name = "");

} // ns kiq
