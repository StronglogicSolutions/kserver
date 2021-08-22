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

#include "system/process/executor/kapplication.hpp"

#include "codec/rapidjson/document.h"
#include "codec/rapidjson/error/en.h"
#include "codec/rapidjson/filereadstream.h"
#include "codec/rapidjson/filewritestream.h"
#include "codec/rapidjson/pointer.h"
#include "codec/rapidjson/prettywriter.h"
#include "codec/rapidjson/stringbuffer.h"
#include "codec/rapidjson/writer.h"

#include <iostream>

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

struct KSession {
  int fd;
  int status;
  uuid id;
};

std::string GetCWD();

std::string GetExecutableCWD();

int findIndexAfter(std::string s, int pos, char c);

/**
 * JSON Tools
 */

std::string GetJSONString(std::string s);

std::string CreateMessage(const char *data, std::string args = "");

std::string CreateEvent(const char *event, int mask, std::string stdout);

std::string CreateEvent(const char *event, std::vector<std::string> args);

std::string CreateEvent(const char *event, int mask,
                        std::vector<std::string> args);
std::string CreateOperation(const char *op, std::vector<std::string> args);
std::string GetOperation(const char *data);
template<typename T>
std::string GetMessage(T data);
std::string GetEvent(std::string data);
bool IsSessionMessageEvent(std::string event);
bool IsCloseEvent(std::string event);
std::vector<std::string> GetArgs(std::string data);
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
bool IsMessage(const char *data);

bool IsOperation(const char *data);

bool IsExecuteOperation(const char *data);
bool IsScheduleOperation(const char *data);
bool IsFileUploadOperation(const char *data);
bool IsIPCOperation(const char *data);
bool IsStartOperation(const char *data);
bool IsStopOperation(const char *data);
bool IsAppOperation(const char* data);
bool IsNewSession(const char *data);
bool IsPing(uint8_t* buffer, ssize_t size);

/**
 * General
 */
using DecodedMessage = Either<std::string, std::vector<std::string>>;
DecodedMessage DecodeMessage(uint8_t* buffer);

namespace SystemUtils {
void SendMail(std::string recipient, std::string message, std::string from);
}
namespace FileUtils {
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
std::vector<uint8_t>     ReadFileAsBytes(const std::string& file_path);
void                     ClearFile(const std::string& file_path);
bool                     CreateTaskDirectory(const std::string& unique_id);
static const uint32_t PACKET_SIZE{4096};
class FileIterator
{
static const uint32_t HEADER_SIZE{4};
public:
FileIterator(const std::string& path)
: m_buffer(PrepareBuffer(std::move(ReadFileAsBytes(path)))),
  data_ptr(nullptr),
  m_bytes_read(0)
{
  if (!m_buffer.empty())
  {
    data_ptr = m_buffer.data();
    m_size   = m_buffer.size();
  }
}

static std::vector<uint8_t> PrepareBuffer (std::vector<uint8_t>&& data)
{
  const uint32_t bytes = (data.size() + HEADER_SIZE);
  std::vector<uint8_t> buffer{};
  buffer.reserve(bytes);
  buffer.emplace_back((bytes >> 24) & 0xFF);
  buffer.emplace_back((bytes >> 16) & 0xFF);
  buffer.emplace_back((bytes >> 8 ) & 0xFF);
  buffer.emplace_back((bytes      ) & 0xFF);
  buffer.insert(buffer.end(), std::make_move_iterator(data.begin()), std::make_move_iterator(data.end()));
  return buffer;
};
struct PacketWrapper
{
PacketWrapper(uint8_t* ptr_, uint32_t size_)
: ptr(ptr_),
  size(size_) {}

uint8_t* data() { return ptr; }

uint8_t* ptr;
uint32_t size;
};

bool has_data()
{
  return (data_ptr != nullptr);
}

std::string to_string()
{
  std::string data_s{};
  for (uint32_t i = 0; i < m_buffer.size(); i++) data_s += std::to_string(+(*(m_buffer.data() + i)));
  return data_s;
}

PacketWrapper next() {
  uint32_t size;
  uint32_t bytes_remaining = m_size - m_bytes_read;
  uint8_t* ptr             = data_ptr;

  if (bytes_remaining < PACKET_SIZE)
  {
    size     = bytes_remaining;
    data_ptr = nullptr;
  }
  else
  {
    size     =  PACKET_SIZE;
    data_ptr += PACKET_SIZE;
  }

  m_bytes_read += size;

  return PacketWrapper{ptr, size};
}

private:

std::vector<uint8_t> m_buffer;
uint8_t*             data_ptr;
uint32_t             m_bytes_read;
uint32_t             m_size;
};
} // namespace FileUtils

namespace StringUtils {
template <typename T>
void Split(const std::string &s, char delim, T result);
std::vector<std::string> Split(const std::string &s, char delim);
std::string sanitizeSingleQuotes(const std::string& s);
std::string SanitizeJSON(std::string s);
std::string GenerateUUIDString();
std::string AlphaNumericOnly(std::string s);
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

std::string FormatTimestamp(int unixtime);
std::string FormatTimestamp(std::string unixtime);
std::string time_as_today(std::string unixtime);
}  // namespace TimeUtils

namespace DataUtils
{
template <typename T>
const std::vector<T> vector_absorb(std::vector<T>&& v, T&& u, bool to_front = false);
template <typename T>
std::vector<T>&& vector_merge(std::vector<T>&& v1, std::vector<T>&& v2);
template <typename ...Args>
void ClearArgs(Args&& ...args);

template void ClearArgs(std::string&&);
template <typename ...Args>
void ClearArgs(Args&& ...args)
{
  (args.clear(), ...);
}

} // namespace DataUtils
