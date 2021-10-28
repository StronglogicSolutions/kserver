#include "config_parser.hpp"

namespace ConfigParser {

INIReader* reader_ptr = nullptr;
INIReader reader{};

std::string requiredConfig(std::string missing_config) {
  return "CONFIG REQUIRED: " + missing_config;
}

/**
 * init  .
 */
bool init() {
  if (reader_ptr == nullptr) {
    reader = INIReader{"config/config.ini"};
    if (reader.ParseError() != 0) {
      reader = INIReader{"config/default.config.ini"};
    }
    reader_ptr = &reader;
  }
  return reader.ParseError() == 0;
}

bool is_initialized() {
  return reader_ptr != nullptr;
}

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
const std::string path()      { return reader.Get("logging", "path",       requiredConfig("[logging] path")); }
const std::string timestamp() { return reader.Get("logging", "timestamp",  "true"); }
} // namespace Logging

namespace Database {
const std::string pass()      { return reader.Get("database", "password",  requiredConfig("[database] password")); }
const std::string name()      { return reader.Get("database", "name",      requiredConfig("[database] name")); }
const std::string user()      { return reader.Get("database", "user",      requiredConfig("[database] user")); }
const std::string port()      { return reader.Get("database", "port",      requiredConfig("[database] port")); }
const std::string host()      { return reader.Get("database", "host",      requiredConfig("[database] host")); }
} // namespace Database

namespace Process {
const std::string executor()  { return reader.Get("process", "executor",   requiredConfig("[process] executor")); }
} // namespace Process

namespace Email {
std::string notification()    { return reader.Get("email", "notification", requiredConfig("[email] notification")); }
std::string admin()           { return reader.Get("email", "admin",        requiredConfig("[email] admin")); }
} // namespace Admin

namespace Platform {
std::string affiliate_content(const std::string& type)
{
  std::string section{"affiliate"};
  section  += '_';
  section  += type;
  std::string value = reader.Get("platform", section, requiredConfig("[platform] " + section));
  return value;
}
} // namespace Platform
} // namespace ConfigParser
