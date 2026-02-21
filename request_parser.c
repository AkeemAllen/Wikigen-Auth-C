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

struct Request *parse_request(char *buffer) {
  struct Request *request = (struct Request *)malloc(sizeof(struct Request));

  // Able to determine method from first two characters
  request->method = get_method(buffer[0], buffer[1]);
  if (strcmp(request->method, "") == 0) {
    request->error = "Invalid HTTP Method";
    return request;
  }
  int method_size = strlen(request->method) + WHITESPACE_SKIP;

  request->resource_path = get_resource_path(buffer, method_size);
  char *query_string = get_query_string(buffer, strlen(request->resource_path) +
                                                    method_size + 1);

  if (strlen(query_string) > 0) {
    char *param;
    request->param_count = 0;

    while ((param = strsep(&query_string, "&")) != NULL &&
           request->param_count < 32) {
      char *equals = strchr(param, '=');
      if (equals != NULL) {
        *equals = '\0';
        request->query_param_keys[request->param_count] = param;
        request->query_param_values[request->param_count] = equals + 1;
        request->param_count++;
      }
    }
    free(query_string);
  }

  char *header;
  int empty_header_count = 0;
  while ((header = strsep(&buffer, "\r\n")) != NULL &&
         request->header_count < 32) {
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
      request->error = "Invalid Header";
      return request;
    }
    *colon = '\0';
    request->header_keys[request->header_count] = header;
    request->header_values[request->header_count] = colon + 1;
    request->header_count++;
  }

  if (buffer != NULL) {
    if (buffer[0] == '\n') {
      buffer++;
    }
    request->body = buffer;
  }

  char *token;
  char *url_path_copy = strdup(request->resource_path);
  request->segment_count = 0;
  while ((token = strsep(&url_path_copy, "/")) != NULL) {
    if (strlen(token) == 0)
      continue;
    request->segments[request->segment_count] = token;
    request->segment_count++;
  }
  free(url_path_copy);

  return request;
}
