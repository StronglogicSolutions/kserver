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

std::string getMessage(const char *data);

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

/**
 * General
 */
using DecodedMessage = Either<std::string, std::vector<std::string>>;
DecodedMessage DecodeMessage(const std::shared_ptr<uint8_t[]>& s_buffer_ptr);

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
void                     clearFile(const std::string& file_path);
bool                     createTaskDirectory(const std::string& unique_id);
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
std::string stripDQuotes(std::string s);

namespace TimeUtils {
int unixtime();

std::string format_timestamp(int unixtime);
std::string format_timestamp(std::string unixtime);
std::string time_as_today(std::string unixtime);
}  // namespace TimeUtils
