#ifndef __LOGGER_H__
#define __LOGGER_H__

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#define SPDLOG_DEBUG_ON
#define SPDLOG_TRACE_ON

#define KLOG SPDLOG_LOGGER_TRACE
// #define KLOG->trace SPDLOG_TRACE

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


#include <chrono>
#include <iostream>
#include <memory>

namespace {
typedef std::shared_ptr<spdlog::logger> log_ptr;
class KLogger;

log_ptr g_logger;

KLogger* g_instance;

class KLogger {
 public:
  KLogger() {
    try {
      // g_logger = spdlog::basic_logger_mt("KLOG", "/tmp/kserver/k.log");
      // spdlog::set_level(spdlog::level::info);
      // spdlog::flush_every(std::chrono::seconds(5));
      // g_logger->info("Initializing logger");
      spdlog::set_pattern("[source %s] [function %!] [line %#] %v");
      auto console = spdlog::basic_logger_mt("KLOG", "/tmp/kserver/k.log");

      spdlog::set_default_logger(console);

      SPDLOG_TRACE("global output with arg {}", 1); // [source main.cpp] [function main] [line 16] global output with arg 1
      SPDLOG_LOGGER_TRACE(console, "logger output with arg {}", 2); // [source main.cpp] [function main] [line 17] logger output with arg 2
      console->info("invoke member function"); // [source ] [function ] [line ] invoke member function
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

  log_ptr static get_logger() { return g_logger; }
};
}  // namespace
#endif  // __LOGGER_H__

