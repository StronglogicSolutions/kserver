#include "session.hpp"
#include "log/logger.h"

namespace kiq {

bool KSession::active() const
{
  bool active = (!(expired()) && status == SESSION_ACTIVE);
  KLOG("Client {} with username {} is {}", fd, user, (active) ? "active" : "not active");
  return active;
}

void KSession::notify()
{
  last_ping = std::chrono::system_clock::now();
  status    = SESSION_ACTIVE;
}

void KSession::verify()
{
  if (expired())
    status = SESSION_INACTIVE;
}

bool KSession::expired() const
{
  return (waiting_time() > Timer::ONE_MINUTE);
}

uint32_t KSession::waiting_time() const
{
  using Duration = Timer::Duration;
  const TimePoint now     = std::chrono::system_clock::now();
  const int64_t   elapsed = std::chrono::duration_cast<Duration>(now - last_ping).count();
  return elapsed;
}

std::string KSession::info() const
{
  auto GetString = [](const int32_t& status)
  { switch(status) { case(1): return "ACTIVE"; case (2): return "INACTIVE"; default: return "INVALID"; } };
  return fmt::format(
    "┌──────────────────────────────────────────────┐\n"\
    "User:   {}\nFD:     {}\nStatus: {}\nID:     {}\nTX:     {}\nRX:     {}\nPing:   {} ms\n"\
    "└──────────────────────────────────────────────┘\n",
    user.name, fd, GetString(status), uuids::to_string(id), tx, rx, waiting_time());
}

SessionMap::Sessions::const_iterator SessionMap::begin() const
{
  return m_sessions.begin();
}

SessionMap::Sessions::const_iterator SessionMap::end() const
{
  return m_sessions.end();
}

SessionMap::Sessions::iterator SessionMap::begin()
{
  return m_sessions.begin();
}

SessionMap::Sessions::iterator SessionMap::end()
{
  return m_sessions.end();
}

SessionMap::Sessions::const_iterator SessionMap::find(int32_t fd) const
{
  return m_sessions.find(fd);
}

bool SessionMap::has(int32_t fd) const
{
  return (find(fd) != m_sessions.end());
}

void SessionMap::init(int32_t fd, const KSession& new_session)
{
  if (m_sessions.find(fd) == m_sessions.end())
    m_sessions.emplace(fd, new_session);
  else
    m_sessions[fd] = new_session;
}

KSession& SessionMap::at(int32_t fd)
{
  return m_sessions.at(fd);
}

} // ns kiq
