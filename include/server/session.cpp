#include "session.hpp"

namespace kiq {

bool KSession::active() const
{
  return (!(expired()) && status == SESSION_ACTIVE);
}

void KSession::notify()
{
  last_ping = std::chrono::system_clock::now();
}

void KSession::verify()
{
  if (expired())
    status = SESSION_INACTIVE;
}

bool KSession::expired() const
{
  using Duration = Timer::Duration;
  const TimePoint now     = std::chrono::system_clock::now();
  const int64_t   elapsed = std::chrono::duration_cast<Duration>(now - last_ping).count();
  return (elapsed > Timer::ONE_MINUTE);
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
