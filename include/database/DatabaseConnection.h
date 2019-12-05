#include "structs.h"
#include <pqxx/pqxx>

#ifndef DATABASECONNECTION_H
#define DATABASECONNECTION_H

class DatabaseConnection {
public:
  // constructor
  DatabaseConnection();
  // init db
  bool setConfig(DatabaseConfiguration config);
  // work
  QueryResult query(DatabaseQuery query);
  // state
  std::string getDbName();

private:
  DatabaseConfiguration m_config;
  std::string m_db_name;
  pqxx::connection getConnection();
  std::string getConnectionString();
  pqxx::result performInsert(DatabaseQuery query);
  pqxx::result performSelect(DatabaseQuery query);
//  bool connected;
};

#endif // DATABASECONNECTION_H
