#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <codec/kmessage_generated.h>
#include <database/DatabaseConnection.h>
#include <log/logger.h>

#include <codec/util.hpp>
#include <config/config_parser.hpp>
#include <executor/executor.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace Request {
using namespace KData;

flatbuffers::FlatBufferBuilder builder(1024);

auto KLOG = KLogger::GetInstance() -> get_logger();

class RequestHandler {
 public:
  RequestHandler() : m_executor(nullptr) {
    if (!ConfigParser::initConfig()) {
      KLOG->info("Unable to load config");
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

  RequestHandler(RequestHandler&& r)
      : m_executor(r.m_executor),
        m_connection(r.m_connection),
        m_credentials(r.m_credentials) {
    r.m_executor = nullptr;
  }

  RequestHandler(const RequestHandler& r)
      : m_executor(nullptr),  // We do not copy the Executor
        m_connection(r.m_connection),
        m_credentials(r.m_credentials) {}

  RequestHandler& operator=(const RequestHandler& handler) {
    this->m_executor = nullptr;
    this->m_connection = handler.m_connection;
    this->m_credentials = handler.m_credentials;
    return *this;
  }

  RequestHandler& operator=(RequestHandler&& handler) {
    if (&handler != this) {
      delete m_executor;
      m_executor = handler.m_executor;
      handler.m_executor = nullptr;
    }
    return *this;
  }

  ~RequestHandler() {
    if (m_executor != nullptr) {
      delete m_executor;
    }
  }

  void initialize(std::function<void(std::string, int)> event_callback_fn) {
    m_executor = new ProcessExecutor();
    m_executor->setEventCallback(
        [this](std::string result, int client_socket_fd) {
          onProcessComplete(result, client_socket_fd);
        });
    m_event_callback_fn = event_callback_fn;
  }

  std::map<int, std::string> operator()(KOperation op) {
    // TODO: We need to fix the DB query so it is orderable and groupable
    DatabaseQuery select_query{.table = "apps",
                               .fields = {"name", "path", "data", "mask"},
                               .type = QueryType::SELECT,
                               .values = {}};
    QueryResult result = m_connection.query(select_query);
    std::map<int, std::string> command_map{};
    std::vector<std::string> names{};
    std::vector<int> masks{};
    // int mask = -1;
    std::string name{""};
    for (const auto& row : result.values) {
      if (row.first == "name") {
        names.push_back(row.second);
      } else if (row.first == "mask") {
        masks.push_back(stoi(row.second));
      }
    }
    if (masks.size() == names.size()) {
      for (int i = 0; i < masks.size(); i++) {
        command_map.emplace(masks.at(i), names.at(i));
      }
    }
    return command_map;
  }

  std::string operator()(uint32_t mask, int client_socket_fd) {
    QueryResult result = m_connection.query(
        DatabaseQuery{.table = "apps",
                      .fields = {"path"},
                      .type = QueryType::SELECT,
                      .values = {},
                      .filter = QueryFilter{std::make_pair(
                          std::string("mask"), std::to_string(mask))}});

    for (const auto& row : result.values) {
      KLOG->info("Field: {}, Value: {}", row.first, row.second);
      m_executor->request(row.second, client_socket_fd);
    }

    return std::string{"Process hopefully complete"};
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
  // Callback
  void onProcessComplete(std::string value, int client_socket_fd) {
    m_event_callback_fn(value, client_socket_fd);
  }

  std::function<void(std::string, int)> m_event_callback_fn;

  ProcessExecutor* m_executor;
  DatabaseConnection m_connection;
  DatabaseCredentials m_credentials;
};
}  // namespace Request
#endif  // __REQUEST_HANDLER_HPP__
