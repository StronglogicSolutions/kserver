#pragma once

#include <string>
#include "INIReader.h"
#include <iostream>

namespace ConfigParser {

extern INIReader* reader_ptr;
extern INIReader  reader;

std::string requiredConfig(std::string missing_config = "");

/**
 * init  .
 */
bool init();

bool is_initialized();

std::string query(const std::string& section, const std::string& name);

namespace Logging {
  std::string level();
  std::string path();
} // namespace Logging

namespace Database {
std::string pass();
std::string name();
std::string user();
std::string port();
std::string host();
} // namespace Database

namespace Process {
std::string executor();
} // namespace Process

namespace Email {
std::string notification();
std::string admin();
} // namespace Admin

namespace Platform {
std::string affiliate_content(const std::string& type);
} // namespace Platform
} // namespace ConfigParser
