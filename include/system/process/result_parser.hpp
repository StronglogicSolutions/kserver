#pragma once

#include <string>
#include <vector>

#include "codec/rapidjson/document.h"
#include "codec/rapidjson/error/en.h"
#include "codec/rapidjson/filereadstream.h"
#include "codec/rapidjson/filewritestream.h"
#include "codec/rapidjson/pointer.h"
#include "codec/rapidjson/prettywriter.h"
#include "codec/rapidjson/stringbuffer.h"
#include "codec/rapidjson/writer.h"

#include "server/types.hpp"
#include "log/logger.h"


static std::string url_string(const std::vector<std::string> urls)
{
  std::string delim{};
  std::string result{};

  for (const auto& url : urls)
  {
    result += delim + url;
    delim = ">";
  }

  return result;
}

struct ProcessEventData {
int32_t                  event;
std::vector<std::string> payload;
};

struct ProcessParseResult {
std::vector<ProcessEventData> data;
};


class ProcessParseInterface {
public:
virtual ~ProcessParseInterface() {}
virtual bool               read(const std::string& s) = 0;
virtual ProcessParseResult get_result() = 0;
};

class KNLPResultParser : public ProcessParseInterface {
public:
KNLPResultParser(const std::string& app_name)
: m_app_name(app_name)
{}

virtual ~KNLPResultParser() {}

struct NLPItem{
std::string type;
std::string value;
};

virtual bool read(const std::string& s) override
{
  using namespace rapidjson;
  Document d{};
  d.Parse(s.c_str());
  if (!d.IsNull() && d.IsArray())
  {
    for (const auto& item : d.GetArray())
    {
      NLPItem nlp_item{};
      if (item.IsObject())
      {
        for (const auto& k : item.GetObject())
        {
          if (strcmp(k.name.GetString(), "type") == 0)
            nlp_item.type = k.value.GetString();
          else
          if (strcmp(k.name.GetString(), "value") == 0)
            nlp_item.value = k.value.GetString();
        }
      }
      m_items.emplace_back(std::move(nlp_item));
    }
    return true;
  }
  return false;
}

virtual ProcessParseResult get_result() override
{
  ProcessParseResult result{};

  KLOG("Returning {} NLP tokens", m_items.size());

  for (const auto& item : m_items)
  {
    result.data.emplace_back(
      ProcessEventData{
        .event = SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT,
        .payload = std::vector<std::string>{
          m_app_name,
          item.type,
          item.value
        }
      }
    );
  }

  return result;
}

private:
std::string m_app_name;
std::vector<NLPItem> m_items;
};

/**
 *
 * INSTAGRAM
 *
 * @brief Instagram Feed Items
 *
 */
struct IGFeedItem {
std::string              time;
uint32_t                 pk;
std::string              id;
std::string              username;
std::string              content;
std::vector<std::string> media_urls;
};

class IGFeedResultParser : public ProcessParseInterface {
public:
IGFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

virtual ~IGFeedResultParser() override {}
/**
 * @brief
 *
 * @param s
 * @return true
 * @return false
 *
 * NOTE: We only take the first media item (the full sized one, from Instagram)
 */
virtual bool read(const std::string& s) {
  using namespace rapidjson;
  Document d{};
  d.Parse(s.c_str());
  if (!d.IsNull() && d.IsArray())
  {
    for (const auto& item : d.GetArray())
    {
      IGFeedItem ig_item{};
      if (item.IsObject())
      {
        for (const auto& k : item.GetObject())
        {
          if (strcmp(k.name.GetString(), "time") == 0)
            ig_item.time = k.value.GetString();
          else
          if (strcmp(k.name.GetString(), "pk") == 0)
            ig_item.pk = k.value.GetUint();
          else
          if (strcmp(k.name.GetString(), "id") == 0)
            ig_item.id = k.value.GetString();
          else
          if (strcmp(k.name.GetString(), "username") == 0)
            ig_item.username = k.value.GetString();
          else
          if (strcmp(k.name.GetString(), "content") == 0)
            ig_item.content = k.value.GetString();
          else
          if (strcmp(k.name.GetString(), "urls") == 0 && k.value.IsArray() && !k.value.Empty())
            ig_item.media_urls.emplace_back(k.value.GetArray()[0].GetString()); // TODO: Confirm we only want first
        }
      }
      m_feed_items.emplace_back(std::move(ig_item));
    }
    return true;
  }
  return false;
}

virtual ProcessParseResult get_result() override {
  ProcessParseResult result{};

  KLOG("Returning {} IG Feed items", m_feed_items.size());

  for (const auto& item : m_feed_items)
  {
    result.data.emplace_back(
      ProcessEventData{
        .event = SYSTEM_EVENTS__PLATFORM_NEW_POST,
        .payload = std::vector<std::string>{
          m_app_name,
          item.id,
          item.username,
          item.time,
          item.content,
          url_string(item.media_urls),
          constants::SHOULD_REPOST,
          constants::PLATFORM_PROCESS_METHOD
        }
      }
    );
  }

  return result;
}

private:
std::vector<IGFeedItem> m_feed_items;
std::string             m_app_name;
};

/**
 *
 * YOUTUBE
 *
 * @brief YouTube Feed Items
 *
 */

struct YTFeedItem {
std::string              id;
std::string              channel_id;
std::string              datetime;
std::string              title;
std::string              description;
std::vector<std::string> keywords;
};

class YTFeedResultParser : public ProcessParseInterface {
public:
YTFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

virtual ~YTFeedResultParser() override {}

virtual bool read(const std::string& s) {
  using namespace rapidjson;

  Document d{};
  d.Parse(s.c_str());
  if (!d.IsNull() && d.IsArray())
  {

    for (const auto& item : d.GetArray())
    {
      YTFeedItem yt_item{};
      if (!item.IsObject())
        continue;

      for (const auto& k : item.GetObject())
      {
        if (strcmp(k.name.GetString(), "channel_id") == 0)
          yt_item.channel_id = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "datetime") == 0)
          yt_item.datetime = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "id") == 0)
          yt_item.id = k.value.GetString();  // Added app_name to make it more unique
        else
        if (strcmp(k.name.GetString(), "description") == 0)
          yt_item.description = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "keywords") == 0 &&
          k.value.IsArray())
        {
          for (const auto& keyword : k.value.GetArray())
            yt_item.keywords.emplace_back(keyword.GetString());
        }
        else
        if (strcmp(k.name.GetString(), "title") == 0)
          yt_item.title = k.value.GetString();
      }

      m_feed_items.emplace_back(std::move(yt_item));
    }
    return true;
  }
  return false;
}

virtual ProcessParseResult get_result() override {
  ProcessParseResult result{};

  KLOG("Returning {} YT Feed items", m_feed_items.size());

  for (const auto& item : m_feed_items)
  {
    std::string keywords{};
    const uint8_t max_keywords = item.keywords.size() > 8 ? 8 : item.keywords.size();

    for (uint8_t i = 0; i < max_keywords; i++)
      keywords += " #" + item.keywords.at(i);


    std::string content = "New video has been uploaded. Hope you find it useful!\n"
      "https://youtube.com/watch?v=" + item.id + "\n\n" + keywords;

    result.data.emplace_back(
      ProcessEventData{
        .event = SYSTEM_EVENTS__PLATFORM_NEW_POST,
        .payload = std::vector<std::string>{
          m_app_name,
          item.id,
          "DEFAULT_USER",
          item.datetime,
          content,
          {},
          constants::SHOULD_REPOST,
          constants::PLATFORM_PROCESS_METHOD
        }
      }
    );
  }

  return result;
}

private:
std::vector<YTFeedItem> m_feed_items;
std::string             m_app_name;
};

/**
 * TWITTER
 *
 * @brief Twitter Feed Items
 *
 */
struct TWFeedItem
{
std::string id;
std::string time;
std::string date;
std::string hashtags;
std::string mentions;
std::string user;
std::string likes;
std::string retweets;
std::string content;
std::string media_urls;
};

class TWFeedResultParser : public ProcessParseInterface
{
public:
TWFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

virtual ~TWFeedResultParser() override {}

virtual bool read(const std::string& s)
{
  using namespace rapidjson;

  Document d{};
  d.Parse(s.c_str());
  if (!d.IsNull() && d.IsArray())
  {

    for (const auto& item : d.GetArray())
    {
      TWFeedItem tw_item{};
      if (!item.IsObject())
        continue;

      for (const auto& k : item.GetObject())
      {
        if (strcmp(k.name.GetString(), "id") == 0)
          tw_item.id = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "content") == 0)
          tw_item.content = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "date") == 0)
          tw_item.date = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "time") == 0)
          tw_item.time = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "user") == 0)
          tw_item.user = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "likes") == 0)
          tw_item.likes = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "retweets") == 0)
          tw_item.retweets = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "hashtags") == 0)
          tw_item.hashtags = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "mentions") == 0)
          tw_item.mentions = k.value.GetString();
        else
        if (strcmp(k.name.GetString(), "urls") == 0)
          tw_item.media_urls = k.value.GetString();
      }

      m_feed_items.emplace_back(std::move(tw_item));
    }
    return true;
  }
  return false;
}

static ProcessEventData ReadEventData(const TWFeedItem& item, const std::string& app_name, const int32_t& event)
{
  std::string content{};

  if (!item.mentions.empty())
    content += item.mentions + '\n';
  content += item.content;
  if (!item.hashtags.empty())
    content += '\n' + item.hashtags;
  content += "\nPosted:   " + item.date;
  content += "\nLikes:    " + item.likes;
  content += "\nRetweets: " + item.retweets;

  return ProcessEventData{
    .event = event,
    .payload = std::vector<std::string>{
      app_name,
      item.id,
      item.user,
      item.time,
      content,
      item.media_urls,
      constants::SHOULD_REPOST,
      constants::PLATFORM_PROCESS_METHOD
    }
  };
}

virtual ProcessParseResult get_result() override
{
  ProcessParseResult result{};
  KLOG("Returning {} TW Feed items", m_feed_items.size());
  for (const auto& item : m_feed_items)
    result.data.emplace_back(TWFeedResultParser::ReadEventData(item, m_app_name, SYSTEM_EVENTS__PLATFORM_NEW_POST));

  return result;
}

protected:
std::string             m_app_name;
std::vector<TWFeedItem> m_feed_items;
};

class TWResearchParser : public TWFeedResultParser
{
public:
TWResearchParser(const std::string& app_name)
: TWFeedResultParser{app_name}
{}

virtual ~TWResearchParser() override {}

  virtual ProcessParseResult get_result() override
  {
    ProcessParseResult result{};
    KLOG("Returning {} TW Feed items", m_feed_items.size());
    for (const auto& item : m_feed_items)
      result.data.emplace_back(TWFeedResultParser::ReadEventData(item, m_app_name, SYSTEM_EVENTS__PROCESS_RESEARCH));

    return result;
  }
};

class ResultProcessor {
public:
ResultProcessor()
{}

ProcessParseResult process(const std::string& output, KApplication app)
{
  ProcessParseResult result;

  if (app.is_kiq)
  {
    std::unique_ptr<ProcessParseInterface> u_parser_ptr;
    if (app.name == "IG Feed")
      u_parser_ptr.reset(new IGFeedResultParser{app.name});
    else
    if (app.name == "YT Feed")
      u_parser_ptr.reset(new YTFeedResultParser{app.name});
    else
    if (app.name == "TW Feed" || app.name == "TW Search")
      u_parser_ptr.reset(new TWFeedResultParser{app.name});
    else
    if (app.name == "TW Research")
      u_parser_ptr.reset(new TWResearchParser{app.name});
    else
    if (app.name == "KNLP")
      u_parser_ptr.reset(new KNLPResultParser{app.name});
    u_parser_ptr->read(output);

    result = u_parser_ptr->get_result();
  }

  return result;
}

};
