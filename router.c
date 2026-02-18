#include "router.h"

struct RouteNode *create_route_node(char *segment,
                                    void (*handler)(int, struct Request *, libsql_connection_t)) {
  struct RouteNode *node = malloc(sizeof(struct RouteNode));
  node->segment = segment;
  node->children = NULL;
  node->child_count = 0;
  node->child_capacity = 0;
  node->handler = handler;
  node->allowed_methods = NULL;
  return node;
}

int add_child_route(struct RouteNode *parent, struct RouteNode *child) {
  if (parent->child_count >= parent->child_capacity) {
    int new_capcity =
        parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
    struct RouteNode **new_children =
        realloc(parent->children, new_capcity * sizeof(struct RouteNode *));

    if (new_children == NULL) {
      return -1;
    }

    parent->children = new_children;
    parent->child_capacity = new_capcity;
  }
  parent->children[parent->child_count] = child;
  parent->child_count++;

  return 0;
}

void print_router(struct RouteNode *node, int level) {
  if (!node)
    return;

  for (int i = 0; i < level; i++)
    printf("|  ");

  printf("- %s\n", node->segment);
  //if (node->handler)
    //node->handler(0, NULL, NULL);
  for (int i = 0; i < (int)node->child_count; i++)
    print_router(node->children[i], level + 1);
}

struct RouteNode *find_route(struct RouteNode *node, char *segment) {
  if (node->segment == NULL) {
    return NULL;
  }

  if (strcmp(node->segment, segment) == 0) {
    return node;
  }

  for (int i = 0; i < (int)node->child_count; i++) {
    struct RouteNode *child = find_route(node->children[i], segment);
    if (child != NULL) {
      return child;
    }
  }

  return NULL;
}
