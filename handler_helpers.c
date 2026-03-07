#include "handler_helpers.h"

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
  libsql_connection_deinit(conn);
  return ERROR_CONTEXT(OK, "OK");
}

ErrorContext get_existing_repo(char *user_name, char *wiki_name,
                               char *access_token, char *data) {
  char existing_repo_url[512];
  snprintf(existing_repo_url, sizeof(existing_repo_url),
           "https://api.github.com/repos/%s/%s", user_name, wiki_name);
  char headers[5][DEFAULT_SIZE];
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
      strncpy(data, response, DEFAULT_SIZE);
      data[DEFAULT_SIZE - 1] = '\0';
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

  strncpy(data, ssh_url->valuestring, DEFAULT_SIZE);
  data[DEFAULT_SIZE - 1] = '\0';

  cJSON_Delete(repo_json);
  free(response);

  return ERROR_CONTEXT(OK, "OK");
}

ErrorContext create_new_repo(char *user_name, char *wiki_name,
                             char *access_token, char *data) {
  char create_repo_url[512];
  snprintf(create_repo_url, sizeof(create_repo_url),
           "https://api.github.com/user/repos");
  char headers[5][DEFAULT_SIZE];
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
    strncpy(data, response, DEFAULT_SIZE * 10);
    data[(DEFAULT_SIZE * 10) - 1] = '\0';
    free(response);
    return ERROR_CONTEXT(OK, "No response errors");
  }

  cJSON *ssh_url = cJSON_GetObjectItem(repo_json, "ssh_url");
  if (ssh_url == NULL) {
    LOG_ERROR("No SSH URL found");
    free(response);
    return ERROR_CONTEXT(INVALID_JSON, "No SSH URL found");
  }

  strncpy(data, ssh_url->valuestring, DEFAULT_SIZE);
  data[DEFAULT_SIZE - 1] = '\0';

  cJSON_Delete(repo_json);
  free(response);

  return ERROR_CONTEXT(OK, "OK");
}

ErrorContext get_acces_token(AccessToken *out, char *code) {
  char *clientSecret = get_env_value("CLIENT_SECRET");
  char *clientID = get_env_value("CLIENT_ID");

  char githubOauthUrl[512];
  snprintf(githubOauthUrl, sizeof(githubOauthUrl),
           "https://github.com/login/oauth/access_token?"
           "client_id=%s&client_secret=%s&code=%s",
           clientID, clientSecret, code);

  char headers[5][DEFAULT_SIZE];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");

  char *response = perform_curl_request(githubOauthUrl, "POST", headers, "");
  if (!response) {
    return ERROR_CONTEXT(REQUEST_ERROR,
                         "Failed to perform request to retrieve access token");
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

ErrorContext get_user_info(UserInfo *out, char *access_token) {
  char githubUserUrl[512];
  snprintf(githubUserUrl, sizeof(githubUserUrl), "https://api.github.com/user");

  char headers[5][DEFAULT_SIZE];
  snprintf(headers[0], sizeof(headers[0]), "Accept: application/json");
  snprintf(headers[1], sizeof(headers[1]), "Authorization: Bearer %s",
           access_token);
  snprintf(headers[2], sizeof(headers[2]), "User-Agent: Wikigen-Auth-C");

  char *response = perform_curl_request(githubUserUrl, "GET", headers, "");
  if (!response) {
    free(response);
    return ERROR_CONTEXT(REQUEST_ERROR,
                         "Failed to perform request to get user info");
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

ErrorContext get_user_name_and_token_from_db(char *username,
                                             char *access_token) {
  libsql_connection_t conn = get_db_connection();
  char db_error_response[1024];
  if (conn.err) {
    LOG_ERROR("Failed to get database connection: %s\n",
              libsql_error_message(conn.err));
    return ERROR_CONTEXT(DATABASE_ERROR, "Failed to get database connection");
  }

  libsql_statement_t query_stmt =
      libsql_connection_prepare(conn, "SELECT username, access_token FROM user "
                                      "WHERE username = ? LIMIT 1");
  libsql_statement_bind_value(query_stmt,
                              libsql_text(username, strlen(username)));
  if (query_stmt.err) {
    LOG_ERROR("Error preparing query: %s\n",
              libsql_error_message(query_stmt.err));
    return ERROR_CONTEXT(DATABASE_ERROR, "Error preparing query");
  }
  libsql_rows_t rows = libsql_statement_query(query_stmt);

  if (rows.err) {
    LOG_ERROR("Error executing query: %s\n", libsql_error_message(rows.err));
    return ERROR_CONTEXT(DATABASE_ERROR, "Error executing query");
  }

  libsql_row_t row;
  row = libsql_rows_next(rows);
  if (row.err) {
    LOG_ERROR("Error retrieving row: %s\n", libsql_error_message(row.err));
    return ERROR_CONTEXT(DATABASE_ERROR, "Error retrieving row");
  }

  if (libsql_row_empty(row)) {
    LOG_ERROR("No record found for user %s", username);
    return ERROR_CONTEXT(NOT_FOUND, "No record found for user");
  }

  // libsql_result_value_t username = libsql_row_value(row, 0);
  libsql_result_value_t row_access_token = libsql_row_value(row, 1);
  strncpy(access_token, (char *)row_access_token.ok.value.text.ptr,
          DEFAULT_SIZE);
  access_token[DEFAULT_SIZE - 1] = '\0';

  libsql_connection_deinit(conn);
  return ERROR_CONTEXT(OK, "OK");
}

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
