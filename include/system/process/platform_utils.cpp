#include "platform.hpp"
#include <logger.hpp>

namespace kiq {
using namespace log;
std::string TaskParserInterface::Content::value() const
{
  return header       + '\n'     + description   + "\n\n\n" + requested_by_phrase + ' '  +
         requested_by + "\n\n\n" + promote_share + "\n\n"   + link_bio  + "\n\n"  + hashtags;
}
//----------------------------
TaskParser::TaskParser(const Task& task, std::string_view pid)
{
  arg_map = arg_map_t{
    { DESCRIPTION_KEY,         [this] (auto arg) { content_.description         = arg; } },
    { HEADER_KEY,              [this] (auto arg) { content_.header              = arg; } },
    { HASHTAGS_KEY,            [this] (auto arg) { content_.hashtags            = arg; } },
    { LINK_BIO_KEY,            [this] (auto arg) { content_.link_bio            = arg; } },
    { REQUESTED_BY_KEY,        [this] (auto arg) { content_.requested_by        = arg; } },
    { REQUESTED_BY_PHRASE_KEY, [this] (auto arg) { content_.requested_by_phrase = arg; } },
    { PROMOTE_SHARE_KEY,       [this] (auto arg) { content_.promote_share       = arg; } },
    { USER_KEY,                [this] (auto arg) { post_.user                   = arg; } },
    { FILE_TYPE_KEY,           [this] (auto arg) {                                     } },
    { DIRECT_MESSAGE_KEY,      [this] (auto arg) {                                     } }
  };

  klog().i("Parsing task:\n{}", task.toString());
  post_.id     = task.id();
  post_.time   = task.datetime;
  post_.name   = task.name;
  post_.repost = "true";
  post_.pid    = pid;
}
//-----------------------------
void TaskParser::parse(const std::string& flag, const std::string& arg)
{
  arg_map[flag](arg);
}
//-----------------------------
void TaskParserInterface::add_file(const std::string& file)
{
  post_.urls += "file://" + file + '>';
}
//-----------------------------
PlatformPost TaskParserInterface::get()
{
  post_.content = content_.value();
  return post_;
}
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
std::string SavePlatformEnvFile(const post_t& post)
{
  using namespace FileUtils;
  using Map = std::unordered_map<std::string, std::string>;
  const std::string directory_name{post.time + post.id + post.pid};
  const std::string filename      {directory_name};

  CreateTaskDirectory(directory_name);

  return SaveEnvFile(CreateEnvFile(
    Map{{"content", post.content},
        {"urls",    post.urls},
        {"args",    post.args}}), filename);
}
//-------------------------------------------------------------------------------------
std::string
GetEnvPath(const std::string& id, const std::string& time, const std::string& pid)
{
  return "data/" + time + id + pid + "/v.env";
}
//-------------------------------------------------------------------------------------
std::string
GetArgsOperation(const std::string& path)
{
  return CreateOperation("Bot", GetJSONArray(FileUtils::ReadEnvToken(path, "args", true)));
}
//-------------------------------------------------------------------------------------
bool PopulatePlatformPost(PlatformPost& post)
{
  using namespace FileUtils;
  const std::string              env_path    = GetEnvPath(post.id, post.time, post.pid);
  const std::vector<std::string> post_values = ReadEnvValues(env_path, PLATFORM_ENV_KEYS);
  const size_t                   size        = post_values.size();
  const bool                     has_args    = (size == 3);
  if (size > 1)
  {
    post.content = post_values.at(constants::PLATFORM_POST_CONTENT_INDEX);
    post.urls    = post_values.at(constants::PLATFORM_POST_URL_INDEX);
    post.args    = (has_args) ? GetArgsOperation(env_path) : "";
    return true;
  }

  return false;
}
//-------------------------------------------------------------------------------------
bool MustRepost(const std::string& s) { return (s == "1" || s == "true" || s == "t" ); }
} // ns kiq
