#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#define BUFFER_SIZE 104857600

struct Request {
    char *method;
    char *resource_path;
    char *segments[256];
    int segment_count;
    char *query_param_keys[32];
    char *query_param_values[32];
    int param_count;
    char *body;
};

struct Request *parse_request(char *buffer);
#endif
