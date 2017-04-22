/*
 * Helpers for parsing URL parameters.
 */

#ifndef LIBURL_H
#define LIBURL_H

#include <stdlib.h>

/*
 * URL path plus accepted query parameters from an HTTP request to our servers.
 */
struct url_params {
  char *path;
  char *key;
  char *val;
};

/* Unmarshalls the valid paramters within URL into a convenient url_params
 * struct. */
struct url_params *url_decode(char *url);

/* Marshalls the non-null params in PARAMS into an HTTP-compatible URL string */
char *url_encode(struct url_params *params);

void url_params_free(struct url_params *);

#endif
