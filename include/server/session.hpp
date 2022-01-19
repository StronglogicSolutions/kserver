#pragma once

#include <unordered_map>
#include "common/time.hpp"
#include "codec/uuid.h"

namespace kiq {
using namespace uuids;

static const int32_t READY_STATUS{0x01};
static const int32_t SESSION_ACTIVE   = 1;
static const int32_t SESSION_INACTIVE = 2;

struct KSession {
using TimePoint = Timer::TimePoint;
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
uint32_t    waiting_time() const;
};

struct SessionMap
{
public:
using Sessions     = std::unordered_map<int32_t, KSession>;
SessionMap::Sessions::const_iterator begin()                                       const;
SessionMap::Sessions::const_iterator end()                                         const;
SessionMap::Sessions::iterator       begin();
SessionMap::Sessions::iterator       end();
SessionMap::Sessions::const_iterator find(int32_t fd)                              const;
bool                                 has(int32_t fd)                               const;
void                                 init(int32_t fd, const KSession& new_session);
KSession&                            at(int32_t fd);

private:

Sessions m_sessions;
};

} // ns kiq
