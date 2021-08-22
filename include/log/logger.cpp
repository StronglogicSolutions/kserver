#include "logger.h"

namespace LOG {
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

const std::unordered_map<spdlog::level::level_enum, std::string> LogLevelStrings{
{spdlog::level::trace,    "trace"},
{spdlog::level::debug,    "debug"},
{spdlog::level::info,     "info"},
{spdlog::level::warn,     "warn"},
{spdlog::level::err,      "error"},
{spdlog::level::critical, "critical"},
{spdlog::level::off,      "off"}
};

KLogger::KLogger(const std::string& logging_level, bool add_timestamp)
{
  add_timestamp = (ConfigParser::Logging::timestamp() == "true");

  try
  {
    if (!ConfigParser::is_initialized()) {
      ConfigParser::init();
    }

    auto                      console_sink       = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::level::level_enum log_level{};
    const std::string         log_format_pattern = (add_timestamp) ?
                                "KLOG [%^%l%$] - %T.%e - %3!#:%-20!s%-20!!%v" :
                                "KLOG [%^%l%$] - %3!#:%-20!s%-20!!%v";

    console_sink->set_level((logging_level.empty()) ?
                              LogLevel.at(ConfigParser::Logging::level()) :
                              LogLevel.at(logging_level));

    console_sink->set_pattern(log_format_pattern);
    spdlog::      set_pattern(log_format_pattern);

    spdlog::set_default_logger(std::make_shared<spdlog::logger>(spdlog::logger("KLOG", console_sink)));
    spdlog::set_level(log_level);
    spdlog::flush_on(spdlog::level::info);

    g_instance = this;

    KLOG("Initialized logger with level {}", LogLevelStrings.at(console_sink->level()));
  }
  catch (const spdlog::spdlog_ex& ex)
  {
    std::cout << "Exception caught during logger initialization: " << ex.what() << std::endl;
  }
}

KLogger::~KLogger()
{
  delete g_instance;
}

void KLogger::init(std::string logging_level) {
  if (g_instance == nullptr) {
    g_instance = new KLogger(logging_level);
  }
}

}  // namespace LOG
