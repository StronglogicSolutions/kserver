#ifndef __DB_STRUCTS_H__
#define __DB_STRUCTS_H__

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <variant>

enum QueryType { INSERT = 0, DELETE = 1, UPDATE = 2, SELECT = 3 };

typedef std::vector<std::pair<std::string, std::string>> TupVec;
typedef std::vector<std::string> Fields;
typedef std::vector<std::string> StringVec;

struct DatabaseCredentials {
  std::string user;
  std::string password;
  std::string name;
};

struct DatabaseConfiguration {
  /* credentials */ DatabaseCredentials credentials;
  /* address */ std::string address;
  /* port */ std::string port;
};

typedef std::tuple<std::string, std::string, std::string> FTuple;

typedef std::vector<std::pair<std::string, std::string>> QueryFilter;
typedef std::vector<FTuple> QueryComparisonFilter;
typedef std::vector<FTuple> QueryComparisonBetweenFilter;
typedef std::vector<std::string> Values;
typedef std::pair<std::string, std::string> QueryValue;
typedef std::vector<QueryValue> QueryValues;

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

struct QueryFilterDev
{
using FilterPair = std::pair<std::string, std::string>;
using Filters = std::vector<FilterPair>;
template <typename... T>
QueryFilterDev static Create(T... args)
{
  QueryFilter filter{};
  filter
}
void Add(const std::string& key, const std::string& value)
{
  m_filters.emplace_back(FilterPair{key, DoubleSingleQuotes(value)});
}

Filters value() { return m_filters; }

private:
Filters m_filters;
};

namespace FilterTypes {
static constexpr int STANDARD = 1;
static constexpr int COMPARISON = 2;
}  // namespace FilterTypes

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
  std::string table;
  std::vector<std::string> fields;
  std::vector<GenericFilter> filter;
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
  std::string table;
  std::vector<std::string> fields;
  QueryType type = QueryType::INSERT;
  StringVec values;
  std::string returning;
};

struct UpdateReturnQuery : Query {
  std::string table;
  std::vector<std::string> fields;
  QueryType type = QueryType::INSERT;
  StringVec values;
  QueryFilter filter;
  std::string returning;
};

struct ComparisonSelectQuery : Query {
  std::string table;
  std::vector<std::string> fields;
  std::vector<std::string> values;
  QueryComparisonFilter filter;
};

struct ComparisonBetweenSelectQuery : Query {
  std::string table;
  std::vector<std::string> fields;
  std::vector<std::string> values;
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
};

struct SimpleJoinQuery{
  std::string              table;
  std::vector<std::string> fields;
  QueryFilter              filter;
  Join                     join;
};

#endif  // __DB_STRUCTS_H__
