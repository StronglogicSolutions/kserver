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
} // namespace ConfigParser
