#pragma once

#define GTEST_REMOVE_LEGACY_TEST_CASEAPI_

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <database/database_connection.hpp>
#include <pqxx/pqxx>
#include <config/config_parser.hpp>
#include <database/kdb.hpp>

using ::testing::_;
using ::testing::Return;

static const auto query_test_value = std::vector<std::map<std::string, std::string>>
  {
    std::map<std::string, std::string>
    {
      {"database_test", "value1"}
    },
    std::map<std::string, std::string>
    {
      {"database_test", "value2"}
    }
  };


DatabaseQuery g_test_query{.table = "apps",
                           .fields = {"name"},
                           .type = QueryType::SELECT,
                           .values = {},
                           .filter = {}};
/** Mocks */
class MockDBConnection : public DatabaseConnection {
  public:
    MockDBConnection() {}
    MOCK_METHOD1(query, QueryResult(DatabaseQuery));
};

class MockDBFixture : public ::testing::Test {
  public:

  bool setConfig(DatabaseConfiguration config) {
    return mock_db_connection.set_config(config);
  }
    void SetUp() override {
      EXPECT_CALL(mock_db_connection, query(_))
      .WillOnce(
        Return(
          QueryResult{
            .table = "database_test",
            .values = query_test_value
          }
        )
      );
    }

  protected:
    QueryResult query(DatabaseQuery query) {
      return mock_db_connection.query(query);
    }

  private:
    MockDBConnection mock_db_connection;

};

TEST_F(MockDBFixture, MockQueryTest) {
  // MockDBConnection mock_db_connection{};
  setConfig(
    DatabaseConfiguration{
      DatabaseCredentials {
        kiq::config::Database::user(),
        kiq::config::Database::pass(),
        kiq::config::Database::name()
      },
      "127.0.0.1",
      "5432"
    }
  );
  DatabaseQuery test_query{.table = "apps",
                                 .fields = {"name"},
                                 .type = QueryType::SELECT,
                                 .values = {},
                                 .filter = {}};

  QueryResult result =  query(test_query);

  auto expected_return_value_1 = std::map<std::string, std::string>{{"database_test", "value1"}};
  auto expected_return_value_2 = std::map<std::string, std::string>{{"database_test", "value2"}};


  EXPECT_EQ(result.table, "database_test");
  EXPECT_EQ(result.values.at(0), expected_return_value_1);
  EXPECT_EQ(result.values[1], expected_return_value_2);
}
