#include "DatabaseConnection.h"
#include <iostream>
#include <pqxx/pqxx>
#include <sstream>
#include <utility>
#include "structs.h"

std::string fieldsAsString(std::vector<std::string> fields)
{
  std::string field_string{""};
  std::string delim = "";

  for (const auto &field : fields)
  {
    field_string += delim + field;
    delim = ",";
  }
  return std::move(field_string);
}

// https://en.wikipedia.oraaaaaaaag/wiki/Row-_and_column-major_order
// std::string valuesAsString(std::vector<std::string> values, size_t number_of_fields) {
//   std::string value_string{"VALUES"};
//   std::string delim{""};
//   // TODO: fix this to work with multiple fields again
//   /*
//   if (index % number_of_fields) {
//     value_string += "),("
//   }
//    */
//   for (const auto &value : values) {
//       value_string += delim + "(" + value + ")";
//       delim = ",";
//   }

//   value_string.erase(value_string.end() - 2);
//   value_string += ")";
//   return std::move(value_string);
// }

std::string valuesAsString(StringVec values, size_t number_of_fields)
{
  std::string value_string{"VALUES ("};
  std::string delim{};
  int index{1};
  for (const auto &value : values)
  {
    if (index++ % number_of_fields == 0)
    {
      delim = "),(";
    }
    else
    {
      delim = ",";
    }
    value_string += "'" + value + "'" + delim;
  }
  value_string.erase(value_string.end() - 2, value_string.end());

  return std::move(value_string);
}

std::string insertStatement(DatabaseQuery query)
{
  return std::string{"INSERT INTO " + query.table + "(" + fieldsAsString(query.fields) + ") " + valuesAsString(query.values, query.fields.size())};
}

std::string insertStatement(InsertReturnQuery query, std::string returning)
{
  if (returning.empty())
  {
    return std::string{"INSERT INTO " + query.table + "(" + fieldsAsString(query.fields) + ") " + valuesAsString(query.values, query.fields.size())};
  }
  else
  {
    std::string test{"INSERT INTO "};
    std::string fields = fieldsAsString(query.fields);
    std::string values = valuesAsString(query.values, query.values.size());
    test += fields;
    test += ") ";
    test += values;
    test += " RETURNING";
    test += returning;
    std::cout << test << std::endl;
    return std::string{"INSERT INTO " + query.table + "(" + fieldsAsString(query.fields) + ") " + valuesAsString(query.values, query.fields.size()) + " RETURNING " + returning};
  }
}

// To filter properly, you must have the same number of values as fields
std::string selectStatement(DatabaseQuery query)
{
  if (!query.filter.empty())
  {
    if (query.filter.size() > 1 && query.filter.at(0).first == query.filter.at(1).first)
    {
      std::string filter_string{"WHERE " + query.filter.at(0).first + " in ("};
      std::string delim{""};
      for (const auto &filter_pair : query.filter)
      {
        filter_string += delim + filter_pair.second;
        delim = ",";
      }
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + filter_string + ")"};
    }
    size_t index = 0;
    std::string filter_string{"WHERE "};
    std::string delim{""};
    for (const auto &filter_pair : query.filter)
    {
      filter_string += delim + filter_pair.first + "='" + filter_pair.second + "'";
      delim = " AND ";
    }
    return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + filter_string};
  }
  return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table};
}

std::string selectStatement(ComparisonSelectQuery query)
{
  if (!query.filter.empty())
  {
    if (query.filter.size() > 1)
    {
      // We do not curently support multiple comparisons in a single query
      return std::string{"SELECT 1"};
    }
    size_t index = 0;
    std::string filter_string{"WHERE "};
    std::string delim{""};
    for (const auto &filter_tup : query.filter)
    {
      filter_string += delim + std::get<0>(filter_tup) + std::get<1>(filter_tup) + std::get<2>(filter_tup);
      //  + "'";
    }
    return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + filter_string};
  }
  return std::string{"SELECT 1"};
}

std::string selectStatement(ComparisonBetweenSelectQuery query)
{
  if (!query.filter.empty())
  {
    if (query.filter.size() > 1)
    {
      // We do not curently support multiple comparisons in a single query
      return std::string{"SELECT 1"};
    }
    size_t index = 0;
    std::string filter_string{"WHERE "};
    std::string delim{""};
    for (const auto &filter_tup : query.filter)
    {
      filter_string += delim + std::get<0>(filter_tup) + " BETWEEN " + std::get<1>(filter_tup) + " AND " + std::get<2>(filter_tup);
      //  + "'";
    }
    return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + filter_string};
  }
  return std::string{"SELECT 1"};
}

// TODO: Update query
// TODO: Filtering

DatabaseConnection::DatabaseConnection() {}

bool DatabaseConnection::setConfig(DatabaseConfiguration config)
{
  m_config = config;
  m_db_name = config.credentials.name;
  return true;
}

pqxx::result DatabaseConnection::performInsert(DatabaseQuery query)
{
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(insertStatement(query));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performInsert(InsertReturnQuery query, std::string returning)
{
  std::string table = query.table;
  std::cout << table << std::endl;
  std::vector<std::string> fields = query.fields;
  std::vector<std::string> values = query.values;
  for (int i = 0; i < fields.size(); i++)
  {
    std::cout << fields.at(i) << std::endl;
    std::cout << values.at(i) << std::endl;
  }

  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(insertStatement(query, returning));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performSelect(DatabaseQuery query)
{
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(selectStatement(query));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performSelect(ComparisonSelectQuery query)
{
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(selectStatement(query));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performSelect(ComparisonBetweenSelectQuery query)
{
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(selectStatement(query));
  worker.commit();

  return pqxx_result;
}

// std::string DatabaseConnection::getDbName(){return }

std::string DatabaseConnection::getConnectionString()
{
  std::string connectionString{};
  connectionString += "dbname = ";
  connectionString += m_config.credentials.name;
  connectionString += " user = ";
  connectionString += m_config.credentials.user;
  connectionString += " password = ";
  connectionString += m_config.credentials.password;
  connectionString += " hostaddr = ";
  connectionString += m_config.address;
  connectionString += " port = ";
  connectionString += m_config.port;
  return connectionString;
}

QueryResult DatabaseConnection::query(DatabaseQuery query)
{
  switch (query.type)
  {
  case QueryType::INSERT:
  {
    try
    {
      pqxx::result pqxx_result = performInsert(query);
      return QueryResult{};
    }
    catch (const pqxx::sql_error &e)
    {
      std::cout << e.what() << "\n"
                << e.query() << std::endl;
    }
    catch (const std::exception &e)
    {
      std::cout << e.what() << std::endl;
    }
  }
  case QueryType::SELECT:
  {
    pqxx::result pqxx_result = performSelect(query);

    QueryResult result{};
    result.table = query.table;

    auto count = query.fields.size();

    for (auto row : pqxx_result)
    {
      int index = 0;
      for (const auto &field : row)
      {
        std::string field_name = query.fields[index++];
        auto row_chars = field.c_str();
        if (row_chars != nullptr)
        {
          std::string value{row_chars};
          auto pair = std::make_pair(field_name, value);
          result.values.push_back(pair);
        }
      }
    }
    return result;
  }

  case QueryType::DELETE:
  {
    std::stringstream query_stream{};

    query_stream << " " << query.table << " VALUES (\"test\")";
    QueryResult result{};
    return result;
  }
  }
  return QueryResult{};
}

QueryResult DatabaseConnection::query(ComparisonSelectQuery query)
{
  pqxx::result pqxx_result = performSelect(query);

  QueryResult result{};
  result.table = query.table;

  auto count = query.fields.size();

  for (auto row : pqxx_result)
  {
    int index = 0;
    for (const auto &field : row)
    {
      std::string field_name = query.fields[index++];
      auto row_chars = field.c_str();
      if (row_chars != nullptr)
      {
        std::string value{row_chars};
        auto pair = std::make_pair(field_name, value);
        result.values.push_back(pair);
      }
    }
  }
  return result;
}

QueryResult DatabaseConnection::query(ComparisonBetweenSelectQuery query)
{
  pqxx::result pqxx_result = performSelect(query);

  QueryResult result{};
  result.table = query.table;

  auto count = query.fields.size();

  for (auto row : pqxx_result)
  {
    int index = 0;
    for (const auto &field : row)
    {
      std::string field_name = query.fields[index++];
      auto row_chars = field.c_str();
      if (row_chars != nullptr)
      {
        std::string value{row_chars};
        auto pair = std::make_pair(field_name, value);
        result.values.push_back(pair);
      }
    }
  }
  return result;
}

std::string DatabaseConnection::query(InsertReturnQuery query)
{
  std::string returning = query.returning;
  pqxx::result pqxx_result = performInsert(query, returning);

  if (!pqxx_result.empty())
  {
    auto row = pqxx_result.at(0);
    if (!row.empty())
    {
      return row.at(0).as<std::string>();
    }
  }
  return "";
}

pqxx::connection DatabaseConnection::getConnection()
{
  std::string connectionString{("dbname = " + m_config.credentials.name +
                                " user = " + m_config.credentials.user +
                                " password = " + m_config.credentials.password +
                                " hostaddr = " + m_config.address + " port " +
                                m_config.port)};
  return pqxx::connection(connectionString);
}

std::string DatabaseConnection::getDbName() { return m_db_name; }
