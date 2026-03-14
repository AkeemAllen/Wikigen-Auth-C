#include "route_handlers.h"
#include "log.h"
#include <stdio.h>

void handle_root(int client_fd, Request *request) {
  send_response(client_fd, 200, CONTENT_TYPE_TEXT, "Welcome to Wikigen-Auth");
}

void handle_create_repo(int client_fd, Request *request) {
  cJSON *json = cJSON_Parse(request->body);
  printf("JSON %s", request->body);
  if (json == NULL) {
    LOG_ERROR("Failed to parse JSON from request body: %s", request->body);
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Failed to parse JSON");
    return;
  }
  cJSON *token = cJSON_GetObjectItem(json, "token");
  if (token == NULL) {
    LOG_ERROR("Token value is undefined");
    send_response(client_fd, 401, CONTENT_TYPE_TEXT,
                  "Token value is undefined");
    return;
  }

  cJSON *wiki_name = cJSON_GetObjectItem(json, "wikiName");
  if (wiki_name == NULL) {
    LOG_ERROR("Wiki Name is undefined");
    send_response(client_fd, 400, CONTENT_TYPE_TEXT, "Wiki name is undefined");
    return;
  }

  char json_response[DEFAULT_SIZE * 11];

  Payload payload = {.user_name = "", .avatar = ""};
  ErrorContext error = verify_jwt(token->valuestring, &payload);
  if (error.code == INVALID_TOKEN) {
    snprintf(json_response, sizeof(json_response),
             "{\"message\": \"%s\", \"status\": %d}", error.message, 401);
    LOG_INFO("Invalid token response %s", json_response);
    send_response(client_fd, 401, CONTENT_TYPE_JSON, json_response);
    return;
  }
  if (error.code != OK) {
    LOG_ERROR(error.message);
    send_response(client_fd, 401, CONTENT_TYPE_TEXT, error.message);
    return;
  }

  char access_token[DEFAULT_SIZE];
  ErrorContext db_username_token_error =
      get_user_name_and_token_from_db(payload.user_name, access_token);
  if (db_username_token_error.code != OK) {
    snprintf(json_response, sizeof(json_response), "{\"message\": \"%s\"}",
             db_username_token_error.message);
    LOG_ERROR("Error getting username and toke: %s",
              db_username_token_error.message);
    send_response(client_fd, 500, CONTENT_TYPE_JSON, json_response);
    return;
  }

  char data[DEFAULT_SIZE * 10];
  ErrorContext existing_repo_error = get_existing_repo(
      payload.user_name, wiki_name->valuestring, access_token, data);

  if (existing_repo_error.code == NOT_FOUND) {
    ErrorContext create_repo_error = create_new_repo(
        payload.user_name, wiki_name->valuestring, access_token, data);
    if (create_repo_error.code == OK) {
      snprintf(json_response, sizeof(json_response), "{\"ssh_url\": \"%s\"}",
               data);

      char origin[512];
      for (int i = 0; i < request->header_count; i++) {
        if (strcmp(request->header_keys[i], "Origin") == 0) {
          strncpy(origin, request->header_values[i], 512);
          origin[512 - 1] = '\0';
          break;
        }
      }
      printf("Origin: %s", origin);

      char response[8192];
      int written = snprintf(response, sizeof(response),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Access-Control-Allow-Origin: %s\r\n"
                             "Content-Length: %d\r\n"
                             "Connection: close\r\n"
                             "\r\n"
                             "%s",
                             origin, (int)strlen(json_response), json_response);

      send(client_fd, response, (size_t)written, 0);
      return;
    }
    return;
  }

  if (existing_repo_error.code == OK) {
    snprintf(json_response, sizeof(json_response), "{\"ssh_url\": \"%s\"}",
             data);
    LOG_INFO("Retrieved existing repo");
    char origin[512];
    for (int i = 0; i < request->header_count; i++) {
      if (strcmp(request->header_keys[i], "Origin") == 0) {
        strncpy(origin, request->header_values[i], 512);
        origin[512 - 1] = '\0';
        break;
      }
    }
    printf("Origin: %s", origin);

    char response[8192];
    int written = snprintf(response, sizeof(response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Access-Control-Allow-Origin: %s\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           origin, (int)strlen(json_response), json_response);
    // send_response(client_fd, 200, CONTENT_TYPE_JSON, json_response);
    send(client_fd, response, (size_t)written, 0);
    return;
  }

  if (existing_repo_error.code != OK) {
    snprintf(json_response, sizeof(json_response), "{\"message\": \"%s\"}",
             existing_repo_error.message);
    LOG_ERROR("Error getting existing repo: %s", existing_repo_error.message);
    send_response(client_fd, 400, CONTENT_TYPE_JSON, json_response);
    return;
  }
}

void handle_authorize(int client_fd, Request *request) {
  char code[50];
  char error_response[512];

  for (int i = 0; i < request->param_count; i++) {
    if (strcmp(request->query_param_keys[i], "code") == 0) {
      strncpy(code, request->query_param_values[i], 50);
      code[50 - 1] = '\0';
    }
  }

  if (strlen(code) == 0) {
    LOG_ERROR("Code Parameter is undefined");
    send_response(client_fd, 400, CONTENT_TYPE_TEXT,
                  "Code Parameter is undefined");
    return;
  }

  AccessToken token = {0};
  ErrorContext token_error = get_acces_token(&token, code);
  if (token_error.code != OK) {
    snprintf(error_response, sizeof(error_response),
             "Error retrieving access token: %s", token_error.message);
    LOG_ERROR(error_response);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error_response);
    return;
  }

  UserInfo user_info = {0};
  ErrorContext user_info_error = get_user_info(&user_info, token.access_token);
  if (user_info_error.code != OK) {
    snprintf(error_response, sizeof(error_response), "%s",
             user_info_error.message);
    LOG_ERROR(error_response);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error_response);
    return;
  }

  ErrorContext db_user_access_token_error =
      insert_or_edit_user_access_token(&user_info, token.access_token);
  if (db_user_access_token_error.code != OK) {
    snprintf(error_response, sizeof(error_response),
             "Error inserting or editing user, access token: %s",
             db_user_access_token_error.message);
    LOG_ERROR(error_response);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error_response);
    return;
  }

  Payload payload = {0};
  strncpy(payload.user_name, user_info.user_name, sizeof(payload.user_name));
  strncpy(payload.avatar, user_info.avatar, sizeof(payload.avatar));

  char created_token[1024];
  ErrorContext error = create_jwt(&payload, created_token);
  if (error.code != OK) {
    LOG_ERROR(error.message);
    send_response(client_fd, 500, CONTENT_TYPE_TEXT, error.message);
    return;
  }

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
