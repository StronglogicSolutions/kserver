#ifndef __KDB_HPP__
#define __KDB_HPP__

#include <database/DatabaseConnection.h>
#include <log/logger.h>

namespace Database {

auto KLOG = KLogger::GetInstance() -> get_logger();

class KDB {
 public:
  KDB() {
    if (!ConfigParser::initConfig()) {
      KLOG->info("Unable to load config");
      return;
    }

    m_credentials = {.user = ConfigParser::getDBUser(),
                     .password = ConfigParser::getDBPass(),
                     .name = ConfigParser::getDBName()};

    DatabaseConfiguration configuration{
        .credentials = m_credentials, .address = "127.0.0.1", .port = "5432"};

    m_connection = DatabaseConnection{};
    m_connection.setConfig(configuration);
  }

  QueryValues select(std::string table, Fields fields,
                     QueryFilter filter = {}) {
    DatabaseQuery select_query{.table = table,
                               .fields = fields,
                               .type = QueryType::SELECT,
                               .filter = filter};
    QueryResult result = m_connection.query(select_query);
    if (!result.values.empty()) {
      return result.values;
    }
    return {{}};
  }

  bool insert(std::string table, Fields fields, Values values) {
    DatabaseQuery insert_query{.table= table,
                               .fields = fields,
                               .type = QueryType::INSERT,
                               .values = values};

    QueryResult result = m_connection.query(insert_query);
    // TODO: add try/catch and handle accordingly
    return true;
  }

 private:
  DatabaseConnection m_connection;
  DatabaseCredentials m_credentials;
};

}  // namespace Database

#endif  // __KDB_HPP__
