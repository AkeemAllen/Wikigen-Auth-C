#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "error.h"
#define BUFFER_SIZE 1048576

typedef struct {
    char *method;
    char *resource_path;
    char *header_keys[32];
    char *header_values[32];
    int header_count;
    char *segments[256];
    int segment_count;
    char *query_param_keys[32];
    char *query_param_values[32];
    int param_count;
    char *body;
} Request;

ErrorContext parse_request(char *buffer, Request *out);
#endif
