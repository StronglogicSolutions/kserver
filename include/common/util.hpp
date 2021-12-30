#pragma once

#define FLATBUFFERS_DEBUG_VERIFICATION_FAILURE

#include <codec/instatask_generated.h>
#include <codec/generictask_generated.h>
#include <codec/kmessage_generated.h>
#include <codec/uuid.h>

#include <stdio.h>
#include <filesystem>
#include <bitset>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iterator>
#include <neither/either.hpp>
#include <string>
#include <utility>
#include <map>
#include <vector>
#include <ctime>

#include "config/config_parser.hpp"
#include "system/process/executor/kapplication.hpp"

#include "codec/rapidjson/document.h"
#include "codec/rapidjson/error/en.h"
#include "codec/rapidjson/filereadstream.h"
#include "codec/rapidjson/filewritestream.h"
#include "codec/rapidjson/pointer.h"
#include "codec/rapidjson/prettywriter.h"
#include "codec/rapidjson/stringbuffer.h"
#include "codec/rapidjson/writer.h"

namespace kiq {
using namespace rapidjson;
using namespace uuids;
using namespace neither;
using namespace KData;
using namespace IGData;
using namespace GenericData;

extern const char ARGUMENT_SEPARATOR;

static const int32_t SESSION_ACTIVE   = 1;
static const int32_t SESSION_INACTIVE = 2;
static const int32_t ALL_CLIENTS      = -1;

typedef std::string                                      KOperation;
typedef std::map<int, std::string>                       CommandMap;
typedef std::vector<std::pair<std::string, std::string>> TupVec;
typedef std::vector<std::map<int, std::string>>          MapVec;
typedef std::vector<std::pair<std::string, std::string>> SessionInfo;
typedef std::vector<KApplication>                        ServerData;
typedef std::pair<std::string, std::string>              FileInfo;

static const int32_t READY_STATUS{0x01};
struct KSession {
  int32_t  fd;
  int32_t  status;
  uuid     id;
  uint32_t tx{0};
  uint32_t rx{0};
};

struct Timer {
using  TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using  Duration  = std::chrono::seconds;
static const uint32_t TEN_MINUTES    = 600;
static const uint32_t TWENTY_MINUTES = 1200;
Timer(const int64_t duration_ = TWENTY_MINUTES);
bool active() const;
bool expired() const;
void start();
void stop();

private:
TimePoint time_point;
bool      timer_active;
int64_t   duration;
};

std::string GetCWD();
std::string GetExecutableCWD();

/**
 * JSON Tools
 */
std::string ToJSONArray         (const std::vector<std::string>&);
std::string CreateMessage       (const char *data, std::string args = "");
std::string CreateEvent         (const std::string& event, int mask, std::string stdout);
std::string CreateEvent         (const std::string& event, std::vector<std::string> args);
std::string CreateEvent         (const std::string& event, int mask, std::vector<std::string> args);
std::string CreateOperation     (const char *op, std::vector<std::string> args);
std::string GetOperation        (const std::string& data);
template<typename T>
std::string GetMessage          (T data);
std::string GetEvent            (std::string data);
bool IsSessionMessageEvent      (std::string event);
bool IsCloseEvent               (std::string event);
std::vector<std::string> GetJSONArray(const std::string& s);
std::vector<std::string> GetArgs(const std::string& data);
std::vector<std::string> GetArgs(const char* data);
CommandMap GetArgMap(const char *data);

std::string CreateSessionEvent(
    int status, std::string message = "",
    std::vector<std::pair<std::string, std::string>> args = {});

std::string CreateMessage(
    const char *data, std::vector<std::pair<std::string, std::string>> args);

std::string CreateMessage(const char *data,
                          std::map<int, std::string> map = {});

std::string CreateMessage(const char *data, std::vector<KApplication> commands);

std::string CreateMessage(const char *data,
                          std::map<int, std::vector<std::string>> map = {});

/**
 * Operations
 */
bool IsMessage            (const std::string& data);
bool IsOperation          (const std::string& data);
bool IsExecuteOperation   (const std::string& data);
bool IsScheduleOperation  (const std::string& data);
bool IsFileUploadOperation(const std::string& data);
bool IsIPCOperation       (const std::string& data);
bool IsStartOperation     (const std::string& data);
bool IsStopOperation      (const std::string& data);
bool IsAppOperation       (const std::string& data);
bool IsPing               (const std::string& data);

/**
 * General
 */
using DecodedMessage = Either<std::string, std::vector<std::string>>;
DecodedMessage DecodeMessage(uint8_t* buffer);

namespace SystemUtils {
void SendMail(const std::string& recipient, const std::string& message, const std::string& subject = "KIQ Notification");
}
namespace FileUtils
{
bool                     CreateDirectory(const char *dir_name);
void                     SaveFile(uint8_t *bytes, int size, const std::string& filename);
void                     SaveFile(     const std::vector<char>& bytes, const char* filename);
void                     SaveFile(     const std::string& env_file_string, const std::string& env_file_path);
std::string              SaveEnvFile(  const std::string& env_file_string, const std::string& unique_id);
std::string              ReadEnvFile(  const std::string& env_file_path, bool relative_path = false);
std::string              ReadRunArgs(  const std::string& env_file_path);
std::string              ReadEnvToken( const std::string& env_file_path, const std::string& token_key);
bool                     WriteEnvToken(const std::string& env_file_path,
                                       const std::string& token_key,
                                       const std::string& token_value);
std::vector<std::string> ExtractFlagTokens(std::string);
std::vector<std::string> ReadFlagTokens(const std::string& env_file_path, const std::string& flags);
std::vector<std::string> ReadEnvValues(const std::string& env_file_path, const std::vector<std::string>& flags);
std::string              CreateEnvFile(std::unordered_map<std::string, std::string>&& key_pairs);
std::string              ReadFile( const std::string& env_file_path);
void                     ClearFile(const std::string& file_path);
bool                     CreateTaskDirectory(const std::string& unique_id);
} // namespace FileUtils

namespace StringUtils {
template <typename T>
void Split(const std::string &s, char delim, T result);
std::vector<std::string> Split(const std::string &s, char delim);
std::string Tokenize(const std::vector<std::string>& v, char delim = ' ');
std::string DoubleSingleQuotes(const std::string& s);
std::string SanitizeSingleQuotes(const std::string& s);
std::string SanitizeArg(std::string s);
std::string SanitizeJSON(std::string s);
std::string GenerateUUIDString();
std::string AlphaNumericOnly(std::string s);
std::string RemoveTags(std::string s);
std::string ToLower(std::string s);
} // namespace StringUtils

// Bit helpers
size_t FindNullIndex(uint8_t *data);

template <typename T>
static std::string ToBinaryString(const T &x);
bool HasNthBitSet(int value, int n);
bool IsDigits(const std::string &s);
std::string StripSQuotes(std::string s);
std::string SanitizeToken(std::string s);

namespace TimeUtils {
int UnixTime();
std::string Now();
std::string FormatTimestamp(int unixtime);
std::string FormatTimestamp(std::string unixtime);
std::string time_as_today(std::string unixtime);
}  // namespace TimeUtils

namespace DataUtils
{
template <typename T>
const std::vector<T> VAbsorb(std::vector<T>&& v, T&& u, bool to_front = false);
template <typename T>
std::vector<T>&& vector_merge(std::vector<T>&& v1, std::vector<T>&& v2);
template <typename ...Args>
void ClearArgs(Args&& ...args);

template void ClearArgs(std::string&&);
template <typename... Args>
void ClearArgs(Args&&... args)
{
  (args.clear(), ...);
}

template <typename ...Args>
bool          NoEmptyArgs(Args&& ...args);
template bool NoEmptyArgs(std::string&&);
template <typename... Args>
bool          NoEmptyArgs(Args&&... args)
{
  for (const auto& arg : {args...})
    if (arg.empty())
      return false;
  return true;
}
template <typename... Args>
bool ClearIfNoEmpty(Args&& ...args);
bool ClearIfNoEmpty(std::string&&);
template <typename... Args>
bool ClearIfNoEmpty(Args&&... args)
{
  return ((args.empty(), ...));
}
} // ns DataUtils
} // ns kiq
