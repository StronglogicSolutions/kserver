#include "logger.h"

namespace LOG {

LogPtr g_logger;

KLogger* g_instance;

const LogLevelMap LogLevel{
  {"trace",    spdlog::level::trace},
  {"debug",    spdlog::level::debug},
  {"info",     spdlog::level::info},
  {"warn",     spdlog::level::warn},
  {"error",    spdlog::level::err},
  {"critical", spdlog::level::critical},
  {"off",      spdlog::level::off}
};

KLogger::KLogger(std::string logging_level) {
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
    auto console_sink =
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(log_level);
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

KLogger::~KLogger() { g_instance = NULL; }

void KLogger::init(std::string logging_level) {
  if (g_instance == nullptr) {
    g_instance = new KLogger(logging_level);
  }
}

LogPtr KLogger::get_logger() { return g_logger; }

}  // namespace LOG
