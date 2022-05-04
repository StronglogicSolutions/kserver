#pragma once

#include <unordered_map>
#include "common/time.hpp"
#include "codec/uuid.h"

namespace kiq {
using namespace uuids;

static const int32_t SESSION_ACTIVE   = 1;
static const int32_t SESSION_INACTIVE = 2;
static const int32_t SESSION_INVALID  = 3;

/**
 * User
 */

struct User
{
std::string name;
std::string token;
};

struct KSession {
using TimePoint = Timer::TimePoint;
User      user;
int32_t   fd;
int32_t   status;
uuid      id;
uint32_t  tx{0};
uint32_t  rx{0};
TimePoint last_ping;

bool        active()       const;
void        notify();
void        verify();
bool        expired()      const;
std::string info()         const;
uint64_t    waiting_time() const;
};

struct SessionMap
{
public:
using Sessions = std::unordered_map<std::string, KSession>;
using FDMap    = std::unordered_map<int32_t, KSession*>;
Sessions::const_iterator begin()                                                    const;
Sessions::const_iterator end()                                                      const;
FDMap::const_iterator    fdend()                                                    const;
Sessions::iterator       begin();
Sessions::iterator       end();
Sessions::iterator       erase(Sessions::iterator);
Sessions::const_iterator find(const std::string& name)                              const;
FDMap::const_iterator    find(int32_t fd)                                           const;
bool                     has (const std::string& name)                              const;
bool                     has (int32_t fd)                                           const;
bool                     init(const std::string& name, const KSession& new_session);
KSession&                at  (const std::string& name);
KSession&                at(int32_t fd);
bool                     logged_in(const User& user)                                const;

private:

Sessions m_sessions;
FDMap    m_session_ptrs;
};

bool ValidateToken(User user);

} // ns kiq
