#ifndef __LOGGER_H__
#define __LOGGER_H__
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks.h"


#include <chrono>
#include <iostream>
#include <memory>

namespace {
typedef std::shared_ptr<spdlog::logger> LogPtr;

class KLogger;

LogPtr g_logger;

KLogger* g_instance;

class KLogger {
 public:
  KLogger() {
    try {
      auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(spdlog::level::trace);
      console_sink->set_pattern("KLOG [%^%l%$] %v");
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/kserver/k.log");
      file_sink->set_level(spdlog::level::trace);
      spdlog::sinks_init_list sink_list = { file_sink, console_sink };
      g_logger =  std::make_shared<spdlog::logger>(spdlog::logger("KLOG", sink_list.begin(), sink_list.end()));
      spdlog::set_default_logger(g_logger);
      spdlog::set_level(spdlog::level::trace);
      spdlog::flush_on(spdlog::level::info);
      g_logger->info("Initializing logger");
    } catch (const spdlog::spdlog_ex& ex) {
      std::cout << "Error: " << ex.what() << std::endl;
    }
    g_instance = this;
  }
  ~KLogger() { g_instance = NULL; }

  static KLogger* GetInstance() {
    if (g_instance == nullptr) {
      g_instance = new KLogger();
    }
    return g_instance;
  }

  static LogPtr get_logger() { return g_logger; }
};
}  // namespace
#endif  // __LOGGER_H__

