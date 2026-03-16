#include "db.h"
#include "error.h"
#include "log.h"
#include "request_parser.h"
#include "response_builder.h"
#include "router.h"
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1048576
#define METHOD_SIZE_GET 4

void *handle_client_request(void *arg);

int main(int argc, char *argv[]) {
  log_init("logs/wikigen_auth.log");
  bool db_success = init_db();
  if (!db_success) {
    LOG_ERROR("Failed to initialize database");
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
    LOG_ERROR("socket failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    LOG_ERROR("setsockopt failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    LOG_ERROR("bind failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    LOG_ERROR("listen failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  LOG_INFO("Server is listening on port %d", PORT);

  init_routes();

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int *client_fd = malloc(sizeof(int));

    *client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (*client_fd < 0) {
      LOG_ERROR("accept failed: %s", strerror(errno));
      continue;
    }

    pthread_t thread_id;
    int ret =
        pthread_create(&thread_id, NULL, handle_client_request, client_fd);
    if (ret != 0) {
      errno = ret;
      LOG_ERROR("pthread_create failed: %s", strerror(errno));
      continue;
    }
    pthread_detach(thread_id);
  }

  log_close();
  close(server_fd);
  return 0;
}

void *handle_client_request(void *arg) {
  int client_fd = *(int *)arg;
  char buffer[BUFFER_SIZE * sizeof(char)];
  int total_bytes_received = 0;

  int bytes_received;
  while ((bytes_received = recv(client_fd, buffer + total_bytes_received,
                                sizeof(buffer) - total_bytes_received - 1, 0)) >
         0) {
    total_bytes_received += bytes_received;
    buffer[total_bytes_received] = '\0';

    char *buffer_start = strstr(buffer, "\r\n\r\n");
    if (buffer_start) {
      int body_start_index = (buffer_start - buffer) + 4;

      char *content_length = strcasestr(buffer, "Content-Length:");
      if (content_length) {
        int content_length_int = atoi(content_length + 15);

        int body_received = total_bytes_received - body_start_index;
        while (body_received < content_length_int) {
          bytes_received = recv(client_fd, buffer + total_bytes_received,
                                sizeof(buffer) - total_bytes_received - 1, 0);
          if (bytes_received <= 0)
            break;
          body_received += bytes_received;
          total_bytes_received += bytes_received;
        }
        buffer[total_bytes_received] = '\0';
      }
      break;
    }
  }

  if (bytes_received < 0) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "No bytes received");
    return NULL;
  }

  Request request;
  LOG_INFO("Received request: %s", buffer);
  ErrorContext error = parse_request(buffer, &request);
  if (error.code != OK) {
    char error_response[1024];
    snprintf(error_response, sizeof(error_response),
             "Failure Parsing Request: %s", error.message);
    LOG_ERROR(error_response);
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, error_response);
    return NULL;
  }

  char content_length[512];
  for (int i = 0; i < request.header_count; i++) {
    if (strcmp(request.header_keys[i], "Content-Length") == 0) {
      strncpy(content_length, request.header_values[i], 512);
      content_length[512 - 1] = '\0';
      break;
    }
  }

  int content_length_int = atoi(content_length + 15);
  if (content_length_int > 0) {
    char body[content_length_int];
    while (recv(client_fd, body, content_length_int, 0) > 0) {
      body[content_length_int] = '\0';
    }
    LOG_INFO("Received body: %s", body);
  }

  // Middleware?
  if (strcmp(request.method, "OPTIONS") == 0) {
    char response[1024];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             "Access-Control-Allow-Headers: *\r\n"
             "Content-Length: 0\r\n"
             "Connection: keep-alive\r\n\r\n");
    LOG_INFO("Sending response: %s", response);
    send(client_fd, response, strlen(response), 0);
  }

  int routed = route_request(client_fd, &request);

  if (routed < 0) {
    send_response(client_fd, 404, CONTENT_TYPE_TEXT, "No route found");
  }

  return NULL;
}
