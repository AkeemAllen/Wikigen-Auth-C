#include "request.h"

char *perform_curl_request(const char *url, const char *method) {
  if (strcmp("POST", method) != 0) {
    return NULL;
  }

  Response response = {.data = malloc(1), .size = 0};
  if (response.data == NULL) {
    return NULL;
  }
  response.data[0] = '\0';

  CURL *curl = curl_easy_init();
  if (!curl) {
    curl_easy_cleanup(curl);
    return NULL;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Accept: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    free(response.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return NULL;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response.data;
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t real_size = size * nmemb;
  Response *resp = (Response *)userp;

  char *ptr = realloc(resp->data, resp->size + real_size + 1);
  if (ptr == NULL) {
    printf("Not enough memory (realloc failed)\n");
    return 0;
  }

  resp->data = ptr;
  memcpy(&(resp->data[resp->size]), contents, real_size);
  resp->size += real_size;
  resp->data[resp->size] = '\0';

  return real_size;
}
