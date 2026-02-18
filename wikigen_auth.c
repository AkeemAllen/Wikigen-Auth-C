#include "request_parser.h"
#include "router.h"
#include "db.h"
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

void *handle_client_request(void *arg);

int main(int argc, char *argv[]) {
  bool db_success = init_db();
  if (!db_success) {
    return 1;
  }

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

  init_routes();

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
    pthread_create(&thread_id, NULL, handle_client_request, (void *)client_fd);
    pthread_detach(thread_id);
  }

  close(server_fd);
  return 0;
}

void *handle_client_request(void *arg) {
  int client_fd = *(int *)arg;
  char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

  ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

  if (bytes_received < 0) {
    send(client_fd, "Improper Header", strlen("Improper Header"), 0);
    free(buffer);
    return NULL;
  }
  buffer[bytes_received] = '\0';

  struct Request *request = parse_request(buffer);
  if (request->error != NULL) {
    send(client_fd, request->error, strlen(request->error), 0);
    free(request);
    free(buffer);
    return NULL;
  }

  int routed = route_request(client_fd, request);
  if (routed < 0) {
    send(client_fd, "No route found", strlen("No route found"), 0);
    free(request);
    free(buffer);
    return NULL;
  }

  send(client_fd, "OK", strlen("OK"), 0);
  free(request);
  free(buffer);

  return NULL;
}
 
