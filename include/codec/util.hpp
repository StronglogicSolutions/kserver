#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <string>
#include <utility>
#include <vector>

#include "json.hpp"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/pointer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using json = nlohmann::json;
using namespace rapidjson;

typedef std::string KOperation;
typedef std::map<int, std::string> CommandMap;
typedef std::vector<std::pair<std::string, std::string>> TupVec;
typedef std::vector<std::map<int, std::string>> MapVec;

struct KSession {
  int id;
  int fd;
  int status;
};

std::string getJsonString(std::string s) {
  Document d;
  d.Parse(s.c_str());
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  d.Accept(writer);
  return buffer.GetString();
}

std::string createMessage(const char* data, std::string args = "") {
  StringBuffer s;
  Writer<StringBuffer> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.String(args.c_str());
  w.EndObject();
  return s.GetString();
}

std::string createOperation(const char* op, std::vector<std::string> args) {
  StringBuffer s;
  Writer<StringBuffer> w(s);
  w.StartObject();
  w.Key("type");
  w.String("operation");
  w.Key("command");
  w.String(op);
  w.Key("args");
  w.StartArray();
  if (!args.empty()) {
    for (const auto& arg : args) {
      w.String(arg.c_str());
    }
  }
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

bool isOperation(const char* data) {
  Document d;
  d.Parse(data);
  return strcmp(d["type"].GetString(), "operation") == 0;
}

bool isExecuteOperation(const char* data) {
  return strcmp(data, "Execute") == 0;
}

std::string getOperation(const char* data) {
  Document d;
  d.Parse(data);
  if (d.HasMember("command")) {
    return d["command"].GetString();
  }
  return "";
}

std::vector<std::string> getArgs(const char* data) {
  Document d;
  d.Parse(data);
  std::vector<std::string> args{};
  if (d.HasMember("args")) {
    for (const auto& v : d["args"].GetArray()) {
      args.push_back(v.GetString());
    }
  }
  return args;
}

CommandMap getArgMap(const char* data) {
  Document d;
  d.Parse(data);
  CommandMap cm{};
  if (d.HasMember("args")) {
    for (const auto& m : d["args"].GetObject()) {
      cm.emplace(std::stoi(m.name.GetString()), m.value.GetString());
    }
  }
  return cm;
}

std::string createMessage(const char* data,
                          std::map<int, std::string> map = {}) {
  StringBuffer s;
  Writer<StringBuffer> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!map.empty()) {
    for (const auto& [k, v] : map) {
      w.Key(std::to_string(k).c_str());
      w.String(v.c_str());
    }
  }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string rapidCreateMessage(const char* data,
                               std::map<int, std::string> map = {}) {
  StringBuffer s;
  Writer<StringBuffer> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!map.empty()) {
    for (const auto& [k, v] : map) {
      w.Key(std::to_string(k).c_str());
      w.String(v.c_str());
    }
  }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

bool isStartOperation(const char* data) {
  Document d;
  d.Parse(data);
  return strcmp(d["command"].GetString(), "start") == 0;
}

bool isStopOperation(const char* data) {
  Document d;
  d.Parse(data);
  return strcmp(d["command"].GetString(), "stop") == 0;
}

bool isNewSession(const char* data) {
  Document d;
  d.Parse(data);
  if (d.HasMember("message")) {
    return strcmp(d["message"].GetString(), "New Session") == 0;
  }
  return false;
}

/* std::string createMessage(const char* data, TupVec v = {}) { */
/*   json data_json{}; */
/*   data_json["type"] = "custom"; */
/*   data_json["message"] = data; */
/*   data_json["args"] = nullptr; */
/*   if (!v.empty()) { */
/*     for (const auto& r : v) { */
/*       data_json["args"][r.first] = r.second; */
/*     } */
/*   } */
/*   return data_json.dump(); */
/* } */

std::string stringTupleVecToJson(
    std::vector<std::pair<std::string, std::string>> v) {
  json j{};
  for (const auto& row : v) {
    j[row.first] = row.second;
  }
  return j;
}

inline size_t findNullIndex(uint8_t* data) {
  size_t index = 0;
  while (data) {
    if (strcmp(const_cast<const char*>((char*)data), "\0") == 0) {
      break;
    }
    index++;
    data++;
  }
  return index;
}

#endif  // __UTIL_HPP__
