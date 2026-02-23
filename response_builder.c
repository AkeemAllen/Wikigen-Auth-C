#include "response_builder.h"
#include <stdio.h>
#include <string.h>

#define RESPONSE_BUFFER_SIZE 8192

static const char *status_text(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

static const char *content_type_string(ContentType content_type) {
    switch (content_type) {
        case CONTENT_TYPE_TEXT: return "text/plain";
        case CONTENT_TYPE_JSON: return "application/json";
        case CONTENT_TYPE_HTML: return "text/html";
        default:                return "text/plain";
    }
}

int send_response(int client_fd, int status_code, ContentType content_type, const char *body) {
    char response[RESPONSE_BUFFER_SIZE];
    int body_len = (body != NULL) ? (int)strlen(body) : 0;

    int written = snprintf(
        response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code,
        status_text(status_code),
        content_type_string(content_type),
        body_len,
        body != NULL ? body : ""
    );

    if (written < 0 || written >= RESPONSE_BUFFER_SIZE) {
        const char *fallback =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 19\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Response too large.";
        return (int)send(client_fd, fallback, strlen(fallback), 0);
    }

    return (int)send(client_fd, response, (size_t)written, 0);
}
