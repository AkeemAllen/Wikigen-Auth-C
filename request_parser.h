#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#define BUFFER_SIZE 104857600

typedef enum {
  OK = 0,
  INVALID_HTTP_METHOD,
  INVALID_HEADER,
} RequestParserError;

struct Request {
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
};

RequestParserError parse_request(char *buffer, struct Request *out);
#endif
