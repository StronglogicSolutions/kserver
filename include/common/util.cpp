#include "util.hpp"

std::string get_cwd() {
  char *working_dir_path = realpath(".", NULL);
  return std::string{working_dir_path};
}

std::string get_executable_cwd() {
  std::string full_path{realpath("/proc/self/exe", NULL)};
  return full_path.substr(0, full_path.size() - (APP_NAME_LENGTH  + 1));
}

int findIndexAfter(std::string s, int pos, char c) {
  for (uint8_t i = pos; i < s.size(); i++) {
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

std::string createMessage(const char *data, std::string args) {
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

std::string getEvent(std::string data) {
  if (!data.empty()) {
    Document d;
    d.Parse(data.c_str());
    if (d.HasMember("event")) {
      return d["event"].GetString();
    }
  }
  return "";
}

bool isSessionMessageEvent(std::string event) {
  return event.compare("Session Message") == 0;
}

bool isCloseEvent(std::string event) {
  return event.compare("Close Session") == 0;
}

std::vector<std::string> getArgs(std::string data) {
  Document d;
  d.Parse(data.c_str());
  std::vector<std::string> args{};
  if (d.HasMember("args")) {
    for (const auto &v : d["args"].GetArray()) {
      args.push_back(v.GetString());
    }
  }
  return args;
}

std::vector<std::string> getArgs(const char* data) {
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
    int status, std::string message,
    std::vector<std::pair<std::string, std::string>> args) {
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
                          std::map<int, std::string> map) {
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

std::string createMessage(const char *data, std::vector<KApplication> commands) {
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartArray();
  if (!commands.empty()) {
    for (const auto& command : commands) {
      w.String(command.mask.c_str());
      w.String(command.name.c_str());
      w.String(command.path.c_str());
      w.String(command.data.c_str());
    }
  }
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string createMessage(const char *data,
                          std::map<int, std::vector<std::string>> map) {
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
  if (*data != '\0') {
    Document d;
    d.Parse(data);
    return d.HasMember("message");
  }
  return false;
}

bool isOperation(const char *data) {
  if (*data != '\0') {
    Document d;
    d.Parse(data);
    return strcmp(d["type"].GetString(), "operation") == 0;
  }
  return false;
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

bool isIPCOperation(const char *data) {
  return strcmp(data, "ipc") == 0;
}

bool isStartOperation(const char *data) { return strcmp(data, "start") == 0; }

bool isStopOperation(const char *data) { return strcmp(data, "stop") == 0; }

bool isAppOperation(const char* data) { return strcmp(data, "AppRequest") == 0; }

/**
 * General
 */

Either<std::string, std::vector<std::string>> getDecodedMessage(
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

        /**
         * /note The specification for the order of these arguments can be found
         * in namespace: Executor::IGTaskIndex
         */
        return right(std::move(
          std::vector<std::string>{
            std::to_string(ig_task->mask()), // Mask always comes first
            ig_task->file_info()->str(), ig_task->time()->str(),
            ig_task->description()->str(), ig_task->hashtags()->str(),
            ig_task->requested_by()->str(), ig_task->requested_by_phrase()->str(),
            ig_task->promote_share()->str(), ig_task->link_bio()->str(),
            std::to_string(ig_task->is_video()),
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
        // Get the message bytes and create a string
        const flatbuffers::Vector<uint8_t> *message_bytes = k_message->data();
        return left(std::string{message_bytes->begin(), message_bytes->end()});
      } else {
        return left(std::string(""));
      }
    } else if (msg_type_byte_code == 0xFC) {
      flatbuffers::Verifier verifier(&raw_buffer[0 + 5], message_byte_size);

      if (VerifyGenericTaskBuffer(verifier)) {
        std::memcpy(decode_buffer, raw_buffer + 5, message_byte_size);
        const GenericData::GenericTask *gen_task = GetGenericTask(&decode_buffer);

        /**
         * /note The specification for the order of these arguments can be found
         * in namespace: Executor::GenericTaskIndex
         */
        return right(std::move(
          std::vector<std::string>{
            std::to_string(gen_task->mask()), // Mask always comes first
            gen_task->file_info()->str(),          gen_task->time()->str(),
            gen_task->description()->str(),        std::to_string(gen_task->is_video()),
            gen_task->header()->str(),             gen_task->user()->str(),
            std::to_string(gen_task->recurring()), std::to_string(gen_task->notify()),
            gen_task->runtime()->str()
          }
        ));
      }
      return right(std::vector<std::string>{});
    } else {
      return left(std::string(""));
    }
  }
}

bool isNewSession(const char *data) {
  if (*data != '\0') {
    Document d;
    d.Parse(data);
    if (d.HasMember("message")) {
      return strcmp(d["message"].GetString(), "New Session") == 0;
    }
  }
  return false;
}

namespace SystemUtils {
  void sendMail(std::string recipient, std::string message, std::string from) {
    std::system(std::string{
      "echo \"" + message + "\" | mail -s 'KServer notification' -a FROM:" + from + " " + recipient
      }.c_str()
    );
  }
}

namespace FileUtils {

bool createDirectory(const char *dir_name) {
  std::error_code err{};
  std::filesystem::create_directory(dir_name, err);
  auto code = err.value();
  if (code == 0) {
    return true;
  }
  std::cout << err.message() << "\n" << err.value() << std::endl;
  return false;
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

std::string readFile(std::string env_file_path) {
    std::ifstream file_stream{env_file_path};
    std::stringstream env_file_stream{};
    env_file_stream << file_stream.rdbuf();
    return env_file_stream.str();
}

void clearFile(std::string file_path) {
  std::ofstream file(file_path);
}

bool createTaskDirectory(std::string uuid) {
  std::string directory_name{"data/" + uuid};
  return createDirectory(directory_name.c_str());
}
}  // namespace FileUtils

namespace StringUtils {
template <typename T>
void split(const std::string &s, char delim, T result) {
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim)) {
        *result++ = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> v{};
    split(s, delim, std::back_inserter(v));
    return v;
}
} // namespace StringUtils

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
    std::chrono::system_clock::now().time_since_epoch()
  ).count();
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