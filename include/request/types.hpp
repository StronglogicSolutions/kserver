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
  FETCH_FILE_ACK        = 0x0A,
  FETCH_TASK_DATA       = 0x0B,
  UNKNOWN               = 0x0C
};

/**
 * int_to_request_type
 *
 * @param [in] {int} Signed integer should represent a byte value
 */
static RequestType int_to_request_type(int byte) {
  switch (byte)
  {
    case(REGISTER_APPLICATION):
      return REGISTER_APPLICATION;

    case (UPDATE_APPLICATION):
      return UPDATE_APPLICATION;

    case (REMOVE_APPLICATION):
      return REMOVE_APPLICATION;

    case (GET_APPLICATION):
      return GET_APPLICATION;

    case (FETCH_SCHEDULE):
      return FETCH_SCHEDULE;

    case (UPDATE_SCHEDULE):
      return UPDATE_SCHEDULE;

    case (FETCH_SCHEDULE_TOKENS):
      return FETCH_SCHEDULE_TOKENS;

    case (TASK_FLAGS):
      return TASK_FLAGS;

    case (FETCH_FILE):
      return FETCH_FILE;

    case (FETCH_FILE_ACK):
      return FETCH_FILE_ACK;

    case (FETCH_TASK_DATA):
      return FETCH_TASK_DATA;
  }
  return UNKNOWN;
}
} // namespace Request
