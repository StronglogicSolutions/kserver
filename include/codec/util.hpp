#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#define FLATBUFFERS_DEBUG_VERIFICATION_FAILURE
#include <codec/instatask_generated.h>
#include <codec/kmessage_generated.h>
#include <codec/uuid.h>

#include <bitset>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iterator>
#include <neither/either.hpp>
#include <string>
#include <utility>
#include <vector>
#include <ctime>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/pointer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <iostream>

using namespace rapidjson;
using namespace uuids;
using namespace neither;
using namespace KData;
using namespace IGData;

static const int SESSION_ACTIVE = 1;
static const int SESSION_INACTIVE = 2;

static const std::string_view APP_NAME = "kserver";
static constexpr int APP_NAME_LENGTH = 7;

typedef std::string KOperation;
typedef std::map<int, std::string> CommandMap;
typedef std::vector<std::pair<std::string, std::string>> TupVec;
typedef std::vector<std::map<int, std::string>> MapVec;
typedef std::vector<std::pair<std::string, std::string>> SessionInfo;
typedef std::map<int, std::string> ServerData;
typedef std::pair<std::string, std::string> FileInfo;

struct KSession {
  int fd;
  int status;
  uuid id;
};

std::string get_cwd() {
  char *working_dir_path = realpath(".", NULL);
  return std::string{working_dir_path};
}

std::string get_executable_cwd() {
  std::string full_path{realpath("/proc/self/exe", NULL)};
  auto return_value = full_path.substr(0, full_path.size() - (APP_NAME_LENGTH  + 1));
  std::cout << return_value << std::endl;
  return return_value;
}

int findIndexAfter(std::string s, int pos, char c) {
  for (int i = pos; i < s.size(); i++) {
    if (s.at(i) == c) {
      return i;
    }
  }
  return -1;
}

/**
 * JSON Tools
 */

std::string getJsonString(std::string s) {
  Document d;
  d.Parse(s.c_str());
  StringBuffer buffer;
  PrettyWriter<StringBuffer> writer(buffer);
  d.Accept(writer);
  return buffer.GetString();
}

std::string createMessage(const char *data, std::string args = "") {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
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

std::string createEvent(const char *event, int mask, std::string stdout) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("event");
  w.Key("mask");
  w.Int(mask);
  w.Key("args");
  w.String(stdout.c_str());
  w.EndObject();
  return s.GetString();
}

std::string createEvent(const char *event, std::vector<std::string> args) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("event");
  w.Key("event");
  w.String(event);
  w.Key("args");
  w.StartArray();
  if (!args.empty()) {
    for (const auto &arg : args) {
      w.String(arg.c_str());
    }
  }
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string createEvent(const char *event, int mask,
                        std::vector<std::string> args) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("event");
  w.Key("event");
  w.String(event);
  w.Key("args");
  w.StartArray();
  if (!args.empty()) {
    for (const auto &arg : args) {
      w.String(arg.c_str());
    }
  }
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string createOperation(const char *op, std::vector<std::string> args) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("operation");
  w.Key("command");
  w.String(op);
  w.Key("args");
  w.StartArray();
  if (!args.empty()) {
    for (const auto &arg : args) {
      w.String(arg.c_str());
    }
  }
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string getOperation(const char *data) {
  Document d;
  d.Parse(data);
  if (d.HasMember("command")) {
    return d["command"].GetString();
  }
  return "";
}

std::string getMessage(const char *data) {
  Document d;
  d.Parse(data);
  if (d.HasMember("message")) {
    return d["message"].GetString();
  }
  return "";
}

std::vector<std::string> getArgs(const char *data) {
  Document d;
  d.Parse(data);
  std::vector<std::string> args{};
  if (d.HasMember("args")) {
    for (const auto &v : d["args"].GetArray()) {
      args.push_back(v.GetString());
    }
  }
  return args;
}

CommandMap getArgMap(const char *data) {
  Document d;
  d.Parse(data);
  CommandMap cm{};
  if (d.HasMember("args")) {
    for (const auto &m : d["args"].GetObject()) {
      cm.emplace(std::stoi(m.name.GetString()), m.value.GetString());
    }
  }
  return cm;
}

std::string createSessionEvent(
    int status, std::string message = "",
    std::vector<std::pair<std::string, std::string>> args = {}) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("event");
  w.Key("event");
  w.String("Session Message");
  w.Key("status");
  w.Int(status);
  w.Key("message");
  w.String(message.c_str());
  w.Key("info");
  w.StartObject();
  if (!args.empty()) {
    for (const auto &v : args) {
      w.Key(v.first.c_str());
      w.String(v.second.c_str());
    }
  }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string createMessage(
    const char *data, std::vector<std::pair<std::string, std::string>> args) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!args.empty()) {
    for (const auto &v : args) {
      w.Key(v.first.c_str());
      w.String(v.second.c_str());
    }
  }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string createMessage(const char *data,
                          std::map<int, std::string> map = {}) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!map.empty()) {
    for (const auto &[k, v] : map) {
      w.Key(std::to_string(k).c_str());
      w.String(v.c_str());
    }
  }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string createMessage(const char *data,
                          std::map<int, std::vector<std::string>> map = {}) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!map.empty()) {
    for (const auto &[k, v] : map) {
      w.Key(std::to_string(k).c_str());
      if (!v.empty()) {
        w.StartArray();
        for (const auto &arg : v) {
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

/**
 * Operations
 */

bool isMessage(const char *data) {
  Document d;
  d.Parse(data);
  return d.HasMember("message");
}

bool isOperation(const char *data) {
  Document d;
  d.Parse(data);
  return strcmp(d["type"].GetString(), "operation") == 0;
}

bool isExecuteOperation(const char *data) {
  return strcmp(data, "Execute") == 0;
}

bool isScheduleOperation(const char *data) {
  return strcmp(data, "Schedule") == 0;
}

bool isFileUploadOperation(const char *data) {
  return strcmp(data, "FileUpload") == 0;
}

bool isStartOperation(const char *data) { return strcmp(data, "start") == 0; }

bool isStopOperation(const char *data) { return strcmp(data, "stop") == 0; }

/**
 * General
 */

Either<std::string, std::vector<std::string>> getSafeDecodedMessage(
    std::shared_ptr<uint8_t[]> s_buffer_ptr) {
  // Obtain the raw buffer so we can read the header
  uint8_t *raw_buffer = s_buffer_ptr.get();

  uint8_t msg_type_byte_code = *(raw_buffer + 4);

  if (msg_type_byte_code == 0xFD) {
    return left(std::to_string(msg_type_byte_code));
  } else{
    auto byte1 = *raw_buffer << 24;
    auto byte2 = *(raw_buffer + 1) << 16;
    auto byte3 = *(raw_buffer + 2) << 8;
    auto byte4 = *(raw_buffer + 3);

    uint32_t message_byte_size = byte1 | byte2 | byte3 | byte4;
    uint8_t decode_buffer[message_byte_size];

    if (msg_type_byte_code == 0xFF) {
      flatbuffers::Verifier verifier(&raw_buffer[0 + 5], message_byte_size);

      if (VerifyIGTaskBuffer(verifier)) {
        std::memcpy(decode_buffer, raw_buffer + 5, message_byte_size);
        const IGData::IGTask *ig_task = GetIGTask(&decode_buffer);

        return right(std::move(
          std::vector<std::string>{
            ig_task->file_info()->str(), ig_task->time()->str(),
            ig_task->description()->str(), ig_task->hashtags()->str(),
            ig_task->requested_by()->str(), ig_task->requested_by_phrase()->str(),
            ig_task->promote_share()->str(), ig_task->link_bio()->str(),
            std::to_string(ig_task->is_video()),
            std::to_string(ig_task->mask()),
            ig_task->header()->str(), ig_task->user()->str()
          }
        ));
      }
      return right(std::vector<std::string>{});
    } else if (msg_type_byte_code == 0xFE) {
      // TODO: Copying into a new buffer for readability - switch to using the
      // original buffer
      flatbuffers::Verifier verifier(&raw_buffer[0 + 5], message_byte_size);
      if (VerifyMessageBuffer(verifier)) {
        std::memcpy(decode_buffer, raw_buffer + 5, message_byte_size);
        // Parse the bytes into an encoded message structure
        auto k_message = GetMessage(&decode_buffer);
        auto id = k_message->id();  // message ID
        // Get the message bytes and create a string
        const flatbuffers::Vector<uint8_t> *message_bytes = k_message->data();
        return left(std::string{message_bytes->begin(), message_bytes->end()});
      } else {
        return left(std::string(""));
      }
    } else {
      return left(std::string(""));
    }
  }
}

std::string getDecodedMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr) {
  // Make sure not an empty buffer
  // Obtain the raw buffer so we can read the header
  uint8_t *raw_buffer = s_buffer_ptr.get();
  uint32_t message_byte_size = (*raw_buffer << 24 | *(raw_buffer + 1) << 16,
                                *(raw_buffer + 2) << 8, +(*(raw_buffer + 3)));
  // TODO: Copying into a new buffer for readability - switch to using the
  // original buffer
  uint8_t decode_buffer[message_byte_size];
  std::memcpy(decode_buffer, raw_buffer + 4, message_byte_size);
  // Parse the bytes into an encoded message structure
  auto k_message = GetMessage(&decode_buffer);
  auto id = k_message->id();  // message ID
  // Get the message bytes and create a string
  const flatbuffers::Vector<uint8_t> *message_bytes = k_message->data();

  return std::string{message_bytes->begin(), message_bytes->end()};
}

bool isNewSession(const char *data) {
  Document d;
  d.Parse(data);
  if (d.HasMember("message")) {
    return strcmp(d["message"].GetString(), "New Session") == 0;
  }
  return false;
}

namespace FileUtils {

/**
 * parseFileInfo
 *
 * Deduces information about a files sent by a client using KY_GUI
 *
 * @param[in] {std::string} `file_info` The information string
 * @returns {std::vector<FileInfo>} A vector of FileInfo objects
 *
 */
std::vector<FileInfo> parseFileInfo(std::string file_info) {
  std::cout << file_info << std::endl;
  std::vector<FileInfo> info_v{};
  info_v.reserve(file_info.size() /
                 25);  // Estimating number of files being represented
  size_t pipe_pos = 0;
  size_t index = 0;
  size_t delim_pos = 0;
  std::string parsing{file_info, file_info.size()};
  do {
    auto timestamp = file_info.substr(index, 10);
    pipe_pos = findIndexAfter(file_info, index, '|');
    auto file_name = file_info.substr(index + 10, (pipe_pos - index - 10));
    delim_pos = findIndexAfter(file_info, index, '::');
    auto type =
        file_info.substr(index + 10 + file_name.size() + 1,
                         (delim_pos - index - 10 - file_name.size() - 1));
    info_v.push_back(FileInfo{file_name, timestamp});
    index += timestamp.size() + file_name.size() + type.size() +
             3;  // 3 strings + 3 delim chars
  } while (index < file_info.size());
  return info_v;
}

bool createDirectory(const char *dir_name) {
  return std::filesystem::create_directory(dir_name);
}

void saveFile(std::vector<char> bytes, const char *filename) {
  std::ofstream output(filename,
                       std::ios::binary | std::ios::out | std::ios::app);
  char *raw_data = bytes.data();
  for (size_t i = 0; i < bytes.size(); i++) {
    output.write(const_cast<const char *>(&raw_data[i]), 1);
  }
  output.close();
}

void saveFile(uint8_t *bytes, int size, std::string filename) {
  std::ofstream output(filename.c_str(),
                       std::ios::binary | std::ios::out | std::ios::app);
  for (int i = 0; i < size; i++) {
    output.write((const char *)(&bytes[i]), 1);
  }
  output.close();
}

std::string saveEnvFile(std::string env_file_string, std::string uuid) {
  std::string relative_path{"data/" + uuid + "/v.env"};
  std::string filename{get_executable_cwd() + "/" + relative_path};
  std::ofstream out{filename.c_str()};
  out << env_file_string;
  return relative_path;
}

std::string readEnvFile(std::string env_file_path) {
    std::ifstream file_stream{env_file_path};
    std::stringstream env_file_stream{};
    env_file_stream << file_stream.rdbuf();
    return env_file_stream.str();
}

bool createTaskDirectory(std::string uuid) {
  std::string directory_name{get_executable_cwd() + "/data/" + uuid};
  std::cout << directory_name << std::endl;
  return createDirectory(directory_name.c_str());
}
}  // namespace FileUtils

// Bit helpers

inline size_t findNullIndex(uint8_t *data) {
  size_t index = 0;
  while (data) {
    if (strcmp(const_cast<const char *>((char *)data), "\0") == 0) {
      break;
    }
    index++;
    data++;
  }
  return index;
}

template <typename T>
static std::string toBinaryString(const T &x) {
  std::stringstream ss;
  ss << std::bitset<sizeof(T) * 8>(x);
  return ss.str();
}

bool hasNthBitSet(int value, int n) {
  auto result = value & (1 << (n - 1));
  if (result) {
    return true;
  }
  return false;
}

// aka isNumber
bool isdigits(const std::string &s) {
  for (char c : s)
    if (!isdigit(c)) {
      return false;
    }
  return true;
}

namespace TimeUtils {
int unixtime() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string_view format_timestamp(int unixtime) {
  char       buf[80];
  const std::time_t time = static_cast<std::time_t>(unixtime);
  struct tm ts = *localtime(&time);
  std::strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &ts);
  return std::string{buf};
}

std::string format_timestamp(std::string unixtime) {
  char       buf[80];
  const std::time_t time = static_cast<std::time_t>(stoi(unixtime));
  struct tm ts = *localtime(&time);
  std::strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &ts);
  return std::string{buf};
}
}  // namespace TimeUtils

#endif  // __UTIL_HPP__
