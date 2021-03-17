#pragma once

/**
 * Values used when attempting to open a socket
 */
static const int SOCKET_ERROR = -1;
static const int SOCKET_OK = 0;
/**
 * Values used when listening for connections to a socket
 */
static const int WAIT_SOCKET_FAILURE = -1;
static const int WAIT_SOCKET_SUCCESS = 0;

#define MAX_BUFFER_SIZE (16384)
#define SMALL_BUFFER_SIZE (8192)

static const uint8_t STR_NOT_FOUND = -1;
