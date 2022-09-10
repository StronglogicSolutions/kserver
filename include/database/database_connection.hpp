#pragma once

#include "db_structs.hpp"
#include "database_interface.hpp"
#include <pqxx/pqxx>

class DatabaseConnection : public DatabaseInterface {
public:
// constructor
DatabaseConnection() {}
DatabaseConnection(DatabaseConnection&& d)
: m_config(std::move(d.m_config)) {}
DatabaseConnection(const DatabaseConnection& d) = delete;
virtual ~DatabaseConnection() override {}

template <typename T>
QueryResult         query(T query);
std::string         query(InsertReturnQuery query);
std::string         query(UpdateReturnQuery query);
virtual QueryResult query(DatabaseQuery query) override;
std::string         name();
virtual bool        set_config(DatabaseConfiguration config) override;

private:
std::string         connection_string();
pqxx::result        do_insert(DatabaseQuery query);
pqxx::result        do_insert(InsertReturnQuery query, std::string returning);
template <typename T>
pqxx::result do_select(T query);
template <typename T>
pqxx::result do_delete(T query);
pqxx::result do_update(UpdateReturnQuery query, std::string returning);

DatabaseConfiguration   m_config;
std::string             m_db_name;

};
