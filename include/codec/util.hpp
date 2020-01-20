#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <codec/instatask_generated.h>
#include <codec/kmessage_generated.h>
#include <codec/uuid.h>

#include <bitset>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <neither/either.hpp>
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
using namespace neither;
using namespace KData;

static const int MAX_PACKET_SIZE = 4096;
static const int HEADER_SIZE = 4;

static const int SESSION_ACTIVE = 1;
static const int SESSION_INACTIVE = 2;

typedef std::string KOperation;
typedef std::map<int, std::string> CommandMap;
typedef std::vector<std::pair<std::string, std::string>> TupVec;
typedef std::vector<std::map<int, std::string>> MapVec;
typedef std::vector<std::pair<std::string, std::string>> SessionInfo;
typedef std::map<int, std::string> ServerData;
struct KSession {
  int fd;
  int status;
  uuid id;
};

std::string get_cwd() {
  char *working_dir_path = realpath(".", NULL);
  return std::string{working_dir_path};
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
  // Make sure not an empty buffer
  // Obtain the raw buffer so we can read the header
  uint8_t *raw_buffer = s_buffer_ptr.get();
  auto val1 = *raw_buffer;
  auto val2 = *(raw_buffer + 1);
  auto val3 = *(raw_buffer + 2);
  auto val4 = *(raw_buffer + 3);

  uint32_t message_byte_size = (*raw_buffer << 24 | *(raw_buffer + 1) << 16,
                                *(raw_buffer + 2) << 8, +(*(raw_buffer + 3)));

  uint8_t task_byte_code = *(raw_buffer + 4);

  uint8_t decode_buffer[message_byte_size];

  if (task_byte_code == 0xFF) {
    flatbuffers::Verifier verifier(&raw_buffer[0 + 5], message_byte_size);
    if (VerifyIGTaskBuffer(verifier)) {
      std::memcpy(decode_buffer, raw_buffer + 5, message_byte_size);
      const KData::IGTask *ig_task = GetIGTask(&decode_buffer);
      /* std::vector<std::string> sv(std::initializer_list<std::string>{ */
      /*     ig_task->filename()->str(), std::to_string(ig_task->time()), */
      /*     ig_task->description()->str(), ig_task->hashtags()->str(), */
      /*     ig_task->requested_by()->str(),
       * ig_task->requested_by_phrase()->str(), */
      /*     ig_task->promote_share()->str(), ig_task->link_bio()->str()}); */
      /* sv.push_back(ig_task->filename()->str()); */
      /* sv.push_back(std::to_string(ig_task->time())); */
      /* sv.push_back(ig_task->description()->str()); */
      /* sv.push_back(ig_task->hashtags()->str()); */
      /* sv.push_back(ig_task->requested_by()->str()); */
      /* sv.push_back(ig_task->requested_by_phrase()->str()); */
      /* sv.push_back(ig_task->promote_share()->str()); */
      /* sv.push_back(ig_task->link_bio()->str()); */
      std::string filename = ig_task->filename()->str();
      std::string time = std::to_string(ig_task->time());
      std::string description = ig_task->description()->str();
      std::string hashtags = ig_task->hashtags()->str();
      std::string requested_by = ig_task->requested_by()->str();
      std::string requested_by_phrase = ig_task->requested_by_phrase()->str();
      std::string promote_share = ig_task->promote_share()->str();
      std::string link_bio = ig_task->link_bio()->str();
      std::vector<std::string> sv(8);
      sv.push_back(filename);
      sv.push_back(time);
      sv.push_back(description);
      sv.push_back(hashtags);
      sv.push_back(requested_by);
      sv.push_back(requested_by_phrase);
      sv.push_back(promote_share);
      sv.push_back(link_bio);

      return right(std::move(sv));
    } else {
      // TODO: Copying into a new buffer for readability - switch to using the
      // original buffer
      flatbuffers::Verifier verifier(&raw_buffer[0 + 4], message_byte_size);
      if (VerifyMessageBuffer(verifier)) {
        std::memcpy(decode_buffer, raw_buffer + 4, message_byte_size);
        // Parse the bytes into an encoded message structure
        auto k_message = GetMessage(&decode_buffer);
        auto id = k_message->id();  // message ID
        // Get the message bytes and create a string
        const flatbuffers::Vector<uint8_t> *message_bytes = k_message->data();

        left(std::string{message_bytes->begin(), message_bytes->end()});
      } else {
        left(std::string(""));
      }
    }
  }
}

std::string getDecodedMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr) {
  // Make sure not an empty buffer
  // Obtain the raw buffer so we can read the header
  uint8_t *raw_buffer = s_buffer_ptr.get();
  auto val1 = *raw_buffer;
  auto val2 = *(raw_buffer + 1);
  auto val3 = *(raw_buffer + 2);
  auto val4 = *(raw_buffer + 3);

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

void createDirectory(const char *dir_name) {
  std::string directory_name = {"data/"};
  directory_name += dir_name;
  std::filesystem::create_directory(directory_name.c_str());
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

void saveEnvFile(std::string env_file_string, std::string filename) {
  std::ofstream out{filename};
  out << env_file_string;
}

void test() {
  char pixels[5];
  std::ofstream output("output.bmp",
                       std::ios::binary | std::ios::out | std::ios::app);
  for (size_t i = 0; i < 5; i++) {
    output.write((char *)&pixels[i], 1);
  }
  output.close();
}
void loadAndPrintFile(std::string_view file_path) {
  std::ifstream ifs("./disgusted_girl.jpg", std::ios::binary);
  std::ifstream::pos_type pos = ifs.tellg();
  std::vector<char> result(pos);
  ifs.seekg(0, std::ios::beg);
  ifs.read(&result[0], pos);

  for (const auto &c : result) {
    std::cout << c;
  }
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
}  // namespace TimeUtils

#endif  // __UTIL_HPP__
