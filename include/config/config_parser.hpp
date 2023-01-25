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
const std::string ipc_port();
const std::string tg_dest ();
const std::string broker_address();
} // namespace Process

namespace Email {
const std::string notification();
const std::string admin();
const std::string command();
} // namespace Admin

namespace Platform {
const std::string affiliate_content(const std::string& type);
const std::string default_user();
} // namespace Platform

namespace ML {
const std::string keural_path();
} // ns ML

namespace Security {
const std::string token_path();
const std::string private_key();
const std::string public_key();
} // ns Security
} // namespace config
} // namespace kiq1
