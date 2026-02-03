#ifndef TOKEN_UTIL_H
#define TOKEN_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jwt.h>

struct Payload {
  char *user_name;
  char *avatar;
};

char *create_jwt(struct Payload *payload);
struct Payload *verify_jwt(char *token);

#endif
