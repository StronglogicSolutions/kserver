#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
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
      g_logger = spdlog::basic_logger_mt("KLOG", "/tmp/kserver/k.log");
      spdlog::set_level(spdlog::level::info);
      spdlog::flush_every(std::chrono::seconds(5));
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

  log_ptr static get_logger() { return g_logger; }
};
}
#endif  // __LOGGER_H__

