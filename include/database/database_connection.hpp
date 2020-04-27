#ifndef __DATABASECONNECTION_HPP__
#define __DATABASECONNECTION_HPP__

#include <pqxx/pqxx>
#include <database/db_structs.hpp>

class DatabaseConnection {
 public:
  // constructor
  DatabaseConnection();
  // init db
  bool setConfig(DatabaseConfiguration config);
  // work
  QueryResult query(DatabaseQuery query);
  std::string query(InsertReturnQuery query);
  std::string query(UpdateReturnQuery query);
  QueryResult query(ComparisonSelectQuery query);
  QueryResult query(ComparisonBetweenSelectQuery query);
  QueryResult query(MultiFilterSelect query);
  QueryResult query(MultiVariantFilterSelect query);
  // state
  std::string getDbName();

 private:
  DatabaseConfiguration m_config;
  std::string m_db_name;
  pqxx::connection getConnection();
  std::string getConnectionString();
  pqxx::result performInsert(DatabaseQuery query);
  pqxx::result performInsert(InsertReturnQuery query, std::string returning);
  template <typename T>
  pqxx::result performSelect(T query);
  pqxx::result performUpdate(UpdateReturnQuery query, std::string returning);
};

#endif  // __DATABASECONNECTION_HPP__
