#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifndef STRUCTS_H
#define STRUCTS_H

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

namespace FilterTypes {
static constexpr int STANDARD = 1;
static constexpr int COMPARISON = 2;
}  // namespace FilterTypes

struct GenericFilter {
  std::string a;
  std::string b;
  std::string comparison;
  int type = FilterTypes::STANDARD;

  GenericFilter(std::string&& a, std::string&&comparison, std::string&& b, int type = FilterTypes::STANDARD) : a(std::move(a)), comparison(std::move(comparison)), b(std::move(b)), type(type) {}
};

struct CompFilter : GenericFilter {
  std::string a;
  std::string b;
  std::string c;
  int type = FilterTypes::COMPARISON;

  CompFilter(std::string&& a, std::string&& b, std::string&&c, int type = FilterTypes::COMPARISON) : GenericFilter(std::move(a), std::move(c), std::move(b), type) {}
};

struct Query {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  // /* type */ QueryType type;
  /* values */ std::vector<std::string> values;
};

struct DatabaseQuery : Query {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  /* type */ QueryType type;
  /* values */ std::vector<std::string> values;
  /* filter */ QueryFilter filter;
};


struct MultiFilterSelect {
  std::string table;
  std::vector<std::string> fields;
  std::vector<GenericFilter> filter;
};

struct InsertReturnQuery : Query {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  /* type */ QueryType type = QueryType::INSERT;
  /* values */ StringVec values;
  /* returning */ std::string returning;
};

struct UpdateReturnQuery : Query {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  /* type */ QueryType type = QueryType::INSERT;
  /* values */ StringVec values;
  /* filter */ QueryFilter filter;
  /* returning */ std::string returning;
};

struct ComparisonSelectQuery : Query {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  /* values */ std::vector<std::string> values;
  /* filter */ QueryComparisonFilter filter;
};

struct ComparisonBetweenSelectQuery : Query {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  /* values */ std::vector<std::string> values;
  /* filter */ std::vector<CompFilter> filter;
};

struct QueryResult {
  /* table */ std::string table;
  /* values */ std::vector<std::pair<std::string, std::string>> values;
};

struct KApplication {
  std::string name;
  std::string path;
  std::string data;

  friend std::ostream &operator<<(std::ostream &out, const KApplication &app) {
    out << "Name: " << app.name << "\nPath: " << app.path
        << "\nData: " << app.data << std::endl;

    return out;
  }
};

#endif  // STRUCTS_H
