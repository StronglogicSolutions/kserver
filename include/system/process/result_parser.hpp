#pragma once

#include <string>
#include <vector>

#include "executor/executor.hpp"
#include "server/types.hpp"

#include "codec/rapidjson/document.h"
#include "codec/rapidjson/error/en.h"
#include "codec/rapidjson/filereadstream.h"
#include "codec/rapidjson/filewritestream.h"
#include "codec/rapidjson/pointer.h"
#include "codec/rapidjson/prettywriter.h"
#include "codec/rapidjson/stringbuffer.h"
#include "codec/rapidjson/writer.h"

static std::string url_string(const std::vector<std::string> urls)
{
  std::string delim{};
  std::string result{};

  for (const auto& url : urls)
  {
    result += delim + url;
    delim = "|";
  }

  return result;
}

struct IGFeedItem {
uint32_t                 time;
uint32_t                 pk;
std::string              id;
std::string              username;
std::string              content;
std::vector<std::string> media_urls;
};

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


class IGFeedResultParser : public ProcessParseInterface {
public:
virtual ~IGFeedResultParser() override {}

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
            ig_item.time = k.value.GetUint();
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
          if (strcmp(k.name.GetString(), "media_urls") == 0 && k.value.IsArray())
          for (const auto& url : k.value.GetArray())
            ig_item.media_urls.emplace_back(url.GetString());
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

  for (const auto& item : m_feed_items)
  {
    if (true || "Not in database")
    {
      result.data.emplace_back(
        ProcessEventData{
          .event = SYSTEM_EVENTS__PLATFORM_NEW_POST,
          .payload = std::vector<std::string>{
            item.id,
            item.content,
            item.username,
            std::to_string(item.time),
            url_string(item.media_urls)
          }
        }
      );
    }
  }
  return result;
}

private:
std::vector<IGFeedItem> m_feed_items;
};

class ResultProcessor {
public:
ResultProcessor()
{}

ProcessParseResult process(const std::string& output, KApplication app)
{
  if (app.name == "Instagram")
  {
    IGFeedResultParser ig_parser{};
    ig_parser.read(output);
    return ig_parser.get_result();
  }

  return ProcessParseResult{};
}

};
