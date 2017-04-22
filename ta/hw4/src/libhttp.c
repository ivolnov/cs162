#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdbool.h>

#include "kvconstants.h"
#include "libhttp.h"

#define LIBHTTP_MESSAGE_MAX_SIZE 8192

static char* http_get_response_message(int status_code);
static bool http_is_valid_method(char *method, int len);

struct http_request *http_request_parse(int fd) {
  struct http_request *req = calloc(1, sizeof(struct http_request));
  if (!req) fatal_malloc();

  char read_buffer[LIBHTTP_MESSAGE_MAX_SIZE + 1];
  int bytes_read = read(fd, read_buffer, LIBHTTP_MESSAGE_MAX_SIZE);
  if (bytes_read <= 0) goto error;
  read_buffer[bytes_read] = '\0'; /* Always null-terminate. */

#ifdef DEBUG
  printf("%s\n", read_buffer);
#endif

  char *read_start, *read_end;
  size_t read_size;

  /* Read in the HTTP method: "[A-Z]*" */
  read_start = read_end = read_buffer;
  read_end = strchr(read_start, ' ');
  if (!read_end || !http_is_valid_method(read_start, read_end - read_start))
    goto error;
  read_size = read_end - read_start;
  req->method = malloc(read_size + 1);
  if (!req->method) fatal_malloc();
  memcpy(req->method, read_start, read_size);
  req->method[read_size] = '\0';
  read_end++; read_start = read_end;

  /* Read in the path. */
  read_start = read_end;
  if (*read_end != '/') goto error;
  read_end++;
  while (*read_end != ' ' && *read_end != '\n') {
    if (*read_end == '\0') goto error;
    read_end++;
  }
  read_size = read_end - read_start;
  if (read_size == 0) goto error;
  req->path = malloc(read_size + 1);
  if (!req->path) fatal_malloc();
  memcpy(req->path, read_start, read_size);
  req->path[read_size] = '\0';

  return req;

error:
  http_request_free(req);
  return NULL;
}

void http_request_free(struct http_request *req) {
  if (!req) return;
  if (req->method)
    free(req->method);
  if (req->path)
    free(req->path);
  free(req);
}

struct http_response *http_response_parse(int fd) {
  struct http_response *res = calloc(1, sizeof(struct http_response));
  if (!res) fatal_malloc();

  char read_buffer[LIBHTTP_MESSAGE_MAX_SIZE + 1];
  int bytes_read = read(fd, read_buffer, LIBHTTP_MESSAGE_MAX_SIZE);
  if (bytes_read <= 0) goto error;
  read_buffer[bytes_read] = '\0'; /* Always null-terminate. */

  char *read_start, *read_end, *read_limit;
  short status = -1;
  int content_length = -1;

  /* Read in HTTP version: "HTTP/1.[01]" */
  read_start = read_end = read_buffer;
  read_end = strchr(read_start, ' ');
  if (!read_end || read_start + 8 != read_end) goto error;
  if (strncmp(read_start, "HTTP/1.1", 8) && strncmp(read_start, "HTTP/1.0", 8))
    goto error;
  read_end++; read_start = read_end;

  /* Read in the response status: "[0-9]{3}" */
  status = strtol(read_start, &read_end, 10);
  if (!status || read_start + 3 != read_end || *read_end != ' ') goto error;
  res->status = status;
  read_end++; read_start = read_end;

  /* Read in respone status text until \r\n */
  read_end = strstr(read_start, "\r\n");
  if (!read_end) {
    read_end = strstr(read_start, "\n");
    if (!read_end) {
      goto error;
    }
  }
  if (strncmp(read_start, http_get_response_message(res->status),
        read_end - read_start)) goto error;

  /* Loop through headers until Content-Length, or bust. */
  while (strncmp(read_end, "\n\n", 2) && strncmp(read_end, "\r\n\r\n", 4)) {
    read_end += 2; read_start = read_end;
    read_limit = strstr(read_start, "\r\n");
    if (!read_limit) {
      read_limit = strstr(read_start, "\n");
      if (!read_limit) {
        goto error;
      }
    }
    read_end = strstr(read_start, ": ");
    if (!read_end || read_end > read_limit) goto error; /* Malformed header */
    if (!strncmp(read_start, "Content-Length", read_end - read_start)) { /* Found it! */
      read_start = read_end + 2;
      content_length = strtol(read_start, &read_end, 10);
      while (*read_end == ' ') read_end++;
      if (read_end != read_limit) goto error;
    }
    read_end = read_limit;
  }
  read_end += 4; read_start = read_end;

  /* Read body */
  res->body = malloc(content_length + 1);
  if (!res->body) fatal_malloc();
  strcpy(res->body, read_start);

  return res;

error:
  http_response_free(res);
  return NULL;
}

void http_response_free(struct http_response *res) {
  if (!res) return;
  if (res->body)
    free(res->body);
  free(res);
}

bool http_is_valid_method(char *method, int len) {
  return !strncmp(method, "GET", len) ||
    !strncmp(method, "PUT", len) ||
    !strncmp(method, "DELETE", len) ||
    !strncmp(method, "POST", len);
}

static char* http_get_response_message(int status_code) {
  switch (status_code) {
    case 100:
      return "Continue";
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 304:
      return "Not Modified";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 500:
      return "Internal Server Error";
    default:
      return NULL;
  }
}

const char *valid_methods[] = {"GET", "PUT", "DELETE", "POST"};

bool is_valid_method(char *method) {
  int i;
  for (i = 0; i < sizeof(valid_methods)/sizeof(char*); i++) {
    if (!strcmp(method, valid_methods[i])) return true;
  }
  return false;
}

struct http_outbound {
  int fd;
  int end;
  char *body;
};

struct http_outbound *http_start_request(int fd, char *method, char *url) {
  if (!is_valid_method(method)) return NULL;
  struct http_outbound *msg = malloc(sizeof(struct http_outbound));
  if (!msg) fatal_malloc();
  msg->body = malloc(LIBHTTP_MESSAGE_MAX_SIZE + 1);
  if (!msg->body) fatal_malloc();
  msg->end = sprintf(msg->body, "%s %s HTTP/1.1\r\n", method, url);

  msg->fd = fd;

  return msg;
}

struct http_outbound *http_start_response(int fd, int status_code) {
  char *status = http_get_response_message(status_code);
  if (!status) return NULL;
  struct http_outbound *msg = malloc(sizeof(struct http_outbound));
  if (!msg) fatal_malloc();
  msg->body = malloc(LIBHTTP_MESSAGE_MAX_SIZE + 1);
  if (!msg->body) fatal_malloc();
  msg->end = sprintf(msg->body, "HTTP/1.1 %d %s\r\n", status_code, status);

  msg->fd = fd;

  return msg;
}

void http_add_header(struct http_outbound *msg, char *key, char *value) {
  msg->end += sprintf(msg->body + msg->end, "%s: %s\r\n", key, value);
}

void http_end_headers(struct http_outbound *msg) {
  msg->end += sprintf(msg->body + msg->end, "\r\n");
}

void http_add_string(struct http_outbound *msg, char *data) {
  if (!data) return;
  http_add_data(msg, data, strlen(data));
}

void http_add_data(struct http_outbound *msg, char *data, size_t size) {
  memcpy(msg->body + msg->end, data, size);
  msg->end += size;
}

int http_send_and_free(struct http_outbound *msg) {
  msg->body[msg->end] = '\0';

  char *send = msg->body;
  int bytes_left = msg->end;

  ssize_t bytes_sent;
  while (bytes_left > 0) {
    bytes_sent = write(msg->fd, send, bytes_left);
    if (bytes_sent < 0)
      return bytes_sent;
    bytes_left -= bytes_sent;
    send += bytes_sent;
  }

  int ret = msg->end;
  free(msg->body);
  free(msg);
  return ret;
}
