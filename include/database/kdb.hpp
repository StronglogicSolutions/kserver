#ifndef __KDB_HPP__
#define __KDB_HPP__

#include <variant>
#include <database/database_connection.hpp>
#include <log/logger.h>

namespace Database {
class KDB {
 public:
  KDB() {
    m_connection->setConfig(DatabaseConfiguration{DatabaseCredentials{.user = ConfigParser::Database::user(),
                     .password = ConfigParser::Database::pass(),
                     .name = ConfigParser::Database::name()}});
  }

  KDB(DatabaseConfiguration config) {
    m_connection = new DatabaseConnection{};
    m_connection->setConfig(config);
  }

  KDB(DatabaseInterface* db_interface, DatabaseConfiguration config) : m_connection(db_interface) {
    m_connection->setConfig(config);
  }

QueryValues select(std::string table, Fields fields,
                     QueryFilter filter = {}) {
    try {
      QueryResult result = m_connection->query(
        DatabaseQuery{
          .table = table,
          .fields = fields,
          .type = QueryType::SELECT,
          .values = {},
          .filter = filter
        });
      if (!result.values.empty()) {
        return result.values;
      }
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return {{}};
  }

  QueryValues select(std::string table, Fields fields,
                     QueryComparisonFilter filter = {}) {
    try {
      QueryResult result = m_connection->query(
        ComparisonSelectQuery{
          .table = table,
          .fields = fields,
          .values = {},
          .filter = filter
        });
      if (!result.values.empty()) {
        return result.values;
      }
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return {{}};
  }

  QueryValues selectCompare(std::string table, Fields fields,
                            std::vector<CompFilter> filter = {}) {
    try {
      ComparisonBetweenSelectQuery select_query{
          .table = table, .fields = fields, .values = {}, .filter = filter};
      QueryResult result = m_connection->query(select_query);
      if (!result.values.empty()) {
        return result.values;
      }
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return {{}};
  }

  QueryValues selectMultiFilter(std::string table, Fields fields,
                                std::vector<GenericFilter> filters) {
    try {
      MultiFilterSelect select_query{
          .table = table, .fields = fields, .filter = filters};
      QueryResult result = m_connection->query(select_query);
      if (!result.values.empty()) {
        return result.values;
      }
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return {{}};
  }

  template <typename FilterA, typename FilterB>
  QueryValues selectMultiFilter(
      std::string table, Fields fields,
      std::vector<std::variant<FilterA, FilterB> > filters) {
    try {
      MultiVariantFilterSelect select_query{
          .table = table, .fields = fields, .filter = filters};
      QueryResult result = m_connection->query(select_query);
      for (const auto &value : result.values) {
        std::cout << "Query value: " << value.second << std::endl;
      }
      if (!result.values.empty()) {
        return result.values;
      }
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return {{}};
  }

  std::string update(std::string table, Fields fields, Values values,
                     QueryFilter filter, std::string returning) {
    try {
      // TODO: At the moment, we are married to passing the "id" field name as
      // the "returning" argument
      UpdateReturnQuery update_query{.table = table,
                                     .fields = fields,
                                     .type = QueryType::UPDATE,
                                     .values = values,
                                     .filter = filter,
                                     .returning = returning};
      std::string result = m_connection->query(update_query);
      return result;
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return "";
  }

  bool insert(std::string table, Fields fields, Values values) {
    DatabaseQuery insert_query{.table = table,
                               .fields = fields,
                               .type = QueryType::INSERT,
                               .values = values};
    try {
      QueryResult result = m_connection->query(insert_query);
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
    return true;
  }

  std::string insert(std::string table, Fields fields, Values values,
                     std::string returning) {
    InsertReturnQuery insert_query{.table = table,
                                   .fields = fields,
                                   .type = QueryType::INSERT,
                                   .values = values,
                                   .returning = returning};
    try {
      return m_connection->query(insert_query);
    } catch (const pqxx::sql_error &e) {
      throw e;
    } catch (const std::exception &e) {
      throw e;
    }
  }

 private:
  DatabaseInterface* m_connection;
  DatabaseCredentials m_credentials;
};

}  // namespace Database

#endif  // __KDB_HPP__
