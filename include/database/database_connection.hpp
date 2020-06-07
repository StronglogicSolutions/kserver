#ifndef __DATABASECONNECTION_HPP__
#define __DATABASECONNECTION_HPP__

#include <database/db_structs.hpp>
#include <database/database_interface.hpp>
#include <pqxx/pqxx>

class DatabaseConnection : public DatabaseInterface {
 public:
  // constructor
  virtual bool setConfig(DatabaseConfiguration config) override;
  virtual QueryResult query(DatabaseQuery query) override;

  template <typename T>
  QueryResult query(T query);
  std::string query(InsertReturnQuery query);
  std::string query(UpdateReturnQuery query);
  std::string databaseName();

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
