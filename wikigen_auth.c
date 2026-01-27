#include "cJSON.h"
#include "parser.h"
#include "request.h"
#include <curl/curl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 104857600
#define METHOD_SIZE_GET 4

void *handle_client(void *arg);
void handle_get_requests(int client_fd, struct URL *url);
void handle_post_requests(int client_fd, char *buffer);
void authorize(int client_fd, char *code);

int main(int argc, char *argv[]) {
  int server_fd;
  ssize_t valread;
  struct sockaddr_in server_address;
  int opt = 1;
  socklen_t addrlen = sizeof(server_address);

  char buffer[1024] = {0};
  char *hello = "Hello";

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  int options_set =
      setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (options_set) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(PORT);

  int is_binded = bind(server_fd, (struct sockaddr *)&server_address,
                       sizeof(server_address));
  if (is_binded < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int *client_fd = malloc(sizeof(int));

    *client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (*client_fd < 0) {
      perror("accept failed");
      continue;
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
    pthread_detach(thread_id);
  }

  close(server_fd);
  return 0;
}

void *handle_client(void *arg) {
  int client_fd = *((int *)arg);
  char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

  ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

  if (bytes_received < 0) {
    send(client_fd, "Improper Header", strlen("Improper Header"), 0);
    free(buffer);
    return NULL;
  }
  buffer[bytes_received] = '\0';

  char method[7];
  strncpy(method, buffer, 7);
  method[7] = '\0';

  if (strstr(method, "GET") != NULL) {
    struct URL *url = parse_url(buffer, strlen("GET") + 1);
    if (url == NULL) {
      char *error = "Invalid URL";
      send(client_fd, error, strlen(error), 0);
      free(url);
      return NULL;
    }
    handle_get_requests(client_fd, url);
    free(url);
    return NULL;
  }

  if (strstr(method, "POST") != NULL) {
    struct URL *url = parse_url(buffer, strlen("POST") + 1);
    if (url == NULL) {
      char *error = "Invalid URL";
      send(client_fd, error, strlen(error), 0);
      free(url);
      return NULL;
    }
    handle_post_requests(client_fd, buffer);
    return NULL;
  }

  return NULL;
}

void handle_get_requests(int client_fd, struct URL *url) {
  if (strcmp(url->path, "/authorize") == 0) {
    char *code = url->paramValues[0];
    authorize(client_fd, code);
  }
}

void handle_post_requests(int client_fd, char *buffer) {
  char *hello = "{method: 'POST'}";
  send(client_fd, hello, strlen(hello), 0);
  // Get Resource Path. Loop through buffer until we hit HTTP
}

typedef struct {
  char *access_token;
  char *token_type;
  char *scope;
} AccessToken;

void authorize(int client_fd, char *code) {
  if (code == NULL || strlen(code) == 0) {
    char *error = "Code Parameter is undefined";
    send(client_fd, error, strlen(error), 0);
    return;
  }

  char *clientSecret = "c748476fdaafff68697825732f79630bdbbfb119";
  char *clientID = "Ov23li9oWejO62cA6Kee";

  // Move Curl implementation into a separate function
  // For now, it should take, URL, method and return the response
  char githubOauthUrl[512];
  snprintf(githubOauthUrl, sizeof(githubOauthUrl),
           "https://github.com/login/oauth/access_token?"
           "client_id=%s&client_secret=%s&code=%s",
           clientID, clientSecret, code);

  char *response = perform_curl_request(githubOauthUrl, "POST");
  if (!response) {
    send(client_fd, "Failed to perform request",
         strlen("Failed to perform request"), 0);
    return;
  }

  cJSON *json = cJSON_Parse(response);
  if (json == NULL) {
    send(client_fd, "Failed to parse JSON", strlen("Failed to parse JSON"), 0);
    free(response);
    return;
  }

  cJSON *error_item = cJSON_GetObjectItem(json, "error");
  if (error_item != NULL) {
    printf("Error: %s\n", error_item->valuestring);
  }

  // Parse into AccessToken struct
  AccessToken token = {0};
  cJSON *access_token = cJSON_GetObjectItem(json, "access_token");
  cJSON *token_type = cJSON_GetObjectItem(json, "token_type");
  cJSON *scope = cJSON_GetObjectItem(json, "scope");

  if (access_token)
    token.access_token = access_token->valuestring;
  if (token_type)
    token.token_type = token_type->valuestring;
  if (scope)
    token.scope = scope->valuestring;

  // Step 3: Print the response
  printf("Access Token: %s\n", token.access_token ? token.access_token : "N/A");
  printf("Token Type: %s\n", token.token_type ? token.token_type : "N/A");
  printf("Scope: %s\n", token.scope ? token.scope : "N/A");

  // Step 3 (alternative): Print raw JSON
  printf("\nRaw JSON Response:\n%s\n", response);

  // Cleanup
  cJSON_Delete(json);
  free(response);

  send(client_fd, "Authorized", strlen("Authorized"), 0);
}
