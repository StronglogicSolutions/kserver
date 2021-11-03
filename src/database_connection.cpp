#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>

#include "database/database_connection.hpp"

std::string fieldsAsString(std::vector<std::string> fields) {
  std::string field_string{""};
  std::string delim = "";

  for (const auto &field : fields) {
    field_string += delim + field;
    delim = ",";
  }
  return field_string;
}

std::string valuesAsString(StringVec values, size_t number_of_fields) {
  std::string value_string{"VALUES ("};
  std::string delim{};
  int index{1};
  for (const auto &value : values) {
    if (index++ % number_of_fields == 0) {
      delim = "),(";
    } else {
      delim = ",";
    }
    value_string += "'";
    value_string += (value.empty()) ? "NULL" : value;
    value_string += "'" + delim;
  }
  value_string.erase(value_string.end() - 2, value_string.end());

  return value_string;
}

static std::string orderStatement(const OrderFilter& filter)
{
  return " ORDER BY " + filter.field + ' ' + filter.order;
}

static std::string limitStatement(const std::string& number)
{
  return " LIMIT " + number;
}

template <typename T>
std::string filterStatement(T filter) {
  std::string filter_string{};
  if constexpr (std::is_same_v<T, MultiOptionFilter>) {
    filter_string += filter.a + " " + filter.comparison + " (";
    std::string delim{};
    for (const auto &option : filter.options) {
      filter_string += delim + option;
      delim = ",";
    }
    filter_string += ")";
  }
  else
  if constexpr (std::is_same_v<T, CompBetweenFilter>)
    filter_string += filter.field + " BETWEEN " + filter.a + " AND " + filter.b;
  else
  if constexpr (std::is_same_v<T, CompFilter>)
    filter_string += filter.a + filter.sign + filter.b;
  else
  if constexpr (std::is_same_v<T, QueryFilter>) // TODO: QueryFilter shouldn't be a vector
  {
    std::string delim{};
    for (const auto& f : filter)
    {
      filter_string += delim + f.first + '=' + '\'' + f.second + '\'';
      delim = " AND ";
    }
  }

  return filter_string;
}

template <typename FilterA, typename FilterB>
std::string getVariantFilterStatement(
    std::vector<std::variant<FilterA, FilterB>> filters) {
  std::string filter_string{};
  uint8_t idx = 0;
  uint8_t filter_count = filters.size();
  for (const auto &filter : filters) {
    if (filter.index() == 0) {
      filter_string += filterStatement(std::get<0>(filter));
    } else {
      filter_string += filterStatement(std::get<1>(filter));
    }
    if (filter_count > (idx + 1)) {
      idx++;
      filter_string += " AND ";
    }
  }
  return filter_string;
}

template <typename FilterA, typename FilterB, typename FilterC>
std::string getVariantFilterStatement(
    std::vector<std::variant<FilterA, FilterB, FilterC>> filters) {
  std::string filter_string{};
  uint8_t idx = 0;
  uint8_t filter_count = filters.size();
  for (const auto &filter : filters) {
    if (filter.index() == 0)
      filter_string += filterStatement(std::get<0>(filter));
    else
    if (filter.index() == 1)
      filter_string += filterStatement(std::get<1>(filter));
    else
      filter_string += filterStatement(std::get<2>(filter));

    if (filter_count > (idx + 1)) {
      idx++;
      filter_string += " AND ";
    }
  }
  return filter_string;
}

// TODO: Phase this out, and only use "filterStatement" above
template <typename T>
std::string getFilterStatement(T filter) {  // TODO: fix template usage
  std::string delim = "";

  if constexpr (std::is_same_v<T, QueryFilter>)
  {
    std::string filter_string{};
    for (const auto& filter_pair : filter)
    {
      filter_string += delim + filter_pair.first + '=' + '\'' + filter_pair.second + '\'';
      delim = " AND ";
    }
    return filter_string;
  }
  else
  if constexpr (std::is_same_v<T, MultiOptionFilter>) {
    std::string filter_string{filter.a + " " + filter.comparison + " ("};
    for (const auto &option : filter.options) {
      filter_string += delim + option;
      delim = ",";
    }
    filter_string += ")";
    return filter_string;
  } else if constexpr (std::is_same_v<T, CompBetweenFilter>) {
    return std::string{filter.field + " BETWEEN " + filter.a +
                                 " AND " + filter.b};
  } else if constexpr (std::is_same_v<T, CompFilter>) {
    return std::string{filter.a + filter.sign + filter.b};
  } else if constexpr (std::is_same_v<T, GenericFilter>) {
    return std::string{filter.a + filter.comparison + filter.b};
  }
  return "";
}

std::string getJoinStatement(Joins joins) {
  std::string join_s{};
  if (!joins.empty()) {
    for (const auto& join : joins) {
      join_s += join.type == JoinType::INNER ?
        "INNER JOIN " :
        "LEFT OUTER JOIN ";
      join_s += join.table + \
       " ON " + join.table + "." + join.field + "=" + join.join_table + "." + join.join_field;
      join_s += " ";
    }
    join_s.pop_back();
  }
  return join_s;
}

std::string insertStatement(DatabaseQuery query) {
  return std::string{"INSERT INTO " + query.table + "(" +
                     fieldsAsString(query.fields) + ") " +
                     valuesAsString(query.values, query.fields.size())};
}

std::string insertStatement(InsertReturnQuery query, std::string returning) { // TODO: always return ID?
  if (returning.empty()) {
    return std::string{"INSERT INTO " + query.table + "(" +
                       fieldsAsString(query.fields) + ") " +
                       valuesAsString(query.values, query.fields.size())};
  } else {
    return std::string{"INSERT INTO " + query.table + "(" +
                       fieldsAsString(query.fields) + ") " +
                       valuesAsString(query.values, query.fields.size()) +
                       " RETURNING " + returning};
  }
}

// To filter properly, you must have the same number of values as fields
std::string updateStatement(UpdateReturnQuery query, std::string returning,
                            bool multiple = false)
{
  const auto filter = query.filter.value();
  if (!filter.empty())
  { // TODO: Handle case for updating multiple rows at once
    if (!multiple)
    {
      std::string filter_string{"WHERE "};
      std::string update_string{"SET "};
      std::string delim = "";
      filter_string += getFilterStatement(filter);
      if (query.values.size() == query.fields.size()) // can only update if the `fields` and
      {                                               // `values` arguments are matching
        for (uint8_t i = 0; i < query.values.size(); i++)
        {
          const auto field = query.fields.at(i);
          const auto value = query.values.at(i);
          update_string += delim + field + "=" + "'" + value + "'";
          delim = ',';
        }
      }
      return std::string{"UPDATE " + query.table + " " + update_string + " " +
                         filter_string + " RETURNING " + returning};
    }
  }
  return "";
}

/**
 * deleteStatement
 *
 * TODO: Implement logic to filter on multiple values
 *
 * @tparam  [in]     T        filter type
 * @param   [in] {T} query    filter object
 * @returns [out] std::string SQL DELETE statement
 */
template <typename T>
std::string deleteStatement(T query)
{
  const auto filter = query.filter.value();
  std::string filter_string{"WHERE "};
  if constexpr (std::is_same_v<T, DatabaseQuery>)
  {
    if (filter.empty())
      return "";

    filter_string += filter.front().first + "='" + filter.front().second + "'";
    return "DELETE FROM " + query.table + " " + filter_string +
          " RETURNING "   + filter.front().first;
  }
}

/**
 * \note Most of this can be removed and replaced with newer implementation as
 * is demonstrated in handling of vector of "variant" filters
 * TODO: Finish refactor and simplify this mess
 */
template <typename T>
std::string selectStatement(T query)
{
  const auto  filter = query.filter;
  std::string delim{""};
  std::string filter_string{" WHERE "};

  if (!filter.empty())
  {
    if constexpr (std::is_same_v<T, Query>)
    {
      if (filter.size() > 1 &&
          filter.front().first == filter.at(1).first)
      {
        filter_string += filter.front().first + " in (";
        for (const auto &filter_pair : filter)
        {
          filter_string += delim + filter_pair.second;
          delim = ",";
        }
        return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string + ")";
      }

      for (const auto &filter_pair : filter)
        filter_string += delim + filter_pair.first + "='" + filter_pair.second + "'"; delim = " AND ";

      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string;
    }
    else
    if constexpr (std::is_same_v<T, DatabaseQuery>)
    {
      if (filter.size() > 1 &&
          filter.front().first == filter.at(1).first)
      {
        filter_string += filter.front().first + " in (";
        for (const auto &filter_pair : filter)
        {
          filter_string += delim + filter_pair.second;
          delim = ",";
        }
        return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string + ")";
      }

      for (const auto &filter_pair : filter)
      {
        filter_string += delim + filter_pair.first + "='" + filter_pair.second + "'";
        delim = " AND ";
      }
      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string;
    }
    else
    if constexpr (std::is_same_v<T, ComparisonSelectQuery>)
    {
      if (filter.size() > 1)
        return "SELECT 1"; // Unsupported

      for (const auto &filter_tup : filter)
      {
        filter_string += delim + std::get<0>(filter_tup) +
                         std::get<1>(filter_tup) + std::get<2>(filter_tup);
        delim = " AND ";
      }
      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string;
    }
    else
    if constexpr (std::is_same_v<T, ComparisonBetweenSelectQuery>)
    {
      if (filter.size() > 1)
        return "SELECT 1"; // Unsupported

      for (const auto &filter : filter)
        filter_string += delim + getFilterStatement(filter);

      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string;
    }
    else
    if constexpr (std::is_same_v<T, MultiFilterSelect>)
    {
      for (const auto &filter : filter)
      {
        filter_string += delim + getFilterStatement(filter);  // *** HERE for variant impl
        delim = " AND ";
      }
      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string;
    }
    else
    if constexpr (std::is_same_v<T, MultiVariantFilterSelect<std::vector<std::variant<CompFilter, CompBetweenFilter>>>>)
    {
      filter_string += getVariantFilterStatement<CompFilter, CompBetweenFilter>(filter);
      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string;
    }
    else
    if constexpr (
      std::is_same_v<T, MultiVariantFilterSelect<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>>>)
    {
      filter_string += getVariantFilterStatement<CompFilter, CompBetweenFilter, MultiOptionFilter>(filter);
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + filter_string};
    }
    else
    if constexpr (
      std::is_same_v<T, MultiVariantFilterSelect<std::vector<std::variant<CompBetweenFilter, QueryFilter>>>>)
    {
      std::string stmt{"SELECT " + fieldsAsString(query.fields) +
                       " FROM " + query.table + filter_string +
                       getVariantFilterStatement<CompBetweenFilter, QueryFilter>(filter)};
      if (query.order.has_value())
        stmt += orderStatement(query.order);
      if (query.limit.has_value())
        stmt += limitStatement(query.limit.count);
      return stmt;
    }
    else
    if constexpr (std::is_same_v<T, JoinQuery<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>>>)
    {
      filter_string += getVariantFilterStatement(filter);
      std::string join_string = getJoinStatement(query.joins);
      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + join_string + filter_string;
    }
    else
    if constexpr (std::is_same_v<T, SimpleJoinQuery>)
    {
      filter_string += getFilterStatement(filter);
      std::string join_string = getJoinStatement({query.join});
      return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + join_string + filter_string;
    }
  }
  return "SELECT " + fieldsAsString(query.fields) + " FROM " + query.table;
}

bool DatabaseConnection::setConfig(DatabaseConfiguration config) {
  m_config = config;
  m_db_name = config.credentials.name;
  return true;
}

pqxx::result DatabaseConnection::performInsert(DatabaseQuery query) {
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(insertStatement(query));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performInsert(InsertReturnQuery query,
                                               std::string returning) {
  std::string table = query.table;
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  // #ifndef NDEBUG
  //   std::cout << "Insert query:\n" << insertStatement(query, returning) << std::endl;
  // #endif
  pqxx::result pqxx_result = worker.exec(insertStatement(query, returning));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performUpdate(UpdateReturnQuery query,
                                               std::string returning) {
  std::string table = query.table;
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  // #ifndef NDEBUG
  //   std::cout << "Update query:\n" << updateStatement(query, returning) << std::endl;
  // #endif
  pqxx::result pqxx_result = worker.exec(updateStatement(query, returning));
  worker.commit();

  return pqxx_result;
}

template <typename T>
pqxx::result DatabaseConnection::performSelect(T query) {
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  // #ifndef NDEBUG
  //   std::cout << "Select query: \n" << selectStatement(query) << std::endl;
  // #endif
  pqxx::result pqxx_result = worker.exec(selectStatement(query));
  worker.commit();

  return pqxx_result;
}

template <typename T>
pqxx::result DatabaseConnection::performDelete(T query) {
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(deleteStatement(query));
  worker.commit();

  return pqxx_result;
}

std::string DatabaseConnection::getConnectionString() {
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

QueryResult DatabaseConnection::query(DatabaseQuery query) {
  switch (query.type) {
    case QueryType::INSERT: {
      try {
        pqxx::result pqxx_result = performInsert(query);
        return QueryResult{};
      } catch (const pqxx::sql_error &e) {
        std::cout << e.what() << "\n" << e.query() << std::endl;
        throw e;
      } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        throw e;
      }
    }
    case QueryType::SELECT: {
      pqxx::result pqxx_result = performSelect(query);
      QueryResult result{.table = query.table};
      result.values.reserve(pqxx_result.size());
      for (const auto &row : pqxx_result) {
        int index = 0;
        for (const auto &value : row) {
          result.values.push_back(
            std::make_pair(query.fields[index++], value.c_str()));
        }
      }
      return result;
    }

    case QueryType::DELETE: {
      pqxx::result pqxx_result   = performDelete(query);
      QueryResult  result{.table = query.table};
      result.values.reserve(pqxx_result.size());
      for (const auto &row : pqxx_result)
        for (const auto &value : row)
          result.values.push_back(std::make_pair(query.filter.value().front().first, value.c_str()));
      return result;
    }

    case QueryType::UPDATE: {
      return QueryResult{};
    }
  }
  return QueryResult{};
}

template <typename T>
QueryResult DatabaseConnection::query(T query) {
  pqxx::result pqxx_result = performSelect(query);
  QueryResult result{.table = query.table};
  result.values.reserve(pqxx_result.size());
  for (const auto &row : pqxx_result) {
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
  JoinQuery<std::vector<std::variant<CompFilter, CompBetweenFilter>>>);

template QueryResult DatabaseConnection::query(
  JoinQuery<std::vector<QueryFilter>>);

template QueryResult DatabaseConnection::query(
  JoinQuery<QueryFilter>);

template QueryResult DatabaseConnection::query(
  SimpleJoinQuery);

template QueryResult DatabaseConnection::query(
  JoinQuery<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>>);

std::string DatabaseConnection::query(InsertReturnQuery query) {
  std::string returning = query.returning;
  pqxx::result pqxx_result = performInsert(query, returning);

  if (!pqxx_result.empty()) {
    auto row = pqxx_result.at(0);
    if (!row.empty()) {
      auto return_value = row.at(0).as<std::string>();
      return return_value;
    }
  }
  return "";
}

std::string DatabaseConnection::query(UpdateReturnQuery query) {
  std::string returning = query.returning;

  pqxx::result pqxx_result = performUpdate(query, returning);
  if (!pqxx_result.empty()) {
    auto row = pqxx_result.at(0);
    if (!row.empty()) {
      return row.at(0).as<std::string>();
    }
  }
  return "";
}

pqxx::connection DatabaseConnection::getConnection() {
  std::string connectionString{("dbname = " + m_config.credentials.name +
                                " user = " + m_config.credentials.user +
                                " password = " + m_config.credentials.password +
                                " hostaddr = " + m_config.address + " port " +
                                m_config.port)};
  return pqxx::connection(connectionString);
}

std::string DatabaseConnection::databaseName() { return m_db_name; }
