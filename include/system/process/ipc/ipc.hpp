#pragma once

#include <string>
#include <vector>
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

namespace index {
const uint8_t EMPTY = 0x01;
const uint8_t TYPE  = 0x02;
const uint8_t DATA  = 0x03;
} // namespace index
} // namespace constants

struct ipc_message
{
ipc_message (uint8_t* data, uint32_t size)
: m_frames{data, data + size} {}

std::vector<uint8_t> m_frames;

std::vector<uint8_t> data() {
  return m_frames;
}

std::string string() {
  return std::string{
    reinterpret_cast<char*>(m_frames.data()),
    reinterpret_cast<char*>(m_frames.data()) + m_frames.size()
  };
}
};
