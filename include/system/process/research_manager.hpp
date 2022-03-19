#pragma once

#include "platform.hpp"
#include "result_parser.hpp"
#include "helper.hpp"

namespace kiq {
using JSONItem = NERResultParser::NLPItem;

bool VerifyTerm(const std::string &term);

enum class Study
{
poll = 0x00,
};

struct Person{
std::string id;
std::string name;
};
struct Identity
{
std::string id;
std::string name;
std::string organization;
};
struct TermHit
{
std::string id;
std::string person;
std::string user;
std::string organization;
std::string time;
std::string term;
std::string sid;

std::string ToString() const;
};

struct TermEvent
{
std::string id;
std::string user;
std::string person;
std::string organization;
std::string term;
std::string type;
std::string time;
std::string tid;

bool        valid()                            const;
std::string to_str(const bool verbose = false) const;
std::string to_JSON()                          const;

static std::string to_str(const std::vector<TermEvent>& events);
};

struct ResearchRequest
{
TermHit hit;
std::string data;
std::string title;
Study type;
Emotion<Emotions> emotion;
Sentiment sentiment;
};


struct TWResearchInputs
{
Emotion<Emotions>        emotions;
Sentiment                sentiment;
std::vector<std::string> poll_results;

bool Ready() const
{
  return ((!emotions.emotions.empty()) && (!poll_results.empty()));
}
};

template <typename T = TWResearchInputs>
struct MLInputGenerator
{
~MLInputGenerator() = default;

void init(const std::string& header)
{
  output = header;
}

void operator()(const std::string& idx, T input)
{
  inputs[idx] = input;
}

void Generate()
{
  if constexpr (std::is_same_v<T, TWResearchInputs>)
  {
    std::stringstream ss;
    for (const auto [id, input] : inputs)
    {
      auto&             e = input.emotions;
      auto&             s = input.sentiment;
      auto&             p = input.poll_results;
      ss        << '\n' + id         << ','
                << e.scores.joy      << ','
                << e.scores.sadness  << ','
                << e.scores.surprise << ','
                << e.scores.fear     << ','
                << e.scores.anger    << ','
                << e.scores.disgust  << ','
                << s.score           << ','
                << p.at(0)           << ','
                << p.at(1)           << ','
                << p.at(2)           << ','
                << p.at(3);
    }
    output += ss.str();
  }
}

T& at(const std::string& idx)
{
  return inputs.at(idx);
}

bool Ready() const
{
  if constexpr (std::is_same_v<T, TWResearchInputs>)
  {
    for (const auto& [_, input] : inputs)
      if (!input.Ready()) break;
    return true;
  }
  return false;
}

std::string GetResult() const
{
  return output;
}

void clear()
{
  output.clear();
  inputs.clear();
}

bool has_data() const
{
  return (output.size() > 0);
}



using input_map_t = std::unordered_map<std::string, T>;

input_map_t inputs;
std::string output;

};

using MaskFn = std::function<int32_t(const std::string&)>;

class ResearchManager
{
public:
ResearchManager(Database::KDB* db_ptr, Platform* plat_ptr, MaskFn mask_fn);

using StudyRequests = std::vector<ResearchRequest>;

std::string            AddTermHit(const std::string& tid,
                                  const std::string& uid,
                                  const std::string& pid,
                                  const std::string& oid  = "",
                                  const std::string& time = "");
std::string            AddUser(const std::string& name,
                               const std::string& pid,
                               const std::string& pers_id,
                               const std::string& type);
std::string            AddTerm(const std::string& name, const std::string& type);
std::string            AddPerson(const std::string& name);
std::string            AddOrganization();
std::string            AddAffiliation();
void                   FindPeople();
void                   FindOrganizations();
void                   FindAffiliations();

std::string            GetTerm(const std::string& term) const;
std::string            GetPerson(const std::string& name);
Person                 GetPersonForUID(const std::string& uid);
Identity               GetIdentity(const std::string& name, const std::string& pid);
std::string            GetUser(const std::string& name, const std::string& pid);
std::vector<TermHit>   GetTermHits(const std::string& term);
std::vector<TermEvent> GetAllTermEvents() const;
void                   GetOrganization();
void                   GetAffiliation();

std::string            SaveTermHit(const JSONItem& term, const std::string& uid, const std::string& sid);
TermEvent              RecordTermEvent(JSONItem&& term, const std::string& user, const std::string& app, const Task& task, const std::string& time);

bool                   OrganizationExists(const std::string& name) const;
bool                   PersonExists(const std::string& name) const;
bool                   UserExists(const std::string& name = "", const std::string& id = "") const;
bool                   TermExists(const std::string& name) const;
bool                   TermHasHits(const std::string& term);
bool                   TermHitExists(const std::string& term, const std::string& time) const;
StudyRequests          AnalyzeTW(const TaskWrapper& root, const TaskWrapper& child, const TaskWrapper& subchild);

template <typename T>
void                   AddMLInput(const std::string& id, const T& input);
void                   GenerateMLData();
std::string            GetMLData();
void                   FinalizeMLInputs(const std::string& id, const std::vector<std::string>& data);
bool                   MLInputReady() const;

private:
using MLGenerator = MLInputGenerator<TWResearchInputs>;
Database::KDB* m_db_ptr;
Platform*      m_plat_ptr;
MaskFn         m_mask_fn;
MLGenerator    m_ml_generator;
};
} // ns kiq
