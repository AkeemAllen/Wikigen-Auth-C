#ifndef RESPONSE_BUILDER_H
#define RESPONSE_BUILDER_H

#include <sys/socket.h>

typedef enum {
    CONTENT_TYPE_TEXT,
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_HTML
} ContentType;

int send_response(int client_fd, int status_code, ContentType content_type, const char *body);

#endif
