#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "libhttp.h"
#include "kvmessage.h"
#include "liburl.h"

/* Receives an HTTP request from socket SOCKFD; returns a decoded KVMessage.
 * Returns NULL if there is an error. */
kvrequest_t *kvrequest_recieve(int sockfd) {
  kvrequest_t *kvreq = calloc(1, sizeof(kvrequest_t));
  kvreq->type = EMPTY;

  struct url_params *params = NULL;
  struct http_request *req = http_request_parse(sockfd);
  if (!req) goto error;
  params = url_decode(req->path);
  if (!params) goto error;

  if (!strcmp(req->method, "GET")) {
    if (!params->key) {
      kvreq->type = INDEX;
    } else {
      kvreq->type = GETREQ;
    }
  } else if (!strcmp(req->method, "PUT")) {
    if (!params->key || !params->val) goto error;
    kvreq->type = PUTREQ;
  } else if (!strcmp(req->method, "DELETE")) {
    if (!params->key) goto error;
    kvreq->type = DELREQ;
  } else if (!strcmp(req->method, "POST")) {  /* REGISTER, COMMIT, ABORT */
    if (!params->path) goto error;
    if (!strcmp(params->path, REGISTER_PATH)) {
      if (!params->key || !params->val) goto error;
      kvreq->type = REGISTER;
    } else if (!strcmp(params->path, COMMIT_PATH)) {
      kvreq->type = COMMIT;
    } else if (!strcmp(params->path, ABORT_PATH)) {
      kvreq->type = ABORT;
    }
  }
  if (kvreq->type == EMPTY)
    goto error;
  kvreq->key = params->key;
  kvreq->val = params->val;

  http_request_free(req);
  free(params->path);
  free(params);
  return kvreq;

error:
  http_request_free(req);
  url_params_free(params);
  kvrequest_free(kvreq);
  return NULL;
}

/* Maps HTTP response codes to their corresponding msgtype_t for KVMessages.
 * Returns EMPTY if the status code isn't supported. */
static msgtype_t kvresponse_get_status_code(short status) {
  switch (status) {
    case 200:
      return GETRESP;
    case 201:
      return SUCCESS;
    case 500:
      return ERROR;
    case 202:
      return VOTE;
    case 204:
      return ACK;
    default:
      return EMPTY;
  }
}

/* Receives an HTTP response from socket SOCKFD; returns a decoded KVMessage.
 * Returns NULL if there is an error. */
kvresponse_t *kvresponse_recieve(int sockfd) {
  kvresponse_t *kvres = calloc(1, sizeof(kvresponse_t));

  struct http_response *res = http_response_parse(sockfd);
  if (!res) goto error;
  kvres->type = kvresponse_get_status_code(res->status);
  if (kvres->type == EMPTY) goto error;
  kvres->body = res->body;

  free(res);
  return kvres;

error:
  http_response_free(res);
  kvresponse_free(kvres);
  return NULL;
}

char *http_method_for_request_type(msgtype_t type) {
  switch (type) {
    case GETREQ:
      return "GET";
    case PUTREQ:
      return "PUT";
    case DELREQ:
      return "DELETE";
    case REGISTER:
    case COMMIT:
    case ABORT:
      return "POST";
    default:
      return NULL;
  }
}

char *path_for_request_type(msgtype_t type) {
  switch (type) {
    case REGISTER:
      return "register";
    case COMMIT:
      return "commit";
    case ABORT:
      return "abort";
    default:
      return NULL;
  }
}

/* Sends REQ on socket SOCKFD. Returns the number of bytes which were sent,
 * and -1 on error. */
int kvrequest_send(kvrequest_t *kvreq, int sockfd) {
  char *method = http_method_for_request_type(kvreq->type);
  if (!method) return -1;

  struct url_params params;
  params.path = path_for_request_type(kvreq->type);
  params.key = kvreq->key;
  params.val = kvreq->val;

  char *url = url_encode(&params);
  struct http_outbound *msg = http_start_request(sockfd, method, url);
  if (!msg) return -1;
  free(url);

  http_end_headers(msg);
  return http_send_and_free(msg);
}

int http_code_for_response_type(msgtype_t type) {
  switch (type) {
    case GETRESP:
      return 200;
    case SUCCESS:
      return 201;
    case ERROR:
      return 500;
    case VOTE:
      return 202;
    case ACK:
      return 204;
    default:
      return -1;
  }
}

int kvresponse_send(kvresponse_t *kvres, int sockfd) {
  int code = http_code_for_response_type(kvres->type);
  if (code < 0) return -1;

  struct http_outbound *msg = http_start_response(sockfd, code);
  if (!msg) return -1;

  char lenbuf[10] = "0";
  if (kvres->body)
    sprintf(lenbuf, "%ld", strlen(kvres->body));
  http_add_header(msg, "Content-Length", lenbuf);
  http_end_headers(msg);
  http_add_string(msg, kvres->body);

  return http_send_and_free(msg);
}

void kvrequest_free(kvrequest_t *kvreq) {
  if (!kvreq) return;
  if (kvreq->key)
    free(kvreq->key);
  if (kvreq->val)
    free(kvreq->val);
  free(kvreq);
}

void kvresponse_free(kvresponse_t *kvres) {
  if (!kvres) return;
  if (kvres->body)
    free(kvres->body);
  free(kvres);
}
