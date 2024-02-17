#include "session.hpp"
#include "config/config_parser.hpp"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <sstream>
#include <logger.hpp>

namespace kiq {
using namespace log;

bool KSession::active() const
{
  bool active = (!(expired()) && status == SESSION_ACTIVE);
  klog().t("{} is {} on {}", user.name, (active) ? "active" : "not active", fd);
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
    klog().t("Session expired for {} on {}", user.name, fd);
    status = SESSION_INACTIVE;
  }
}

bool KSession::expired() const
{
  return (waiting_time() > Timer::ONE_MINUTE);
}

uint64_t KSession::waiting_time() const
{
  using Duration = Timer::Duration;
  return std::chrono::duration_cast<Duration>(std::chrono::system_clock::now() - last_ping).count();
}

std::string KSession::info() const
{
  auto GetString = [](const int32_t& status)
  { switch(status) { case(1): return "ACTIVE"; case (2): return "INACTIVE"; default: return "INVALID"; } };
  return fmt::format(
    "┌──────────────────────────────────────────────┐\n"\
    " User:   {}\n FD:     {}\n Status: {}\n ID:     {}\n TX:     {}\n RX:     {}\n Ping:   {} ms\n"\
    "└──────────────────────────────────────────────┘\n",
    user.name, fd, GetString(status), uuids::to_string(id), tx, rx, waiting_time());
}

SessionMap::Sessions::const_iterator SessionMap::begin() const
{
  return m_sessions.begin();
}

SessionMap::Sessions::const_iterator SessionMap::end() const
{
  return m_sessions.cend();
}

SessionMap::FDMap::const_iterator SessionMap::fdend() const
{
  return m_session_ptrs.cend();
}

SessionMap::Sessions::iterator SessionMap::begin()
{
  return m_sessions.begin();
}

SessionMap::Sessions::iterator SessionMap::end()
{
  return m_sessions.end();
}

SessionMap::Sessions::iterator SessionMap::erase(SessionMap::Sessions::iterator it)
{
  klog().t("SessionMap erasing FD {} and Name {}", it->second.fd, it->first);
  if (auto ptr_it = find(it->second.fd); ptr_it != m_session_ptrs.end())
    m_session_ptrs.erase(ptr_it);
  else
    klog().e("attempted to end session for {}, but was unable to find session pointer. Incomplete erasure", it->first);
  return m_sessions.erase(it);
}

SessionMap::Sessions::const_iterator SessionMap::find(const std::string& name) const
{
  return m_sessions.find(name);
}

SessionMap::FDMap::const_iterator SessionMap::find(int32_t fd) const
{
  return m_session_ptrs.find(fd);
}

bool SessionMap::has(const std::string& name) const
{
  return (find(name) != m_sessions.end());
}

bool SessionMap::has(int32_t fd) const
{
  return (find(fd) != m_session_ptrs.end());
}

bool SessionMap::init(const std::string& name, const KSession& new_session)
{
  bool result{true};

  klog().d("Initializing session for {}", name);

  if (!has(name))
  {
    klog().d("Adding new session");
    m_sessions.emplace(name, new_session);
  }
  else
  if (logged_in(new_session.user))
  {
    klog().d("Already logged in");
    result = false;
  }
  else
  {
    // m_session_ptrs.erase(m_sessions[name].fd);
    klog().d("Session exists but not logged in. Replacing with new session");
    m_sessions[name] = new_session;
  }

  if (result)
  {
    klog().d("Updating session_ptrs for {}. Setting session active and starting ping", new_session.fd);
    m_session_ptrs[new_session.fd] = &m_sessions[name];
    m_sessions[name].notify();
  }
  else
  {
    klog().w("Erasing {} from sessions", name);
    m_sessions.erase(name);
  }

  return result;
}

KSession& SessionMap::at(const std::string& name)
{
  const auto it = m_sessions.find(name);
  if (it == m_sessions.end())
    klog().e("SessionMap::at() Unable to find {}. Exception will be thrown", name);
  return m_sessions.at(name);
}

KSession& SessionMap::at(int32_t fd)
{
  const auto it = m_session_ptrs.find(fd);
  if (it == m_session_ptrs.end())
    throw kiq_error{fmt::format("SessionMap::at() Unable to find {}. Exception will be thrown", fd)};
  return *m_session_ptrs.at(fd);
}

bool SessionMap::logged_in(const User& user) const
{
  for (const auto& [fd, session] : m_sessions)
    if (session.user.name == user.name && session.active())
    {
      klog().i("User {} is already logged in  as client {}", user.name, session.fd);
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
         const std::string token       = ReadFileSimple(path + '/' + user.name);
  try
  {
    const auto decoded = jwt::decode(user.token);
    verifier.verify(decoded);

    if (decoded.get_payload_claim("user").as_string() != user.name)
      klog().e("Token does not belong to user");
    else
    if (token != user.token)
      klog().e("Token does not match");
    else
    if (Expired(decoded.get_expires_at()))
      klog().e("Token has expired");
    else
    {
      klog().t("Token valid for {}", user.name);
      return true;
    }
  }
  catch(const std::exception& e)
  {
    klog().e("Exception thrown while validating token: {}", e.what());
  }
  return false;
}
} // ns kiq
