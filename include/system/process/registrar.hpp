#ifndef __REGISTRAR_HPP__
#define __REGISTRAR_HPP__

#include <codec/util.hpp>
#include <database/kdb.hpp>
#include <system/process/executor/kapplication.hpp>

class Registrar {
public:

Registrar()
: m_kdb(Database::KDB{}) {}

void add() {}

void remove() {}

void update() {}

bool find(KApplication application) {
  QueryValues result = m_kdb.select(
    "apps",
    Fields{
      "name",
      "mask",
      "path",
      "data"
    },
    QueryFilter{
      { "name", application.name }
    }
  );

  if (result.empty()) {
    return false;
  }

  return true;
}

private:
Database::KDB m_kdb;
};

#endif  // __REGISTRAR_HPP__
