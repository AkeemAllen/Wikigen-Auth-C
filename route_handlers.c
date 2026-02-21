#include "route_handlers.h"

void handle_root(int client_fd, struct Request *request) {}

void handle_create_repo(int client_fd, struct Request *request) {
  cJSON *json = cJSON_Parse(request->body);
  if (json == NULL) {
    send(client_fd, "Failed to parse JSON", strlen("Failed to parse JSON"), 0);
    return;
  }
  cJSON *token = cJSON_GetObjectItem(json, "token");
  cJSON *wiki_name = cJSON_GetObjectItem(json, "wikiName");

  struct Payload *payload = verify_jwt(token->valuestring);
  if (payload == NULL) {
    send(client_fd, "Failed to verify JWT", strlen("Failed to verify JWT"), 0);
    return;
  }

  printf("Creating connection\n");
  libsql_connection_t conn = get_db_connection();
  if (conn.err) {
    fprintf(stderr, "Failed to get database connection: %s\n",
            libsql_error_message(conn.err));
    return;
  }

  libsql_statement_t query_stmt =
      libsql_connection_prepare(conn, "SELECT * FROM user");
  if (query_stmt.err) {
    fprintf(stderr, "Error preparing query: %s\n",
            libsql_error_message(query_stmt.err));
  }

  libsql_rows_t rows = libsql_statement_query(query_stmt);
  if (rows.err) {
    fprintf(stderr, "Error executing query: %s\n",
            libsql_error_message(rows.err));
  }

  libsql_row_t row;
  while (!(row = libsql_rows_next(rows)).err && !libsql_row_empty(row)) {
    libsql_result_value_t id = libsql_row_value(row, 0);
    libsql_result_value_t username = libsql_row_value(row, 1);

    if (id.err || username.err) {
      fprintf(stderr, "Error retrieving row values\n");
      continue;
    }

    printf("%lld %s\n", (long long)id.ok.value.integer,
           (char *)username.ok.value.text.ptr);

    libsql_row_deinit(row);
  }
}

typedef struct {
  char *access_token;
  char *token_type;
  char *scope;
} AccessToken;

void handle_authorize(int client_fd, struct Request *request) {
  char *code = NULL;
  for (int i = 0; i < request->param_count; i++) {
    if (strcmp(request->query_param_keys[i], "code") == 0) {
      printf("Code: %s\n", request->query_param_values[i]);
      code = strdup(request->query_param_values[i]);
    }
  }

  if (code == NULL || strlen(code) == 0) {
    char *error = "Code Parameter is undefined";
    send(client_fd, error, strlen(error), 0);
    return;
  }

  char *clientSecret = get_env_value("CLIENT_SECRET");
  char *clientID = get_env_value("CLIENT_ID");

  // Move Curl implementation into a separate function
  // For now, it should take, URL, method and return the response
  char githubOauthUrl[512];
  snprintf(githubOauthUrl, sizeof(githubOauthUrl),
           "https://github.com/login/oauth/access_token?"
           "client_id=%s&client_secret=%s&code=%s",
           clientID, clientSecret, code);

  char headers[20][100];
  strcpy(headers[0], "Accept: application/json");

  char *response = perform_curl_request(githubOauthUrl, "POST", headers);
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
    send(client_fd, "Error", strlen("Error"), 0);
    free(response);
    return;
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

  char githubUserUrl[512];
  snprintf(githubUserUrl, sizeof(githubUserUrl), "https://api.github.com/user");

  char github_user_headers[20][100];
  strcpy(github_user_headers[0], "Accept: */*");
  strcpy(github_user_headers[1], "Authorization: Bearer ");
  strcat(github_user_headers[1], token.access_token);
  strcpy(github_user_headers[2], "User-Agent: Wikigen-Auth-C");

  char *githubUserResponse =
      perform_curl_request(githubUserUrl, "GET", github_user_headers);
  if (!githubUserResponse) {
    send(client_fd, "Failed to perform request",
         strlen("Failed to perform request"), 0);
    free(githubUserResponse);
    return;
  }

  cJSON *user_json = cJSON_Parse(githubUserResponse);
  if (user_json == NULL) {
    send(client_fd, "Failed to parse JSON", strlen("Failed to parse JSON"), 0);
    free(githubUserResponse);
    return;
  }

  cJSON *user_error_item = cJSON_GetObjectItem(user_json, "error");
  if (user_error_item != NULL) {
    printf("Error: %s\n", user_error_item->valuestring);
    send(client_fd, "Error", strlen("Error"), 0);
    free(githubUserResponse);
    return;
  }

  cJSON *user_id = cJSON_GetObjectItem(user_json, "id");
  cJSON *user_login = cJSON_GetObjectItem(user_json, "login");
  // Rule select query in turso
  libsql_connection_t conn = get_db_connection();
  if (conn.err) {
    fprintf(stderr, "Failed to get database connection: %s\n",
            libsql_error_message(conn.err));
    return;
  }

  libsql_statement_t query_stmt =
      libsql_connection_prepare(conn, "SELECT * FROM user WHERE github_id = ?");
  libsql_statement_bind_value(query_stmt, libsql_integer(user_id->valueint));
  if (query_stmt.err) {
    fprintf(stderr, "Error preparing query: %s\n",
            libsql_error_message(query_stmt.err));
  }

  libsql_rows_t rows = libsql_statement_query(query_stmt);
  if (rows.err) {
    fprintf(stderr, "Error executing query: %s\n",
            libsql_error_message(rows.err));
  }

  printf("Select Statement Executed\n");

  libsql_row_t row;
  if (!(row = libsql_rows_next(rows)).err && !libsql_row_empty(row)) {
    libsql_statement_t update_stmt = libsql_connection_prepare(
        conn, "UPDATE User SET access_token = ? WHERE github_id = ?");
    libsql_statement_bind_value(
        update_stmt,
        libsql_text(token.access_token, strlen(token.access_token)));
    libsql_statement_bind_value(update_stmt, libsql_integer(user_id->valueint));
    if (update_stmt.err) {
      fprintf(stderr, "Error preparing query: %s\n",
              libsql_error_message(update_stmt.err));
    }

    libsql_statement_execute(update_stmt);
    printf("Update Statement Executed\n");
    libsql_statement_deinit(update_stmt);
  } else {
    libsql_statement_t insert_stmt = libsql_connection_prepare(
        conn, "INSERT INTO User (username, github_id, access_token) VALUES (?, "
              "?, ?)");
    libsql_statement_bind_value(
        insert_stmt,
        libsql_text(user_login->valuestring, strlen(user_login->valuestring)));
    libsql_statement_bind_value(insert_stmt, libsql_integer(user_id->valueint));
    libsql_statement_bind_value(
        insert_stmt,
        libsql_text(token.access_token, strlen(token.access_token)));
    if (insert_stmt.err) {
      fprintf(stderr, "Error preparing query: %s\n",
              libsql_error_message(insert_stmt.err));
    }
    libsql_statement_execute(insert_stmt);
    printf("Update Statement Executed\n");
    libsql_statement_deinit(insert_stmt);
  }

  libsql_statement_deinit(query_stmt);

  cJSON *user_avatar_url = cJSON_GetObjectItem(user_json, "avatar_url");

  struct Payload *payload = malloc(sizeof(struct Payload));
  payload->user_name = user_login->valuestring;
  payload->avatar = user_avatar_url->valuestring;

  char *created_token = create_jwt(payload);
  printf("Token Created: %s\n", created_token);

  // Cleanup
  cJSON_Delete(json);
  free(response);

  cJSON_Delete(user_json);
  free(githubUserResponse);

  send(client_fd, "Authorized", strlen("Authorized"), 0);
}
