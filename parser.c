#include "parser.h"
#define WHITESPACE_SKIP 1

char *get_method(char first_char, char second_char) {
  if (first_char == 'G') {
    return "GET";
  }

  if (first_char == 'D') {
    return "DELETE";
  }

  if (first_char == 'P') {
    if (second_char == 'O') {
      return "POST";
    }
    if (second_char == 'U') {
      return "PUT";
    }
  }
}

struct Request *parse_request(char *buffer) {
  struct Request *request = (struct Request *)malloc(sizeof(struct Request));

  // Able to determine method from first two characters
  request->method = get_method(buffer[0], buffer[1]);
  int method_size = strlen(request->method) + WHITESPACE_SKIP;

  request->resource_path = (char *)malloc(1024 * sizeof(char));

  char *queryString;

  int buffer_size = strlen(buffer);
  for (int i = method_size; i < (int)buffer_size; i++) {
    if (buffer[i] == ' ') {
      request->resource_path[i - method_size] = '\0';
      break;
    }

    if (buffer[i] == '?') {
      queryString = (char *)malloc(strlen(buffer) * sizeof(char));
      int skipped_path_length = strlen(request->resource_path);

      for (size_t i = skipped_path_length; i < strlen(buffer); i++) {
        if (buffer[i] == ' ') {
          request->resource_path[i - method_size] = '\0';
          queryString[i - skipped_path_length] = '\0';
          break;
        }

        queryString[i - skipped_path_length] = buffer[i];
      }
    }

    request->resource_path[i - method_size] = buffer[i];
  }

  if (strlen(queryString) > 0) {
    char *param;
    request->param_count = 0;

    while ((param = strsep(&queryString, "&")) != NULL &&
           request->param_count < 32) {
      char *equals = strchr(param, '=');
      if (equals != NULL) {
        *equals = '\0';
        request->query_param_keys[request->param_count] = param;
        request->query_param_values[request->param_count] = equals + 1;
        request->param_count++;
      }
    }
    free(queryString);
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
