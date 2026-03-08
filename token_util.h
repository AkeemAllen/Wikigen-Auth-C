#ifndef TOKEN_UTIL_H
#define TOKEN_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jwt.h>
#include "env.h"
#include "error.h"

typedef struct {
  char user_name[100];
  char avatar[100];
} Payload;

ErrorContext create_jwt(Payload *payload, char *out);
ErrorContext verify_jwt(char *token, Payload *out);

#endif
