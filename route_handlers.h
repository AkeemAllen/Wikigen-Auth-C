#ifndef REQUEST_HANDLERS_H
#define REQUEST_HANDLERS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "request_parser.h"
#include "libsql.h"
#include "cJSON.h"
#include "db.h"
#include "curl_request.h"
#include "token_util.h"
#include "error.h"
#include "log.h"
#include "response_builder.h"
#include "handler_helpers.h"


#define DEFAULT_SIZE 1048

void handle_root(int client_fd, Request *request);
void handle_create_repo(int client_fd,Request *request);
void handle_authorize(int client_fd,Request *request);
void handle_test(int client_fd,  Request *request);

#endif
