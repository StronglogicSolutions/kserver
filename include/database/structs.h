#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifndef STRUCTS_H
#define STRUCTS_H

enum QueryType { INSERT = 0, DELETE = 1, UPDATE = 2, SELECT = 3 };

typedef std::vector<std::pair<std::string, std::string>> TupVec;

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

typedef std::vector<std::pair<std::string, std::string>> QueryFilter;

struct DatabaseQuery {
  /* table */ std::string table;
  /* fields */ std::vector<std::string> fields;
  /* type */ QueryType type;
  /* values */ std::vector<std::string> values;
  /* filter */ QueryFilter filter;
};

struct QueryResult {
  /* table */ std::string table;
  /* values */ std::vector<std::pair<std::string, std::string>> values;
};

struct KApplication {
  std::string name;
  std::string path;
  std::string data;

  friend std::ostream& operator<<(std::ostream& out, const KApplication& app) {
    out << "Name: " << app.name << "\nPath: " << app.path
        << "\nData: " << app.data << std::endl;

    return out;
  }
};

#endif  // STRUCTS_H
