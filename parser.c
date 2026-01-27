#include "parser.h"
#include <stdio.h>
#include <string.h>

struct URL *parse_url(int client_fd, char *buffer, int method_size) {
  char *buffer_cpy = strdup(buffer);
  char *buffer_split_token;
  char split_buffer[1024][1024];
  int split_count = 0;

  while ((buffer_split_token = strsep(&buffer_cpy, "\n")) != NULL) {
    if (strlen(buffer_split_token) == 0)
      continue;
    strncpy(split_buffer[split_count], buffer_split_token,
            strlen(buffer_split_token));
    split_count++;
  }

  char *method_resource_httpv;
  for (int i = 0; i < 5; i++) {
    printf("Split Buffer: %s", split_buffer[i]);
  }
  // strncpy(method_resource_httpv, split_buffer[0], strlen(split_buffer[0]));

  struct URL *url = (struct URL *)malloc(sizeof(struct URL));
  url->path = (char *)malloc(strlen(method_resource_httpv) + 1);

  for (int i = method_size; i < strlen(method_resource_httpv); i++) {

    printf("Query Strings Loop\n");
    printf("Method Resource Http: %s", method_resource_httpv);
    if (method_resource_httpv[i] == '?') {
      printf("Query Strings\n");
      url->queryStrings = (char *)malloc(strlen(split_buffer[0]) + 1);
      int skippedURLPathLength = strlen(url->path) + method_size;
      for (int i = skippedURLPathLength; i < strlen(split_buffer[0]); i++) {
        if (split_buffer[0][i] == ' ')
          break;
        url->queryStrings[i - skippedURLPathLength] = split_buffer[0][i];
      }
    }
    if (method_resource_httpv[i] == ' ')
      break;

    url->path[i - method_size] = method_resource_httpv[i];
    printf("Query Strings Loop\n");
    printf("Method Size: %d, I: %d\n", method_size, i);
  }

  if (strlen(url->queryStrings) > 0) {
    char *param;
    char *queryStringCopy = strdup(url->queryStrings + 1);
    url->paramCount = 0;

    while ((param = strsep(&queryStringCopy, "&")) != NULL &&
           url->paramCount < 32) {
      char *equals = strchr(param, '=');
      if (equals != NULL) {
        *equals = '\0';
        url->paramKeys[url->paramCount] = param;
        url->paramValues[url->paramCount] = equals + 1;
        url->paramCount++;
      }
    }
  }

  char *token;
  char *urlPathCopy = strdup(url->path);
  url->segmentCount = 0;
  while ((token = strsep(&urlPathCopy, "/")) != NULL) {
    if (strlen(token) == 0)
      continue;
    url->segments[url->segmentCount] = token;
    url->segmentCount++;
  }

  return url;
}

void test_parsr_url(char *buffer) { char *request_segments[10]; }
