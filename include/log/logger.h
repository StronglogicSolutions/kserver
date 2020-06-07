#ifndef __LOGGER_H__
#define __LOGGER_H__
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

namespace LOG {
using LogPtr = std::shared_ptr<spdlog::logger>;
using LogLevelMap = std::map<std::string, spdlog::level::level_enum>;

class KLogger;

LogPtr g_logger;

KLogger* g_instance;

LogLevelMap LogLevel{
    {"trace", spdlog::level::trace}, {"debug", spdlog::level::debug},
    {"info", spdlog::level::info},   {"warn", spdlog::level::warn},
    {"error", spdlog::level::err},   {"critical", spdlog::level::critical},
    {"off", spdlog::level::off}};

class KLogger {
 public:
  KLogger(std::string logging_level = "", std::string logging_path = "") {
    try {
      // TODO: Not an appropriate responsibility
      if (!ConfigParser::is_initialized()) {
        ConfigParser::init();
      }
      spdlog::level::level_enum log_level{};
      if (logging_level.empty()) {
        std::string level = ConfigParser::Logging::level();
        std::cout << "KLogger initializing at level: " << level << std::endl;
        log_level = LogLevel.at(level);
      } else {
        log_level = LogLevel.at(logging_level);
      }

      std::string log_path{};
      if (logging_path.empty()) {
        log_path = ConfigParser::Logging::path();
      } else {
        log_path = logging_path;
      }
      auto console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(log_level);
      /* std::string log_format_pattern{ */
      /*     "KLOG [%^%l%$] - %a %b %d %H:%M:%S - %-20!s%3!#::%-20!! - %v"}; */
      std::string log_format_pattern{"KLOG [%^%l%$] - %3!#:%-20!s%-20!!%v"};
      console_sink->set_pattern(log_format_pattern);
      spdlog::set_pattern(log_format_pattern);
      g_logger = std::make_shared<spdlog::logger>(
          spdlog::logger("KLOG", console_sink));
      spdlog::set_default_logger(g_logger);
      spdlog::set_level(log_level);
      spdlog::flush_on(spdlog::level::info);
      KLOG("Initializing logger");
    } catch (const spdlog::spdlog_ex& ex) {
      std::cout << "Error: " << ex.what() << std::endl;
    }
    g_instance = this;
  }
  ~KLogger() { g_instance = NULL; }

  static void init() {
    if (g_instance == nullptr) {
      g_instance = new KLogger();
    }
  }

  static LogPtr get_logger() { return g_logger; }
};
}  // namespace LOG
#endif  // __LOGGER_H__

