#ifndef __SOCKETSERVER__
#define __SOCKETSERVER__

#include "kvserver.h"
#include "tpcleader.h"
#include "wq.h"

/* Socket Server defines helper functions for communicating over sockets.
 *
 * connect_to can be used to make a request to a listening host. You will not
 * need to modify this, but you will likely want to utilize it.
 *
 * server_run can be used to start a server (containing a TPCLeader or KVServer)
 * listening on a given port. See the comment above server_run for more information.
 *
 * The server struct stores extra information on top of the stored TPCLeader or
 * KVServer.
 */

typedef struct server {
  /* If this server represents a TPC Leader. */
  int leader;
  /* If this server is currently listening. */
  int listening;
  /* The socket fd this server is operating on. */
  int sockfd;
  /* The maximum number of concurrent jobs that can run. */
  int max_threads;
  /* The port this server will listen on. */
  int port;
  /* The hostname this server will listen on. */
  char *hostname;
  /* The work queue this server will use to process jobs. */
  wq_t wq;
  /* The kvserver OR tpcleader this server represents. */
  union {
    kvserver_t kvserver;
    tpcleader_t tpcleader;
  };
} server_t;

int connect_to(const char *host, int port, int timeout);
int server_run(const char *hostname, int port, server_t *server);
void server_stop(server_t *server);

#endif
