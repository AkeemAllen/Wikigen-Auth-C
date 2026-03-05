#ifndef TOKEN_UTIL_H
#define TOKEN_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jwt.h>
#include "env.h"

typedef struct {
  char *user_name;
  char *avatar;
} Payload;

char *create_jwt(Payload *out);
Payload *verify_jwt(char *token);

#endif
