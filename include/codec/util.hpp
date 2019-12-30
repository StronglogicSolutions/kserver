#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <codec/uuid.h>

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/pointer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using namespace rapidjson;
using namespace uuids;

static const int MAX_PACKET_SIZE = 4096;
static const int HEADER_SIZE = 4;

typedef std::string KOperation;
typedef std::map<int, std::string> CommandMap;
typedef std::vector<std::pair<std::string, std::string>> TupVec;
typedef std::vector<std::map<int, std::string>> MapVec;

struct KSession {
  int fd;
  int status;
  uuid id;
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

bool isFileUploadOperation(const char* data) {
  return strcmp(data, "FileUpload") == 0;
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

std::string createMessage(
    const char* data, std::vector<std::pair<std::string, std::string>> args) {
  StringBuffer s;
  Writer<StringBuffer> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!args.empty()) {
    for (const auto& v : args) {
      w.Key(v.first.c_str());
      w.String(v.second.c_str());
    }
  }
  w.EndObject();
  w.EndObject();
  return s.GetString();
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

std::string createMessage(const char* data,
                          std::map<int, std::vector<std::string>> map = {}) {
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
      if (!v.empty()) {
        w.StartArray();
        for (const auto& arg : v) {
          w.String(arg.c_str());
        }
        w.EndArray();
      }
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
  return strcmp(data, "start") == 0;
}

bool isStopOperation(const char* data) {
  return strcmp(data, "stop") == 0;
}

bool isNewSession(const char* data) {
  Document d;
  d.Parse(data);
  if (d.HasMember("message")) {
    return strcmp(d["message"].GetString(), "New Session") == 0;
  }
  return false;
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

void test() {
  char pixels[5];
  std::ofstream output("output.bmp",
                       std::ios::binary | std::ios::out | std::ios::app);
  for (size_t i = 0; i < 5; i++) {
    output.write((char*)&pixels[i], 1);
  }
  output.close();
}
void loadAndPrintFile(std::string_view file_path) {
  /* std::string filename = ""; */
  /* // prepare a file to read */
  /* double d = 3.14; */
  /* std::ofstream(filename,
   * std::ios::binary).write(reinterpret_cast<char*>(&d), sizeof d) */
  /*    << 123 << "abc"; */
  // open file for reading
  std::ifstream ifs("./disgusted_girl.jpg", std::ios::binary);
  std::ifstream::pos_type pos = ifs.tellg();
  std::vector<char> result(pos);
  ifs.seekg(0, std::ios::beg);
  ifs.read(&result[0], pos);

  for (const auto& c : result) {
    std::cout << c;
  }
}

#endif  // __UTIL_HPP__
