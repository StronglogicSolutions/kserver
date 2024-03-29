#pragma once

#include <iostream>
#include <string>

namespace kiq {
struct KApplication {
  std::string id;
  std::string name;
  std::string path;
  std::string data;
  std::string mask;
  bool        is_kiq;

  bool is_valid() const
  {
    return (!name.empty());
  }

  friend std::ostream &operator<<(std::ostream &out, const KApplication &app) {
    out << "ID: " << app.id << "Name: " << app.name << "\nPath: " << app.path
        << "\nData: " << app.data <<  "\nIs KIQ App: " << static_cast<const char*>((app.is_kiq) ? "Yes" : "No") << std::endl;
    return out;
  }

  std::vector<std::string> vector()
  {
    return std::vector<std::string>{name, path, data, mask};
  }
};
} // ns kiq