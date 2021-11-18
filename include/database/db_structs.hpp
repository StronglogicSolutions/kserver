#pragma once

#include <string>
#include <utility>
#include <vector>
#include <variant>

enum class QueryType {
  INSERT = 0,
  DELETE = 1,
  UPDATE = 2,
  SELECT = 3
};

using TupVec                       = std::vector<std::pair<std::string, std::string>>;
using FTuple                       = std::tuple<std::string, std::string, std::string>;
using Fields                       = std::vector<std::string>;
using StringVec                    = std::vector<std::string>;
using QueryComparisonFilter        = std::vector<FTuple>;
using QueryComparisonBetweenFilter = std::vector<FTuple>;
using Values                       = std::vector<std::string>;
using QueryValue                   = std::pair<std::string, std::string>;
using QueryValues                  = std::vector<QueryValue>;

struct DatabaseCredentials {
  std::string user;
  std::string password;
  std::string name;
};

struct DatabaseConfiguration {
  DatabaseCredentials credentials;
  std::string         address;
  std::string         port;
};


static std::string DoubleSingleQuotes(const std::string& s)
{
  std::string o;
  for (const char& c : s)
    if (c == '\'')
      o += "''";
    else
      o += c;
  return o;
}


struct QueryFilter;

template<typename... Ts>
using AllString = decltype((((QueryFilter)std::string(std::declval<Ts>())), ...));

struct QueryFilter
{
using FilterPair = std::pair<std::string, std::string>;
using Filters    = std::vector<FilterPair>;

QueryFilter() {}

template<typename... Ts>
QueryFilter(Ts... args)
{
  Add(args...);
}

template<typename... Ts, typename S = std::string>
void Add(Ts&&... args)
{
  const std::vector<S> v{args...};
  if (v.size() % 2) throw std::invalid_argument{"Must provide alternating key/filter pairs"};

  for (size_t i = 0; i < v.size() - 1; i += 2)
  {
    const auto& key   = v[i    ];
    const auto& value = v[i + 1];
    if (value.empty())  continue;
    m_filters.emplace_back(FilterPair{key, DoubleSingleQuotes(value)});
  }
}

bool       empty()      const
{
  auto empty_value = m_filters.empty();
  return empty_value;
}
size_t     size()       const { return m_filters.size();  }
FilterPair front()      const { return m_filters.front(); }
FilterPair at(size_t i) const { return m_filters.at(i);   }
Filters    value()            { return m_filters;         }

Filters::const_iterator cbegin() const { return m_filters.cbegin(); }
Filters::const_iterator cend()   const { return m_filters.cend();   }
Filters::const_iterator begin()  const { return m_filters.cbegin(); }
Filters::const_iterator end()    const { return m_filters.cend();   }
Filters::iterator       begin()        { return m_filters.begin();  }
Filters::iterator       end()          { return m_filters.end();    }

private:
Filters m_filters;
};

template<typename... Ts, typename = AllString<Ts...>>
static QueryFilter CreateFilter(Ts&&... args)
{
  QueryFilter filter{};
  filter.Add(args...);
  return filter;
}

namespace FilterTypes {
static constexpr int STANDARD = 1;
static constexpr int COMPARISON = 2;
}  // ns FilterTypes

struct GenericFilter {
  std::string a;
  std::string b;
  std::string comparison;
  int type = FilterTypes::STANDARD;
};

struct CompFilter {
  std::string a;
  std::string b;
  std::string sign;
};

struct CompBetweenFilter {
  std::string field;
  std::string a;
  std::string b;
};

struct MultiOptionFilter {
  std::string a;
  std::string comparison;
  std::vector<std::string> options;
};

struct OrderFilter
{
std::string field;
std::string order;
bool has_value()
{
  return (!field.empty());
}
};

struct LimitFilter
{
std::string count;
bool has_value()
{
  return (!count.empty());
}
};

struct Query {
std::string table;
std::vector<std::string> fields;
std::vector<std::string> values;
};

struct DatabaseQuery : Query {
std::string table;
std::vector<std::string> fields;
QueryType type;
std::vector<std::string> values;
QueryFilter filter;
};

struct FullQuery : Query {
std::string              table;
std::vector<std::string> fields;
QueryType                type;
std::vector<std::string> values;
QueryFilter              filter;
};

struct MultiFilterSelect {
std::string                table;
std::vector<std::string>   fields;
std::vector<GenericFilter> filter;
OrderFilter                order;
LimitFilter                limit;
};

template <typename T>
struct MultiVariantFilterSelect {
std::string              table;
std::vector<std::string> fields;
T                        filter;
OrderFilter              order;
LimitFilter              limit;
};

struct InsertReturnQuery : Query {
std::string              table;
std::vector<std::string> fields;
QueryType                type = QueryType::INSERT;
StringVec                values;
std::string              returning;
};

struct UpdateReturnQuery : Query {
std::string table;
Fields      fields;
QueryType   type = QueryType::INSERT;
StringVec   values;
QueryFilter filter;
std::string returning;
};

struct ComparisonSelectQuery : Query {
std::string           table;
Fields                fields;
Values                values;
QueryComparisonFilter filter;
};

struct ComparisonBetweenSelectQuery : Query {
std::string             table;
Fields                  fields;
Values                  values;
std::vector<CompFilter> filter;
};

struct QueryResult {
std::string table;
std::vector<std::pair<std::string, std::string>> values;
};

enum JoinType {
  INNER = 0x00,
  OUTER = 0x01
};

struct Join {
std::string table;
std::string field;
std::string join_table;
std::string join_field;
JoinType    type;
};

using Joins = std::vector<Join>;

template <typename T>
struct JoinQuery : MultiVariantFilterSelect<T> {
std::string              table;
std::vector<std::string> fields;
T                        filter;
Joins                    joins;
OrderFilter              order;
LimitFilter              limit;
};

struct SimpleJoinQuery
{
std::string              table;
std::vector<std::string> fields;
QueryFilter              filter;
Join                     join;
OrderFilter              order;
LimitFilter              limit;
};
