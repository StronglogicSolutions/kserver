#include "util.hpp"

namespace kiq {

static const std::string_view APP_NAME         = "kserver";
static       int              APP_NAME_LENGTH  = 7;
const char                    ARGUMENT_SEPARATOR{'\x1f'};

std::string GetCWD()
{
  return realpath(".", NULL);
}

std::string GetExecutableCWD()
{
  std::string full_path{realpath("/proc/self/exe", NULL)};
  return full_path.substr(0, full_path.size() - (APP_NAME_LENGTH  + 1));
}

std::string ReadFileSimple(const std::string& path)
{
  std::ifstream     fs{path};
  std::stringstream ss;
  ss << fs.rdbuf();
  return ss.str();
}

/**
 * JSON Tools
 */

std::string CreateMessage(const char *data, std::string args)
{
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

std::string CreateEvent(const char *event, int mask, std::string stdout)
{
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

std::string CreateEvent(const std::string& event, std::vector<std::string> args)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("event");
  w.Key("event");
  w.String(event.c_str());
  w.Key("args");
  w.StartArray();
  if (!args.empty())
    for (const auto &arg : args)
      w.String(arg.c_str());
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string CreateEvent(const std::string& event, int mask, std::vector<std::string> args)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("event");
  w.Key("event");
  w.String(event.c_str());
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

std::vector<std::string> GetJSONArray(const std::string& s)
{
  Document                 d;
  std::vector<std::string> v;
  if (!(d.Parse(s.c_str()).HasParseError()) && d.IsArray())
    for (const auto& arg : d.GetArray())
      v.emplace_back(arg.GetString());
  return v;
}

std::string ToJSONArray(const std::vector<std::string>& args)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartArray();
  for (const auto &arg : args)
      w.String(arg.c_str());
  w.EndArray();
  return s.GetString();
}

std::string CreateOperation(const char *op, std::vector<std::string> args)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("operation");
  w.Key("command");
  w.String(op);
  w.Key("args");
  w.StartArray();
  if (!args.empty())
    for (const auto &arg : args)
      w.String(arg.c_str());
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string GetOperation(const std::string& data)
{
  if (data.size())
  {
    Document d;
    if (!(d.Parse(data.c_str()).HasParseError()) && d.HasMember("command"))
      return d["command"].GetString();
  }
  return "";
}

std::string GetToken(const std::string& data)
{
  Document d;
  return (!(d.Parse(data.c_str()).HasParseError()) && d.HasMember("token")) ?
    d["token"].GetString() :
    "";
}

User GetAuth(const std::string& data)
{
  User user;
  Document d;
  if (!(d.Parse(data.c_str()).HasParseError()))
  {
    if (d.HasMember("token"))
      user.token = d["token"].GetString();
    if (d.HasMember("user"))
      user.name = d["user"].GetString();
  }
  return user;
}

template<typename T>
std::string GetMessage(T data)
{
  Document d;
  if constexpr (std::is_same_v<T, std::string>)
    d.Parse(data.c_str());
  else
  if constexpr (std::is_same_v<T, const char*>)
    d.Parse(data);
  else
    return "";
  if ((!d.HasParseError()) && d.HasMember("message"))
    return d["message"].GetString();
  return "";
}

template std::string GetMessage(std::string);
template std::string GetMessage(const char*);

std::string GetEvent(std::string data)
{
  if (!data.empty())
  {
    Document d;
    if ((!d.Parse(data.c_str()).HasParseError()) && d.HasMember("event"))
      return d["event"].GetString();
  }
  return "";
}

bool IsSessionMessageEvent(const std::string& event)
{
  return event.compare("Session Message") == 0;
}

bool IsCloseEvent(const std::string& event)
{
  return event.compare("Close Session") == 0;
}

std::vector<std::string> GetArgs(const std::string& data)
{
  std::vector<std::string> v;
  if (data.size())
  {
    Document d;
    if (!(d.Parse(data.c_str()).HasParseError()) && d.HasMember("args"))
      for (const auto& arg : d["args"].GetArray())
        v.emplace_back(arg.GetString());
  }

  return v;
}

std::vector<std::string> GetArgs(const char* data)
{
  std::vector<std::string> args{};
  if (data)
  {
    Document d;
    if (!(d.Parse(data).HasParseError()) && d.HasMember("args"))
      for (const auto &v : d["args"].GetArray())
        args.push_back(v.GetString());
  }
  return args;
}

CommandMap GetArgMap(const char *data)
{
  CommandMap cm{};
  if (data)
  {
    Document   d;
    if (!(d.Parse(data)).HasParseError() && d.HasMember("args"))
      for (const auto &m : d["args"].GetObject())
        cm.emplace(std::stoi(m.name.GetString()), m.value.GetString());
  }
  return cm;
}

using KIQArgMap = std::vector<std::pair<std::string, std::string>>;
std::string CreateSessionEvent(int status, std::string message, KIQArgMap args)
{
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

  for (const auto &v : args)
  {
    w.Key(v.first.c_str());
    w.String(v.second.c_str());
  }

  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string CreateMessage(const char *data, std::vector<std::pair<std::string, std::string>> args)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!args.empty())
    for (const auto &v : args)
    {
      w.Key(v.first.c_str());
      w.String(v.second.c_str());
    }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string CreateMessage(const char *data, std::map<int, std::string> map)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!map.empty())
    for (const auto &[k, v] : map)
    {
      w.Key(std::to_string(k).c_str());
      w.String(v.c_str());
    }
  w.EndObject();
  w.EndObject();
  return s.GetString();
}

std::string CreateMessage(const char *data, std::vector<KApplication> commands)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartArray();
  if (!commands.empty())
    for (const auto& command : commands)
    {
      w.String(command.mask.c_str());
      w.String(command.name.c_str());
      w.String(command.path.c_str());
      w.String(command.data.c_str());
    }
  w.EndArray();
  w.EndObject();
  return s.GetString();
}

std::string CreateMessage(const char *data, std::map<int, std::vector<std::string>> map)
{
  StringBuffer s;
  Writer<StringBuffer, Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("type");
  w.String("custom");
  w.Key("message");
  w.String(data);
  w.Key("args");
  w.StartObject();
  if (!map.empty())
  {
    for (const auto &[k, v] : map)
    {
      w.Key(std::to_string(k).c_str());
      if (!v.empty())
      {
        w.StartArray();
        for (const auto &arg : v)
          w.String(arg.c_str());
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
bool IsMessage(const std::string& data)
{
  if (data.size())
  {
    Document d;
    return (!d.Parse(data.c_str()).HasParseError() && d.HasMember("message"));
  }
  return false;
}

bool IsOperation(const std::string& data)
{
  if (data.size())
  {
    Document d;
    if (!d.Parse(data.c_str()).HasParseError() && d.HasMember("type"))
      return strcmp(d["type"].GetString(), "operation") == 0;
  }
return false;
}

bool IsExecuteOperation(const std::string& data)    { return data == "Execute";    }
bool IsScheduleOperation(const std::string& data)   { return data == "Schedule";   }
bool IsFileUploadOperation(const std::string& data) { return data == "FileUpload"; }
bool IsIPCOperation(const std::string& data)        { return data == "ipc";        }
bool IsStartOperation(const std::string& data)      { return data == "start";      }
bool IsStopOperation (const std::string& data)      { return data == "stop";       }
bool IsAppOperation  (const std::string& data)      { return data == "AppRequest"; }

static bool VerifyFlatbuffer(const uint8_t* buffer, const uint32_t& size)
{
  flatbuffers::Verifier verifier{buffer, size};
  return VerifyMessageBuffer(verifier);
}

bool IsPing(const std::string& data)
{
  return (data == "253");
}

/**
 * @brief Get the Decoded Message object
 *
 * @param   [in]  {shared_ptr<uint8_t*>}                          s_buffer_ptr
 * @returns [out] {Either<std::string, std::vector<std::string>>}
 */
DecodedMessage DecodeMessage(uint8_t* buffer)
{
  const auto IGTaskPayload = [](const IGData::IGTask* task)
  {
    return std::vector<std::string>{
      std::to_string(task->mask()),                      task->file_info()    ->str()    ,
                     task->time()               ->str(), task->description()  ->str()    ,
                     task->hashtags()           ->str(), task->requested_by() ->str()    ,
                     task->requested_by_phrase()->str(), task->promote_share()->str()    ,
                     task->link_bio()           ->str(), std::to_string(task->is_video()),
                     task->header()             ->str(), task->user()         ->str()};
  };
  const auto TaskPayload = [](const GenericData::GenericTask* task)
  {
    return std::vector<std::string>{
      std::to_string(task->mask()),                      task->file_info()  ->str(),
                     task->time()->str(),                task->description()->str(),
      std::to_string(task->is_video()),                  task->header()     ->str(),
                     task->user()->str(), std::to_string(task->recurring()),
      std::to_string(task->notify()),                    task->runtime()    ->str()};
  };
  const auto GetMessage = [](const KData::Message* message)
  {
    const flatbuffers::Vector<uint8_t>* message_bytes = message->data();
    return std::string{message_bytes->begin(), message_bytes->end()};
  };

  const uint8_t msg_type_byte_code = *(buffer + 4);

  if (msg_type_byte_code == 0xFD)
    return left(std::to_string(msg_type_byte_code));

  else
  {
    const unsigned int byte1 = *(buffer    ) << 0x18;
    const unsigned int byte2 = *(buffer + 1) << 0x10;
    const unsigned int byte3 = *(buffer + 2) << 0x08;
    const unsigned int byte4 = *(buffer + 3) << 0x00;
    const int32_t      message_byte_size = byte1 | byte2 | byte3 | byte4;
          uint8_t      decode_buffer[message_byte_size];

    std::memcpy(decode_buffer, buffer + 5, message_byte_size);

    if (VerifyFlatbuffer(decode_buffer, message_byte_size))
    {
      switch (msg_type_byte_code)
      {
        case (0xFF):
          return right(IGTaskPayload(GetIGTask(&decode_buffer)));
        break;
        case (0xFE):
          return left(GetMessage(KData::GetMessage(&decode_buffer)));
        break;
        case (0xFC):
          return right(TaskPayload(GetGenericTask(&decode_buffer)));
        break;
      }
    }
  }
  return right(std::vector<std::string>{});
}

bool ValidateToken(User user)
{
  using Verifier = jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>;
  static const std::string private_key = ReadFileSimple(config::Security::private_key());
  static const std::string public_key  = ReadFileSimple(config::Security::public_key());
  static const std::string path        = config::Security::token_path();
  static const Verifier    verifier    = jwt::verify()
    .allow_algorithm(jwt::algorithm::es256k(public_key, private_key, "", ""))
    .with_issuer    ("kiq");
        const std::string token        = ReadFileSimple(path + '/' + user.name);
  try
  {
    const auto decoded     = jwt::decode(user.token);
    verifier.verify(decoded);
    const auto user_claim  = decoded.get_payload_claim("user");
    return ((user_claim.as_string() == user.name) && (token == user.token));
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception thrown while validating token: " <<  e.what() << std::endl;
  }
  return false;
}

namespace SystemUtils {
static const char* MAIL_HEADER{"MIME-Version: 1.0\nContent-Type: text/html\n\n"};
void SendMail(const std::string& recipient, const std::string& message, const std::string& subject)
{
  const std::string exec_s{"echo -e 'FROM: " + config::Email::admin() + "\nSubject: " + subject + "\n" + MAIL_HEADER +
                        StringUtils::SanitizeSingleQuotes(message) + "'|" + config::Email::command() + ' ' + recipient};
  std::system(exec_s.c_str());
}
} // namespace SystemUtils

namespace FileUtils {

bool CreateDirectory(const char *dir_name) {
  std::error_code err{};
  std::filesystem::create_directory(dir_name, err);
  if (!err.value())
    return true;
  return false;
}

void SaveFile(const std::vector<char>& bytes, const char *filename) {
  std::ofstream output(filename,
                       std::ios::binary | std::ios::out | std::ios::app);
  const char* raw_data = bytes.data();
  for (size_t i = 0; i < bytes.size(); i++) {
    output.write(const_cast<const char *>(&raw_data[i]), 1);
  }
  output.close();
}

void SaveFile(uint8_t *bytes, int size, const std::string& filename) {
  std::ofstream output(filename.c_str(),
                       std::ios::binary | std::ios::out | std::ios::app);
  for (int i = 0; i < size; i++) {
    output.write((const char *)(&bytes[i]), 1);
  }
  output.close();
}

std::string SaveEnvFile(const std::string& env_file_string, const std::string& unique_id) {
  const std::string relative_directory{"data/" + unique_id};
  const std::string relative_path{relative_directory + "/v.env"};
  const std::string task_directory{GetExecutableCWD() + "/" + relative_directory};
  if (!std::filesystem::exists(task_directory))
    CreateDirectory(task_directory.c_str());
  const std::string filename{task_directory + "/v.env"};
  std::ofstream out{filename.c_str()};
  out << env_file_string;
  return relative_path;
}

void saveFopenFile(std::vector<char> bytes, const char *filename) {
  std::ofstream output(filename,
                       std::ios::binary | std::ios::out | std::ios::app);
  char *raw_data = bytes.data();
  for (size_t i = 0; i < bytes.size(); i++) {
    output.write(const_cast<const char *>(&raw_data[i]), 1);
  }
  output.close();
}

void SaveFile(const std::string& env_file_string, const std::string& env_file_path) {
  std::ofstream out{env_file_path.c_str(), (std::ios::trunc | std::ios::out | std::ios::binary)};
  out << env_file_string;
}

static void remove_double_quotes(std::string& s)
{
  s.erase(
    std::remove(s.begin(), s.end(),'\"'),
    s.end()
  );
}


static void trim_outer_whitespace(std::string& s)
{
  if (s.front() == ' ') s.erase(s.begin());
  if (s.back()  == ' ') s.pop_back();
}

static std::string trim_outer_quotes(std::string& s)
{
  if (s.front() == '"') s.erase(s.begin());
  if (s.back()  == '"') s.pop_back();
  return s;
}

static std::string SanitizeToken(std::string& s)
{
  remove_double_quotes(s);
  trim_outer_whitespace(s);
  return s;
}


std::string ReadEnvFile(const std::string& env_file_path, bool relative_path)
{
  const std::string       full_path = (relative_path) ? GetCWD() + "/" + env_file_path : env_file_path;
  const std::ifstream     file_stream{full_path};
        std::stringstream env_file_stream{};
  env_file_stream << file_stream.rdbuf();

  return env_file_stream.str();
}

std::string ReadRunArgs(const std::string& env_file_path)
{
  static const std::string token_key{"R_ARGS="};
  std::string run_arg_s{};
  std::string env = ReadEnvFile(env_file_path);
  if (!env.empty())
  {
    auto start = env.find(token_key);
    if (start != std::string::npos)
    {
      auto sub_s = env.substr(start);
      auto end   = sub_s.find_first_of(ARGUMENT_SEPARATOR);
      run_arg_s  = sub_s.substr(token_key.size(), end);
    }
  }
  return SanitizeToken(run_arg_s);
}

std::string ReadFile(const std::string& env_file_path) {
    std::ifstream file_stream{env_file_path};
    std::stringstream env_file_stream{};
    env_file_stream << file_stream.rdbuf();
    std::string return_s = env_file_stream.str();
    if (return_s.back() == '\n')
      return return_s.substr(0, return_s.size() - 1);
    return return_s;
}

std::string CreateEnvFile(std::unordered_map<std::string, std::string>&& key_pairs)
{
  const std::string SHEBANG{"#!/usr/bin/env bash\n"};
  std::string       environment_file{SHEBANG};

  for (const auto& [key, value] : key_pairs)
  {
    environment_file += key + "=\"" + value + '\"' + ARGUMENT_SEPARATOR + '\n';
  }

  return environment_file;
}

std::string ReadEnvToken(const std::string& env_file_path, const std::string& token_key, bool is_json)
{
  std::string run_arg_s{};
  std::string env = ReadEnvFile(env_file_path);
  if (!env.empty())
  {
    auto start = env.find('\n' + token_key);
    if (start != std::string::npos)
    {
      auto sub_s = env.substr(start + token_key.size() + 2);
      auto end   = sub_s.find_first_of(ARGUMENT_SEPARATOR);
      run_arg_s  = sub_s.substr(0, end);
    }
  }
  return (is_json) ? trim_outer_quotes(run_arg_s) : SanitizeToken(run_arg_s);
}

bool WriteEnvToken(const std::string& env_file_path, const std::string& token_key, const std::string& token_value) {
  std::string env = ReadEnvFile(env_file_path);
  if (!env.empty())
  {
    auto key_index = env.find(token_key);
    if (key_index != std::string::npos)
    {
      auto start_index = key_index + token_key.size() + 1;
      auto rem_s       = env.substr(start_index);
      auto end_index   = rem_s.find_first_of(ARGUMENT_SEPARATOR);

      if (end_index != std::string::npos) {
        end_index += start_index;
        env.replace(start_index, end_index - start_index, token_value);
        SaveFile(env, env_file_path);
        return true;
      }
    }
  }
  return false;
}

std::vector<std::string> ExtractFlagTokens(std::string flags)
{
  static const char        token_symbol{'$'};
  std::vector<std::string> tokens{};
  auto                     delim_index = flags.find_first_of(token_symbol);
  while (delim_index != std::string::npos)
  {
    std::string       token_start = flags.substr(delim_index);
    const auto        end_index   = token_start.find_first_of(' ') - 1;
    const std::string token       = token_start.substr(1, end_index);
    tokens.push_back(token);

    if (token_start.size() >= token.size())
    {
      flags       = token_start.substr(token.size());
      delim_index = flags.find_first_of(token_symbol);
    }
    else
      break;
  }
  return tokens;
}

std::vector<std::string> ReadFlagTokens(const std::string& env_file_path, const std::string& flags)
{
  std::vector<std::string> tokens = ExtractFlagTokens(flags);
  std::vector<std::string> token_values{};
  // TODO: Do this in one pass without reading the entire environment file each time
  for (const auto& token : tokens)
    token_values.emplace_back(ReadEnvToken(env_file_path, token));
  return token_values;
}

std::vector<std::string> ReadEnvValues(const std::string& env_file_path, const std::vector<std::string>& flags) {
  std::vector<std::string> values{};
  for (const auto& flag : flags)
    values.emplace_back(ReadEnvToken(env_file_path, flag));

  return values;
}

void ClearFile(std::string file_path) {
  std::ofstream file(file_path);
}

bool CreateTaskDirectory(const std::string& unique_id) {
  std::string directory_name{"data/" + unique_id};
  return CreateDirectory(directory_name.c_str());
}
}  // namespace FileUtils

namespace StringUtils {
template <typename T>
void Split(const std::string &s, char delim, T result)
{
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim))
      *result++ = item;
}

std::vector<std::string> Split(const std::string &s, char delim)
{
    std::vector<std::string> v{};
    if (s.size())
      Split(s, delim, std::back_inserter(v));
    return v;
}

std::string Tokenize(const std::vector<std::string>& v, char delim)
{
  std::string s{};
  if (v.size())
  {
    for (const auto& arg : v)
      s += arg + delim;
    s.pop_back();
  }
  return s;
}

std::string SanitizeSingleQuotes(const std::string& s) {
  std::string o{};

  for (const char& c : s) {
    if (c == '\'')
      o += "&#39;";
    else
    if (c == '"')
      o += "&#34;";
    else
      o += c;
  }

  return o;
}

std::string DoubleSingleQuotes(const std::string& s)
{
  std::string o;
  for (const char& c : s)
    if (c == '\'')
      o += "''";
    else
      o += c;
  return o;
}

std::string SanitizeJSON(std::string s) {
  std::string o{};
  o.reserve(s.size());
  for (const char& c : s)
  {
    if (c == '\"')
      o += '"';
    else
      o += c;
  }

  return o;
}

std::string SanitizeArg(std::string s)
{
  s.erase(std::remove_if(
    s.begin(), s.end(),
    [](char c)
    {
      return c == '\"';
    }),
    s.end());

  return s;
}

std::string GenerateUUIDString()
{
  return uuids::to_string(uuids::uuid_system_generator{}());
}

std::string AlphaNumericOnly(std::string s)
{
  s.erase(std::remove_if(
    s.begin(), s.end(),
    [](char c)
    {
      return !isalnum(c);
    }),
    s.end()
  );

  return s;
}

std::string RemoveTags(std::string s)
{
  static const auto filter = [](const char& c) { return (c == '#' || c == '@' || static_cast<wchar_t>(c) == wchar_t{L'＠'}); };
  s.erase(std::remove_if(s.begin(), s.end(), filter), s.end());
  return s;
}

std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](char c) { return tolower(c);});
  return s;
}
} // namespace StringUtils

// Bit helpers

inline size_t FindNullIndex(uint8_t *data) {
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
static std::string ToBinaryString(const T &x) {
  std::stringstream ss;
  ss << std::bitset<sizeof(T) * 8>(x);
  return ss.str();
}

bool HasNthBitSet(int value, int n) {
  auto result = value & (1 << (n - 1));
  if (result) {
    return true;
  }
  return false;
}

std::string StripSQuotes(std::string s) {
  s.erase(
    std::remove(s.begin(), s.end(),'\''),
    s.end()
  );
  return s;
}

// aka isNumber
bool IsDigits(const std::string &s) {
  for (char c : s)
    if (!isdigit(c)) {
      return false;
    }
  return true;
}

namespace TimeUtils {
std::string Now()
{
  return std::to_string(UnixTime());
}

int UnixTime()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string FormatTimestamp(int unixtime)
{
  char               buf[80];
  const  std::time_t time = static_cast<std::time_t>(unixtime);
  struct tm          ts   = *(localtime(&time));
  std::strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &ts);
  return std::string{buf};
}

std::string FormatTimestamp(const std::string& unixtime)
{
  char               buf[80];
  const  std::time_t time = static_cast<std::time_t>(std::stoi(unixtime));
  struct tm          ts   = *(localtime(&time));
  std::strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &ts);
  return std::string{buf};
}

std::string DatabaseTime(const std::string& unixtime)
{
  char               buf[80];
  const  std::time_t time = static_cast<std::time_t>(std::stoi(unixtime));
  struct tm          ts   = *(localtime(&time));
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ts);
  return std::string{buf};
}

std::string time_as_today(const std::string& unixtime)
{
  const std::time_t time = static_cast<std::time_t>(stoi(unixtime));
  const std::time_t now  = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm         ts   = *localtime(&time);
  struct tm         tn   = *localtime(&now);
  ts.tm_year = tn.tm_year;
  ts.tm_mon  = tn.tm_mon;
  ts.tm_mday = tn.tm_mday;

  return std::to_string(mktime(&ts));
}
}  // namespace TimeUtils

namespace DataUtils {
template <typename T>
const std::vector<T> VAbsorb(std::vector<T>&& v, T&& u, bool to_front)
{
  std::vector<T> c_v{};
  c_v.reserve(v.size() + 1);
  if (to_front)
  {
    c_v.insert(c_v.begin(),
              std::make_move_iterator(v.begin()),
              std::make_move_iterator(v.end()));
    c_v.emplace_back(std::move(u));
  }
  else
  {
    c_v.emplace_back(std::move(u));
    c_v.insert(c_v.begin() + 1,
              std::make_move_iterator(v.begin()),
              std::make_move_iterator(v.end()));
  }
  return c_v;
}

template const std::vector<std::string> VAbsorb(std::vector<std::string>&& v, std::string&& u, bool to_front);

template <typename T>
std::vector<T>&& vector_merge(std::vector<T>&& v1, std::vector<T>&& v2)
{
 v1.insert(v1.end(), std::make_move_iterator(v2.begin()), std::make_move_iterator(v2.end()));
 return std::move(v1);
}

template std::vector<std::string>&& vector_merge(std::vector<std::string>&& v1, std::vector<std::string>&& v2);
} // ns DataUtils
} // ns kiq
