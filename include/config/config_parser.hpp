#ifndef __CONFIG_PARSER_HPP__
#define __CONFIG_PARSER_HPP__

#include <string>
#include "INIReader.h"

namespace ConfigParser {

INIReader* reader_ptr = nullptr;
INIReader reader;

inline static std::string requiredConfig(std::string missing_config = "") {
  return "CONFIG REQUIRED: " + missing_config;
}

/**
 * init
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

namespace Logging {
  std::string level() {
    return reader.Get("logging", "level", "info");
  }
  std::string path() {
    return reader.Get("logging", "path", requiredConfig("[logging] path"));
  }
} // namespace Logging

namespace Database {
std::string pass() {
  return reader.Get("database", "password", requiredConfig("[database] password"));;
}

std::string name() { return reader.Get("database", "name", requiredConfig("[database] name")); }

std::string user() { return reader.Get("database", "user", requiredConfig("[database] user")); }
} // namespace Database

namespace Process {
std::string executor() {
  return reader.Get("process", "executor", requiredConfig("[process] executor"));
}
} // namespace Process

namespace Admin {
std::string email() {
  return reader.Get("admin", "email", requiredConfig("[admin] email"));
}
} // namespace Admin
}  // namespace ConfigParser
#endif  // __CONFIG_PARSER_HPP__
