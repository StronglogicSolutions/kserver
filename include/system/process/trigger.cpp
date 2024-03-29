#include "trigger.hpp"
#include <logger.hpp>

namespace kiq {
using namespace kiq::log;

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
   * get_triggers
   * @lambda function
   * @returns [out] std::vector<TriggerConfig>
   */
  const auto get_triggers = [&]() -> std::vector<TriggerConfig> {
    std::vector<TriggerConfig> configs{};
    std::string                id, trigger_mask, token_name, token_value;

    auto query = this->m_db->select("triggers", {"id", "trigger_mask", "token_name", "token_value"},
      CreateFilter("mask", std::to_string(task->mask)));

    for (const auto& row : query)
    {
      id           = row.at("id");
      trigger_mask = row.at("trigger_mask");
      token_name   = row.at("token_name");
      token_value  = row.at("token_value");

      TriggerConfig config;
      auto tok_v = FileUtils::ReadEnvToken(task->env, token_name);
      bool match = tok_v == token_value;

      if (match)
      {
        config.application = get_app_info(std::stoi(trigger_mask));

        if (config.application.is_valid())
        {
          TriggerParamInfo& param_info = config.info;
          param_info.map["id"] = id; // TODO: Possibly best to remove this line
          query.clear();
          query = this->m_db->select("trigger_map", {"old", "new"}, CreateFilter("tid", id));

          for (const auto& m_row : query)
            param_info.map[m_row.at("old")] = m_row.at("new");

          query.clear();
          query = this->m_db->select("trigger_config", {"token_name", "section", "name"}, CreateFilter("tid", id));

          std::string token_name, section, name, config_value;
          for (const auto& c_row : query)

            param_info.config_info_v.emplace_back(ParamConfigInfo{
              .token_name     = c_row.at("token_name"),
              .config_section = c_row.at("section"),
              .config_name    = c_row.at("name")
            });
        }

        configs.emplace_back(std::move(config));
      }
    }

    return configs;
  };

  std::vector<Task> tasks{};

  for (TriggerConfig& config : get_triggers())
  {
    if (config.ready())
    {
      std::string       environment_file{};
      std::string       flags {};
      Task              new_task = Task::clone_basic(*task, std::stoi(config.application.mask));
      const std::string uuid     = StringUtils::GenerateUUIDString();
      // TODO: better to clone envfile and change?
      for (const auto& token : FileUtils::ExtractFlagTokens(task->flags))
      {
        auto map_it = config.info.map.find(token);
        if (map_it != config.info.map.end())
        {
          auto token_name = map_it->second;
          environment_file +=
            '\n' + token_name + "=\"" + FileUtils::ReadEnvToken(task->env, token) +
            '\"' + ARGUMENT_SEPARATOR;
          flags += AsExecutionFlag(token_name, flags.empty() ? "" : " ");
        }
      }

      for (const auto& param_info : config.info.config_info_v)
      {
        const std::string config_value = config::query(param_info.config_section, param_info.config_name);
        if (!config_value.empty())
        {
          environment_file +=
            '\n' + param_info.token_name + "=\"" + FileUtils::ReadFile(config_value) + '\"'  +
            ARGUMENT_SEPARATOR;
          flags += AsExecutionFlag(param_info.token_name, flags.empty() ? "" : " ");
        }
      }

      new_task.flags = flags;
      new_task.env         = FileUtils::SaveEnvFile(environment_file, uuid);
      tasks.emplace_back(std::move(new_task));
    }
  }

  return tasks;
}

/**
 * @brief
 *
 * @param config
 * @return true
 * @return false
 */
bool Trigger::add(TriggerConfig config)
{
  bool error{false};

  if (config.ready())
  {
    auto id = m_db->insert(
      "triggers",
      {"mask", "trigger_mask", "token_name", "token_value"},
      {std::to_string(config.mask), config.application.mask, config.token_name, config.token_value}, "id");

    error = id.empty();

    if (error)
    {
      klog().e("Error inserting trigger into trigger table");
      return false;
    }

    for (const auto& config_mapping : config.info.map)
    {
      const auto& old_token = config_mapping.first;
      const auto& new_token = config_mapping.second;

      error = m_db->insert("trigger_map", {"tid", "old", "new"}, { id, old_token, new_token });
    }

    if (error)
    {
      klog().e("Error inserting trigger map");
      return false;
    }

    for (const auto& info : config.info.config_info_v)
      error = m_db->insert("trigger_config",
                           { "token_name", "section", "name" },
                           { info.token_name, info.config_section, info.config_name } );

    if (error)
    {
      klog().e("Error inserting trigger config");
    }
  }

  return !(error);
}
} // ns kiq