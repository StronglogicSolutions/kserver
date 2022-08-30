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
  using sinks_t  = std::vector<spdlog::sink_ptr>;
  const bool timestamp = (config::Logging::timestamp() == "true");
  const auto path      =  config::Logging::path();
  try
  {
    static const auto        console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    static const auto        file_sink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path);
    static const loglevel    level        = LogLevel.at(logging_level);
    static const std::string format       = (timestamp) ? "KIQ [%^%-5l%$] - %T.%e - %-20!s :%-4!# - %-22!!%v" :
                                                          "KIQ [%^%-5l%$] - %-20!s :%-4!# - %-22!!%v";

    const auto final_loglevel = (logging_level.empty()) ? LogLevel.at(config::Logging::level()) : LogLevel.at(logging_level);

    console_sink->set_level(final_loglevel);
    console_sink->set_pattern(format);
    file_sink   ->set_level(final_loglevel);
    file_sink   ->set_pattern(format);
    spdlog::      set_pattern(format);
    sinks_t sinks{console_sink, file_sink};
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(spdlog::logger("KLOG", sinks.begin(), sinks.end())));
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
