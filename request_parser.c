#include "request_parser.h"
#define WHITESPACE_SKIP 1

int get_method(char *buffer, char *out) {
  char first_word[7];

  size_t i = 0;
  while (i < 7 && buffer[i] != ' ' && buffer[i] != '\0') {
    first_word[i] = buffer[i];
    i++;
  }
  first_word[i] = '\0';

  strncpy(out, first_word, 10);
  out[10 - 1] = '\0';

  if (strcmp(out, "GET") == 0) {
    return 0;
  }
  if (strcmp(out, "POST") == 0) {
    return 0;
  }
  if (strcmp(out, "OPTIONS") == 0) {
    return 0;
  }
  if (strcmp(out, "DELETE") == 0) {
    return 0;
  }
  if (strcmp(out, "PATCH") == 0) {
    return 0;
  }
  if (strcmp(out, "PUT") == 0) {
    return 0;
  }

  return -1;
}

void get_resource_path(char *buffer, int skip, char *out) {
  int buffer_size = strlen(buffer);

  if (buffer_size >= 1024) {
    LOG_ERROR("Buffer size is too large");
    return;
  }
  for (int i = skip; i < buffer_size; i++) {
    if (buffer[i] == ' ' || buffer[i] == '?') {
      out[i - skip] = '\0';
      break;
    }

    out[i - skip] = buffer[i];
  }
  out[buffer_size - skip] = '\0';

  return;
}

void get_query_string(char *buffer, int skip, char *out) {
  int buffer_size = strlen(buffer);

  if (buffer_size >= 1024) {
    LOG_ERROR("Query string is too large");
    return;
  }
  for (int i = skip; i < buffer_size; i++) {
    if (buffer[i] == ' ') {
      out[i - skip] = '\0';
      break;
    }

    out[i - skip] = buffer[i];
  }
  out[buffer_size - skip] = '\0';

  return;
}

ErrorContext parse_request(char *buffer, Request *out) {
  // Able to determine method from first two characters
  if (get_method(buffer, out->method) < 0) {
    return ERROR_CONTEXT(INVALID_HTTP_METHOD, "Invalid HTTP method: %s",
                         out->method);
  }

  int method_size = strlen(out->method) + WHITESPACE_SKIP;

  get_resource_path(buffer, method_size, out->resource_path);
  if (strlen(out->resource_path) == 0) {
    return ERROR_CONTEXT(INVALID_PAYLOAD, "Resource Path %s", buffer);
  }

  char query_string[1024];
  get_query_string(buffer, strlen(out->resource_path) + method_size + 1,
                   query_string);

  LOG_INFO("Request URL: %s %s", out->method, out->resource_path);

  if (strlen(query_string) > 0) {
    char *param;
    char *temp_query_string = query_string;
    out->param_count = 0;

    while ((param = strsep(&temp_query_string, "&")) != NULL &&
           out->param_count < 32) {
      char *equals = strchr(param, '=');
      if (equals != NULL) {
        *equals = '\0';
        strncpy(out->query_param_keys[out->param_count], param, strlen(param));
        out->query_param_keys[out->param_count][strlen(param)] = '\0';
        strncpy(out->query_param_values[out->param_count], equals + 1,
                strlen(equals + 1));
        out->query_param_values[out->param_count][strlen(equals + 1)] = '\0';
        out->param_count++;
      }
    }
  }

  for (int i = 0; i < out->param_count; i++) {
    LOG_INFO("Param %s: %s", out->query_param_keys[i],
             out->query_param_values[i]);
  }

  char *header;
  int empty_header_count = 0;
  while ((header = strsep(&buffer, "\r\n")) != NULL && out->header_count < 32) {
    if (strstr(header, "HTTP/1.1") != NULL) {
      continue;
    }

    if (strcmp(header, "") == 0) {
      empty_header_count++;
      // If we have more than one empty header, we've reached the body
      if (empty_header_count > 1) {
        break;
      }
      continue;
    }
    empty_header_count = 0;

    char *colon = strchr(header, ':');
    if (colon == NULL) {
      return ERROR_CONTEXT(INVALID_HEADER, "Invalid header %s", header);
    }
    *colon = '\0';
    strncpy(out->header_keys[out->header_count], header, strlen(header));
    out->header_keys[out->header_count][strlen(header)] = '\0';
    strncpy(out->header_values[out->header_count], colon + 1,
            strlen(colon + 1));
    out->header_values[out->header_count][strlen(colon + 1)] = '\0';
    out->header_count++;
  }

  if (buffer != NULL) {
    if (buffer[0] == '\n') {
      buffer++;
    }
    strncpy(out->body, buffer, strlen(buffer));
    out->body[strlen(buffer)] = '\0';
  }

  char *token;
  char *url_path_copy = strdup(out->resource_path);
  out->segment_count = 0;
  while ((token = strsep(&url_path_copy, "/")) != NULL) {
    if (strlen(token) == 0)
      continue;
    if (out->segment_count >= 255) {
      return ERROR_CONTEXT(
          INVALID_PAYLOAD,
          "Too many segments: Number of path segments should not exceed 256");
    }
    strncpy(out->segments[out->segment_count], token, strlen(token));
    out->segments[out->segment_count][strlen(token)] = '\0';
    out->segment_count++;
  }
  free(url_path_copy);

  return ERROR_CONTEXT(OK, "OK");
}
