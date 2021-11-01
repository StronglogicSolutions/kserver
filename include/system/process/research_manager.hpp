#include "platform.hpp"

struct TermHit
{
std::string person;
std::string organization;
std::string time;
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

std::string GetPersonForUID(const std::string& uid)
{
  std::string id{};
  for (const auto& row : m_db_ptr->select("platform_user", {"pers_id"}, QueryFilter{{"id", uid}}))
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

std::string RecordTermEvent(const std::string& term, const std::string& user, const std::string& app)
{
  std::string id;
  auto pid = m_plat_ptr->GetPlatformID(app);
  if (pid.empty()) return id;

  auto uid = GetUser(user, pid);
  if (uid.empty()) return id;

  return SaveTermHit(term, uid);
}

std::vector<TermHit> GetTermHits(const std::string& term, bool insert = true)
{
  auto db = m_db_ptr;
  const auto DoQuery = [&db](const std::string& tid)
  {
    return db->selectJoin("term_hit", {"term_hit.time", "user.id", "user.name", "organization.id", "organization.name"}, {QueryFilter{{"term_hit.tid", tid}}}, Joins{
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
      }
    });
  };

  std::vector<TermHit> result{};
  std::string tid;
  if (!TermExists(term))
  {
    if (!insert)
      return result;
    tid = AddTerm(term);
  }
  else
    tid = GetTerm(term);

  if (tid.size())
  {
    std::string time, person, organization;
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

      if (DataUtils::NoEmptyArgs(time, person, organization))
      {
        KLOG("Term hit was discovered for {}:\nPerson: {}\nOrg: {}\nTime: {}", person, organization, time);
        result.emplace_back(TermHit{person, organization, time});
        DataUtils::ClearArgs(time, person, organization);
      }
    }
  }
  return result;
}

std::string AddUser(const std::string& name, const std::string& pid, const std::string& pers_id, const std::string& type);
std::string AddOrganization();
std::string AddTerm(const std::string& name) { return ""; } // TODO: Implement!
std::string AddAffiliation();
void        GetOrganization();
void        GetAffiliation();
void        FindPeople();
void        FindOrganizations();
void        FindAffiliations();
void        AnalyzeTermHit(const std::string& term, const std::string& hid)
{
  auto hits = GetTermHits(term);
}

private:
Database* m_db_ptr;
Platform* m_plat_ptr;
};
