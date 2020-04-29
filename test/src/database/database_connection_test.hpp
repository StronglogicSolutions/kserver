#ifndef __DATABASE_CONNECTION_TEST_HPP__
#define __DATABASE_CONNECTION_TEST_HPP__

#include <gtest/gtest.h>
#include <database/database_connection.hpp>
#include <config/config_parser.hpp>


TEST(DatabaseConnection, ConnectionStringTest) {
  DatabaseConnection db_connection{};

  db_connection.setConfig(
    DatabaseConfiguration{
      DatabaseCredentials {
        ConfigParser::Database::user(),
        ConfigParser::Database::pass(),
        ConfigParser::Database::name()
      },
      "127.0.0.1",
      "5432"
    }
  );
  QueryResult result = db_connection.query(DatabaseQuery{.table = "apps",
                                 .fields = {"name"},
                                 .type = QueryType::SELECT,
                                 .values = {},
                                 .filter = {}});

  EXPECT_EQ(result.table, "apps");

}

#endif // __DATABASE_CONNECTION_TEST_HPP__
