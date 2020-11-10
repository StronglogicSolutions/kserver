#ifndef __REGISTRAR_HPP__
#define __REGISTRAR_HPP__

#include <codec/util.hpp>
#include <database/kdb.hpp>
#include <system/process/executor/kapplication.hpp>

namespace constants {
const uint8_t REGISTER_NAME_INDEX = 0x01;
const uint8_t REGISTER_PATH_INDEX = 0x02;
const uint8_t REGISTER_DATA_INDEX = 0x03;
const uint8_t REGISTER_MASK_INDEX = 0x04;
} // namespace constants

KApplication args_to_application(std::vector<std::string> args) {
  KApplication application{};
  if (!args.empty()) {
    application.name = args.at(constants::REGISTER_NAME_INDEX);
    application.path = args.at(constants::REGISTER_PATH_INDEX);
    application.data = args.at(constants::REGISTER_DATA_INDEX);
    application.mask = args.at(constants::REGISTER_MASK_INDEX);
  }
  return application;
}

class Registrar {
public:

Registrar()
: m_kdb(Database::KDB{}) {}

std::string add(KApplication application) {
  if (application.name.empty()) {
    // TODO: error or warning ?
    return "";
  }

  if (find(application)) {
    // TODO: error or warning ?
    return "";
  }

  auto id = m_kdb.insert(
    "apps",
    {
      "name",
      "data",
      "path",
      "mask"
    },
    {
      "application.name",
      "application.data",
      "application.path",
      "application.mask"
    },
    "id"
  );

  return id;
}

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
