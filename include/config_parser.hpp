#include <string>

#include "INIReader.h"

namespace ConfigParser {

INIReader reader("config/config.ini");

bool initConfig() {
  if (reader.ParseError() < 0) {
    return false;
  }

  std::string name = reader.Get("project", "author", "UNKNOWN");
  if (name != "UNKNOWN") {
    return true;
  }
  return false;
}

std::string getDBPass() {
  return reader.Get("database", "password", "PASS_ERROR");
}

std::string getDBName() { return reader.Get("database", "name", "NAME_ERROR"); }

std::string getDBUser() { return reader.Get("database", "user", "USER_ERROR"); }
}  // namespace ConfigParser
