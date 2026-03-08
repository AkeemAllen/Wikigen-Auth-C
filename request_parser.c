#include "request_parser.h"
#define WHITESPACE_SKIP 1

void get_method(char first_char, char second_char, char *out) {
  if (first_char == 'G') {
    strncpy(out, "GET", 3);
    out[3] = '\0';
    return;
  }

  if (first_char == 'D') {
    strncpy(out, "DELETE", 6);
    out[6] = '\0';
    return;
  }

  if (first_char == 'P') {
    if (second_char == 'A') {
      strncpy(out, "PATCH", 5);
      out[5] = '\0';
      return;
    }
    if (second_char == 'O') {
      strncpy(out, "POST", 4);
      out[4] = '\0';
      return;
    }
    if (second_char == 'U') {
      strncpy(out, "PUT", 3);
      out[3] = '\0';
      return;
    }
  }
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
  get_method(buffer[0], buffer[1], out->method);
  if (strcmp(out->method, "") == 0) {
    return ERROR_CONTEXT(INVALID_HTTP_METHOD, "Invalid HTTP method");
  }

  int method_size = strlen(out->method) + WHITESPACE_SKIP;

  get_resource_path(buffer, method_size, out->resource_path);
  if (strlen(out->resource_path) == 0) {
    return ERROR_CONTEXT(INVALID_PAYLOAD, "Invalid resource path");
  }

  char query_string[1024];
  get_query_string(buffer, strlen(out->resource_path) + method_size + 1,
                   query_string);

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
      return ERROR_CONTEXT(INVALID_HEADER, "Invalid header");
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
