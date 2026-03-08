#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "error.h"
#include "log.h"
#define BUFFER_SIZE 1048576

typedef struct {
    char method[10];
    char resource_path[1024];
    char header_keys[32][128];
    char header_values[32][512];
    int header_count;
    char segments[256][128];
    int segment_count;
    char query_param_keys[32][128];
    char query_param_values[32][512];
    int param_count;
    char body[2048];
} Request;

ErrorContext parse_request(char *buffer, Request *out);
#endif
