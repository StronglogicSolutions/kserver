#include "logger.h"

namespace kiq {
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

KLogger::KLogger(const std::string& logging_level)
{
  using loglevel = spdlog::level::level_enum;
  bool timestamp = (config::Logging::timestamp() == "true");

  try
  {
    static const auto        console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    static const loglevel    level        = LogLevel.at(logging_level);
    static const std::string format       = (timestamp) ? "KLOG [%^%l%$] - %T.%e - %4!#:%-20!s%-22!!%v" :
                                                          "KLOG [%^%l%$] - %4!#:%-20!s%-22!!%v";

    console_sink->set_level((logging_level.empty()) ? LogLevel.at(config::Logging::level()) :
                                                      LogLevel.at(logging_level));
    console_sink->set_pattern(format);
    spdlog::      set_pattern(format);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(spdlog::logger("KLOG", console_sink)));
    spdlog::set_level(level);
    spdlog::flush_on(spdlog::level::info);

    g_instance = this;

    KLOG("Logger initialized at {} level", LogLevelStrings.at(console_sink->level()));
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

void KLogger::Init(const std::string& logging_level)
{
  if (g_instance == nullptr) g_instance = new KLogger(logging_level);
}

} // namespace LOG
} // namespace kiq
