#include "request_parser.h"
#define WHITESPACE_SKIP 1

char *get_method(char first_char, char second_char) {
  if (first_char == 'G') {
    return "GET";
  }

  if (first_char == 'D') {
    return "DELETE";
  }

  if (first_char == 'P') {
    if (second_char == 'A') {
      return "PATCH";
    }
    if (second_char == 'O') {
      return "POST";
    }
    if (second_char == 'U') {
      return "PUT";
    }
  }

  return "";
}

char *get_resource_path(char *buffer, int skip) {
  char *resource_path = (char *)malloc(1024 * sizeof(char));
  int buffer_size = strlen(buffer);

  for (int i = skip; i < buffer_size; i++) {
    if (buffer[i] == ' ' || buffer[i] == '?') {
      resource_path[i - skip] = '\0';
      break;
    }

    resource_path[i - skip] = buffer[i];
  }

  return resource_path;
}

char *get_query_string(char *buffer, int skip) {
  char *query_string = (char *)malloc(1024 * sizeof(char));
  int buffer_size = strlen(buffer);

  for (int i = skip; i < buffer_size; i++) {
    if (buffer[i] == ' ') {
      query_string[i - skip] = '\0';
      break;
    }

    query_string[i - skip] = buffer[i];
  }

  return query_string;
}

RequestParserError parse_request(char *buffer, struct Request *out) {
  // Able to determine method from first two characters
  out->method = get_method(buffer[0], buffer[1]);

  if (strcmp(out->method, "") == 0) {
    return INVALID_HTTP_METHOD;
  }
  int method_size = strlen(out->method) + WHITESPACE_SKIP;

  out->resource_path = get_resource_path(buffer, method_size);
  char *query_string =
      get_query_string(buffer, strlen(out->resource_path) + method_size + 1);

  if (strlen(query_string) > 0) {
    char *param;
    out->param_count = 0;

    while ((param = strsep(&query_string, "&")) != NULL &&
           out->param_count < 32) {
      char *equals = strchr(param, '=');
      if (equals != NULL) {
        *equals = '\0';
        out->query_param_keys[out->param_count] = param;
        out->query_param_values[out->param_count] = equals + 1;
        out->param_count++;
      }
    }
    free(query_string);
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
      return INVALID_HEADER;
    }
    *colon = '\0';
    out->header_keys[out->header_count] = header;
    out->header_values[out->header_count] = colon + 1;
    out->header_count++;
  }

  if (buffer != NULL) {
    if (buffer[0] == '\n') {
      buffer++;
    }
    out->body = buffer;
  }

  char *token;
  char *url_path_copy = strdup(out->resource_path);
  out->segment_count = 0;
  while ((token = strsep(&url_path_copy, "/")) != NULL) {
    if (strlen(token) == 0)
      continue;
    out->segments[out->segment_count] = token;
    out->segment_count++;
  }
  free(url_path_copy);

  return OK;
}
