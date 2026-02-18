#ifndef REQUEST_HANDLERS_H
#define REQUEST_HANDLERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request_parser.h"
#include "libsql.h"
#include "cJSON.h"
#include "db.h"
#include "curl_request.h"

void handle_root(int client_fd, struct Request *request);
void handle_create_repo(int client_fd, struct Request *request);
void handle_authorize(int client_fd, struct Request *request);

#endif
