#include <pqxx/pqxx>

#include "structs.h"

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
  std::string query(InsertReturnQuery query);
  QueryResult query(ComparisonSelectQuery query);
  QueryResult query(ComparisonBetweenSelectQuery query);
  QueryResult query(MultiFilterSelect query);
  // state
  std::string getDbName();

 private:
  DatabaseConfiguration m_config;
  std::string m_db_name;
  pqxx::connection getConnection();
  std::string getConnectionString();
  pqxx::result performInsert(DatabaseQuery query);
  pqxx::result performInsert(InsertReturnQuery query, std::string returning);
  pqxx::result performSelect(DatabaseQuery query);
  pqxx::result performSelect(ComparisonSelectQuery query);
  pqxx::result performSelect(ComparisonBetweenSelectQuery query);
  pqxx::result performSelect(MultiFilterSelect query);
  //  bool connected;
};

#endif  // DATABASECONNECTION_H
