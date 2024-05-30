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
const std::string admin()        { return reader.Get("system", "admin", "admin"); }
}
namespace Logging {
const std::string level()        { return reader.Get("logging", "level",      "info"); }
const std::string path()         { return reader.Get("logging", "path",       RequiredConfig("[logging] path")); }
const std::string timestamp()    { return reader.Get("logging", "timestamp",  "true"); }
} // namespace Logging

namespace Database {
const std::string pass()         { return reader.Get("database", "password",  RequiredConfig("[database] password")); }
const std::string name()         { return reader.Get("database", "name",      RequiredConfig("[database] name")); }
const std::string user()         { return reader.Get("database", "user",      RequiredConfig("[database] user")); }
const std::string port()         { return reader.Get("database", "port",      RequiredConfig("[database] port")); }
const std::string host()         { return reader.Get("database", "host",      RequiredConfig("[database] host")); }
} // namespace Database

namespace Process {
const std::string executor()     { return reader.Get("process", "executor",   RequiredConfig("[process] executor"));   }
const std::string ipc_port()     { return reader.Get("process", "ipc_port",   "28473");                                }
const std::string tg_dest ()     { return reader.Get("process", "tg_dest",    "-1");                                   }
const std::string broker_address() { return reader.Get("process", "broker_address", RequiredConfig("[process] broker_address")); }
const std::string sentnl_address() { return reader.Get("process", "sentnl_address", RequiredConfig("[process] sentnl_address")); }
const std::string kai_address() { return reader.Get("process", "kai_address", RequiredConfig("[process] broker_address")); }
} // namespace Process

namespace Email {
const std::string notification() { return reader.Get("email", "notification", RequiredConfig("[email] notification")); }
const std::string admin()        { return reader.Get("email", "admin",        RequiredConfig("[email] admin"));        }
const std::string command()      { return reader.Get("email", "command",      RequiredConfig("[email] command"));      }
} // namespace Admin

namespace Platform {
const std::string affiliate_content(const std::string& type)
{
  std::string key{"affiliate_" + type};
  std::string value = reader.Get("platform", key, RequiredConfig("[platform] " + key));
  return value;
}
const std::string default_user() { return reader.Get("platform", "default_user", "");                                  }
} // namespace Platform

namespace ML {
const std::string keural_path()
{ return reader.Get("machine_learning", "keural_path", RequiredConfig("[machine_learning] input_path"));        }
}

namespace Security {
const std::string token_path()   { return reader.Get("security", "token_path",  ""); }
const std::string private_key()  { return reader.Get("security", "private_key", ""); }
const std::string public_key()   { return reader.Get("security", "public_key",  ""); }
} // ns Security
} // namespace config::kiq
