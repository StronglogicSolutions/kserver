#pragma once

#include "executor/task_handlers/task.hpp"

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

namespace kiq {
class ProcessParseInterface {
public:
virtual ~ProcessParseInterface() {}
virtual bool               read(const std::string& s) = 0;
virtual ProcessParseResult get_result() = 0;
};

class ResearchResultInterface {
public:
virtual ~ResearchResultInterface() {}
static ProcessParseResult GetNOOPResult();
}; // ResearchResultInterface

class NERResultParser : public ProcessParseInterface,
                        public ResearchResultInterface {
public:
NERResultParser(const std::string& app_name);
virtual ~NERResultParser() override;

struct NLPItem{
std::string type;
std::string value;
};

virtual bool read(const std::string& s) override;
virtual ProcessParseResult get_result() override;

private:
std::string          m_app_name;
std::vector<NLPItem> m_items;
}; // NERResultParser

struct Emotions {
float joy;
float sadness;
float surprise;
float fear;
float anger;
float disgust;
std::vector<float> GetVector() const;
std::string        str()       const;
};
struct EmoStrings {
std::string joy;
std::string sadness;
std::string surprise;
std::string fear;
std::string anger;
std::string disgust;

std::vector<std::string> GetVector() const;
std::string str() const;
};

template <typename T = Emotions>
struct Emotion {
std::vector<std::string> emotions;
T                        scores;
static Emotion<Emotions> Create(const std::vector<std::string>& v);
std::string              str() const;
};

class EmotionResultParser : public ProcessParseInterface,
                            public ResearchResultInterface {
public:
EmotionResultParser(const std::string& app_name);
virtual ~EmotionResultParser() override;


virtual bool read(const std::string& s) override;

virtual ProcessParseResult get_result() override;

private:
std::string                      m_app_name;
std::vector<Emotion<EmoStrings>> m_items;
}; // EmotionResultParser

struct Keyword
{
std::string  word;
float        score;

std::string str() const;
};
struct Sentiment
{
std::string          type;
float                score;
std::vector<Keyword> keywords;
std::vector<std::string> GetPayload() const;
static Sentiment         Create(const std::vector<std::string>& v);
std::string              str() const;
};

class SentimentResultParser : public ProcessParseInterface,
                              public ResearchResultInterface {
public:
SentimentResultParser(const std::string& app_name);

virtual ~SentimentResultParser() override;


virtual bool read(const std::string& s) override;
virtual ProcessParseResult get_result() override;

private:
std::string            m_app_name;
std::vector<Sentiment> m_items;
}; // SentimentResultParser

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
IGFeedResultParser(const std::string& app_name);

virtual ~IGFeedResultParser() override;
virtual bool read(const std::string& s) override;
virtual ProcessParseResult get_result() override;

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
YTFeedResultParser(const std::string& app_name);
virtual ~YTFeedResultParser()           override;
virtual bool read(const std::string& s) override;
virtual ProcessParseResult get_result() override;

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

class TWFeedResultParser : public ProcessParseInterface {
public:
TWFeedResultParser(const std::string& app_name);
virtual ~TWFeedResultParser() override;

virtual bool               read(const std::string& s) override;
static ProcessEventData    ReadEventData(const TWFeedItem& item, const std::string& app_name, const int32_t& event);
virtual ProcessParseResult get_result() override;

protected:
std::string             m_app_name;
std::vector<TWFeedItem> m_feed_items;
};

class TWResearchParser : public TWFeedResultParser
{
public:
TWResearchParser(const std::string& app_name);
virtual ~TWResearchParser() override;

virtual ProcessParseResult get_result() override;
};

struct TGVoteItem
{
std::string option;
int64_t     votes;
};

class PollResultParser : public ProcessParseInterface {
public:
PollResultParser();
virtual ~PollResultParser() override;
virtual bool read(const std::string& s) override;
virtual ProcessParseResult get_result() override;

private:
std::vector<TGVoteItem> m_items;
std::string             m_app_name;
};

struct KTFeedItem
{
using media_t = std::vector<std::string>;
std::string id;
std::string date;
std::string user;
std::string text;
media_t     media;
};

class KTFeedResultParser : public ProcessParseInterface {
public:
KTFeedResultParser(const std::string& app_name);
virtual ~KTFeedResultParser() final;

virtual bool               read(const std::string& s) override;
static ProcessEventData    ReadEventData(const TWFeedItem& item, const std::string& app_name, const int32_t& event);
virtual ProcessParseResult get_result() override;

protected:
std::string             m_app_name;
std::vector<KTFeedItem> m_feed_items;
};


class ResultProcessor {
public:
ResultProcessor();
template <typename T>
ProcessParseResult process(const std::string& output, const T& agent);
};

} // ns kiq
