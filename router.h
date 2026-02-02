#ifndef ROUTE_H
#define ROUTE_H

#include <stdio.h>
#include <stdlib.h>
#include "request_parser.h"

struct RouteNode {
    char *segment;                    
    struct RouteNode **children;      
    size_t child_count;               
    size_t child_capacity;            
    void (*handler)(int, struct Request*); 
    char *allowed_methods;            
};

struct RouteNode *create_route_node(const char *segment, void (*handler)(int, struct Request*));
int add_child_route(struct RouteNode *parent, struct RouteNode *child);
void print_router(struct RouteNode *node, int level);
struct RouteNode *find_route(struct RouteNode *node, char *segment);

#endif
