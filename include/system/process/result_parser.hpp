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

#include "executor/executor.hpp"
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
    {
      keywords += '#' + item.keywords.at(i);
    }

    std::string content = "New video has been uploaded. Hope you find it useful!\n"
      "https://youtube.com/watch?v=" + item.id + "\n\n" + keywords;

    result.data.emplace_back(
      ProcessEventData{
        .event = SYSTEM_EVENTS__PLATFORM_NEW_POST,
        .payload = std::vector<std::string>{
          m_app_name,
          item.id,
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

class ResultProcessor {
public:
ResultProcessor()
{}

ProcessParseResult process(const std::string& output, KApplication app)
{
  if (app.name == "IG Feed")
  {
    IGFeedResultParser ig_parser{app.name};
    ig_parser.read(output);
    return ig_parser.get_result();
  }

  if (app.name == "YT Feed")
  {
    YTFeedResultParser yt_parser(app.name);
    yt_parser.read(output);
    return yt_parser.get_result();
  }

  return ProcessParseResult{};
}

};
