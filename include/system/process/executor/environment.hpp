#ifndef __ENVIRONMENT_HPP__
#define __ENVIRONMENT_HPP__

#include <system/process/executor/executor.hpp>
#include <system/process/executor/task_handlers/task.hpp>
#include <codec/util.hpp>
// #include <database/kdb.hpp>

using namespace Executor;
// using namespace Database;

std::vector<std::string> exec_flags_to_vector(std::string flags) {
  throw std::invalid_argument{"fix this!"};
  return std::vector<std::string>{};
}

/**
 * ExecutionEnvironment Interface
 *
 * @interface
 */
class ExecutionEnvironment {
public:
virtual ~ExecutionEnvironment() {}

virtual void                     setTask(Task task)    = 0;
virtual bool                     prepareRuntime()      = 0;
virtual std::vector<std::string> getProcessArguments() = 0;
};

/**
 * Environment Class
 *
 * @class
 */
class Environment : public ExecutionEnvironment {
public:

/**
 * setTask
 */
virtual void setTask(Task task) override {
  m_task = task;
}

/**
 * prepareRuntime
 */
virtual bool prepareRuntime() override {
  if (m_task.validate()) {
    std::string env  = FileUtils::readEnvFile(m_task.envfile);
    KApplication app = ProcessExecutor::getAppInfo(m_task.execution_mask);

    m_args.push_back(app.path);

    std::vector<std::string> exec_flag_v = exec_flags_to_vector(m_task_execution_flags);

    for (const auto&  : m_task.execution_flags) {
      m_args.push_back(parseExecArgument(runtime_flag, env));
    }
  }
}

/**
 * getProcessArguments
 */
virtual std::vector<std::string> getProcessArguments() override { return m_args; }

private:

std::string parseExecArgument(std::string flag, const std::string& env) {
  std::string argument{};
  std::string::size_type index = env.find(flag);
  if (index != std::string::npos) {
    auto parsed = env.substr(index);
    argument = parsed.substr(parsed.find_first_of('\''), parsed.find_first_of('\n') - 1);
    std::cout << "Parsed argument: " << argument << std::endl;
  }
  return argument;
}

Task                     m_task;
std::vector<std::string> m_args;
};

#endif // __ENVIRONMENT_HPP__
