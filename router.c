#include "router.h"
#include "route_handlers.h"

static struct RouteNode *g_router;

struct RouteNode *create_route_node(char *segment,
                                    void (*handler)(int, struct Request *)) {
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
  // if (node->handler)
  // node->handler(0, NULL, NULL);
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

int route_request(int client_fd, struct Request *request) {
  struct RouteNode *current_node = g_router;
  for (int i = 0; i < request->segment_count; i++) {
    printf("Segment: %s\n", request->segments[i]);
    current_node = find_route(current_node, request->segments[i]);

    if (current_node == NULL) {
      printf("No route found for %s\n", request->segments[i]);
      return -1;
    }

    if (i == request->segment_count - 1) {
      current_node->handler(client_fd, request);
      return 0;
    }
  }
  send(client_fd, "No route found", strlen("No route found"), 0);
  return -1;
}

void init_routes() {
  g_router = create_route_node("", handle_root);
  struct RouteNode *create_repo =
      create_route_node("create_repo", handle_create_repo);
  struct RouteNode *authorize =
      create_route_node("authorize", handle_authorize);

  add_child_route(g_router, create_repo);
  add_child_route(g_router, authorize);
}
