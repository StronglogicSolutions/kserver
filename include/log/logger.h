#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "common/INIReader.h"

#include <chrono>
#include <string>
#include <iostream>
#include <map>

#include "spdlog/sinks/stdout_color_sinks.h"

#define KLOG SPDLOG_INFO
#define ELOG SPDLOG_ERROR
#define VLOG SPDLOG_TRACE


namespace ConfigParser {

static INIReader* reader_ptr = nullptr;
static INIReader reader{};

static std::string requiredConfig(std::string missing_config) {
  return "CONFIG REQUIRED: " + missing_config;
}

/**
 * init  .
 */
static bool init() {
  if (reader_ptr == nullptr) {
    reader = INIReader{"config/config.ini"};
    if (reader.ParseError() != 0) {
      reader = INIReader{"config/default.config.ini"};
    }
    reader_ptr = &reader;
  }
  return reader.ParseError() == 0;
}

static bool is_initialized() {
  return reader_ptr != nullptr;
}

namespace Logging {
  static std::string level() {
    return reader.Get("logging", "level", "info");
  }
  static std::string path() {
    return reader.Get("logging", "path", requiredConfig("[logging] path"));
  }
} // namespace Logging

namespace Database {
static std::string pass() {
  return reader.Get("database", "password", requiredConfig("[database] password"));;
}

static std::string name() { return reader.Get("database", "name", requiredConfig("[database] name")); }

static std::string user() { return reader.Get("database", "user", requiredConfig("[database] user")); }

static std::string port() { return reader.Get("database", "port", requiredConfig("[database] port")); }

static std::string host() { return reader.Get("database", "host", requiredConfig("[database] host")); }
} // namespace Database

namespace Process {
static std::string executor() {
  return reader.Get("process", "executor", requiredConfig("[process] executor"));
}
} // namespace Process

namespace Email {
static std::string notification() {
  return reader.Get("email", "notification", requiredConfig("[email] notification"));
}
static std::string admin() {
  return reader.Get("email", "admin", requiredConfig("[email] admin"));
}
} // namespace Admin
}  // namespace ConfigParser


namespace LOG {
using LogPtr = std::shared_ptr<spdlog::logger>;
using LogLevelMap = std::map<std::string, spdlog::level::level_enum>;

extern const LogLevelMap LogLevel;

class KLogger {
 public:
  KLogger(std::string logging_level);
  ~KLogger();

  static void init(std::string logging_level = "");

  static LogPtr get_logger();
};
}  // namespace LOG
