#ifndef ROUTE_H
#define ROUTE_H

#include <stdio.h>
#include <stdlib.h>
#include "request_parser.h"
#include "libsql.h"

typedef struct RouteNode {
    char *segment;                    
    struct RouteNode **children;      
    size_t child_count;               
    size_t child_capacity;            
    void (*handler)(int, Request*);
    char *allowed_methods;            
} RouteNode;

RouteNode *create_route_node(char *segment, void (*handler)(int,Request*));
int add_child_route(RouteNode *parent,RouteNode *child);
RouteNode *find_route(RouteNode *node, char *segment);
int route_request(int client_fd,  Request *request);
void init_routes();

#endif
