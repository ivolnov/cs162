/*
 * A simple HTTP library for parsing and sending HTTP messages.
 */

#ifndef LIBHTTP_H
#define LIBHTTP_H

#include <stddef.h>

/*--- RECIEVING AND PARSING ---*/

/* Request parsing. */
struct http_request {
  char *method;
  char *path;
};

struct http_request *http_request_parse(int fd);
void http_request_free(struct http_request *);

/* Response parsing. */
struct http_response {
  short status;
  char *body;
};

struct http_response *http_response_parse(int fd);
void http_response_free(struct http_response *);

/*--- SENDING ---*/

/* Represents an under-construction, outbound HTTP message. */
struct http_outbound;

/* There two functions returns an http_outbound MSG which represents an
 * under-construction, outbound HTTP request and response, respecitvely.  MSG
 * should be further built upon using the other functions below, and finally
 * sent with http_send_and_free.
 */
struct http_outbound *http_start_request(int fd, char *method, char *url);
struct http_outbound *http_start_response(int fd, int status_code);

void http_add_header(struct http_outbound *, char *key, char *value);
void http_end_headers(struct http_outbound *);
void http_add_string(struct http_outbound *, char *data);
void http_add_data(struct http_outbound *, char *data, size_t size);

/* Sends off the http_outbound message and frees its resources. */
int http_send_and_free(struct http_outbound *);

#endif
