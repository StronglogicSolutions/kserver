#ifndef __KAPPLICATION_HPP__
#define __KAPPLICATION_HPP__

#include <iostream>
#include <string>

struct KApplication {
  std::string name;
  std::string path;
  std::string data;
  std::string mask;

  friend std::ostream &operator<<(std::ostream &out, const KApplication &app) {
    out << "Name: " << app.name << "\nPath: " << app.path
        << "\nData: " << app.data << std::endl;
    return out;
  }
};

#endif  // _KAPPLICATION_HPP__

