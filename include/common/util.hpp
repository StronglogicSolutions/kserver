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

std::string get_cwd();

std::string get_executable_cwd();

int findIndexAfter(std::string s, int pos, char c);

/**
 * JSON Tools
 */

std::string getJsonString(std::string s);

std::string createMessage(const char *data, std::string args = "");

std::string createEvent(const char *event, int mask, std::string stdout);

std::string createEvent(const char *event, std::vector<std::string> args);

std::string createEvent(const char *event, int mask,
                        std::vector<std::string> args);

std::string createOperation(const char *op, std::vector<std::string> args);

std::string getOperation(const char *data);

template<typename T>
std::string getMessage(T data);

std::string getEvent(std::string data);

bool isSessionMessageEvent(std::string event);

bool isCloseEvent(std::string event);

std::vector<std::string> getArgs(std::string data);

std::vector<std::string> getArgs(const char* data);

CommandMap getArgMap(const char *data);

std::string createSessionEvent(
    int status, std::string message = "",
    std::vector<std::pair<std::string, std::string>> args = {});

std::string createMessage(
    const char *data, std::vector<std::pair<std::string, std::string>> args);

std::string createMessage(const char *data,
                          std::map<int, std::string> map = {});

std::string createMessage(const char *data, std::vector<KApplication> commands);

std::string createMessage(const char *data,
                          std::map<int, std::vector<std::string>> map = {});

/**
 * Operations
 */
bool isMessage(const char *data);

bool isOperation(const char *data);

bool isExecuteOperation(const char *data);
bool isScheduleOperation(const char *data);

bool isFileUploadOperation(const char *data);

bool isIPCOperation(const char *data);
bool isStartOperation(const char *data);
bool isStopOperation(const char *data);
bool isAppOperation(const char* data);

bool isNewSession(const char *data);

bool isPing(uint8_t* buffer, ssize_t size);

/**
 * General
 */
using DecodedMessage = Either<std::string, std::vector<std::string>>;
DecodedMessage DecodeMessage(uint8_t* buffer);

namespace SystemUtils {
  void sendMail(std::string recipient, std::string message, std::string from);
}

namespace FileUtils {
bool                     createDirectory(const char *dir_name);
void                     saveFile(uint8_t *bytes, int size, const std::string& filename);
void                     saveFile(     const std::vector<char>& bytes, const char* filename);
void                     saveFile(     const std::string& env_file_string, const std::string& env_file_path);
std::string              saveEnvFile(  const std::string& env_file_string, const std::string& unique_id);
std::string              readEnvFile(  const std::string& env_file_path, bool relative_path = false);
std::string              readRunArgs(  const std::string& env_file_path);
std::string              readEnvToken( const std::string& env_file_path, const std::string& token_key);
bool                     writeEnvToken(const std::string& env_file_path,
                                       const std::string& token_key,
                                       const std::string& token_value);
std::vector<std::string> extractFlagTokens(std::string);
std::vector<std::string> readFlagTokens(const std::string& env_file_path, const std::string& flags);
std::vector<std::string> readEnvValues(const std::string& env_file_path, const std::vector<std::string>& flags);
std::string              createEnvFile(std::unordered_map<std::string, std::string>&& key_pairs);
std::string              readFile( const std::string& env_file_path);
std::vector<uint8_t>     readFileAsBytes(const std::string& file_path);
void                     clearFile(const std::string& file_path);
bool                     createTaskDirectory(const std::string& unique_id);
static const uint32_t PACKET_SIZE{4096};
class FileIterator
{
public:
FileIterator(const std::string& path)
: m_buffer(readFileAsBytes(path)),
  data_ptr(nullptr)
{
  if (!m_buffer.empty())
  {
    data_ptr = m_buffer.data();
    m_size   = m_buffer.size();
  }
}

struct PacketWrapper
{
  PacketWrapper(uint8_t* ptr_, uint32_t size_)
  : ptr(ptr_),
    size(size_) {}
  uint8_t* ptr;
  uint32_t size;

  uint8_t* data() { return ptr; }
};

bool has_data()
{
  return data_ptr != nullptr;
}

PacketWrapper next() {
  uint8_t* ptr = data_ptr;
  uint32_t bytes_remaining = m_size - m_bytes_read;
  uint32_t size{};
  if (bytes_remaining < PACKET_SIZE)
  {
    size = bytes_remaining;
    data_ptr = nullptr;
  }
  else
  {
    size      = PACKET_SIZE;
    data_ptr += PACKET_SIZE;
  }
    return PacketWrapper{ptr, size};
}

private:

std::vector<uint8_t> m_buffer;
uint8_t*             data_ptr;
uint32_t             m_bytes_read;
uint32_t             m_size;
};
}  // namespace FileUtils

namespace StringUtils {
template <typename T>
void split(const std::string &s, char delim, T result);
std::vector<std::string> split(const std::string &s, char delim);
std::string sanitizeSingleQuotes(const std::string& s);
std::string SanitizeJSON(std::string s);
std::string generate_uuid_string();
std::string AlphaNumericOnly(std::string s);
} // namespace StringUtils

// Bit helpers

size_t findNullIndex(uint8_t *data);

template <typename T>
static std::string toBinaryString(const T &x);

bool hasNthBitSet(int value, int n);
bool isdigits(const std::string &s);
std::string stripSQuotes(std::string s);
std::string sanitize_token(std::string s);

namespace TimeUtils {
int unixtime();

std::string format_timestamp(int unixtime);
std::string format_timestamp(std::string unixtime);
std::string time_as_today(std::string unixtime);
}  // namespace TimeUtils

namespace DataUtils
{
template <typename T>
const std::vector<T> vector_absorb(std::vector<T>&& v, T&& u, bool to_front = false);
template <typename ...Args>
void ClearArgs(Args&& ...args);

template void ClearArgs(std::string&&);
template <typename ...Args>
void ClearArgs(Args&& ...args)
{
  (args.clear(), ...);
}

} // namespace DataUtils
