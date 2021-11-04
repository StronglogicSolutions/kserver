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
    return mock_db_connection.setConfig(config);
  }
    void SetUp() override {
      EXPECT_CALL(mock_db_connection, query(_))
      .WillOnce(
        Return(
          QueryResult{
            .table="database_test",
            .values{
              {"database_test", "value1"},
              {"database_test", "value2"}
            }
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
        ConfigParser::Database::user(),
        ConfigParser::Database::pass(),
        ConfigParser::Database::name()
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

  auto expected_return_value_1 = std::pair<std::string, std::string>{"database_test", "value1"};
  auto expected_return_value_2 = std::pair<std::string, std::string>{"database_test", "value2"};


  EXPECT_EQ(result.table, "database_test");
  EXPECT_EQ(result.values.at(0), expected_return_value_1);
  EXPECT_EQ(result.values[1], expected_return_value_2);
}

TEST(KDBTest, SelectJoinTest)
{
  Database::KDB db{};
  auto tid = "39";
  EXPECT_NO_THROW(
    db.selectJoin("term_hit", {"term_hit.time", "platform_user.id", "platform_user.name", "organization.id", "organization.name"}, {CreateFilter("term_hit.tid", tid)}, Joins{
        Join{
          .table = "platform_user",
          .field = "id",
          .join_table = "term_hit",
          .join_field = "uid"
        },
        Join{
          .table = "person",
          .field = "id",
          .join_table = "platform_user",
          .join_field = "pers_id"
        },
        Join{
          .table = "affiliation",
          .field = "pid",
          .join_table = "person",
          .join_field = "id"
        },
        Join{
          .table = "organization",
          .field = "id",
          .join_table = "affiliation",
          .join_field = "oid"
        }
      }
    )
  );
}