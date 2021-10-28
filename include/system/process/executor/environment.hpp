#pragma once

#include "task_handlers/task.hpp"
#include "executor.hpp"
#include "database/kdb.hpp"

static KApplication get_app_info(int mask)
{
  Database::KDB     kdb{}; KApplication k_app{};
  const std::string mask_s = std::to_string(mask);

  QueryValues values = kdb.select(
    "apps",                           // table
    {
      "path", "data", "name"          // fields
    },
    {
      {"mask", mask_s}  // filter
    }
  );

  for (const auto &value_pair : values) {
    if (value_pair.first == "path")
      k_app.path = value_pair.second;
    else
    if (value_pair.first == "data")
      k_app.data = value_pair.second;
    else
    if (value_pair.first == "name")
      k_app.name = value_pair.second;
  }

  k_app.mask = mask_s;
  return k_app;
}

struct ExecutionState{
  std::string              path;
  std::vector<std::string> argv;
};

static const std::string RUNTIME_FLAG{"R_ARGS"};

/**
 * exec_flags_to_vector
 *
 * Helper function to parse a string of flag expressions into a vector of flag tokens
 *
 * @param   [in]  {std::string}
 * @returns [out] {std::vector<std::string>}
 */
inline std::vector<std::string> exec_flags_to_vector(std::string flag_s) {
  std::vector<std::string> flags{};
  for (const auto& expression : StringUtils::Split(flag_s, ' ')) {
    flags.push_back(expression.substr(expression.find_first_of('$') + 1));
  }
  return flags;
}


inline std::string parse_filename(std::string filename) {
  return std::string{"--filename=" + filename};
}
/**
 * ExecutionEnvironment Interface
 *
 * @interface
 */
class ExecutionEnvironment {
public:
virtual ~ExecutionEnvironment() {}

virtual void           setTask(Task task)    = 0;
virtual bool           prepareRuntime()      = 0;
virtual ExecutionState get()                 = 0;
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
    std::string  envfile = FileUtils::ReadEnvFile(m_task.envfile, true);
    KApplication app     = get_app_info(m_task.execution_mask);

    m_state.path = app.path;


    for (const auto& runtime_arg : StringUtils::Split(parseExecArguments(envfile), ' '))
      m_state.argv.emplace_back(StringUtils::SanitizeArg(runtime_arg));

    for (const auto& runtime_flag : exec_flags_to_vector(m_task.execution_flags)) {
      auto arg = parseNamedArgument(runtime_flag, envfile);
      if (!arg.empty()) m_state.argv.emplace_back(arg);
    }

    for (const auto& filename : m_task.filenames) {
      m_state.argv.push_back(parse_filename(filename));
    }

    return (!m_state.argv.empty());
  }
  return false;
}

/**
 * getProcessArguments
 */
virtual ExecutionState get() override { return m_state; }

private:
std::string parseNamedArgument(std::string flag, const std::string& env)
{
  using namespace constants;

  std::string            argument{};
  std::string::size_type index       = env.find(flag);

  if (index != std::string::npos) {
    auto parsed = env.substr(index);

    argument += (PARAM_KEY_MAP.find(flag) == PARAM_KEY_MAP.end()) ?
                  flag :
                  PARAM_KEY_MAP.at(flag);
    argument += "=";
    argument += parsed.substr(parsed.find_first_of('\"'), (parsed.find_first_of(ARGUMENT_SEPARATOR) - flag.size() - 1));
  }

  return argument;
}

std::string parseExecArguments(const std::string& env) {
  std::string            argument{};
  std::string::size_type index       = env.find(RUNTIME_FLAG);

  if (index != std::string::npos) {
    auto parsed = env.substr(index);
    auto end    = parsed.find_first_of(ARGUMENT_SEPARATOR);
    argument += (end != std::string::npos) ?
      parsed.substr(parsed.find_first_of('\"'), (end - RUNTIME_FLAG.size() - 1)) :
      parsed.substr(parsed.find_first_of('\"'));
  }
  return argument;
}



Task           m_task;
ExecutionState m_state;
};
