#ifndef __DATABASE_INTERFACE_HPP__
#define __DATABASE_INTERFACE_HPP__

#include <database/db_structs.hpp>

class DatabaseInterface {
 public:
  virtual QueryResult query(DatabaseQuery query) = 0;
  virtual ~DatabaseInterface() {}
  virtual std::string query(InsertReturnQuery query) = 0;
  virtual std::string query(UpdateReturnQuery query) = 0;
  virtual bool setConfig(DatabaseConfiguration config) = 0;
};

#endif  // __DATABASE_INTERFACE_HPP__

