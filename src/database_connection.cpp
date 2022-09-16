#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>

#include "database/database_connection.hpp"
#include "helpers.hpp"

bool DatabaseConnection::set_config(DatabaseConfiguration config)
{
  m_config = config;
  m_db_name = config.credentials.name;
  return true;
}

pqxx::result DatabaseConnection::do_insert(DatabaseQuery query)
{
  pqxx::connection connection(connection_string().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(insert_statement(query));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::do_insert(InsertReturnQuery query, std::string returning)
{
  std::string table = query.table;
  pqxx::connection connection(connection_string().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(insert_statement(query, returning));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::do_update(UpdateReturnQuery query, std::string returning)
{
  std::string table = query.table;
  pqxx::connection connection(connection_string().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(update_statement(query, returning));
  worker.commit();

  return pqxx_result;
}

template <typename T>
pqxx::result DatabaseConnection::do_select(T query)
{
  pqxx::connection connection(connection_string().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(select_statement(query));
  worker.commit();

  return pqxx_result;
}

template <typename T>
pqxx::result DatabaseConnection::do_delete(T query)
{
  pqxx::connection connection(connection_string().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(delete_statement(query));
  worker.commit();

  return pqxx_result;
}

std::string DatabaseConnection::connection_string()
{
  std::string s{};
  s += "dbname = ";
  s += m_config.credentials.name;
  s += " user = ";
  s += m_config.credentials.user;
  s += " password = ";
  s += m_config.credentials.password;
  s += " hostaddr = ";
  s += m_config.address;
  s += " port = ";
  s += m_config.port;
  return s;
}

QueryResult DatabaseConnection::query(DatabaseQuery query)
{
  switch (query.type)
  {
    case QueryType::INSERT:
    {
      try
      {
        pqxx::result pqxx_result = do_insert(query);
        return QueryResult{};
      }
      catch (const pqxx::sql_error &e)
      {
        std::cerr << e.what() << "\n" << e.query() << std::endl;
        throw e;
      }
      catch (const std::exception &e)
      {
        std::cerr << e.what() << std::endl;
        throw e;
      }
    }
    case QueryType::SELECT:
    {
      pqxx::result pqxx_result = do_select(query);
      QueryResult result{.table = query.table};
      result.values.reserve(pqxx_result.size());
      for (const auto &row : pqxx_result)
      {
        int index = 0;
        for (const auto &value : row)
          result.values.push_back(std::make_pair(query.fields[index++], value.c_str()));
      }
      return result;
    }

    case QueryType::DELETE:
    {
      pqxx::result pqxx_result   = do_delete(query);
      QueryResult  result{.table = query.table};
      result.values.reserve(pqxx_result.size());
      for (const auto &row : pqxx_result)
        for (const auto &value : row)
          result.values.push_back(std::make_pair(query.filter.value().front().first, value.c_str()));
      return result;
    }

    case QueryType::UPDATE:
    {
      return QueryResult{};
    }
  }
  return QueryResult{};
}

template <typename T>
QueryResult DatabaseConnection::query(T query)
{
  pqxx::result pqxx_result = do_select(query);
  QueryResult result{.table = query.table};
  result.values.reserve(pqxx_result.size());
  for (const auto &row : pqxx_result)
  {
    int index{};
    for (const auto &value : row)
      result.values.push_back(std::make_pair(query.fields[index++], value.c_str()));
  }
  return result;
}

template QueryResult DatabaseConnection::query(
  MultiVariantFilterSelect<std::vector<std::variant<CompFilter, CompBetweenFilter>>>);

template QueryResult DatabaseConnection::query(
  MultiVariantFilterSelect<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>>);

template QueryResult DatabaseConnection::query(
  MultiVariantFilterSelect<std::vector<std::variant<CompBetweenFilter, QueryFilter>>>);

template QueryResult DatabaseConnection::query(
  MultiVariantFilterSelect<std::vector<std::variant<QueryComparisonFilter, QueryFilter>>>);

template QueryResult DatabaseConnection::query(
  JoinQuery<std::vector<std::variant<CompFilter, CompBetweenFilter>>>);

template QueryResult DatabaseConnection::query(
  JoinQuery<std::vector<QueryFilter>>);

template QueryResult DatabaseConnection::query(
  JoinQuery<QueryFilter>);

template QueryResult DatabaseConnection::query(
  SimpleJoinQuery);

template QueryResult DatabaseConnection::query(
  JoinQuery<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>>);

template QueryResult DatabaseConnection::query<ComparisonSelectQuery>(ComparisonSelectQuery);

std::string DatabaseConnection::query(InsertReturnQuery query)
{
  if (const pqxx::result pqxx_result = do_insert(query, query.returning); !pqxx_result.empty())
  {
    const auto row = pqxx_result.at(0);
    if (!row.empty())
      return row.at(0).as<std::string>();
  }
  return "";
}

std::string DatabaseConnection::query(UpdateReturnQuery query)
{
  if (const pqxx::result pqxx_result = do_update(query, query.returning); !pqxx_result.empty())
  {
    const auto row = pqxx_result.at(0);
    if (!row.empty())
      return row.at(0).as<std::string>();
  }
  return "";
}

std::string DatabaseConnection::name() { return m_db_name; }
