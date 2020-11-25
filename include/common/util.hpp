#ifndef __UTIL_HPP__
#define __UTIL_HPP__
#define FLATBUFFERS_DEBUG_VERIFICATION_FAILURE

#include <codec/instatask_generated.h>
#include <codec/generictask_generated.h>
#include <codec/kmessage_generated.h>
#include <codec/uuid.h>

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

#include <system/process/executor/kapplication.hpp>

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
using namespace GenericData;

static const int              SESSION_ACTIVE   = 1;
static const int              SESSION_INACTIVE = 2;

static const std::string_view APP_NAME         = "kserver";
static       int              APP_NAME_LENGTH  = 7;

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

Either<std::string, std::vector<std::string>> getDecodedMessage(std::shared_ptr<uint8_t[]> s_buffer_ptr);

namespace SystemUtils {
  void sendMail(std::string recipient, std::string message, std::string from);
}

namespace FileUtils {
bool        createDirectory(const char *dir_name);
void        saveFile(std::vector<char> bytes, const char *filename);
void        saveFile(uint8_t *bytes, int size, std::string filename);
std::string saveEnvFile(std::string env_file_string, std::string uuid);
std::string readEnvFile(std::string env_file_path);
std::string readFile(std::string env_file_path);
void        clearFile(std::string file_path);
bool        createTaskDirectory(std::string uuid);
}  // namespace FileUtils

namespace StringUtils {
template <typename T>
void split(const std::string &s, char delim, T result);

std::vector<std::string> split(const std::string &s, char delim);
} // namespace StringUtils

// Bit helpers

size_t findNullIndex(uint8_t *data);

template <typename T>
static std::string toBinaryString(const T &x);

bool hasNthBitSet(int value, int n);
bool isdigits(const std::string &s);

namespace TimeUtils {
int unixtime();

std::string_view format_timestamp(int unixtime);

std::string format_timestamp(std::string unixtime);
}  // namespace TimeUtils

#endif  // __UTIL_HPP__