#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <thread>

/**

            ┌───────────────────────────────────────────────────────────┐
            │░░░░░░░░░░░░░░░░░░░░░ PROTOCOL ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
            │░░░░░░░░        1. Empty              ░░░░░░░░░░░░░░░░░░░░░░░│
            │░░░░░░░░        2. Type               ░░░░░░░░░░░░░░░░░░░░░░░│
            │░░░░░░░░        3. Data               ░░░░░░░░░░░░░░░░░░░░░░░│
            │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
            └───────────────────────────────────────────────────────────┘
 */

namespace constants {
static const uint8_t IPC_OK_TYPE         {0x00};
static const uint8_t IPC_KEEPALIVE_TYPE  {0x01};
static const uint8_t IPC_KIQ_MESSAGE     {0x02};
static const uint8_t IPC_PLATFORM_TYPE   {0x03};
static const uint8_t IPC_PLATFORM_ERROR  {0x04};
static const uint8_t IPC_PLATFORM_REQUEST{0x05};
static const uint8_t IPC_PLATFORM_INFO   {0x06};

static const std::unordered_map<uint8_t, const char*> IPC_MESSAGE_NAMES{
  {IPC_OK_TYPE,          "IPC_OK_TYPE"},
  {IPC_KEEPALIVE_TYPE,   "IPC_KEEPALIVE_TYPE"},
  {IPC_KIQ_MESSAGE,      "IPC_KIQ_MESSAGE"},
  {IPC_PLATFORM_TYPE,    "IPC_PLATFORM_TYPE"},
  {IPC_PLATFORM_ERROR,   "IPC_PLATFORM_ERROR"},
  {IPC_PLATFORM_REQUEST, "IPC_PLATFORM_REQUEST"},
  {IPC_PLATFORM_INFO,    "IPC_PLATFORM_INFO"}
};

namespace index {
static const uint8_t EMPTY     = 0x00;
static const uint8_t TYPE      = 0x01;
static const uint8_t PLATFORM  = 0x02;
static const uint8_t ID        = 0x03;
static const uint8_t INFO      = 0x03;
static const uint8_t INFO_TYPE = 0x04;
static const uint8_t USER      = 0x04;
static const uint8_t DATA      = 0x05;
static const uint8_t URLS      = 0x06;
static const uint8_t REQ_ARGS  = 0x06;
static const uint8_t REPOST    = 0x07;
static const uint8_t ARGS      = 0x08;
static const uint8_t CMD       = 0x09;
static const uint8_t KIQ_DATA  = 0x02;
static const uint8_t ERROR     = 0x05;
} // namespace index

static const uint8_t TELEGRAM_COMMAND_INDEX = 0x00;
static const uint8_t MASTODON_COMMAND_INDEX = 0x01;
static const uint8_t DISCORD_COMMAND_INDEX  = 0x02;
static const uint8_t YOUTUBE_COMMAND_INDEX  = 0x03;
static const uint8_t NO_COMMAND_INDEX       = 0x04;

static const char*   IPC_COMMANDS[]{
"telegram:messages",
"mastodon:comments",
"discord:messages",
"youtube:livestream",
"no:command"
};

} // namespace constants

class ipc_message
{
public:
using byte_buffer   = std::vector<uint8_t>;
using u_ipc_msg_ptr = std::unique_ptr<ipc_message>;
virtual ~ipc_message() {}

uint8_t type() const
{
  return m_frames.at(constants::index::TYPE).front();
}

std::vector<byte_buffer> data() {
  return m_frames;
}

std::vector<byte_buffer> m_frames;

virtual std::string to_string() const
{
  return ::constants::IPC_MESSAGE_NAMES.at(type());
}
};

class platform_error : public ipc_message
{
public:
platform_error(const std::string& name, const std::string& id, const std::string& user, const std::string& error)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_PLATFORM_ERROR},
    byte_buffer{name.data(), name.data() + name.size()},
    byte_buffer{id.data(), id.data() + id.size()},
    byte_buffer{user.data(), user.data() + user.size()},
    byte_buffer{error.data(), error.data() + error.size()}
  };
}

platform_error(const std::vector<byte_buffer>& data)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{data.at(constants::index::TYPE)},
    byte_buffer{data.at(constants::index::PLATFORM)},
    byte_buffer{data.at(constants::index::ID)},
    byte_buffer{data.at(constants::index::USER)},
    byte_buffer{data.at(constants::index::ERROR)}
  };
}

const std::string name() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::PLATFORM).data()),
    m_frames.at(constants::index::PLATFORM).size()
  };
}

const std::string user() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::USER).data()),
    m_frames.at(constants::index::USER).size()
  };
}

const std::string error() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::ERROR).data()),
    m_frames.at(constants::index::ERROR).size()
  };
}

const std::string id() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::ID).data()),
    m_frames.at(constants::index::ID).size()
  };
}

std::string to_string() const override
{
  return  "(Type): "     + ipc_message::to_string() + ',' +
          "(Platform): " + name()                   + ',' +
          "(ID):"       + id()                      + ',' +
          "(User): "     + user()                   + ',' +
          "(Error):"     + error();
}
};

class okay_message : public ipc_message
{
public:
okay_message()
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_OK_TYPE}
  };
}

virtual ~okay_message() override {}
};

class keepalive : public ipc_message
{
public:
keepalive()
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_KEEPALIVE_TYPE}
  };
}

virtual ~keepalive() override {}
};

class kiq_message : public ipc_message
{
public:
kiq_message(const std::string& payload)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_KIQ_MESSAGE},
    byte_buffer{payload.data(), payload.data() + payload.size()}
  };
}

kiq_message(const std::vector<byte_buffer>& data)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{data.at(constants::index::TYPE)},
    byte_buffer{data.at(constants::index::KIQ_DATA)}
  };
}

const std::string payload() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::KIQ_DATA).data()),
    m_frames.at(constants::index::KIQ_DATA).size()
  };
}

std::string to_string() const override
{
  return  "(Type): "    + ipc_message::to_string() + ',' +
          "(Payload): " + payload();
}

};

class platform_message : public ipc_message
{
public:
platform_message(const std::string& platform, const std::string& id, const std::string& user, const std::string& content, const std::string& urls, const bool repost = false, uint32_t cmd = 0x00, const std::string& args = "")
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_PLATFORM_TYPE},
    byte_buffer{platform.data(), platform.data() + platform.size()},
    byte_buffer{id.data(), id.data() + id.size()},
    byte_buffer{user.data(), user.data() + user.size()},
    byte_buffer{content.data(), content.data() + content.size()},
    byte_buffer{urls.data(),    urls.data() + urls.size()},
    byte_buffer{static_cast<uint8_t>(repost)},
    byte_buffer{args.data(), args.data() + args.size()},
    byte_buffer{static_cast<unsigned char>((cmd >> 24) & 0xFF),
                static_cast<unsigned char>((cmd >> 16) & 0xFF),
                static_cast<unsigned char>((cmd >> 8 ) & 0xFF),
                static_cast<unsigned char>((cmd      ) & 0xFF)}
  };
}

platform_message(const std::vector<byte_buffer>& data)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{data.at(constants::index::TYPE)},
    byte_buffer{data.at(constants::index::PLATFORM)},
    byte_buffer{data.at(constants::index::ID)},
    byte_buffer{data.at(constants::index::USER)},
    byte_buffer{data.at(constants::index::DATA)},
    byte_buffer{data.at(constants::index::URLS)},
    byte_buffer{data.at(constants::index::REPOST)},
    byte_buffer{data.at(constants::index::ARGS)},
    byte_buffer{data.at(constants::index::CMD)}
  };
}

virtual ~platform_message() override {}

const std::string platform() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::PLATFORM).data()),
    m_frames.at(constants::index::PLATFORM).size()
  };
}

const std::string id() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::ID).data()),
    m_frames.at(constants::index::ID).size()
  };
}

const std::string user() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::USER).data()),
    m_frames.at(constants::index::USER).size()
  };
}

const std::string content() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::DATA).data()),
    m_frames.at(constants::index::DATA).size()
  };
}

const std::string urls() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::URLS).data()),
    m_frames.at(constants::index::URLS).size()
  };
}

const bool repost() const
{
  return (m_frames.at(constants::index::REPOST).front() != 0x00);
}

const std::string args() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::ARGS).data()),
    m_frames.at(constants::index::ARGS).size()
  };
}

const uint32_t cmd() const
{
  auto bytes = m_frames.at(constants::index::CMD).data();
  auto cmd   = static_cast<uint32_t>(bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3]);

  return cmd;
}

std::string to_string() const override
{
  auto text = content();
  if (text.size() > 120) text = text.substr(0, 120);
  return  "(Type):" + ipc_message::to_string()   + ',' +
          "(Platform):" + platform()             + ',' +
          "(ID):" + id()                         + ',' +
          "(User):" + user()                     + ',' +
          "(Content):" + text                    + ',' +
          "(URLS):" + urls()                     + ',' +
          "(Repost):" + std::to_string(repost()) + ',' +
          "(Args):" + args()                     + ',' +
          "(Cmd):" + std::to_string(cmd());
}

};

class platform_request : public ipc_message
{
public:
platform_request(const std::string& platform, const std::string& id, const std::string& user, const std::string& data, const std::string& args)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_PLATFORM_REQUEST},
    byte_buffer{platform.data(), platform.data() + platform.size()},
    byte_buffer{id.data(), id.data() + id.size()},
    byte_buffer{user.data(), user.data() + user.size()},
    byte_buffer{data.data(), data.data() + data.size()},
    byte_buffer{args.data(), args.data() + args.size()}
  };
}

platform_request(const std::vector<byte_buffer>& data)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{data.at(constants::index::TYPE)},
    byte_buffer{data.at(constants::index::PLATFORM)},
    byte_buffer{data.at(constants::index::ID)},
    byte_buffer{data.at(constants::index::USER)},
    byte_buffer{data.at(constants::index::DATA)},
    byte_buffer{data.at(constants::index::REQ_ARGS)}
  };
}

const std::string platform() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::PLATFORM).data()),
    m_frames.at(constants::index::PLATFORM).size()
  };
}

const std::string id() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::ID).data()),
    m_frames.at(constants::index::ID).size()
  };
}

const std::string user() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::USER).data()),
    m_frames.at(constants::index::USER).size()
  };
}

const std::string content() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::DATA).data()),
    m_frames.at(constants::index::DATA).size()
  };
}

const std::string args() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::REQ_ARGS).data()),
    m_frames.at(constants::index::REQ_ARGS).size()
  };
}

std::string to_string() const override
{
  auto text = content();
  if (text.size() > 120) text = text.substr(0, 120);
  return  "(Type): "     + ipc_message::to_string() + ',' +
          "(Platform): " + platform()               + ',' +
          "(ID): "       + id()                     + ',' +
          "(User): "     + user()                   + ',' +
          "(Content): "  + text                     + ',' +
          "(Args): "     + args();
}
};

class platform_info : public ipc_message
{
public:
platform_info(const std::string& platform, const std::string& info, const std::string& type)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_PLATFORM_INFO},
    byte_buffer{platform.data(), platform.data() + platform.size()},
    byte_buffer{info.data(), info.data() + info.size()},
    byte_buffer{type.data(), type.data() + type.size()}
  };
}

platform_info(const std::vector<byte_buffer>& data)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{data.at(constants::index::TYPE)},
    byte_buffer{data.at(constants::index::PLATFORM)},
    byte_buffer{data.at(constants::index::INFO)},
    byte_buffer{data.at(constants::index::INFO_TYPE)}
  };
}

const std::string platform() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::PLATFORM).data()),
    m_frames.at(constants::index::PLATFORM).size()
  };
}

const std::string info() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::INFO).data()),
    m_frames.at(constants::index::INFO).size()
  };
}

const std::string type() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::INFO_TYPE).data()),
    m_frames.at(constants::index::INFO_TYPE).size()
  };
}

std::string to_string() const override
{
  return  "(Type):"    + ipc_message::to_string() + ',' +
          "(Platform)" + platform()               + ',' +
          "(Type):"    + type()                   + ',' +
          "(Info):"    + info();
}
};

static ipc_message::u_ipc_msg_ptr DeserializeIPCMessage(std::vector<ipc_message::byte_buffer>&& data)
{
   uint8_t message_type = *(data.at(constants::index::TYPE).data());

   switch (message_type)
   {
    case (constants::IPC_OK_TYPE):          return std::make_unique<okay_message>();
    case (constants::IPC_KEEPALIVE_TYPE):   return std::make_unique<keepalive>();
    case (constants::IPC_KIQ_MESSAGE):      return std::make_unique<kiq_message>(data);
    case (constants::IPC_PLATFORM_TYPE):    return std::make_unique<platform_message>(data);
    case (constants::IPC_PLATFORM_INFO):    return std::make_unique<platform_info>(data);
    case (constants::IPC_PLATFORM_ERROR):   return std::make_unique<platform_error>(data);
    case (constants::IPC_PLATFORM_REQUEST): return std::make_unique<platform_request>(data);
    default:                                return nullptr;
   }
}

using timepoint = std::chrono::time_point<std::chrono::system_clock>;
using duration  = std::chrono::milliseconds;
static const duration time_limit = std::chrono::milliseconds(60000);
static const duration hb_rate    = std::chrono::milliseconds(600);
class session_daemon {
public:
  session_daemon()
  : m_active(false),
    m_valid(true)
  {
    m_future = std::async(std::launch::async, [this] { while (true) loop(); });
  }

  ~session_daemon()
  {
    if (m_future.valid())
      m_future.wait();
  }

  void add_observer(std::string_view peer, std::function<void()> callback)
  {
    observer_t observer{hbtime_t{}, callback};
    m_observers.try_emplace(peer, observer);
    m_observers.at(peer).first.first = std::chrono::system_clock::now();
  }

  void reset()
  {
    if (!m_active) m_active = true;
    m_tp = std::chrono::system_clock::now();
  }

  bool validate(std::string_view peer)
  {
    if (m_active)
    {
      if (auto it = m_observers.find(peer); it != m_observers.end())
      {
        duration & interval = it->second.first.second;
        timepoint& tpoint   = it->second.first.first;
        const auto now      = std::chrono::system_clock::now();
                   interval = std::chrono::duration_cast<duration>(now - tpoint);
                   tpoint   = now;
        if (interval < time_limit)
          return true;
        else
          it->second.second();
      }
    }
    return false;
  }

  void stop()
  {
    m_active = false;
    m_valid  = true;
  }

  bool active() const
  {
    return m_active;
  }

  void loop()
  {
    for (const auto& [_, observer] : m_observers)
      if (observer.first.second > time_limit)
        observer.second();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

private:
  using hbtime_t    = std::pair<timepoint, duration>;
  using observer_t  = std::pair<hbtime_t, std::function<void()>>;
  using observers_t = std::map<std::string_view, observer_t>;

  timepoint         m_tp;
  duration          m_duration;
  bool              m_active;
  bool              m_valid;
  observers_t       m_observers;
  std::future<void> m_future;

};
