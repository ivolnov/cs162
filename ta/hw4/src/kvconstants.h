#ifndef __KV_CONSTANTS__
#define __KV_CONSTANTS__

/* KVConstants contains general purpose constants for use throughout the
 * project. */

#include <execinfo.h>
#include <stdint.h>
#include <string.h>
#include "md5.h"

#define fatal(msg, code) { \
  fprintf(stderr, "%s\n", msg); \
  void *btarray[10]; \
  int btents = backtrace(btarray, 10); \
  backtrace_symbols_fd(btarray, btents, 2); \
  exit(code); \
}

#define fatal_malloc() \
  fatal("malloc failed", 55);  /* 55 == ENOBUGS */

#define alloc_msg(buf, msg) \
  if (msg) { \
    buf = malloc(strlen(msg) + 1); \
    if (!buf) fatal_malloc(); \
    strcpy(buf, msg); \
  } else { \
    buf = NULL; \
  }

/* Maximum length for keys and values. */
#define MAX_KEYLEN 1024
#define MAX_VALLEN 1024

/* Maximum length for a file name. */
#define MAX_FILENAME 1024

/* Error messages to be used with KVMessage. */
#define MSG_COMMIT "commit"
#define ERRMSG_NO_KEY "error: no key"
#define ERRMSG_KEY_LEN "error: improper key length"
#define ERRMSG_VAL_LEN "error: value too long"
#define ERRMSG_INVALID_REQUEST "error: invalid request"
#define ERRMSG_NOT_IMPLEMENTED "error: not implemented"
#define ERRMSG_NOT_AT_CAPACITY "error: follower_capacity not yet full"
#define ERRMSG_FOLLOWER_CAPACITY "error: follower capacity already full"
#define ERRMSG_GENERIC_ERROR "error: unable to process request"

/* Paths for API endpoints. */
#define COMMIT_PATH MSG_COMMIT
#define ABORT_PATH "abort"
#define REGISTER_PATH "register"

/* Convert an error code to an error message. */
#define GETMSG(error) ((error == ERRKEYLEN) ? ERRMSG_KEY_LEN : \
                      ((error == ERRVALLEN) ? ERRMSG_VAL_LEN : \
                      ((error == ERRNOKEY)  ? ERRMSG_NO_KEY  : \
                                              ERRMSG_GENERIC_ERROR)))


/* Message types for use by KVMessage. */
typedef enum {
  /* Requests */
  INDEX,
  GETREQ,
  PUTREQ,
  DELREQ,
  REGISTER,
  COMMIT,
  ABORT,
  /* Responses */
  GETRESP,
  SUCCESS,
  ERROR,
  VOTE,
  ACK,
  /* Empty (interal error) */
  EMPTY
} msgtype_t;

/* Possible TPC states. */
typedef enum {
  TPC_INIT,
  TPC_WAIT,
  TPC_READY,
  TPC_ABORT,
  TPC_COMMIT
} tpc_state_t;

/* Error types/values */
/* Error for invalid key length. */
#define ERRKEYLEN -11
/* Error for invalid value length. */
#define ERRVALLEN -12
/* Error for invalid (not present) key. */
#define ERRNOKEY -13
/* Error for invalid message type. */
#define ERRINVLDMSG -14
/* Error code used to represent that a filename was too long. */
#define ERRFILLEN -15
/* Error code used to represent that a file could not be created. */
#define ERRFILCRT -16
/* Error returned if error was encountered accessing a file. */
#define ERRFILACCESS -17

/* A 64-bit string hash, based on MD5.
 * Do NOT change this function. */
static inline uint64_t strhash64(const char *str) {
  unsigned char result[16];
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, str, strlen(str));
  MD5_Final(result, &ctx);
  /* This only works for little-endian machines. */
  return *(uint64_t *) result;
}

#endif
