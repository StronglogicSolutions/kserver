#pragma once

#include <common/util.hpp>
#include <database/kdb.hpp>
#include <system/process/executor/kapplication.hpp>

namespace Registrar {
namespace constants {
const uint8_t REGISTER_NAME_INDEX = 0x01;
const uint8_t REGISTER_PATH_INDEX = 0x02;
const uint8_t REGISTER_DATA_INDEX = 0x03;
const uint8_t REGISTER_MASK_INDEX = 0x04;
} // namespace constants

static KApplication args_to_application(std::vector<std::string> args) {
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
  std::string id{};

  if (application.name.empty()) {
    // TODO: error or warning ?
    return "";
  }

  try {
    if (find(application)) {
    // TODO: error or warning ?
    return "";
  }

  id = m_kdb.insert(
    "apps",
    {
      "name",
      "data",
      "path",
      "mask"
    },
    {
      application.name,
      application.data,
      application.path,
      application.mask
    },
    "id"
  );

  } catch (const std::exception& e) {
    std::cout << "Caught exception: " << e.what() << std::endl;
  }

 return id;
}

std::string remove(KApplication application) {
  return m_kdb.remove("apps", CreateFilter("name", application.name, "path", application.path));
}

void update() {}

bool find(KApplication application) {
  QueryValues result = m_kdb.select("apps", {"name", "mask", "path", "data"}, CreateFilter("name", application.name));
  return (result.size() > 2);
}

private:
Database::KDB m_kdb;
};

} // namespace Registrar
