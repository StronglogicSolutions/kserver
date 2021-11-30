#pragma once

#include "common/util.hpp"
#include "INIReader.h"

namespace kiq {
struct RuntimeConfig {
bool        timestamp{true};
std::string loglevel{"trace"};
};

static RuntimeConfig ParseRuntimeArguments(int argc, char** argv)
{
  RuntimeConfig config{};

  for (int i = 1; i < argc; i++)
  {
    std::string argument = argv[i];

    if (argument.find("--timestamp") == 0)
      config.timestamp = (StringUtils::ToLower(argument.substr(12)) == "true");
    else
    if (argument.find("--loglevel")  == 0)
      config.loglevel = argument.substr(11);
  }
  return config;
}

namespace config {
std::string query(const std::string& section, const std::string& name);

namespace System {
const std::string admin();
}

namespace Logging {
const std::string level();
const std::string path();
const std::string timestamp();
} // namespace Logging

namespace Database {
const std::string pass();
const std::string name();
const std::string user();
const std::string port();
const std::string host();
} // namespace Database

namespace Process {
const std::string executor();
} // namespace Process

namespace Email {
std::string notification();
std::string admin();
} // namespace Admin

namespace Platform {
std::string affiliate_content(const std::string& type);
} // namespace Platform
} // namespace config
} // namespace kiq