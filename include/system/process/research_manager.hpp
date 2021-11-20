#pragma once

#include "platform.hpp"
#include "result_parser.hpp"

using JSONItem = KNLPResultParser::NLPItem;
class ResearchManager
{
public:
ResearchManager(Database::KDB* db_ptr, Platform* plat_ptr);
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

bool        valid() const;
std::string ToString(const bool verbose = false) const;
std::string ToJSON()                             const;

static std::string NToString(const std::vector<TermEvent>& events);
};

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

std::string            GetTerm(const std::string& term);
std::string            GetPerson(const std::string& name);
Person                 GetPersonForUID(const std::string& uid);
Identity               GetIdentity(const std::string& name, const std::string& pid);
std::string            GetUser(const std::string& name, const std::string& pid);
std::vector<TermHit>   GetTermHits(const std::string& term);
std::vector<TermEvent> GetAllTermEvents() const;
void                   GetOrganization();
void                   GetAffiliation();

std::string            SaveTermHit(const JSONItem& term, const std::string& uid, const std::string& sid);
void                   AnalyzeTermHit(const TermHit& hit, const Task& task);
TermEvent              RecordTermEvent(JSONItem&& term, const std::string& user, const std::string& app, const Task& task);

bool                   OrganizationExists(const std::string& name) const;
bool                   PersonExists(const std::string& name) const;
bool                   UserExists(const std::string& name = "", const std::string& id = "") const;
bool                   TermExists(const std::string& name) const;
bool                   TermHasHits(const std::string& term);

private:
Database::KDB* m_db_ptr;
Platform*      m_plat_ptr;
};
