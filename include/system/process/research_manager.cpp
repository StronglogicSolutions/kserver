#include "research_manager.hpp"

namespace kiq {
static const char* POLL_Q = "Rate the civilization impact:";
/**
 * VerifyTerm
 */
bool VerifyTerm(const std::string &term)
{
  using SearchFromStart = bool;
  using VerifyFunction = std::function<bool(const std::string &)>;

  static const size_t npos = std::string::npos;
  static const std::vector<const char *> RejectPatterns{
    "&amp;", "Thu Feb", "Wed Feb", "Sun Nov", "Wed Nov", "@…", "Thu Oct"};
  static const std::vector<VerifyFunction> VerifyFunctions{
    [](const std::string &s)
    {
      size_t i{}, x{};
      while (i++ < 2)
      {
        x = s.find("@", x);
        if (x == npos)
          return true;
        else
          x++;
      }
      return false;
    },
    [](const std::string &s)
    {
      for (const auto &p : RejectPatterns)
        if (s.find(p) != npos)
          return false;
      return true;
    }};

  for (const auto &fn : VerifyFunctions)
    if (!fn(term))
    {
      KLOG("Term {} was rejected", term);
      return false;
    }

  return true;
}

/**
 * ParseTermEvents
 */
static std::vector<TermEvent> ParseTermEvents(const QueryValues &values)
{
  using Event = TermEvent;
  using Events = std::vector<Event>;

  Events events;
  std::string id, time, person, organization, user, type, term;

  for (const auto &value : values)
  {
    if (value.first == "term_hit.time")
      time = value.second;
    else
    if (value.first == "person.name")
      person = value.second;
    else
    if (value.first == "organization.name")
      organization = value.second;
    else
    if (value.first == "platform_user.name")
      user = value.second;
    else
    if (value.first == "term.id")
      id = value.second;
    else
    if (value.first == "term.name")
      term = value.second;
    else
    if (value.first == "term.type")
      type = value.second;
    if (DataUtils::NoEmptyArgs(id, time, person, organization, term, type, user))
    {
      events.emplace_back(Event{id, user, person, organization, term, type, time});
      DataUtils::ClearArgs(id, user, person, organization, term, type, time);
    }
  }

  return events;
}

/**
 * ResearchManager
 * @consructor
 */
ResearchManager::ResearchManager(Database::KDB *db_ptr, Platform *plat_ptr, MaskFn mask_fn)
: m_db_ptr(db_ptr),
  m_plat_ptr(plat_ptr),
  m_mask_fn(mask_fn)
{
  m_ml_generator.init("label,1x1,1x2");
}

/**
 * ToString
 */
std::string TermHit::ToString() const
{
  return "Person: " + person + '\n' +
         "User:   " + user + '\n' +
         "Org:    " + organization + '\n' +
         "Time:   " + time + '\n';
}

/**
 * PersonExists
 */
bool ResearchManager::PersonExists(const std::string &name) const
{
  return m_db_ptr->select("person", {"id"}, CreateFilter("name", name)).size();
}

/**
 * UserExists
 */
bool ResearchManager::UserExists(const std::string &name, const std::string &id) const
{
  if (id.empty() && name.empty())
    return false;
  return m_db_ptr->select("person", {"id"}, CreateFilter("id", id, "name", name)).size();
}

/**
 * OrganizationExists
 */
bool ResearchManager::OrganizationExists(const std::string &name) const
{
  return m_db_ptr->select("organization", {"id"}, CreateFilter("name", name)).size();
}

/**
 * TermExists
 */
bool ResearchManager::TermExists(const std::string &name) const
{
  return m_db_ptr->select("term", {"id"}, CreateFilter("name", name)).size();
}

/**
 * AddPerson
 */
std::string ResearchManager::AddPerson(const std::string &name)
{
  if (name.empty())
    return "";
  return m_db_ptr->insert("person", {"name"}, {name}, "id");
}

/**
 * AddTermHit
 */
std::string ResearchManager::AddTermHit(const std::string &tid, const std::string &uid, const std::string &pid, const std::string &oid, const std::string &time)
{
  if (tid.empty() || uid.empty() || pid.empty())
    throw std::invalid_argument{"Must provide tid, uid and pid"};

  auto db = m_db_ptr;
  Values values{tid, uid, pid};
  Fields fields{"tid", "uid", "pid"};
  if (oid.size())
  {
    values.emplace_back(oid);
    fields.emplace_back("oid");
  }
  if (time.size())
  {
    values.emplace_back(time);
    fields.emplace_back("time");
  }

  return db->insert("term_hit", fields, values, "id");
}

/**
 * GetTerm
 */
std::string ResearchManager::GetTerm(const std::string &term) const
{
  for (const auto &row : m_db_ptr->select("term", {"id"}, CreateFilter("name", term)))
    if (row.first == "id")
      return row.second;
  return "";
}

/**
 * GetPerson
 */
std::string ResearchManager::GetPerson(const std::string &name)
{
  std::string id{};
  for (const auto &row : m_db_ptr->select("person", {"id"}, CreateFilter("name", name)))
    if (row.first == "id")
      id = row.second;
  return id;
}

/**
 * GetPersonForUID
 */
Person ResearchManager::GetPersonForUID(const std::string &uid)
{
  Person person{};
  const Fields fields{"platform_user.pers_id", "person.name"};
  const QueryFilter filter{"platform_user.id", uid};
  const Join join{"person", "id", "platform_user", "pers_id"};

  for (const auto &row : m_db_ptr->selectSimpleJoin("platform_user", fields, filter, join))
    if (row.first == "platform_user.pers_id")
      person.id = row.second;
    else if (row.first == "person.name")
      person.name = row.second;
  return person;
}

/**
 * GetIdentity
 */
Identity ResearchManager::GetIdentity(const std::string &name, const std::string &pid)
{
  if (name.empty() || pid.empty())
    throw std::invalid_argument{"Must provide name and platform id"};
  Identity identity{};
  auto db = m_db_ptr;
  Joins joins = {{"person", "id", "platform_user", "pers_id"}, {"affiliation", "pid", "person", "id"}, {"organization", "id", "affiliation", "oid"}};
  Fields fields = {"platform_user.id", "platform_user.name", "organization.name"};
  auto filter = CreateFilter("platform_user.name", name, "platform_user.pid", pid);

  for (const auto &row : db->selectJoin("platform_user", fields, filter, joins))
    if (row.first == "platform_user.id")
      identity.id = row.second;
    else if (row.first == "platform_user.name")
      identity.name = row.second;
    else if (row.first == "organization.name")
      identity.organization = row.second;

  return identity;
}

/**
 * GetUser
 */
std::string ResearchManager::GetUser(const std::string &name, const std::string &pid)
{
  if (name.empty() || pid.empty())
    throw std::invalid_argument{"Must provide name and platform id"};
  std::string uid;
  auto db = m_db_ptr;
  QueryFilter filter = CreateFilter("name", name, "pid", pid);
  for (const auto &row : db->select("platform_user", {"id"}, filter))
    if (row.first == "id")
      uid = row.second;
  return uid;
}

/**
 * SaveTermHit
 */
std::string ResearchManager::SaveTermHit(const JSONItem& term, const std::string& uid, const std::string& sid)
{
  const auto& value = term.value;
  const auto& type  = term.type;
        auto  db    =  m_db_ptr;
  const auto  tid   = (TermExists(value)) ? GetTerm(value) : AddTerm(value, type);
  return db->insert("term_hit", {"tid", "uid", "sid"}, {tid, uid, sid}, "id");
}

/**
 * TermHasHits
 */
bool ResearchManager::TermHasHits(const std::string &term)
{
  return (GetTermHits(term).size()); // TODO: we need a simpler method for getting hit
}

/**
 * valid
 */
bool TermEvent::valid() const
{
  return (id.size());
}

/**
 * ToString
 */
std::string TermEvent::ToString(const bool verbose) const
{
  return (verbose) ? "ID  : " + id + '\n' +
                         "Term: " + term + '\n' +
                         "Type: " + type + '\n' +
                         "User: " + user + '\n' +
                         "Org : " + organization + '\n' +
                         "Time: " + time
                   : "New term added at " + time + ":\n" + id + ": " +
                         "Term \"" + term + "\" of type " + type + " by " + user + " (" + person + ") from " + organization + '\n';
}

std::string TermEvent::ToJSON() const
{
  rapidjson::StringBuffer s;
  Writer<rapidjson::StringBuffer, rapidjson::Document::EncodingType, ASCII<>> w(s);
  w.StartObject();
  w.Key("id");
  w.String(id.c_str());
  w.Key("term");
  w.String(term.c_str());
  w.Key("type");
  w.String(type.c_str());
  w.Key("user");
  w.String(user.c_str());
  w.Key("person");
  w.String(person.c_str());
  w.Key("organization");
  w.String(organization.c_str());
  w.Key("time");
  w.String(time.c_str());
  w.EndObject();

  return s.GetString();
}
/**
 * RecordtermEvent
 */
TermEvent ResearchManager::RecordTermEvent(JSONItem&& term, const std::string& user, const std::string& app, const Task& task, const std::string& time)
{
  TermEvent event{};
  event.term = term.value;
  event.type = term.type;
  event.user = user;

  if (!TermHitExists(term.value, TimeUtils::DatabaseTime(time)) && HasBasePlatform(app))
  {
    const auto pid = m_plat_ptr->GetPlatformID(GetBasePlatform(app));
    if (!pid.empty())
    {
      const auto identity = GetIdentity(user, pid);
      if (!identity.id.empty())
      {
        event.id           = SaveTermHit(term, identity.id, task.id());
        event.tid          = GetTerm(term.value);
        event.organization = identity.organization;
        event.person       = GetPersonForUID(identity.id).name;
        event.time         = time;
      }
    }
  }

  return event;
}

/**
 * GetTermHits
 */
std::vector<TermHit> ResearchManager::GetTermHits(const std::string &term)
{
  auto db = m_db_ptr;
  const auto DoQuery = [&db](const std::string &tid) -> QueryValues
  {
    try
    {
      const Fields fields{"term_hit.id", "term_hit.time",      "term_hit.sid",
                          "person.name", "platform_user.name", "organization.name"};
      const Joins  joins {{"platform_user", "id",  "term_hit",      "uid"},
                          {"person",        "id",  "platform_user", "pers_id"},
                          {"affiliation",   "pid", "person",        "id"},
                          {"organization",  "id",  "affiliation",   "oid"}};
      return db->selectJoin("term_hit", fields, {CreateFilter("term_hit.tid", tid)}, joins);
    }
    catch (const std::exception &e)
    {
      ELOG("Exception from KDB: {}", e.what());
    }
    return {};
  };

  std::vector<TermHit> result{};
  if (!TermExists(term))
    return result;

  std::string tid = GetTerm(term);
  std::string time, person, organization, user, sid, id;
  for (const auto& value : DoQuery(tid))
  {
    if (value.first == "term_hit.time")      time         = value.second;
    else
    if (value.first == "term_hit.id")        id           = value.second;
    else
    if (value.first == "term_hit.sid")       sid          = value.second;
    else
    if (value.first == "person.name")        person       = value.second;
    else
    if (value.first == "organization.name")  organization = value.second;
    else
    if (value.first == "platform_user.name") user         = value.second;

    if (DataUtils::NoEmptyArgs(id, time, person, organization, user, sid))
    {
      KLOG("Term {} has previous hit: Person: {} - Time: {}", term, person, time);
      result.emplace_back(TermHit{id, person, user, organization, time, term, sid});
      DataUtils::ClearArgs(time, person, user, organization);
    }
  }
  return result;
}

/**
 * AddTerm
 */
std::string ResearchManager::AddTerm(const std::string &name, const std::string &type)
{
  return m_db_ptr->insert("term", {"name", "type"}, {name, type}, "id");
}

std::vector<TermEvent> ResearchManager::GetAllTermEvents() const
{
  auto db = m_db_ptr;
  const Fields fields{"term_hit.id", "term.id", "term.name", "term.type", "term_hit.time", "platform_user.name", "organization.name", "person.name"};
  const Joins  joins {{"platform_user", "id", "term_hit",      "uid"},
                      {"person",        "id", "platform_user", "pers_id"},
                      {"affiliation",   "pid", "person",       "id"},
                      {"term",          "id",  "term_hit",     "tid"},
                      {"organization",  "id",  "affiliation",  "oid"}};
  const auto query = db->selectJoin("term_hit", fields, QueryFilter{}, joins);

  const auto events = ParseTermEvents(query);

  return events;
}

ResearchManager::StudyRequests
ResearchManager::AnalyzeTW(const TaskWrapper& root, const TaskWrapper& child, const TaskWrapper& subchild)
{
  using namespace FileUtils;
  using Emotion   = Emotion<Emotions>;
  using Sentiment = Sentiment;
  using Terms     = std::vector<JSONItem>;
  using Hits      = std::vector<TermHit>;

  auto  FindMask        = m_mask_fn;
  auto  MakePollMessage = [](const auto& text, const auto& emo, const auto& sts, const auto& hit)
  {
    return "Please rate the civilizational impact of the following statement:\n\n\"" + text +
            "\"\n\nEMOTION\n" + emo.str() + '\n' + "SENTIMENT\n" + sts.str() +"\nHIT\n" + hit;
  };
  auto  GetTokens       = [](const auto& payload)
  {
    std::vector<JSONItem> tokens{};
    if (payload.size())
      for (size_t i = 1; i < (payload.size() - 1); i += 2)
        tokens.emplace_back(JSONItem{payload[i], payload[i + 1]});
    return tokens;
  };

  ResearchManager::StudyRequests requests{};

  KLOG("Performing final analysis on research triggered by {}", root.id);
  const std::string child_text     = child   .task.GetToken(constants::DESCRIPTION_KEY);
  const std::string sub_c_text     = subchild.task.GetToken(constants::DESCRIPTION_KEY);
  const auto        ner_parent     = *(FindParent(&child,        FindMask(NER_APP)));
  const TaskWrapper sub_c_emo_tk   = *(FindParent(&subchild,     FindMask(EMOTION_APP)));
  const TaskWrapper child_c_emo_tk = *(FindParent(&sub_c_emo_tk, FindMask(EMOTION_APP)));
  const auto        ner_data       = ner_parent.event.payload;
  const auto        sub_c_emo_data = sub_c_emo_tk.event.payload;
  const auto        child_emo_data = child_c_emo_tk.event.payload;
  const auto        sub_c_sts_data = subchild.event.payload;
  const auto        child_sts_data = child.event.payload;
  const Terms       terms_data     = GetTokens(ner_data);
  const Emotion     child_emo      = Emotion::Create(child_emo_data);
  const Emotion     sub_c_emo      = Emotion::Create(sub_c_emo_data);
  const Sentiment   child_sts      = Sentiment::Create(child_sts_data);
  const Sentiment   sub_c_sts      = Sentiment::Create(sub_c_sts_data);
  const Hits        hits           = GetTermHits(StringUtils::RemoveTags(terms_data.front().value));

  for (const TermHit& hit : hits)
  {
    requests.emplace_back(ResearchRequest{
      hit,
      MakePollMessage(child_text, child_emo, child_sts, hit.ToString()),
      POLL_Q,
      Study::poll,
      child_emo,
      child_sts});
  }
  return requests;
}

bool ResearchManager::TermHitExists(const std::string& term, const std::string& time) const
{
  auto db    = m_db_ptr;
  auto query = db->select("term_hit", {"id"}, CreateFilter("tid", GetTerm(term), "time", time));
  for (const auto& value : query)
    if (value.first == "id")
     return true;
  return false;
}


void ResearchManager::GenerateMLData()
{
  KLOG("Generating model training data");
  m_ml_generator.Generate();
}


void ResearchManager::FinalizeMLInputs(const std::string& id, const std::vector<std::string>& data)
{
  VLOG("Adding data to model input data generator from research with ID {}", id);
  m_ml_generator.at(std::stoi(id)).poll_results = data; // TODO: poll results must be parsed
}

template <typename T>
void ResearchManager::AddMLInput(const std::string& id, const T& input)
{
  VLOG("Adding research data with ID {}");
  m_ml_generator.operator()(std::stoi(id), input);
}
template void ResearchManager::AddMLInput(const std::string&, const TWResearchInputs&);

std::string ResearchManager::GetMLData()
{
  VLOG("Getting model training data");
  return m_ml_generator.GetResult();
}


} // ns kiq
