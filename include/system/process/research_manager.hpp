#include "platform.hpp"

struct TermHit
{
std::string person;
std::string user;
std::string organization;
std::string time;

std::string ToString() const
{
  return "Person: " + person       + '\n' +
         "User:   " + user         + '\n' +
         "Org:    " + organization + '\n' +
         "Time:   " + time         + '\n';
}
};

class ResearchManager
{
public:
ResearchManager(Database::KDB* db_ptr, Platform* plat_ptr)
: m_db_ptr(db_ptr),
  m_plat_ptr(plat_ptr)
{}

using Database = Database::KDB;
bool PersonExists(const std::string& name) const
{
        auto db     = m_db_ptr;
  const auto filter = QueryFilter{{"name", name}};
  return db->select("person", {"id"}, filter).size();
}

bool UserExists(const std::string& name = "", const std::string& id = "") const
{
  if (id.empty() && name.empty()) return false;

        auto db     = m_db_ptr;
  QueryFilter filter{};
  if (id.size())
    filter.emplace_back("id", id);
  if (name.size())
    filter.emplace_back("name", name);

  return db->select("person", {"id"}, filter).size();
}

bool OrganizationExists(const std::string& name) const
{
        auto db     = m_db_ptr;
  const auto filter = QueryFilter{{"name", name}};
  return db->select("organization", {"id"}, filter).size();
}

bool TermExists(const std::string& name) const
{
        auto db     = m_db_ptr;
  const auto filter = QueryFilter{{"name", name}};
  return db->select("term", {"id"}, filter).size();
}

std::string AddPerson(const std::string& name)
{
  if (name.empty())
    return "";
  return m_db_ptr->insert("person", {"name"}, {name}, "id");
}

std::string AddTermHit(const std::string& tid, const std::string& uid, const std::string& pid, const std::string& oid = "", const std::string& time = "")
{
  if (tid.empty() || uid.empty() || pid.empty())
    throw std::invalid_argument{"Must provide tid, uid and pid"};

  auto   db = m_db_ptr;
  Values values{tid, uid, pid};
  Fields fields{"tid", "uid", "pid"};
  if (oid.size())
  {
    values.emplace_back(oid);
    fields.emplace_back("oid)");
  }
  if (time.size())
  {
    values.emplace_back(time);
    fields.emplace_back("time");
  }

  return db->insert("term_hit", fields, values, "id");
}

std::string GetTerm(const std::string& term)
{
  std::string id{};
  for (const auto& row : m_db_ptr->select("term", {"id"}, QueryFilter{{"name", term}}))
    if (row.first == "id")
      id = row.second;
  return id;
}

std::string GetPerson(const std::string& name)
{
  std::string id{};
  for (const auto& row : m_db_ptr->select("person", {"id"}, QueryFilter{{"name", name}}))
    if (row.first == "id")
      id = row.second;
  return id;
}

struct Person{
std::string id;
std::string name;
};

Person GetPersonForUID(const std::string& uid)
{
  Person person {};
  Fields fields {"platform_user.pers_id", "person.name"};
  auto   filter = CreateFilter("platform_user.id", uid);
  Join   join   {"person", "id", "platform_user", "pers_id"};

  for (const auto& row : m_db_ptr->selectSimpleJoin("platform_user", fields, filter, join))
    if (row.first == "platform_user.pers_id")
      person.id   = row.second;
    else
    if (row.first == "person.name")
      person.name = row.second;

  return person;
}

struct Identity
{
std::string id;
std::string name;
std::string organization;
};

Identity GetIdentity(const std::string& name, const std::string& pid)
{
  if (name.empty() || pid.empty())
    throw std::invalid_argument{"Must provide name and platform id"};
  Identity identity{};
  auto     db     = m_db_ptr;
  Joins    joins  = {{"person", "id", "platform_user", "pers_id"}, {"affiliation", "pid", "person", "id"},
                     {"organization", "id", "affiliation", "oid"}};
  Fields   fields = {"platform_user.id", "platform_user.name", "organization.name"};
  auto     filter = CreateFilter("platform_user.name", name, "platform_user.pid", pid);

  for (const auto& row : db->selectJoin("platform_user", fields, filter, joins))
    if (row.first == "platform_user.id")
      identity.id           = row.second;
    else
    if (row.first == "platform_user.name")
      identity.name         = row.second;
    else
    if (row.first == "organization.name")
      identity.organization = row.second;

  return identity;
}

std::string GetUser(const std::string& name, const std::string& pid)
{
  if (name.empty() || pid.empty())
    throw std::invalid_argument{"Must provide name and platform id"};
  std::string uid;
  auto        db = m_db_ptr;
  QueryFilter filter{{"name", name}, {"pid", pid}};
  for (const auto& row : db->select("platform_user", {"id"}, filter))
    if (row.first == "id")
      uid = row.second;
  return uid;
}

std::string SaveTermHit(const std::string& term, const std::string& uid)
{
  auto db  = m_db_ptr;
  auto tid = (TermExists(term)) ? GetTerm(term) : AddTerm(term);
  return db->insert("term_hit", {"tid", "uid"}, {tid, uid}, "id");
}

bool TermHasHits(const std::string& term)
{
  return (TermExists(term) && GetTermHits(term).size());
}

struct TermEvent
{
std::string id;
std::string user;
std::string person;
std::string organization;
std::string term;
std::string type;
std::string time;

std::string ToString() const
{
  return "ID  : " + id           + '\n' +
         "Term: " + term         + '\n' +
         "Type: " + type         + '\n' +
         "User: " + user         + '\n' +
         "Org : " + organization + '\n' +
         "Time: " + time;
}
};

TermEvent RecordTermEvent(const JSONItem& term, const std::string& user, const std::string& app)
{
  TermEvent event{};
  event.term = term.value;
  event.type = term.type;
  event.user = user;
  if (HasBasePlatform(app))
  {
    const auto pid = m_plat_ptr->GetPlatformID(GetBasePlatform(app));
    if (!pid.empty())
    {
      auto identity = GetIdentity(user, pid);
      if (!identity.id.empty())
      {
        event.id           = SaveTermHit(term, identity.id);
        event.organization = identity.organization;
        event.person       = GetPersonForUID(identity.id).name;
        event.time         = TimeUtils::FormatTimestamp(TimeUtils::UnixTime());
      }
    }
  }
  return event;
}

std::vector<TermHit> GetTermHits(const std::string& term)
{
  auto db = m_db_ptr;
  const auto DoQuery = [&db](const std::string& tid)
  {
    try
    {
      Fields fields{"term_hit.time", "platform_user.id", "platform_user.name", "organization.id", "organization.name"};
      return db->selectJoin("term_hit", fields, {CreateFilter("term_hit.tid", tid)}, Joins{
        Join{
          .table      = "platform_user",
          .field      = "id",
          .join_table = "term_hit",
          .join_field = "uid"
        },
        Join{
          .table      = "person",
          .field      = "id",
          .join_table = "platform_user",
          .join_field = "pers_id"
        },
        Join{
          .table      = "affiliation",
          .field      = "pid",
          .join_table = "person",
          .join_field = "id"
        },
        Join{
          .table      = "organization",
          .field      = "id",
          .join_table = "affiliation",
          .join_field = "oid"
        }
      });
    }
    catch (const std::exception& e)
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

  std::string time, person, organization, user;
  for (const auto& value : DoQuery(tid))
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
      organization = value.second;

    if (DataUtils::NoEmptyArgs(time, person, organization, user))
    {
      KLOG("Term hit was discovered for {}:\nPerson: {}\nOrg: {}\nTime: {}", person, organization, time);
      result.emplace_back(TermHit{person, user, organization, time});
      DataUtils::ClearArgs(time, person, user, organization);
    }
  }
  return result;
}

std::string AddUser(const std::string& name, const std::string& pid, const std::string& pers_id, const std::string& type);
std::string AddOrganization();
std::string AddTerm(const std::string& name)
{
  return m_db_ptr->insert("term", {"name"}, {name}, "id");
}
std::string AddAffiliation();
void        GetOrganization();
void        GetAffiliation();
void        FindPeople();
void        FindOrganizations();
void        FindAffiliations();
void        AnalyzeTermHit(const std::string& term, const std::string& hid)
{
  auto hits = GetTermHits(term);
  std::string hit_info;
  for (const auto& hit : hits)
    KLOG("Returned hit:\n{}\n MORE TO DO!!!", hit.ToString());
}

private:
Database* m_db_ptr;
Platform* m_plat_ptr;
};
