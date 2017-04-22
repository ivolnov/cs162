#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#include "kvconstants.h"
#include "liburl.h"

static char **param_ptr(struct url_params *params, char *key, int keylen) {
  if (!strncmp(key, "key", keylen))
    return &params->key;
  if (!strncmp(key, "val", keylen))
    return &params->val;
  return NULL;
}

struct url_params *url_decode(char *url) {
  struct url_params *params = calloc(1, sizeof(struct url_params));
  if (!params) fatal_malloc();

  char *read_end;
  int pathlen = 0;

  /* Write path to url_params struct */
  read_end = strchr(url, '?');
  if (read_end == NULL) { /* No params found. */
    if ((pathlen = strlen(url)) > 1) {
      params->path = malloc(pathlen);
      if (!params->path) fatal_malloc();
      strcpy(params->path, url+1);
    }
    goto end;
  }
  pathlen = read_end - url;
  if (pathlen > 1) { /* Path must have more chars than just '/' */
    params->path = malloc(pathlen);
    if (!params->path) fatal_malloc();
    memcpy(params->path, url+1, pathlen-1);
    params->path[pathlen-1] = '\0';
  }
  url = read_end+1;

  /* Loop through parameters, pulling ones we support (key, val) */
  char *key_end;
  char *param_value;
  char **struct_param_ptr;
  while (*read_end != '\0') {
    key_end = strchr(url, '=');
    if (key_end == NULL) break;
    read_end = strchr(url, '&');
    if (read_end == NULL) read_end = url+strlen(url);

    struct_param_ptr = param_ptr(params, url, key_end - url);
    if (struct_param_ptr == NULL) {
      if (*url == '\0') goto end;
      url = read_end+1;
      continue;
    }

    param_value = malloc(read_end - key_end);
    if (!param_value) fatal_malloc();
    strncpy(param_value, key_end+1, read_end - key_end - 1);
    param_value[read_end - key_end - 1] = '\0';

    *struct_param_ptr = param_value;
    url = read_end+1;
  }

end:
  return params;
}

char *url_encode(struct url_params *params) {
  char buf[128];
  int end = 0;
  if (params->path)
    end += sprintf(buf + end, "/%s?", params->path);
  else
    end += sprintf(buf + end, "/?");
  if (params->key) end += sprintf(buf + end, "key=%s&", params->key);
  if (params->val) end += sprintf(buf + end, "val=%s&", params->val);
  buf[end - 1] = '\0';

  char *url = malloc(end);
  strcpy(url, buf);

  return url;
}

void url_params_free(struct url_params *params) {
  if (!params) return;
  if (params->path)
    free(params->path);
  if (params->key)
    free(params->key);
  if (params->val)
    free(params->val);
  free(params);
}
