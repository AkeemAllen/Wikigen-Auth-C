#ifndef HANDLER_HELPERS_H
#define HANDLER_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "libsql.h"
#include "cJSON.h"
#include "db.h"
#include "curl_request.h"
#include "token_util.h"
#include "error.h"
#include "log.h"
#include "response_builder.h"

#define DEFAULT_SIZE 1048

typedef struct {
  char *access_token;
  char *token_type;
  char *scope;
} AccessToken;

typedef struct {
  int32_t id;
  char *user_name;
  char *avatar;
} UserInfo;

// create_repo
ErrorContext get_existing_repo(char *user_name, char *wiki_name,
                               char *access_token, char *data);
ErrorContext create_new_repo(char *user_name, char *wiki_name,
                             char *access_token, char *data);
ErrorContext get_user_name_and_token_from_db(char *username, char *access_token);

// authorize
ErrorContext get_user_info(UserInfo *out, char *access_token);

ErrorContext get_acces_token(AccessToken *out, char *code);
ErrorContext insert_or_edit_user_access_token(UserInfo *user_info,
                                              char *access_token);

char *load_html_with_token(const char *token_value);
#endif
