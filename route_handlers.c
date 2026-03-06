#include "route_handlers.h"
#include "error.h"
#include "log.h"
#include "response_builder.h"
#include <string.h>

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

void handle_root(int client_fd, Request *request) {
  send_response(client_fd, 200, CONTENT_TYPE_TEXT, "Welcome to Wikigen-Auth");
}

void handle_test(int client_fd, Request *request) {
  send_response(client_fd, 200, CONTENT_TYPE_TEXT, "Test");
  printf("Test\n");
}

ErrorContext get_existing_repo(char *user_name, char *wiki_name,
                               char *access_token, char *data) {
  char existing_repo_url[512];
  snprintf(existing_repo_url, sizeof(existing_repo_url),
           "https://api.github.com/repos/%s/%s", user_name, wiki_name);
  char headers[20][DEFAULT_SIZE];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");
  snprintf(headers[1], sizeof(headers[1]), "Authorization: Bearer %s",
           access_token);
  snprintf(headers[2], sizeof(headers[2]), "User-Agent: Wikigen-Auth-C");

  char *response = perform_curl_request(existing_repo_url, "GET", headers, "");
  if (!response) {
    return ERROR_CONTEXT(REQUEST_ERROR,
                         "Failed to perform request for existing repo");
  }

  cJSON *repo_json = cJSON_Parse(response);
  if (repo_json == NULL) {
    LOG_ERROR("Failed to parse JSON");
    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "Failed to parse JSON");
  }

  cJSON *status = cJSON_GetObjectItem(repo_json, "status");
  if (status != NULL) {
    if (strcmp(status->valuestring, "404") == 0 ||
        strcmp(status->valuestring, "403") == 0) {
      strncpy(data, response, sizeof(*data) - 1);
      data[sizeof(*data) - 1] = '\0';
      free(response);
      LOG_ERROR("Failed to fetch existing repository");
      return ERROR_CONTEXT(NOT_FOUND, "Failed to fetch existing repository");
    }

    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "Invalid status");
  }

  cJSON *ssh_url = cJSON_GetObjectItem(repo_json, "ssh_url");
  if (ssh_url == NULL) {
    LOG_ERROR("No SSH URL found");
    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "No SSH URL found");
  }

  strncpy(data, ssh_url->valuestring, sizeof(*data) - 1);
  data[sizeof(*data) - 1] = '\0';

  cJSON_Delete(repo_json);
  free(response);

  return ERROR_CONTEXT(OK, "OK");
}

ErrorContext create_new_repo(char *user_name, char *wiki_name,
                             char *access_token, char *data) {
  char create_repo_url[512];
  snprintf(create_repo_url, sizeof(create_repo_url),
           "https://api.github.com/user/repos");
  char headers[20][DEFAULT_SIZE];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");
  snprintf(headers[1], sizeof(headers[1]), "Authorization: Bearer %s",
           access_token);
  snprintf(headers[2], sizeof(headers[2]), "User-Agent: Wikigen-Auth-C");

  char body[1024];
  snprintf(body, sizeof(body), "{\"name\":\"%s\"}", wiki_name);

  char *response = perform_curl_request(create_repo_url, "POST", headers, body);
  if (!response) {
    return ERROR_CONTEXT(REQUEST_ERROR,
                         "Failed to perform request to create repo");
  }

  cJSON *repo_json = cJSON_Parse(response);
  if (repo_json == NULL) {
    LOG_ERROR("Failed to parse JSON");
    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "Failed to parse JSON");
  }

  cJSON *errors = cJSON_GetObjectItem(repo_json, "errors");
  if (errors == NULL) {
    strncpy(data, response, sizeof(*data) - 1);
    data[sizeof(*data) - 1] = '\0';
    free(response);
    return ERROR_CONTEXT(OK, "No response errors");
  }

  cJSON *ssh_url = cJSON_GetObjectItem(repo_json, "ssh_url");
  if (ssh_url == NULL) {
    LOG_ERROR("No SSH URL found");
    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "No SSH URL found");
  }

  strncpy(data, ssh_url->valuestring, sizeof(*data) - 1);
  data[sizeof(*data) - 1] = '\0';

  cJSON_Delete(repo_json);
  free(response);

  return ERROR_CONTEXT(OK, "OK");
}

void handle_create_repo(int client_fd, Request *request) {
  cJSON *json = cJSON_Parse(request->body);
  if (json == NULL) {
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Failed to parse JSON");
    return;
  }
  cJSON *token = cJSON_GetObjectItem(json, "token");
  if (token == NULL) {
    LOG_ERROR("Token is undefined");
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Token is undefined");
    return;
  }

  cJSON *wiki_name = cJSON_GetObjectItem(json, "wikiName");
  if (wiki_name == NULL) {
    LOG_ERROR("Wiki Name is undefined");
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Wiki name is undefined");
    return;
  }

  Payload *payload = verify_jwt(token->valuestring);
  if (payload == NULL) {
    send_response(client_fd, 401, CONTENT_TYPE_TEXT, "Failed to verify JWT");
    return;
  }

  libsql_connection_t conn = get_db_connection();
  char db_error_response[1024];
  if (conn.err) {
    LOG_ERROR("Failed to get database connection: %s\n",
              libsql_error_message(conn.err));
    snprintf(db_error_response, sizeof(db_error_response),
             "Failed to get database connection: %s",
             libsql_error_message(conn.err));
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, db_error_response);
    return;
  }

  libsql_statement_t query_stmt =
      libsql_connection_prepare(conn, "SELECT username, access_token FROM user "
                                      "WHERE username = ? LIMIT 1");
  libsql_statement_bind_value(
      query_stmt, libsql_text(payload->user_name, strlen(payload->user_name)));
  if (query_stmt.err) {
    LOG_ERROR("Error preparing query: %s\n",
              libsql_error_message(query_stmt.err));
    snprintf(db_error_response, sizeof(db_error_response),
             "Error preparing query: %s", libsql_error_message(query_stmt.err));
    send_response(client_fd, 500, CONTENT_TYPE_JSON, db_error_response);
    return;
  }
  libsql_rows_t rows = libsql_statement_query(query_stmt);

  if (rows.err) {
    LOG_ERROR("Error executing query: %s\n", libsql_error_message(rows.err));
    snprintf(db_error_response, sizeof(db_error_response),
             "Error executing query: %s", libsql_error_message(rows.err));
    send_response(client_fd, 500, CONTENT_TYPE_JSON, db_error_response);
    return;
  }

  libsql_row_t row;
  row = libsql_rows_next(rows);
  if (row.err) {
    LOG_ERROR("Error retrieving row: %s\n", libsql_error_message(row.err));
    snprintf(db_error_response, sizeof(db_error_response),
             "Error retrieving row: %s", libsql_error_message(row.err));
    send_response(client_fd, 500, CONTENT_TYPE_JSON, db_error_response);
    return;
  }

  if (libsql_row_empty(row)) {
    LOG_ERROR("No record found for user %s", payload->user_name);
    snprintf(db_error_response, sizeof(db_error_response),
             "No record found for user %s", payload->user_name);
    send_response(client_fd, 500, CONTENT_TYPE_JSON, db_error_response);
    return;
  }

  libsql_result_value_t username = libsql_row_value(row, 0);
  libsql_result_value_t access_token = libsql_row_value(row, 1);

  char data[DEFAULT_SIZE];
  ErrorContext existing_repo_error =
      get_existing_repo(payload->user_name, wiki_name->valuestring,
                        (char *)access_token.ok.value.text.ptr, data);

  char json_response[DEFAULT_SIZE * 2];
  if (existing_repo_error.code == NOT_FOUND) {
    ErrorContext create_repo_error =
        create_new_repo(payload->user_name, wiki_name->valuestring,
                        (char *)access_token.ok.value.text.ptr, data);
    if (create_repo_error.code == OK) {
      if (strcmp(create_repo_error.message, "No response errors") == 0) {
        send_response(client_fd, 200, CONTENT_TYPE_JSON, data);
        return;
      }

      snprintf(json_response, sizeof(json_response), "{\"ssh_url\": \"%s\"}",
               data);
      send_response(client_fd, 400, CONTENT_TYPE_JSON, json_response);
      return;
    }
    return;
  }

  if (existing_repo_error.code == OK) {
    snprintf(json_response, sizeof(json_response), "{\"ssh_url\": \"%s\"}",
             data);
    send_response(client_fd, 200, CONTENT_TYPE_JSON, json_response);
    return;
  }

  if (existing_repo_error.code != OK) {
    snprintf(json_response, sizeof(json_response), "{\"message\": \"%s\"}",
             existing_repo_error.message);
    send_response(client_fd, 400, CONTENT_TYPE_JSON, json_response);
    return;
  }
}

typedef struct {
  char *access_token;
  char *token_type;
  char *scope;
} AccessToken;

ErrorContext get_acces_token(AccessToken *out, char *code) {
  char *clientSecret = get_env_value("CLIENT_SECRET");
  char *clientID = get_env_value("CLIENT_ID");

  char githubOauthUrl[512];
  snprintf(githubOauthUrl, sizeof(githubOauthUrl),
           "https://github.com/login/oauth/access_token?"
           "client_id=%s&client_secret=%s&code=%s",
           clientID, clientSecret, code);

  char headers[20][DEFAULT_SIZE];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");

  char *response = perform_curl_request(githubOauthUrl, "POST", headers, "");
  if (!response) {
    LOG_ERROR("Failed to perform request to %s", response);
    return ERROR_CONTEXT(REQUEST_ERROR, "Failed to perform request");
  }

  cJSON *json = cJSON_Parse(response);
  if (json == NULL) {
    free(response);
    LOG_ERROR("Failed to parse JSON");
    return ERROR_CONTEXT(INVALID_JSON, "Failed to parse JSON");
  }

  cJSON *access_token = cJSON_GetObjectItem(json, "access_token");
  cJSON *token_type = cJSON_GetObjectItem(json, "token_type");
  cJSON *scope = cJSON_GetObjectItem(json, "scope");

  if (access_token)
    out->access_token = strdup(access_token->valuestring);
  if (token_type)
    out->token_type = strdup(token_type->valuestring);
  if (scope)
    out->scope = strdup(scope->valuestring);

  cJSON_Delete(json);
  free(response);

  return ERROR_CONTEXT(OK, "OK");
}

typedef struct {
  int32_t id;
  char *user_name;
  char *avatar;
} UserInfo;

ErrorContext get_user_info(UserInfo *out, char *access_token) {
  char githubUserUrl[512];
  snprintf(githubUserUrl, sizeof(githubUserUrl), "https://api.github.com/user");

  char headers[20][DEFAULT_SIZE];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");
  snprintf(headers[1], sizeof(headers[1]), "Authorization: Bearer %s",
           access_token);
  snprintf(headers[2], sizeof(headers[2]), "User-Agent: Wikigen-Auth-C");

  char *response = perform_curl_request(githubUserUrl, "GET", headers, "");
  if (!response) {
    free(response);
    return ERROR_CONTEXT(REQUEST_ERROR, "Failed to perform request");
  }

  cJSON *user_json = cJSON_Parse(response);
  if (user_json == NULL) {
    LOG_ERROR("Failed to parse JSON");
    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "Failed to parse JSON");
  }

  cJSON *user_id = cJSON_GetObjectItem(user_json, "id");
  cJSON *user_login = cJSON_GetObjectItem(user_json, "login");
  cJSON *user_avatar_url = cJSON_GetObjectItem(user_json, "avatar_url");

  if (user_id)
    out->id = user_id->valueint;
  if (user_login)
    out->user_name = strdup(user_login->valuestring);
  if (user_avatar_url)
    out->avatar = strdup(user_avatar_url->valuestring);

  cJSON_Delete(user_json);
  free(response);

  return ERROR_CONTEXT(OK, "OK");
}

ErrorContext insert_or_edit_user_access_token(UserInfo *user_info,
                                              char *access_token) {
  // Rule select query in turso
  libsql_connection_t conn = get_db_connection();
  if (conn.err) {
    LOG_ERROR("Failed to get database connection: %s\n",
              libsql_error_message(conn.err));
    return ERROR_CONTEXT(DATABASE_ERROR, "Failed to get database connection");
  }

  libsql_statement_t query_stmt =
      libsql_connection_prepare(conn, "SELECT * FROM user WHERE github_id = ?");
  libsql_statement_bind_value(query_stmt, libsql_integer(user_info->id));
  if (query_stmt.err) {
    LOG_ERROR("Error preparing query: %s\n",
              libsql_error_message(query_stmt.err));
    return ERROR_CONTEXT(DATABASE_ERROR, "Error preparing query");
  }

  libsql_rows_t rows = libsql_statement_query(query_stmt);
  if (rows.err) {
    LOG_ERROR("Error executing query: %s\n", libsql_error_message(rows.err));
  }

  libsql_row_t row;
  if (!(row = libsql_rows_next(rows)).err && !libsql_row_empty(row)) {
    libsql_statement_t update_stmt = libsql_connection_prepare(
        conn, "UPDATE User SET access_token = ? WHERE github_id = ?");
    libsql_statement_bind_value(
        update_stmt, libsql_text(access_token, strlen(access_token)));
    libsql_statement_bind_value(update_stmt, libsql_integer(user_info->id));
    if (update_stmt.err) {
      LOG_ERROR("Error preparing query: %s\n",
                libsql_error_message(update_stmt.err));
      return ERROR_CONTEXT(DATABASE_ERROR, "Error preparing query");
    }

    libsql_statement_execute(update_stmt);
    libsql_statement_deinit(update_stmt);
  } else {
    libsql_statement_t insert_stmt = libsql_connection_prepare(
        conn, "INSERT INTO User (username, github_id, access_token) VALUES (?, "
              "?, ?)");
    libsql_statement_bind_value(
        insert_stmt,
        libsql_text(user_info->user_name, strlen(user_info->user_name)));
    libsql_statement_bind_value(insert_stmt, libsql_integer(user_info->id));
    libsql_statement_bind_value(
        insert_stmt, libsql_text(access_token, strlen(access_token)));
    if (insert_stmt.err) {
      LOG_ERROR("Error preparing query: %s\n",
                libsql_error_message(insert_stmt.err));
      return ERROR_CONTEXT(DATABASE_ERROR, "Error preparing query");
    }
    libsql_statement_execute(insert_stmt);
    libsql_statement_deinit(insert_stmt);
  }

  libsql_statement_deinit(query_stmt);
  return ERROR_CONTEXT(OK, "OK");
}

void handle_authorize(int client_fd, Request *request) {
  char *code = NULL;
  char error_response[512];

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

  AccessToken token = {0};
  ErrorContext token_error = get_acces_token(&token, code);
  if (token_error.code != OK) {
    snprintf(error_response, sizeof(error_response),
             "Error retrieving access token: %s", token_error.message);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error_response);
    return;
  }

  UserInfo user_info = {0};
  ErrorContext user_info_error = get_user_info(&user_info, token.access_token);
  if (user_info_error.code != OK) {
    snprintf(error_response, sizeof(error_response),
             "Error retrieving user info: %s", user_info_error.message);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error_response);
    return;
  }

  ErrorContext db_user_access_token_error =
      insert_or_edit_user_access_token(&user_info, token.access_token);
  if (db_user_access_token_error.code != OK) {
    snprintf(error_response, sizeof(error_response),
             "Error inserting or editing user, access token: %s",
             db_user_access_token_error.message);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error_response);
    return;
  }

  Payload payload = {0};
  payload.user_name = user_info.user_name;
  payload.avatar = user_info.avatar;

  char *created_token = create_jwt(&payload);

  char *html = load_html_with_token(created_token);
  if (!html) {
    LOG_ERROR("Failed to load HTML");
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, "Failed to load HTML");
    return;
  }

  free(token.access_token);
  free(token.token_type);
  free(token.scope);
  free(user_info.user_name);
  free(user_info.avatar);

  send_response(client_fd, 200, CONTENT_TYPE_HTML, html);
  return;
}
