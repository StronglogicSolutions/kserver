#pragma once

#include <iostream>
#include <string>

struct KApplication {
  std::string id;
  std::string name;
  std::string path;
  std::string data;
  std::string mask;
  bool        is_kiq;

  bool is_valid()
  {
    return (!name.empty());
  }

  friend std::ostream &operator<<(std::ostream &out, const KApplication &app) {
    out << "ID: " << app.id << "Name: " << app.name << "\nPath: " << app.path
        << "\nData: " << app.data <<  "\nIs KIQ App: " << static_cast<const char*>((app.is_kiq) ? "Yes" : "No") << std::endl;
    return out;
  }
};
