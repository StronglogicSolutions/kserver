#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <config/config_parser.hpp>
#include <iostream>
#include <map>
#include <memory>

#include "spdlog/sinks/stdout_color_sinks.h"

#define KLOG SPDLOG_INFO
#define ELOG SPDLOG_ERROR
#define VLOG SPDLOG_TRACE

namespace kiq::LOG {
using LogPtr = std::shared_ptr<spdlog::logger>;
using LogLevelMap = std::map<std::string, spdlog::level::level_enum>;

extern const LogLevelMap LogLevel;

class KLogger {
 public:
  KLogger(const std::string& logging_level = config::Logging::level());
  ~KLogger();

  static void Init(const std::string& logging_level = "");
};
}  // namespace kiq::LOG
