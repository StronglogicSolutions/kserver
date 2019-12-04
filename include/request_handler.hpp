#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <iostream>
#include <string>
#include <vector>

#include "DatabaseConnection.h"
#include "codec/kmessage_generated.h"
#include "config_parser.hpp"

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

  std::string operator()(std::vector<uint32_t> bits) {
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
      unsigned char message_bytes[] = {11, 2, 13, 14, 15, 16, 17, 18};
      // std::vector<unsigned char> vector_bytes(message_bytes);
      // std::vector<flatbuffers::Offset<uint8_t>>
      // flatbuffers_byte_vector(vector_bytes);

      auto byte_vector = builder.CreateVector(message_bytes, 8);
      auto message = CreateMessage(builder, 0, byte_vector);

      return paths;
    }
    return std::string{"Application not found"};
  }

 private:
  DatabaseConnection m_connection;
  DatabaseCredentials m_credentials;
};

#endif  // __REQUEST_HANDLER_HPP__
