#include "router.h"
#include "log.h"
#include "route_handlers.h"

static RouteNode *g_router;

RouteNode *create_route_node(char *segment, void (*handler)(int, Request *)) {
  RouteNode *node = malloc(sizeof(RouteNode));
  node->segment = segment;
  node->children = NULL;
  node->child_count = 0;
  node->child_capacity = 0;
  node->handler = handler;
  node->allowed_methods = NULL;
  return node;
}

int add_child_route(RouteNode *parent, RouteNode *child) {
  if (parent->child_count >= parent->child_capacity) {
    int new_capcity =
        parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
    RouteNode **new_children =
        realloc(parent->children, new_capcity * sizeof(RouteNode *));

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

RouteNode *find_route(RouteNode *node, char *segment) {
  if (node->segment == NULL) {
    return NULL;
  }

  if (strcmp(node->segment, segment) == 0) {
    return node;
  }

  for (int i = 0; i < (int)node->child_count; i++) {
    RouteNode *child = find_route(node->children[i], segment);
    if (child != NULL) {
      return child;
    }
  }

  return NULL;
}

int route_request(int client_fd, Request *request) {
  RouteNode *current_node = g_router;
  for (int i = 0; i < request->segment_count; i++) {
    current_node = find_route(current_node, request->segments[i]);

    if (current_node == NULL) {
      LOG_INFO("No route found for %s", request->segments[i]);
      return -1;
    }

    if (i == request->segment_count - 1) {
      current_node->handler(client_fd, request);
      return 0;
    }
  }
  return -1;
}

void init_routes() {
  g_router = create_route_node("", handle_root);
  RouteNode *create_repo = create_route_node("create_repo", handle_create_repo);
  RouteNode *authorize = create_route_node("authorize", handle_authorize);
  RouteNode *test = create_route_node("test", handle_test);

  add_child_route(g_router, create_repo);
  add_child_route(g_router, authorize);
  add_child_route(g_router, test);
}
