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

struct IGFeedItem {
uint32_t                 time;
uint32_t                 pk;
std::string              id;
std::string              username;
std::string              content;
std::vector<std::string> media_urls;
};

class JSONParserInterface {
public:
virtual ~JSONParserInterface() {}
virtual bool read(const std::string& s) = 0;
};



class IGFeedResultParser : public JSONParserInterface {
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
          if (k.name.GetString() == "time")
            ig_item.time = k.value.GetUint();
          else
          if (k.name.GetString() == "pk")
            ig_item.pk = k.value.GetUint();
          else
          if (k.name.GetString() == "id")
            ig_item.id = k.value.GetString();
          else
          if (k.name.GetString() == "username")
            ig_item.username = k.value.GetString();
          else
          if (k.name.GetString() == "content")
            ig_item.content = k.value.GetString();
          else
          if (k.name.GetString() == "media_urls" && k.value.IsArray())
          for (const auto& url : k.value.GetArray())
            ig_item.media_urls.emplace_back(url.GetString());
        }
      }
      m_feed_items.emplace_back(std::move(ig_item));
    }
  }
}

private:
std::vector<IGFeedItem> m_feed_items;
};