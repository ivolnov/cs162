#ifndef __KV_MESSAGE__
#define __KV_MESSAGE__

#include "kvconstants.h"

/* Contains struct and methods for KVRequest and KVResponse, our internal
 * representation of API messages.  */

typedef struct {
  /* The type of this request. */
  msgtype_t type;
  /* The key this request addresses. May be NULL, depending on type. */
  char *key;
  /* The value this request holds. May be NULL, depending on type. */
  char *val;
} kvrequest_t;

typedef struct {
  /* The type of this response. */
  msgtype_t type;
  /* The body of this response. May be NULL, depending on type. */
  char *body;
} kvresponse_t;

/* Recieves an HTTP request on SOCKFD and unmarshalls it into a KVRequest. */
kvrequest_t *kvrequest_recieve(int sockfd);
/* Recieves an HTTP response on SOCKFD and unmarshalls it into a KVResponse. */
kvresponse_t *kvresponse_recieve(int sockfd);

/* Marshalls a KVRequest or KVResponse into a HTTP message, respectively, and
 * sends it on SOCKFD. */
int kvrequest_send(kvrequest_t *, int sockfd);
int kvresponse_send(kvresponse_t *, int sockfd);

void kvrequest_free(kvrequest_t *);
void kvresponse_free(kvresponse_t *);

#endif
