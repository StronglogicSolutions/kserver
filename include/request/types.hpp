#pragma once

#include <string>
#include <cstring>

namespace Request {

const uint8_t REQUEST_TYPE_INDEX = 0x00;

enum RequestType {
  REGISTER_APPLICATION  = 0x00,
  UPDATE_APPLICATION    = 0x01,
  REMOVE_APPLICATION    = 0x02,
  GET_APPLICATION       = 0x03,
  FETCH_SCHEDULE        = 0x04,
  UPDATE_SCHEDULE       = 0x05,
  FETCH_SCHEDULE_TOKENS = 0x06,
  TRIGGER_CREATE        = 0x07,
  TASK_FLAGS            = 0x08,
  FETCH_FILE            = 0x09,
  UNKNOWN               = 0x0A
};

/**
 * int_to_request_type
 *
 * @param [in] {int} Signed integer should represent a byte value
 */
static RequestType int_to_request_type(int byte) {
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
  if (byte == FETCH_SCHEDULE)
  {
    return FETCH_SCHEDULE;
  }
  if (byte == UPDATE_SCHEDULE)
  {
    return UPDATE_SCHEDULE;
  }
  if (byte == FETCH_SCHEDULE_TOKENS)
  {
    return FETCH_SCHEDULE_TOKENS;
  }
  if (byte == TASK_FLAGS)
    return TASK_FLAGS;
  if (byte == FETCH_FILE)
    return FETCH_FILE;
  return UNKNOWN;
}
} // namespace Request
