#include "cJSON.h"
#include "request.h"
#include "request_parser.h"
#include "router.h"
#include "token_util.h"
#include "libsql.h"
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
static struct RouteNode *g_router;

void *handle_client_request(void *arg);
int route_request(int client_fd, struct Request *request, libsql_connection_t conn);
void authorize(int client_fd, char *code);
void handle_authorize(int client_fd, struct Request *request, libsql_connection_t conn);
void handle_create_repo(int client_fd, struct Request *request, libsql_connection_t conn);

void handle_root(int client_fd, struct Request *request, libsql_connection_t conn) {}

struct ThreadData {
  int client_fd;
  libsql_connection_t conn;
};

int main(int argc, char *argv[]) {
  libsql_setup((libsql_config_t){0});

  char *db_url = get_env_value("TURSO_DATABASE_URL");
  char *auth_token = get_env_value("TURSO_AUTH_TOKEN");

  libsql_database_t db = libsql_database_init((libsql_database_desc_t){
      .url = db_url,
      .auth_token = auth_token
    });

  if (db.err) {
    fprintf(stderr,"Failed to initialize database: %s\n", libsql_error_message(db.err));
    return 1;
  }

  libsql_connection_t conn = libsql_database_connect(db);
  if (conn.err) {
    fprintf(stderr,"Failed to connect to database: %s\n", libsql_error_message(conn.err));
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

  g_router = create_route_node("", handle_root);
  struct RouteNode *create_repo =
      create_route_node("create_repo", handle_create_repo);

  add_child_route(g_router, create_repo);

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

    printf("No thread data created\n");
    struct ThreadData *thread_data = malloc(sizeof(struct ThreadData));
    thread_data->client_fd = *client_fd;
    thread_data->conn = conn;

    printf("thread data created\n");

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client_request, (void *)thread_data);
    pthread_detach(thread_id);
    printf("thread detached\n");
  }

  close(server_fd);
  return 0;
}

void *handle_client_request(void *arg) {
  struct ThreadData *thread_data = (struct ThreadData *)arg;
  int client_fd = thread_data->client_fd;
  printf("client fd: %d\n", client_fd);
  libsql_connection_t conn = thread_data->conn;
  printf("Conn\n");

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

  int routed = route_request(client_fd, request, conn);
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

int route_request(int client_fd, struct Request *request, libsql_connection_t conn) {
  struct RouteNode *current_node = g_router;
  for (int i = 0; i < request->segment_count; i++) {
    printf("Segment: %s\n", request->segments[i]);
    current_node = find_route(current_node, request->segments[i]);

    if (current_node == NULL) {
      printf("No route found for %s\n", request->segments[i]);
      return -1;
    }

    if (i == request->segment_count - 1) {
      current_node->handler(client_fd, request, conn);
      return 0;
    }
  }
  send(client_fd, "No route found", strlen("No route found"), 0);
  return -1;
}

void handle_authorize(int client_fd, struct Request *request, libsql_connection_t conn) {
  printf("Authorized\n");
}

void handle_create_repo(int client_fd, struct Request *request, libsql_connection_t conn) {
  //cJSON *json = cJSON_Parse(request->body);
  //if (json == NULL) {
  //  send(client_fd, "Failed to parse JSON", strlen("Failed to parse JSON"), 0);
  //  return;
  //}
  //cJSON *token = cJSON_GetObjectItem(json, "token");
  //cJSON *wiki_name = cJSON_GetObjectItem(json, "wikiName");

  //struct Payload *payload = verify_jwt(token->valuestring);
  //if (payload == NULL) {
  //  send(client_fd, "Failed to verify JWT", strlen("Failed to verify JWT"), 0);
  //  return;
  //}

  libsql_statement_t query_stmt =
        libsql_connection_prepare(conn, "SELECT * FROM user");
    if (query_stmt.err) {
        fprintf(
            stderr,
            "Error preparing query: %s\n",
            libsql_error_message(query_stmt.err)
        );
    }

    libsql_rows_t rows = libsql_statement_query(query_stmt);
    if (rows.err) {
        fprintf(
            stderr,
            "Error executing query: %s\n",
            libsql_error_message(rows.err)
        );
    }

    libsql_row_t row;
    while (!(row = libsql_rows_next(rows)).err && !libsql_row_empty(row)) {
        libsql_result_value_t id = libsql_row_value(row, 0);
        libsql_result_value_t username = libsql_row_value(row, 1);

        if (id.err || username.err) {
            fprintf(stderr, "Error retrieving row values\n");
            continue;
        }

        printf(
            "%lld %s\n",
            (long long)id.ok.value.integer,
            (char *)username.ok.value.text.ptr
        );

        libsql_row_deinit(row);
    }

}

// void handle_get_requests(int client_fd, struct URL *url) {
//   if (strcmp(url->path, "/authorize") == 0) {
//     char *code = url->paramValues[0];
//     authorize(client_fd, code);
//   }
// }
//
// void handle_post_requests(int client_fd, char *buffer) {
//   char *hello = "{method: 'POST'}";
//   send(client_fd, hello, strlen(hello), 0);
//   // Get Resource Path. Loop through buffer until we hit HTTP
// }
//
// typedef struct {
//   char *access_token;
//   char *token_type;
//   char *scope;
// } AccessToken;
//
// void authorize(int client_fd, char *code) {
//   if (code == NULL || strlen(code) == 0) {
//     char *error = "Code Parameter is undefined";
//     send(client_fd, error, strlen(error), 0);
//     return;
//   }
//
//   char *clientSecret = "c748476fdaafff68697825732f79630bdbbfb119";
//   char *clientID = "Ov23li9oWejO62cA6Kee";
//
//   // Move Curl implementation into a separate function
//   // For now, it should take, URL, method and return the response
//   char githubOauthUrl[512];
//   snprintf(githubOauthUrl, sizeof(githubOauthUrl),
//            "https://github.com/login/oauth/access_token?"
//            "client_id=%s&client_secret=%s&code=%s",
//            clientID, clientSecret, code);
//
//   char *response = perform_curl_request(githubOauthUrl, "POST");
//   if (!response) {
//     send(client_fd, "Failed to perform request",
//          strlen("Failed to perform request"), 0);
//     return;
//   }
//
//   cJSON *json = cJSON_Parse(response);
//   if (json == NULL) {
//     send(client_fd, "Failed to parse JSON", strlen("Failed to parse JSON"),
//     0); free(response); return;
//   }
//
//   cJSON *error_item = cJSON_GetObjectItem(json, "error");
//   if (error_item != NULL) {
//     printf("Error: %s\n", error_item->valuestring);
//   }
//
//   // Parse into AccessToken struct
//   AccessToken token = {0};
//   cJSON *access_token = cJSON_GetObjectItem(json, "access_token");
//   cJSON *token_type = cJSON_GetObjectItem(json, "token_type");
//   cJSON *scope = cJSON_GetObjectItem(json, "scope");
//
//   if (access_token)
//     token.access_token = access_token->valuestring;
//   if (token_type)
//     token.token_type = token_type->valuestring;
//   if (scope)
//     token.scope = scope->valuestring;
//
//   // Step 3: Print the response
//   printf("Access Token: %s\n", token.access_token ? token.access_token :
//   "N/A"); printf("Token Type: %s\n", token.token_type ? token.token_type :
//   "N/A"); printf("Scope: %s\n", token.scope ? token.scope : "N/A");
//
//   // Step 3 (alternative): Print raw JSON
//   printf("\nRaw JSON Response:\n%s\n", response);
//
//   // Cleanup
//   cJSON_Delete(json);
//   free(response);
//
//   send(client_fd, "Authorized", strlen("Authorized"), 0);
// }
