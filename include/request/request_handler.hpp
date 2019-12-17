#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <codec/kmessage_generated.h>
#include <database/DatabaseConnection.h>

#include <config/config_parser.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace KData;

flatbuffers::FlatBufferBuilder builder(1024);

class RequestHandler {
 public:
  RequestHandler() {
    if (!ConfigParser::initConfig()) {
      std::cout << "Unable to load config" << std::endl;
      return;
    }

    m_credentials = {.user = ConfigParser::getDBUser(),
                     .password = ConfigParser::getDBPass(),
                     .name = ConfigParser::getDBName()};

    DatabaseConfiguration configuration{
        .credentials = m_credentials, .address = "127.0.0.1", .port = "5432"};

    m_connection = DatabaseConnection{};
    m_connection.setConfig(configuration);
  }

  std::pair<uint8_t*, int> operator()(std::vector<uint32_t> bits) {
    QueryFilter filter{};

    for (const auto& bit : bits) {
      filter.push_back(
          std::make_pair(std::string("mask"), std::to_string(bit)));
    }
    DatabaseQuery select_query{.table = "apps",
                               .fields = {"name", "path", "data"},
                               .type = QueryType::SELECT,
                               .values = {},
                               .filter = filter};
    QueryResult result = m_connection.query(select_query);
    std::string paths{};
    if (!result.values.empty()) {
      for (const auto& row : result.values) {
        if (row.first == "path") paths += row.second + "\n";
      }
      unsigned char message_bytes[] = {33, 34, 45, 52, 52, 122, 101, 35};
      auto byte_vector = builder.CreateVector(message_bytes, 8);
      auto message = CreateMessage(builder, 0, byte_vector);
      builder.Finish(message);

      uint8_t* message_buffer = builder.GetBufferPointer();
      int size = builder.GetSize();

      std::pair<uint8_t*, int> ret_tuple =
          std::make_pair(message_buffer, std::ref(size));

      return ret_tuple;
    }
    /* return std::string{"Application not found"}; */
    uint8_t byte_array[6]{1, 2, 3, 4, 5, 6};
    return std::make_pair(&byte_array[0], 6);
  }

 private:
  DatabaseConnection m_connection;
  DatabaseCredentials m_credentials;
};

#endif  // __REQUEST_HANDLER_HPP__
