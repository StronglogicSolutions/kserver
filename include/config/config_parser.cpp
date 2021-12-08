#include "config_parser.hpp"

namespace kiq::config {
static INIReader reader{"config/config.ini"};

const auto RequiredConfig = [](const std::string& arg) -> std::string { return "CONFIG REQUIRED: " + arg; };

static bool init()
{
  if (reader.ParseError() != 0)
    reader = INIReader{"config/default.config.ini"};
  if (reader.ParseError() != 0)
    reader = INIReader{"../config/default.config.ini"};
  if (reader.ParseError() != 0)
    throw std::invalid_argument{"Failed to initialize config. Please create config/config.ini"};
  return true;
}

static bool initialized = init();

std::string query(const std::string& section, const std::string& name)
{
  const std::string value = reader.Get(section, name, "");
  return value;
}

namespace System {
const std::string admin() { return reader.Get("system", "admin", "admin"); }
}
namespace Logging {
const std::string level()     { return reader.Get("logging", "level",      "info"); }
const std::string path()      { return reader.Get("logging", "path",       RequiredConfig("[logging] path")); }
const std::string timestamp() { return reader.Get("logging", "timestamp",  "true"); }
} // namespace Logging

namespace Database {
const std::string pass()      { return reader.Get("database", "password",  RequiredConfig("[database] password")); }
const std::string name()      { return reader.Get("database", "name",      RequiredConfig("[database] name")); }
const std::string user()      { return reader.Get("database", "user",      RequiredConfig("[database] user")); }
const std::string port()      { return reader.Get("database", "port",      RequiredConfig("[database] port")); }
const std::string host()      { return reader.Get("database", "host",      RequiredConfig("[database] host")); }
} // namespace Database

namespace Process {
const std::string executor()  { return reader.Get("process", "executor",   RequiredConfig("[process] executor"));   }
const std::string ipc_port()  { return reader.Get("process", "ipc_port",   "28473");                                }
} // namespace Process

namespace Email {
std::string notification()    { return reader.Get("email", "notification", RequiredConfig("[email] notification")); }
std::string admin()           { return reader.Get("email", "admin",        RequiredConfig("[email] admin"));        }
std::string command()         { return reader.Get("email", "command",      RequiredConfig("[email] command"));      }
} // namespace Admin

namespace Platform {
std::string affiliate_content(const std::string& type)
{
  std::string section{"affiliate"};
  section  += '_';
  section  += type;
  std::string value = reader.Get("platform", section, RequiredConfig("[platform] " + section));
  return value;
}
} // namespace Platform
} // namespace config::kiq

