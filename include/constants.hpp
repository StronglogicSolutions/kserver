#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__
#ifndef TRX_SOCKET_CONSTANTS
#define TRX_SOCKET_CONSTANTS 1
/**
 * Values used when attempting to open a socket
 */
const int SOCKET_ERROR = -1;
const int SOCKET_OK = 0;
/**
 * Values used when listening for connections to a socket
 */
const int WAIT_SOCKET_FAILURE = -1;
const int WAIT_SOCKET_SUCCESS = 0;

#endif
#endif  // __CONSTANTS_H__
