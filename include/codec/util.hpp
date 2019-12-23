#include <string>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

struct KSession {
  int id;
  int fd;
  int status;
};

std::string createMessage(const char* data) {
  json data_json{};
  data_json["type"] = "custom";
  data_json["program"] = "placeholder";
  data_json["message"] = data;

  return data_json.dump();
}

std::string createOperation(const char* op, std::vector<std::string> args) {
  json operation_json{};
  operation_json["type"] = "operation";
  operation_json["command"] = op;
  if (!args.empty()) {
    operation_json["args"] = args;
  }
  return operation_json.dump();
}

bool isOperation(json data) { return *(data.find("type")) == "operation"; }

bool isStartOperation(std::string operation) { return operation == "start"; }

bool isStopOperation(std::string operation) { return operation == "stop"; }