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

struct Request *parse_request(char *buffer) {
  struct Request *request = (struct Request *)malloc(sizeof(struct Request));

  // Able to determine method from first two characters
  request->method = get_method(buffer[0], buffer[1]);
  if (strcmp(request->method, "") == 0) {
    request->error = "Invalid HTTP Method";
    return request;
  }
  int method_size = strlen(request->method) + WHITESPACE_SKIP;

  request->resource_path = (char *)malloc(1024 * sizeof(char));

  char *query_string;
  // TODO: This is horribly written, fix it
  int buffer_size = strlen(buffer);
  for (int i = method_size; i < (int)buffer_size; i++) {
    if (buffer[i] == ' ') {
      request->resource_path[i - method_size] = '\0';
      break;
    }

    if (buffer[i] == '?') {
      query_string = (char *)malloc(strlen(buffer) * sizeof(char));
      int skipped_path_length =
          strlen(request->resource_path) + method_size + 1;

      for (size_t i = skipped_path_length; i < strlen(buffer); i++) {
        if (buffer[i] == ' ') {
          request->resource_path[i - method_size] = '\0';
          query_string[i - skipped_path_length] = '\0';
          break;
        }

        query_string[i - skipped_path_length] = buffer[i];
      }
      break;
    }

    request->resource_path[i - method_size] = buffer[i];
  }

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
