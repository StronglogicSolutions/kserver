#pragma once

#include <string>
#include <vector>
#include <memory>
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
const uint8_t IPC_OK_TYPE      {0x00};
const uint8_t KIQ_MESSAGE      {0x01};
const uint8_t IPC_PLATFORM_TYPE{0x02};

namespace index {
const uint8_t EMPTY  = 0x00;
const uint8_t TYPE   = 0x01;
const uint8_t NAME   = 0x02;
const uint8_t ID     = 0x03;
const uint8_t DATA   = 0x04;
const uint8_t URLS   = 0x05;
const uint8_t REPOST = 0x06;
const uint8_t KIQ_DATA = 0x02;
} // namespace index
} // namespace constants

class ipc_message
{
public:
using byte_buffer = std::vector<uint8_t>;
using u_ipc_msg_ptr = std::unique_ptr<ipc_message>;
virtual ~ipc_message() {}

uint8_t type()
{
  return m_frames.at(constants::index::TYPE).front();
}

std::vector<byte_buffer> data() {
  return m_frames;
}

std::vector<byte_buffer> m_frames;
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

class kiq_message : public ipc_message
{
public:
kiq_message(const std::string& payload)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::KIQ_MESSAGE},
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

const std::string payload()
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::KIQ_DATA).data()),
    m_frames.at(constants::index::KIQ_DATA).size()
  };
}

};

class platform_message : public ipc_message
{
public:
platform_message(const std::string& name, const std::string& id, const std::string& content, const std::string& urls, const bool repost = false)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{constants::IPC_PLATFORM_TYPE},
    byte_buffer{name.data(), name.data() + name.size()},
    byte_buffer{id.data(), id.data() + id.size()},
    byte_buffer{content.data(), content.data() + content.size()},
    byte_buffer{urls.data(), urls.data() + urls.size()},
    byte_buffer{static_cast<uint8_t>(repost)}
  };
}

platform_message(const std::vector<byte_buffer>& data)
{
  m_frames = {
    byte_buffer{},
    byte_buffer{data.at(constants::index::TYPE)},
    byte_buffer{data.at(constants::index::NAME)},
    byte_buffer{data.at(constants::index::ID)},
    byte_buffer{data.at(constants::index::DATA)},
    byte_buffer{data.at(constants::index::URLS)},
    byte_buffer{data.at(constants::index::REPOST)}
  };
}

virtual ~platform_message() override {}

const std::string name() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::NAME).data()),
    m_frames.at(constants::index::NAME).size()
  };
}

const std::string id() const
{
  return std::string{
    reinterpret_cast<const char*>(m_frames.at(constants::index::ID).data()),
    m_frames.at(constants::index::ID).size()
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
};

ipc_message::u_ipc_msg_ptr DeserializeIPCMessage(std::vector<ipc_message::byte_buffer>&& data)
{
   uint8_t message_type = *(data.at(constants::index::TYPE).data());

   if (message_type == constants::IPC_PLATFORM_TYPE)
     return std::make_unique<platform_message>(data);
   if (message_type == constants::IPC_OK_TYPE)
    return std::make_unique<okay_message>();
   if (message_type == constants::KIQ_MESSAGE)
    return std::make_unique<kiq_message>(data);

   return nullptr;
}
