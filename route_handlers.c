#include "route_handlers.h"
#include "response_builder.h"
#include <stdio.h>

// AI generated function
char *load_html_with_token(const char *token_value) {
  // --- Read file into buffer ---
  FILE *f = fopen("code.html", "rb");
  if (!f) {
    perror("fopen");
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  rewind(f);

  char *file_buf = malloc(file_size + 1);
  if (!file_buf) {
    fclose(f);
    return NULL;
  }

  if (fread(file_buf, 1, file_size, f) != (size_t)file_size) {
    free(file_buf);
    fclose(f);
    return NULL;
  }
  file_buf[file_size] = '\0';
  fclose(f);

  // --- Replace ${token} with token_value ---
  const char *placeholder = "${token}";
  size_t placeholder_len = strlen(placeholder);
  size_t token_len = strlen(token_value);

  // Find the placeholder
  char *pos = strstr(file_buf, placeholder);
  if (!pos) {
    // No placeholder found — return file as-is
    return file_buf;
  }

  // Allocate output buffer
  size_t new_size = file_size - placeholder_len + token_len + 1;
  char *out_buf = malloc(new_size);
  if (!out_buf) {
    free(file_buf);
    return NULL;
  }

  // Build the result: part before + token + part after
  size_t prefix_len = pos - file_buf;
  memcpy(out_buf, file_buf, prefix_len);
  memcpy(out_buf + prefix_len, token_value, token_len);
  strcpy(out_buf + prefix_len + token_len, pos + placeholder_len);

  free(file_buf);
  return out_buf;
}

void handle_root(int client_fd, struct Request *request) {
  send_response(client_fd, 200, CONTENT_TYPE_TEXT, "Welcome to Wikigen-Auth");
}

void handle_test(int client_fd, struct Request *request) {
  send_response(client_fd, 200, CONTENT_TYPE_TEXT, "Test");
  printf("Test\n");
}

void handle_create_repo(int client_fd, struct Request *request) {
  cJSON *json = cJSON_Parse(request->body);
  if (json == NULL) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Failed to parse JSON");
    return;
  }
  cJSON *token = cJSON_GetObjectItem(json, "token");
  cJSON *wiki_name = cJSON_GetObjectItem(json, "wikiName");

  struct Payload *payload = verify_jwt(token->valuestring);
  if (payload == NULL) {
    send_response(client_fd, 401, CONTENT_TYPE_TEXT, "Failed to verify JWT");
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
      code = strdup(request->query_param_values[i]);
    }
  }

  if (code == NULL || strlen(code) == 0) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT,
                  "Code Parameter is undefined");
    return;
  }

  char *clientSecret = get_env_value("CLIENT_SECRET");
  char *clientID = get_env_value("CLIENT_ID");

  char githubOauthUrl[512];
  snprintf(githubOauthUrl, sizeof(githubOauthUrl),
           "https://github.com/login/oauth/access_token?"
           "client_id=%s&client_secret=%s&code=%s",
           clientID, clientSecret, code);

  char headers[20][100];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");

  char *response = perform_curl_request(githubOauthUrl, "POST", headers);
  if (!response) {
    send_response(client_fd, 500, CONTENT_TYPE_TEXT,
                  "Failed to perform request");
    return;
  }

  cJSON *json = cJSON_Parse(response);
  if (json == NULL) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Failed to parse JSON");
    free(response);
    return;
  }

  cJSON *error_item = cJSON_GetObjectItem(json, "error");
  if (error_item != NULL) {
    printf("Error: %s\n", error_item->valuestring);
    send_response(client_fd, 401, CONTENT_TYPE_TEXT, "Error");
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

  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");
  snprintf(headers[1], sizeof(headers[1]), "Authorization: Bearer ");
  strncat(headers[1], token.access_token, strlen(token.access_token) + 1);
  snprintf(headers[2], sizeof(headers[2]), "User-Agent: Wikigen-Auth-C");

  char *githubUserResponse =
      perform_curl_request(githubUserUrl, "GET", headers);
  if (!githubUserResponse) {
    send_response(client_fd, 500, CONTENT_TYPE_TEXT,
                  "Failed to perform request");
    free(githubUserResponse);
    return;
  }

  cJSON *user_json = cJSON_Parse(githubUserResponse);
  if (user_json == NULL) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Failed to parse JSON");
    free(githubUserResponse);
    return;
  }

  cJSON *user_error_item = cJSON_GetObjectItem(user_json, "error");
  if (user_error_item != NULL) {
    printf("Error: %s\n", user_error_item->valuestring);
    send_response(client_fd, 401, CONTENT_TYPE_TEXT, "Error");
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
    libsql_statement_deinit(insert_stmt);
  }

  libsql_statement_deinit(query_stmt);

  cJSON *user_avatar_url = cJSON_GetObjectItem(user_json, "avatar_url");

  struct Payload *payload = malloc(sizeof(struct Payload));
  payload->user_name = user_login->valuestring;
  payload->avatar = user_avatar_url->valuestring;

  char *created_token = create_jwt(payload);

  // Cleanup
  cJSON_Delete(json);
  free(response);

  cJSON_Delete(user_json);
  free(githubUserResponse);

  char *html = load_html_with_token(created_token);
  if (!html) {
    fprintf(stderr, "Failed to load HTML\n");
    return;
  }

  send_response(client_fd, 200, CONTENT_TYPE_HTML, html);
  return;
}
