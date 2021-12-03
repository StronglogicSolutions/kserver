#include "research_manager.hpp"

namespace kiq {
/**
 * VerifyTerm
 */
static bool VerifyTerm(const std::string &term)
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
      return false;

  return true;
}

/**
 * ParseTermEvents
 */
static std::vector<ResearchManager::TermEvent> ParseTermEvents(const QueryValues &values)
{
  using Event = ResearchManager::TermEvent;
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
ResearchManager::ResearchManager(Database::KDB *db_ptr, Platform *plat_ptr)
    : m_db_ptr(db_ptr),
      m_plat_ptr(plat_ptr)
{
}

/**
 * ToString
 */
std::string ResearchManager::TermHit::ToString() const
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
std::string ResearchManager::GetTerm(const std::string &term)
{
  std::string id{};
  for (const auto &row : m_db_ptr->select("term", {"id"}, CreateFilter("name", term)))
    if (row.first == "id")
      id = row.second;
  return id;
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
ResearchManager::Person ResearchManager::GetPersonForUID(const std::string &uid)
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
ResearchManager::Identity ResearchManager::GetIdentity(const std::string &name, const std::string &pid)
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
  const auto value = StringUtils::RemoveTags(term);
  return (TermExists(value) && GetTermHits(value).size()); // TODO: we need a simpler method for getting hit #
}

/**
 * valid
 */
bool ResearchManager::TermEvent::valid() const
{
  return (id.size());
}

/**
 * ToString
 */
std::string ResearchManager::TermEvent::ToString(const bool verbose) const
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

std::string ResearchManager::TermEvent::ToJSON() const
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
ResearchManager::TermEvent ResearchManager::RecordTermEvent(JSONItem&& term, const std::string& user, const std::string& app, const Task& task)
{
  TermEvent event{};

  if (VerifyTerm(term.value))
  {
    term.value = StringUtils::RemoveTags(term.value);
    event.term = term.value;
    event.type = term.type;
    event.user = user;
    if (HasBasePlatform(app))
    {
      const auto pid = m_plat_ptr->GetPlatformID(GetBasePlatform(app));
      if (!pid.empty())
      {
        const auto identity = GetIdentity(user, pid);
        if (!identity.id.empty())
        {
          event.id           = SaveTermHit(term, identity.id, task.id());
          event.organization = identity.organization;
          event.person       = GetPersonForUID(identity.id).name;
          event.time         = TimeUtils::FormatTimestamp(TimeUtils::UnixTime());
        }
      }
    }
  }
  else
    KLOG("Term {} was rejected", term.value);

  return event;
}

/**
 * GetTermHits
 */
std::vector<ResearchManager::TermHit> ResearchManager::GetTermHits(const std::string &term)
{
  auto db = m_db_ptr;
  const auto DoQuery = [&db](const std::string &tid) -> QueryValues
  {
    try
    {
      Fields fields{"term_hit.time", "term_hit.sid", "person.name", "platform_user.name", "organization.name"};
      return db->selectJoin("term_hit", fields, {CreateFilter("term_hit.tid", tid)}, Joins{Join{.table = "platform_user", .field = "id", .join_table = "term_hit", .join_field = "uid"}, Join{.table = "person", .field = "id", .join_table = "platform_user", .join_field = "pers_id"}, Join{.table = "affiliation", .field = "pid", .join_table = "person", .join_field = "id"}, Join{.table = "organization", .field = "id", .join_table = "affiliation", .join_field = "oid"}});
    }
    catch (const std::exception &e)
    {
      ELOG("Exception from KDB: {}", e.what());
    }
    return {};
  };

  std::vector<TermHit> result{};
  std::string tid;
  if (!TermExists(term))
    return result;

  tid = GetTerm(term);

  std::string time, person, organization, user, sid;
  for (const auto &value : DoQuery(tid))
  {
    if (value.first == "term_hit.time")
      time = value.second;
    else
    if (value.first == "term_hit.sid")
      sid = value.second;
    else
    if (value.first == "person.name")
      person = value.second;
    else
    if (value.first == "organization.name")
      organization = value.second;
    else
    if (value.first == "platform_user.name")
      user = value.second;

    if (DataUtils::NoEmptyArgs(time, person, organization, user, sid))
    {
      KLOG("Term {} has previous hit:\nPerson: {}\nOrg: {}\nTime: {}", term, person, organization, time);
      result.emplace_back(TermHit{person, user, organization, time, term});
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


/**
 * GetAllTermHits
 * id
   user
   person
   organization
   term
   type
   time
 */
std::vector<ResearchManager::TermEvent> ResearchManager::GetAllTermEvents() const
{
  auto db = m_db_ptr;
  Fields fields{"term_hit.id", "term.id", "term.name", "term.type", "term_hit.time", "platform_user.name", "organization.name", "person.name"};
  const auto query = db->selectJoin("term_hit", fields, QueryFilter{}, Joins{{"platform_user", "id", "term_hit", "uid"}, {"person", "id", "platform_user", "pers_id"}, {"affiliation", "pid", "person", "id"}, {"term", "id", "term_hit", "tid"}, {"organization", "id", "affiliation", "oid"}});

  const auto events = ParseTermEvents(query);

  return events;
}
} // ns kiq
