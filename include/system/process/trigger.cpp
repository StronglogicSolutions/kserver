#include "trigger.hpp"

/**
 * @brief processTriggers
 *
 * @param task
 * @param field_value
 * @param field_name
 * @return true
 * @return false
 */

std::vector<Task> Trigger::process(Task* task)
{
  /**
   * get_trigger
   * @lambda function
   * @returns [out] KApplication
   */
  const auto get_trigger = [&]() -> TriggerConfig {
    TriggerConfig config{};
    std::string   id, mask, token_name, token_value;

    auto query = this->m_db->select("triggers",
      {"id", "trigger_mask", "token_name", "token_value"},
      QueryFilter{{"mask", std::to_string(task->execution_mask)}}
    );

    // TODO: Assumes only one possible trigger. Amend to support multiple
    for (const auto& value : query)
      if (value.first == "id")
        id = value.second;
      else
      if (value.first == "trigger_mask")
        mask = value.second;
      else
      if (value.first == "token_name")
        token_name = value.second;
      else
      if (value.first == "token_value")
        token_value = value.second;

    if (!mask.empty() && !token_name.empty() && !token_value.empty())
    {
      auto tok_v = FileUtils::readEnvToken(task->envfile, token_name);
      bool match = tok_v == token_value;
      if (match)
        config.application = get_app_info(std::stoi(mask));
    }

    if (config.application.is_valid())
    {
      TriggerParamInfo& param_info = config.info;
      param_info.map["id"] = id;
      TriggerPair pair{};
      query.clear();
      query = this->m_db->select("trigger_map",
        {"old", "new"},
        QueryFilter{{"tid", id}}
      );

      for (const auto& value : query)
      {
        if (value.first == "old")
          pair.first = value.second;
        else
        if (value.first == "new")
          pair.second = value.second;
        if (!pair.first.empty() && !pair.second.empty())
          param_info.map[pair.first] = pair.second;
      }

      query.clear();
      query = this->m_db->select("trigger_config",
        {"token_name", "section", "name"},
        QueryFilter{{"tid", id}}
      );

      std::string token_name, section, name, config_value;
      for (const auto& value : query)
      {
        if (value.first == "token_name")
          token_name = value.second;
        else
        if (value.first == "section")
          section = value.second;
        else
        if (value.first == "name")
          name = value.second;
        if (!token_name.empty() && !section.empty() && !name.empty())
          param_info.config_info_v.emplace_back(ParamConfigInfo{
            .token_name = token_name,
            .config_section = section,
            .config_name = name
          });
      }
    }

    config.ready = !(config.info.map.empty()) || !(config.info.config_info_v.empty());
    return config;
  };

  std::vector<Task> tasks{};
  TriggerConfig     config = get_trigger();

  if (config.ready)
  {
    std::string       environment_file{};
    std::string       execution_flags {};
    Task              new_task = Task::clone_basic(*task, std::stoi(config.application.mask));
    const std::string uuid     = StringUtils::generate_uuid_string();
    // TODO: better to clone envfile and change?
    for (const auto& token : FileUtils::extractFlagTokens(task->execution_flags))
    {
      auto map_it = config.info.map.find(token);
      if (map_it != config.info.map.end())
      {
        auto token_name = map_it->second;
        environment_file += token_name + "=\"" + FileUtils::readEnvToken(task->envfile, token) + '\"' + ARGUMENT_SEPARATOR + '\n';
        execution_flags += AsExecutionFlag(token_name);
      }
    }

    for (const auto& param_info : config.info.config_info_v)
    {
      const std::string config_value = ConfigParser::query(param_info.config_section, param_info.config_name);
      if (!config_value.empty())
      {
        environment_file += param_info.token_name + "=\"" + FileUtils::readFile(config_value) +
        '\"'  + ARGUMENT_SEPARATOR + '\n';
        execution_flags += AsExecutionFlag(param_info.token_name);
      }
    }
    new_task.execution_flags = execution_flags;
    new_task.envfile         = FileUtils::saveEnvFile(environment_file, uuid);
    tasks.emplace_back(std::move(new_task));
  }

  return tasks;
}
