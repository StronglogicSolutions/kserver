#include "session.hpp"
#include "log/logger.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <sstream>

namespace kiq {

bool KSession::active() const
{
  bool active = (!(expired()) && status == SESSION_ACTIVE);
  VLOG("Client {} with username {} is {}", fd, user.name, (active) ? "active" : "not active");
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
  {
    VLOG("Session expired for {} on {}", user.name, fd);
    status = SESSION_INACTIVE;
  }
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

bool SessionMap::init(int32_t fd, const KSession& new_session)
{
  bool result{true};

  if (m_sessions.find(fd) == m_sessions.end())
    m_sessions.emplace(fd, new_session);
  else
  if (logged_in(new_session.user))
    result = false;
  else
    m_sessions[fd] = new_session;

  return result;
}

KSession& SessionMap::at(int32_t fd)
{
  return m_sessions.at(fd);
}

bool SessionMap::logged_in(const User& user) const
{
  for (const auto& [fd, session] : m_sessions)
    if (session.user.name == user.name && session.active())
    {
      KLOG("User {} is already logged in  as client {}", user.name, session.fd);
      return true;
    }
  return false;
}

bool ValidateToken(User user)
{
  auto Expired        = [](const jwt::date&   date) { return std::chrono::system_clock::now() > date;          };
  auto ReadFileSimple = [](const std::string& path) { std::stringstream ss; ss << std::ifstream{path}.rdbuf();
                                                                                              return ss.str(); };
  using Verifier = jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>;
  static const std::string private_key = ReadFileSimple(config::Security::private_key());
  static const std::string public_key  = ReadFileSimple(config::Security::public_key());
  static const std::string path        = config::Security::token_path();
  static const Verifier    verifier    = jwt::verify()
    .allow_algorithm(jwt::algorithm::es256k(public_key, private_key, "", ""))
    .with_issuer    ("kiq");
        const std::string token        = ReadFileSimple(path + '/' + user.name);
  try
  {
    const auto decoded = jwt::decode(user.token);
    verifier.verify(decoded);

    if (decoded.get_payload_claim("user").as_string() != user.name)
      ELOG("Token does not belong to user");
    else
    if (token != user.token)
      ELOG("Token does not match");
    else
    if (Expired(decoded.get_expires_at()))
      ELOG("Token has expired");
    else
    {
      VLOG("Token valid for {}", user.name);
      return true;
    }
  }
  catch(const std::exception& e)
  {
    ELOG("Exception thrown while validating token: {}", e.what());
  }
  return false;
}
} // ns kiq
