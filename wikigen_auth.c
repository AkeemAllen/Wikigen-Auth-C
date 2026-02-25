#include "db.h"
#include "request_parser.h"
#include "response_builder.h"
#include "router.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
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

  ssize_t valread;
  struct sockaddr_in server_address;
  int opt = 1;
  socklen_t addrlen = sizeof(server_address);

  char buffer[1024] = {0};
  char *hello = "Hello";

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  printf("Server is listening on port %d\n", PORT);

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
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Improper Header");
    free(buffer);
    return NULL;
  }
  buffer[bytes_received] = '\0';

  struct Request *request = parse_request(buffer);

  if (request->error != NULL) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, request->error);
    free(request);
    free(buffer);
    return NULL;
  }

  int routed = route_request(client_fd, request);

  if (routed < 0) {
    send_response(client_fd, 404, CONTENT_TYPE_TEXT, "No route found");
  }
  free(request);
  free(buffer);
  printf("\n");

  return NULL;
}
