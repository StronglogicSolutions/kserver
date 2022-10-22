
#include "result_parser.hpp"

static const uint8_t JOY_INDEX      = 0x00;
static const uint8_t SADNESS_INDEX  = 0x01;
static const uint8_t SURPRISE_INDEX = 0x02;
static const uint8_t FEAR_INDEX     = 0x03;
static const uint8_t ANGER_INDEX    = 0x04;
static const uint8_t DISGUST_INDEX  = 0x05;
static const uint8_t EMOTION_NUM    = 0x06;

namespace kiq {

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


ProcessParseResult ResearchResultInterface::GetNOOPResult()
{
  return ProcessParseResult{{ProcessEventData{.code = SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT}}};
}


NERResultParser::NERResultParser(const std::string& app_name)
: m_app_name(app_name)
{}

NERResultParser::~NERResultParser() {}

bool NERResultParser::read(const std::string& s)
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

ProcessParseResult NERResultParser::get_result()
{
  ProcessParseResult result{};

  KLOG("Returning {} NLP tokens", m_items.size());

  if (m_items.empty()) return GetNOOPResult();

  for (const auto& item : m_items)
    result.events.emplace_back(
      ProcessEventData{
        .code =SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT,
        .payload = std::vector<std::string>{m_app_name, item.type, item.value}});
  return result;
}


EmotionResultParser::EmotionResultParser(const std::string& app_name)
: m_app_name(app_name)
{}

EmotionResultParser::~EmotionResultParser() {}

std::vector<float> Emotions::GetVector() const
{
  return {joy, sadness, surprise, fear, anger, disgust};
}

std::string Emotions::str() const
{
  return "Joy: " + std::to_string     (joy) + '\n' +
         "Sadness: " + std::to_string (sadness) + '\n' +
         "Surprise: " + std::to_string(surprise) + '\n' +
         "Fear: " + std::to_string    (fear) + '\n' +
         "Anger: " + std::to_string   (anger) + '\n' +
         "Disgust: " + std::to_string (disgust) + '\n';
}

std::vector<std::string> EmoStrings::GetVector() const
{
  return {joy, sadness, surprise, fear, anger, disgust};
}

std::string EmoStrings::str() const
{
  return "Joy: " +      joy + '\n' +
         "Sadness: " +  sadness + '\n' +
         "Surprise: " + surprise + '\n' +
         "Fear: " +     fear + '\n' +
         "Anger: " +    anger + '\n' +
         "Disgust: " +  disgust + '\n';
}

template <>
Emotion<Emotions> Emotion<Emotions>::Create(const std::vector<std::string>& v)
{
  auto Partition = [](const auto& v) { return std::vector<std::string>{0x01 + v.begin() + EMOTION_NUM, v.end()}; };
  Emotion<Emotions> emotion{
    .scores = Emotions{
      .joy      = std::stof(v[0x01 + JOY_INDEX]),
      .sadness  = std::stof(v[0x01 + SADNESS_INDEX]),
      .surprise = std::stof(v[0x01 + SURPRISE_INDEX]),
      .fear     = std::stof(v[0x01 + FEAR_INDEX]),
      .anger    = std::stof(v[0x01 + ANGER_INDEX]),
      .disgust  = std::stof(v[0x01 + DISGUST_INDEX])}};
  for (const auto& e_s : Partition(v))
    emotion.emotions.emplace_back(e_s);

  return emotion;
}

template <>
std::string Emotion<Emotions>::str() const
{
  std::string s{};
  s += "Emotions: ";
  for (const auto& e : emotions) s += e + ", ";
  s.pop_back(); s.pop_back();
  s += "\nScores: " + scores.str();
  return s;
}

bool EmotionResultParser::read(const std::string& s)
{
  using namespace rapidjson;
  Document d{};
  d.Parse(s.c_str());
  if (!d.HasParseError() && !d.IsNull() && d.IsObject())
  {
    Emotion<EmoStrings> emotion{};
    if (d.HasMember("anger"))
      emotion.scores.anger = d["anger"].GetString();
    if (d.HasMember("disgust"))
      emotion.scores.disgust = d["disgust"].GetString();
    if (d.HasMember("fear"))
      emotion.scores.fear = d["fear"].GetString();
    if (d.HasMember("joy"))
      emotion.scores.joy = d["joy"].GetString();
    if (d.HasMember("sadness"))
      emotion.scores.sadness = d["sadness"].GetString();
    if (d.HasMember("surprise"))
      emotion.scores.surprise = d["surprise"].GetString();
    if (d.HasMember("emotions"))
      for (const auto& value : d["emotions"].GetArray())
        emotion.emotions.emplace_back(value.GetString());

    m_items.emplace_back(std::move(emotion));
    return true;
  }
  return false;
}

ProcessParseResult EmotionResultParser::get_result()
{
  const auto MakePayload = [this](const Emotion<EmoStrings>& e) -> std::vector<std::string>
  {
    std::vector<std::string> payload{m_app_name};
    const auto&                    scores   = e.scores.GetVector();
    const auto&                    emotions = e.emotions;
    payload.insert(payload.end(), scores  .begin(), scores  .end());
    payload.insert(payload.end(), emotions.begin(), emotions.end());
    return payload;
  };
  ProcessParseResult result{};

  KLOG("Returning {} Emotion objects", m_items.size());

  if (m_items.empty()) return GetNOOPResult();

  for (const auto& item : m_items)
    result.events.emplace_back(
      ProcessEventData{
        .code =SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT,
        .payload = MakePayload(item)});
  return result;
}

SentimentResultParser::SentimentResultParser(const std::string& app_name)
: m_app_name(app_name)
{}

SentimentResultParser::~SentimentResultParser() {}


std::string Keyword::str() const
{
  return "Word: " + word + '\n' + "Score: " + std::to_string(score) + '\n';
}

std::string Sentiment::str() const
{
  std::string s{};
  s += "Type: " + type + '\n' +
       "Score: " + std::to_string(score) + '\n';
  for (const auto& keyword : keywords)
    s += keyword.str();

  return s;
}

std::vector<std::string> Sentiment::GetPayload() const
{
  std::vector<std::string> v{type, std::to_string(score)};
  for (const Keyword& keyword : keywords)
  {
    v.emplace_back(keyword.word);
    v.emplace_back(std::to_string(keyword.score));
  }
  return v;
}

Sentiment Sentiment::Create(const std::vector<std::string>& v)
{
  Sentiment sentiment{v[0], std::stof(v[1])};
  if (v.size() > 2)
    for (size_t i = 2; i < v.size(); i += 2)
       sentiment.keywords.emplace_back(Keyword{v[i    ], std::stof(v[i + 1])});
  return sentiment;
}

bool SentimentResultParser::read(const std::string& s)
{
  using namespace rapidjson;
  Document d{};
  d.Parse(s.c_str());
  if (!d.HasParseError() && !d.IsNull() && d.IsObject())
  {
    Sentiment sentiment{};
    if (d.HasMember("type"))
      sentiment.type = d["type"].GetString();
    if (d.HasMember("score"))
      sentiment.score = d["score"].GetFloat();
    if (d.HasMember("keywords"))
      for (const auto& value : d["keywords"].GetArray())
        sentiment.keywords.emplace_back(Keyword{value["word"].GetString(), value["score"].GetFloat()});

    m_items.emplace_back(std::move(sentiment));
    return true;
  }
  return false;
}

ProcessParseResult SentimentResultParser::get_result()
{
  ProcessParseResult result{};
  KLOG("Returning {} Sentiment objects", m_items.size());

  if (m_items.empty()) return GetNOOPResult();

  for (const auto& item : m_items)
    result.events.emplace_back(
      ProcessEventData{
        .code =SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT,
        .payload = item.GetPayload()});
  return result;
}


IGFeedResultParser::IGFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

IGFeedResultParser::~IGFeedResultParser() {}

bool IGFeedResultParser::read(const std::string& s)
{
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

ProcessParseResult IGFeedResultParser::get_result()
{
  ProcessParseResult result{};

  KLOG("Returning {} IG Feed items", m_feed_items.size());

  for (const auto& item : m_feed_items)
  {
    result.events.emplace_back(
      ProcessEventData{
        .code =SYSTEM_EVENTS__PLATFORM_NEW_POST,
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

YTFeedResultParser::YTFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

YTFeedResultParser::~YTFeedResultParser() {}

bool YTFeedResultParser::read(const std::string& s)
{
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

ProcessParseResult YTFeedResultParser::get_result()
{
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

    result.events.emplace_back(
      ProcessEventData{
        .code =SYSTEM_EVENTS__PLATFORM_NEW_POST,
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


TWFeedResultParser::TWFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

TWFeedResultParser::~TWFeedResultParser() {}

bool TWFeedResultParser::read(const std::string& s)
{
  auto GetJSONError = [](auto& d)                { return GetParseError_En(d.GetParseError());   };
  auto ParseOK      = [](auto& d, const auto& s) { return !(d.Parse(s.c_str()).HasParseError()); };
  rapidjson::Document d{};

  if (!ParseOK(d, s))
    ELOG("Error parsing JSON: {}", GetJSONError(d));
  else
  if (d.IsArray())
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

ProcessEventData TWFeedResultParser::ReadEventData(const TWFeedItem& item, const std::string& app_name, const int32_t& event)
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
    .code    = event,
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

ProcessParseResult TWFeedResultParser::get_result()
{
  ProcessParseResult result{};
  KLOG("Returning {} TW Feed items", m_feed_items.size());
  for (const auto& item : m_feed_items)
    result.events.emplace_back(TWFeedResultParser::ReadEventData(item, m_app_name, SYSTEM_EVENTS__PLATFORM_NEW_POST));

  return result;
}

TWResearchParser::TWResearchParser(const std::string& app_name)
: TWFeedResultParser{app_name}
{}

TWResearchParser::~TWResearchParser() {}

ProcessParseResult TWResearchParser::get_result()
{
  ProcessParseResult result{};
  KLOG("Returning {} TW Feed items", m_feed_items.size());
  for (const auto& item : m_feed_items)
    result.events.emplace_back(TWFeedResultParser::ReadEventData(item, m_app_name, SYSTEM_EVENTS__PROCESS_RESEARCH));

  return result;
}

KTFeedResultParser::KTFeedResultParser(const std::string& app_name)
: m_app_name{app_name}
{}

KTFeedResultParser::~KTFeedResultParser() {}

bool KTFeedResultParser::read(const std::string& s)
{
  auto get_date     = [](const auto& t)
  {
    auto s = std::to_string(t);
    if (s.size() == 13)
      return std::string{s.begin(), s.end() - 3};
    return s;
  };
  auto GetJSONError = [](auto& d)                { return GetParseError_En(d.GetParseError());   };
  auto ParseOK      = [](auto& d, const auto& s) { return !(d.Parse(s.c_str()).HasParseError()); };
  rapidjson::Document d{};

  if (!ParseOK(d, s))
    ELOG("Error parsing JSON: {}", GetJSONError(d));
  else
  if (d.IsArray())
  {
    for (const auto& item : d.GetArray())
    {
      KTFeedItem kt_item{};
      if (!item.IsObject())
        continue;

      if (item.HasMember("id"))
        kt_item.id = item["id"].GetString();
      if (item.HasMember("text"))
          kt_item.text = item["text"].GetString();
      if (item.HasMember("date"))
          kt_item.date = get_date(item["date"].GetInt64());
      if (item.HasMember("user"))
          kt_item.user = item["user"].GetString();
      if (item.HasMember("media"))
        for (const auto& url : item["media"].GetArray())
          kt_item.media.push_back(url.GetString());

      m_feed_items.emplace_back(std::move(kt_item));
    }

    return true;
  }

  return false;
}

ProcessParseResult KTFeedResultParser::get_result()
{
  ProcessParseResult result;

  for (const auto& item : m_feed_items)
    result.events.emplace_back(ProcessEventData{
      .code    = SYSTEM_EVENTS__PLATFORM_NEW_POST,
      .payload = {
        m_app_name,
        item.id,
        "DEFAULT_USER",
        item.date,
        item.text,
        {},
        constants::SHOULD_REPOST,
        constants::PLATFORM_PROCESS_METHOD}});

  return result;
}
ProcessEventData KTFeedResultParser::ReadEventData(const TWFeedItem& item, const std::string& app_name, const int32_t& event)
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
    .code    = event,
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

PollResultParser::PollResultParser()
: m_app_name{"TelegramPollResult"}
{}

PollResultParser::~PollResultParser() {}

bool PollResultParser::read(const std::string& s)
{
  using namespace rapidjson;
  Document d{};

  if (!(d.Parse(s.c_str()).HasParseError()) &&
      !(d.IsNull()) && d.IsArray())
  {
    for (const auto& item : d.GetArray())
    {
      TGVoteItem vote{};
      if (item.IsObject())
      {
        for (const auto& k : item.GetObject())
        {
          if (strcmp(k.name.GetString(), "option") == 0)
            vote.option = k.value.GetString();
          else
          if (strcmp(k.name.GetString(), "value") == 0)
            vote.votes  = k.value.GetInt64();
        }
      }
      m_items.emplace_back(std::move(vote));
    }
    return true;
  }
  return false;
}

ProcessParseResult PollResultParser::get_result()
{
  auto MakePayload = [](const auto& v) { std::vector<std::string> p; for (const auto& i : v) p.push_back(std::to_string(i.votes));
                                                                                                                  return p;     };
  KLOG("Returning vote result with {} options", m_items.size());
  return ProcessParseResult{{ProcessEventData{.code = SYSTEM_EVENTS__PROCESS_RESEARCH_RESULT, .payload = MakePayload(m_items)}}};
}

ResultProcessor::ResultProcessor()
{}

template <typename T>
ProcessParseResult ResultProcessor::process(const std::string& output, const T& agent)
{
  std::unique_ptr<ProcessParseInterface> u_parser_ptr;
  ProcessParseResult                     result;

  if constexpr (std::is_same_v<T, KApplication>)
  {
    const KApplication& app = agent;
    if (app.is_kiq)
    {
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
      if (app.name == "KNLP - NER")
        u_parser_ptr.reset(new NERResultParser{app.name});
      else
      if (app.name == "KNLP - Emotion")
        u_parser_ptr.reset(new EmotionResultParser{app.name});
      else
      if (app.name == "KNLP - Sentiment")
        u_parser_ptr.reset(new SentimentResultParser{app.name});
      else
      if (app.name == "KETTR")
        u_parser_ptr.reset(new KTFeedResultParser{app.name});
    }
    else
      ELOG("Could not process result from unknown application");
  }
  else
  if constexpr (std::is_same_v<T, PlatformIPC>)
  {
    const PlatformIPC& ipc = agent;

    switch (ipc.command)
    {
      case (TGCommand::poll_result):
        u_parser_ptr.reset(new PollResultParser{});
    }
  }

  u_parser_ptr->read(output);
  return u_parser_ptr->get_result();
}

template ProcessParseResult ResultProcessor::process(const std::string&, const KApplication&);
template ProcessParseResult ResultProcessor::process(const std::string&, const PlatformIPC&);

} // ns kiq
