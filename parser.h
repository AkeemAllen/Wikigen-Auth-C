#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#define BUFFER_SIZE 104857600

// TODO: Rename to Request and Request Parsing
struct URL {
    char *path;
    char *segments[256];
    char *paramKeys[32];
    char *paramValues[32];
    char *queryStrings;
    int segmentCount;
    int paramCount;
    char *body;
};

struct URL *parse_url(char *buffer, int method_size);
#endif
