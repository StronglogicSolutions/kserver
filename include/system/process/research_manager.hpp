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
  const auto filter = CreateFilter("name", name);
  return db->select("person", {"id"}, filter).size();
}

bool UserExists(const std::string& name = "", const std::string& id = "") const
{
  if (id.empty() && name.empty()) return false;

        auto db     = m_db_ptr;
  QueryFilter filter{};
  if (id.size())
    filter.Add("id", id);
  if (name.size())
    filter.Add("name", name);

  return db->select("person", {"id"}, filter).size();
}

bool OrganizationExists(const std::string& name) const
{
        auto db     = m_db_ptr;
  const auto filter = CreateFilter("name", name);
  return db->select("organization", {"id"}, filter).size();
}

bool TermExists(const std::string& name) const
{
        auto db     = m_db_ptr;
  const auto filter = CreateFilter("name", name);
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
  for (const auto& row : m_db_ptr->select("term", {"id"}, CreateFilter("name", term)))
    if (row.first == "id")
      id = row.second;
  return id;
}

std::string GetPerson(const std::string& name)
{
  std::string id{};
  for (const auto& row : m_db_ptr->select("person", {"id"}, CreateFilter("name", name)))
    if (row.first == "id")
      id = row.second;
  return id;
}

std::string GetPersonForUID(const std::string& uid)
{
  std::string id{};
  for (const auto& row : m_db_ptr->select("platform_user", {"pers_id"}, CreateFilter("id", uid)))
    if (row.first == "pers_id")
      id = row.second;
  return id;
}

std::string GetUser(const std::string& name, const std::string& pid)
{
  if (name.empty() || pid.empty())
    throw std::invalid_argument{"Must provide name and platform id"};
  std::string uid;
  auto        db = m_db_ptr;
  QueryFilter filter = CreateFilter("name", name, "pid", pid);
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

std::string RecordTermEvent(const std::string& term, const std::string& user, const std::string& app)
{
  std::string id;
  if (HasBasePlatform(app))
  {
    const auto pid = m_plat_ptr->GetPlatformID(GetBasePlatform(app));
    if (!pid.empty())
    {
      auto uid = GetUser(user, pid);
      if (!uid.empty())
      return SaveTermHit(term, uid);
    }
  }
  return id;
}

std::vector<TermHit> GetTermHits(const std::string& term)
{
  auto db = m_db_ptr;
  const auto DoQuery = [&db](const std::string& tid)
  {
    return db->selectJoin("term_hit", {"term_hit.time", "user.id", "user.name", "organization.id", "organization.name"}, {CreateFilter("term_hit.tid", tid)}, Joins{
      Join{
        .table = "platform_user",
        .field = "platform_user.id",
        .join_table = "term_hit",
        .join_field = "term_hit.uid"
      },
      Join{
        .table = "organization",
        .field = "organization.id",
        .join_table = "term_hit",
        .join_field = "term_hit.oid"
      },
      Join{
        .table = "person",
        .field = "person.id",
        .join_table = "platform_user",
        .join_field = "platform_user.pers_id"
      }
    });
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
