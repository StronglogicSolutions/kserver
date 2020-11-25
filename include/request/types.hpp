#ifndef __REQUEST_TYPES_HPP__
#define __REQUEST_TYPES_HPP__

#include <string>
#include <cstring>

namespace Request {
namespace constants {
const uint8_t REQUEST_TYPE_INDEX = 0x00;
} // namespace constants

enum RequestType {
  REGISTER_APPLICATION = 0x00,
  UPDATE_APPLICATION   = 0x01,
  REMOVE_APPLICATION   = 0x02,
  GET_APPLICATION      = 0x03,
  UNKNOWN              = 0x04
};

/**
 * int_to_request_type
 *
 * @param [in] {int} Signed integer should represent a byte value
 */
RequestType int_to_request_type(int byte) {
  if (byte == REGISTER_APPLICATION) {
    return REGISTER_APPLICATION;
  }
  else
  if (byte == UPDATE_APPLICATION)
  {
    return UPDATE_APPLICATION;
  }
  else
  if (byte == REMOVE_APPLICATION)
  {
    return REMOVE_APPLICATION;
  }
  else
  if (byte == GET_APPLICATION)
  {
    return GET_APPLICATION;
  }
  return UNKNOWN;
}
} // namespace Request
#endif  // __REQUEST_TYPES_HPP__
