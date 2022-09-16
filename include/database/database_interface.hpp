#pragma once

#include "db_structs.hpp"

class DatabaseInterface {
 public:
  virtual ~DatabaseInterface() {}
  virtual QueryResult query(DatabaseQuery query)        = 0;
  virtual std::string query(InsertReturnQuery query)    = 0;
  virtual std::string query(UpdateReturnQuery query)    = 0;
  virtual bool set_config(DatabaseConfiguration config) = 0;
};
