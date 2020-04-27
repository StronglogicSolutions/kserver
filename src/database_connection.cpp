#include <database/database_connection.hpp>

#include <iostream>
#include <pqxx/pqxx>
#include <sstream>
#include <utility>
#include <memory>
#include <type_traits>
#include <variant>

std::string fieldsAsString(std::vector<std::string> fields) {
  std::string field_string{""};
  std::string delim = "";

  for (const auto &field : fields) {
    field_string += delim + field;
    delim = ",";
  }
  return std::move(field_string);
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
    value_string += "'" + value + "'" + delim;
  }
  value_string.erase(value_string.end() - 2, value_string.end());

  return std::move(value_string);
}

std::string insertStatement(DatabaseQuery query) {
  return std::string{"INSERT INTO " + query.table + "(" +
                     fieldsAsString(query.fields) + ") " +
                     valuesAsString(query.values, query.fields.size())};
}

std::string insertStatement(InsertReturnQuery query, std::string returning) {
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
std::string updateStatement(UpdateReturnQuery query, std::string returning, bool multiple = false) {
  if (!query.filter.empty()) {
    if (!multiple) { // TODO: Handle case for updating multiple rows at once
      std::string filter_string{"WHERE " + query.filter.at(0).first + " = "};
      filter_string += query.filter.at(0).second;
      std::string update_string{"SET "};
      std::string delim = "";
      if (query.values.size() ==
          query.fields.size()) {  // can only update if the `fields` and `values`
                                  // arguments are matching
        for (int i = 0; i < query.values.size(); i++) {
          auto field = query.fields.at(i);
          auto value = query.values.at(i);
          update_string += field + "=" + value;
          delim = " AND ";
        }
      }
      return std::string{"UPDATE " + query.table + " " + update_string + " " +
                          filter_string + " RETURNING " + returning};
    }
  }
  return "";
}

template <typename T>
std::string filterStatement(T filter) {
  std::string filter_string{};
  if constexpr(std::is_same_v<T, MultiOptionFilter>) {
    filter_string += filter.a + " " + filter.comparison + " (";
    std::string delim = "";
    for (const auto& option : filter.options) {
      filter_string += delim + option;
      delim = ",";
    }
    filter_string += ")";
  } else if constexpr(std::is_same_v<T, CompBetweenFilter>) {
    filter_string += filter.field + " BETWEEN " +
                  filter.a + " AND " +
                  filter.b;
  } else if constexpr(std::is_same_v<T, CompFilter>) {
    filter_string += filter.a + filter.sign + filter.b;
  }
  return std::move(filter_string);
}

template <typename FilterA, typename FilterB>
std::string getVariantFilterStatement(std::vector<std::variant<FilterA, FilterB>> filters) {
  std::string filter_string{};
  auto idx = 0;
  auto filter_count = filters.size();
  for (const auto& filter : filters) {
    if (filter.index() == 0) {
      filter_string += filterStatement(std::get<0>(filter));
    } else {
      filter_string += filterStatement(std::get<1>(filter));
    }
    if (filter_count > idx + 1) {
      idx++;
      filter_string += " AND ";
    }
  }
  return filter_string;
}

// TODO: Phase this out, and only use "filterStatement" above
template <typename T>
std::string getFilterStatement(T filter) { // TODO: fix template usage
  if constexpr(std::is_same_v<T, MultiOptionFilter>) {
    std::string filter_string{filter.a + " " + filter.comparison + " ("};
    std::string delim = "";
    for (const auto& option : filter.options) {
      filter_string += delim + option;
      delim = ",";
    }
    filter_string += ")";
    return std::move(filter_string);
  } else if constexpr(std::is_same_v<T, CompBetweenFilter>) {
    return std::move(std::string{filter.field + " BETWEEN " +
                       filter.a + " AND " +
                       filter.b});
  } else if constexpr(std::is_same_v<T, CompFilter>) {
    return std::move(std::string{filter.a + filter.sign + filter.b});
  } else if constexpr(std::is_same_v<T, GenericFilter>) {
    return std::move(std::string{filter.a + filter.comparison + filter.b});
  }
}

/**
 * \note Most of this can be removed and replaced with newer implementation as is demonstrated in
 * handling of vector of "variant" filters
 * TODO: Finish refactor and simplify this mess
 */
template <typename T>
std::string selectStatement(T query) {
  std::string delim{""};
  std::string filter_string{"WHERE "};
  size_t index = 0;
  if (!query.filter.empty()) {
    if constexpr (std::is_same_v<T, Query>) {
      if (query.filter.size() > 1 &&
          query.filter.at(0).first == query.filter.at(1).first) {
        filter_string += query.filter.at(0).first + " in (";
        for (const auto &filter_pair : query.filter) {
          filter_string += delim + filter_pair.second;
          delim = ",";
        }
        return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                          query.table + " " + filter_string + ")"};
      }
      for (const auto &filter_pair : query.filter) {
        filter_string +=
            delim + filter_pair.first + "='" + filter_pair.second + "'";
        delim = " AND ";
      }
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                        query.table + " " + filter_string};
    } else if constexpr (std::is_same_v<T, DatabaseQuery>) {
      if (query.filter.size() > 1 &&
          query.filter.at(0).first == query.filter.at(1).first) {
        filter_string += query.filter.at(0).first + " in (";
        for (const auto &filter_pair : query.filter) {
          filter_string += delim + filter_pair.second;
          delim = ",";
        }
        return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                          query.table + " " + filter_string + ")"};
      }
      for (const auto &filter_pair : query.filter) {
        filter_string +=
            delim + filter_pair.first + "='" + filter_pair.second + "'";
        delim = " AND ";
      }
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                        query.table + " " + filter_string};
    } else if constexpr (std::is_same_v<T, ComparisonSelectQuery>) {
      if (query.filter.size() > 1) {
        // We do not curently support multiple comparisons in a single query
        return std::string{"SELECT 1"};
      }
      for (const auto &filter_tup : query.filter) {
        filter_string += delim + std::get<0>(filter_tup) +
                          std::get<1>(filter_tup) + std::get<2>(filter_tup);
        delim = " AND ";
      }
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                      query.table + " " + filter_string};
    } else if constexpr (std::is_same_v<T, ComparisonBetweenSelectQuery>) {
      if (query.filter.size() > 1) {
        // We do not curently support multiple comparisons in a single query
        return std::string{"SELECT 1"};
      }
      for (const auto &filter : query.filter) {
        filter_string += delim + getFilterStatement(filter);
      }
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                        query.table + " " + filter_string};
    } else if constexpr (std::is_same_v<T, MultiFilterSelect>) {
      for (const auto &filter : query.filter) {
        filter_string += delim + getFilterStatement(filter); // *** HERE for variant impl
        delim = " AND ";
      }
      return {"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table +
              " " + filter_string};
    } else if constexpr(std::is_same_v<T, MultiVariantFilterSelect>) {
      filter_string += getVariantFilterStatement<CompBetweenFilter, MultiOptionFilter>(query.filter);
      return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " + query.table + " " + filter_string};
    }
  } else {
    return std::string{"SELECT " + fieldsAsString(query.fields) + " FROM " +
                    query.table};
  }
}

DatabaseConnection::DatabaseConnection() {}

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
  pqxx::result pqxx_result = worker.exec(insertStatement(query, returning));
  worker.commit();

  return pqxx_result;
}

pqxx::result DatabaseConnection::performUpdate(UpdateReturnQuery query,
                                               std::string returning) {
  std::string table = query.table;
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(updateStatement(query, returning));
  worker.commit();

  return pqxx_result;
}

template <typename T>
pqxx::result DatabaseConnection::performSelect(T query) {
  pqxx::connection connection(getConnectionString().c_str());
  pqxx::work worker(connection);
  pqxx::result pqxx_result = worker.exec(selectStatement(query));
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
      } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
      }
    }
    case QueryType::SELECT: {
      pqxx::result pqxx_result = performSelect(query);
      QueryResult result{};
      result.table = query.table;
      auto count = query.fields.size();

      for (auto row : pqxx_result) {
        int index = 0;
        for (const auto &field : row) {
          std::string field_name = query.fields[index++];
          auto row_chars = field.c_str();
          if (row_chars != nullptr) {
            std::string value{row_chars};
            auto pair = std::make_pair(field_name, value);
            result.values.push_back(pair);
          }
        }
      }
      return result;
    }

    case QueryType::DELETE: {
      std::stringstream query_stream{};
      query_stream << " " << query.table << " VALUES (\"test\")";
      QueryResult result{};
      return result;
    }
  }
  return QueryResult{};
}

QueryResult DatabaseConnection::query(ComparisonSelectQuery query) {
  pqxx::result pqxx_result = performSelect(query);
  QueryResult result{};
  result.table = query.table;
  auto count = query.fields.size();

  for (auto row : pqxx_result) {
    int index = 0;
    for (const auto &field : row) {
      std::string field_name = query.fields[index++];
      auto row_chars = field.c_str();
      if (row_chars != nullptr) {
        std::string value{row_chars};
        auto pair = std::make_pair(field_name, value);
        result.values.push_back(pair);
      }
    }
  }
  return result;
}

QueryResult DatabaseConnection::query(MultiFilterSelect query) {
  pqxx::result pqxx_result = performSelect(query);
  QueryResult result{};
  result.table = query.table;
  auto count = query.fields.size();

  for (auto row : pqxx_result) {
    int index = 0;
    for (const auto &field : row) {
      std::string field_name = query.fields[index++];
      auto row_chars = field.c_str();
      if (row_chars != nullptr) {
        std::string value{row_chars};
        auto pair = std::make_pair(field_name, value);
        result.values.push_back(pair);
      }
    }
  }
  return result;
}

QueryResult DatabaseConnection::query(MultiVariantFilterSelect query) {
  pqxx::result pqxx_result = performSelect(query);
  QueryResult result{};
  result.table = query.table;
  auto count = query.fields.size();

  for (auto row : pqxx_result) {
    int index = 0;
    for (const auto &field : row) {
      std::string field_name = query.fields[index++];
      auto row_chars = field.c_str();
      if (row_chars != nullptr) {
        std::string value{row_chars};
        auto pair = std::make_pair(field_name, value);
        result.values.push_back(pair);
      }
    }
  }
  return result;
}

QueryResult DatabaseConnection::query(ComparisonBetweenSelectQuery query) {
  pqxx::result pqxx_result = performSelect(query);
  QueryResult result{};
  result.table = query.table;
  auto count = query.fields.size();

  for (auto row : pqxx_result) {
    int index = 0;
    for (const auto &field : row) {
      std::string field_name = query.fields[index++];
      auto row_chars = field.c_str();
      if (row_chars != nullptr) {
        std::string value{row_chars};
        auto pair = std::make_pair(field_name, value);
        result.values.push_back(pair);
      }
    }
  }
  return result;
}

std::string DatabaseConnection::query(InsertReturnQuery query) {
  std::string returning = query.returning;
  pqxx::result pqxx_result = performInsert(query, returning);

  if (!pqxx_result.empty()) {
    auto row = pqxx_result.at(0);
    if (!row.empty()) {
      return row.at(0).as<std::string>();
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

std::string DatabaseConnection::getDbName() { return m_db_name; }
