#ifndef REQUEST_H
#define REQUEST_H

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t size;
} Response;

char *perform_curl_request(const char *url, const char *method, char *received_headers[20]);
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);

#endif
