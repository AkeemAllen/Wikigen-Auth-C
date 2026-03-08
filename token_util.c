#include "token_util.h"
#include "error.h"
#include "log.h"

char *get_jwks() {
  FILE *file = fopen("JWKS.json", "r");
  if (file == NULL) {
    LOG_ERROR("Failed to open JWKS.json file");
    return NULL;
  }

  size_t total_size = 1024;
  char *jwks = malloc(total_size);
  jwks[0] = '\0';

  char line[1024];
  while (fgets(line, sizeof(line), file)) {
    size_t line_size = strlen(line);
    size_t current_size = strlen(jwks);

    if (current_size + line_size + 1 > total_size) {
      total_size *= 2;
      jwks = realloc(jwks, total_size);
      if (jwks == NULL) {
        LOG_ERROR("Failed to realloc JWKS");
        return NULL;
      }
    }

    strcat(jwks, line);
  }

  fclose(file);
  return jwks;
}

ErrorContext create_jwt(Payload *payload, char *out) {
  jwt_builder_t *builder = jwt_builder_new();

  if (builder == NULL) {
    LOG_ERROR("Failed to create JWT builder");
    return ERROR_CONTEXT(ERROR, "Failed to create JWT builder");
  }

  jwt_value_t user_name;
  jwt_set_SET_STR(&user_name, "user_name", payload->user_name);
  jwt_value_t avatar;
  jwt_set_SET_STR(&avatar, "avatar", payload->avatar);
  jwt_value_t iat;
  jwt_set_SET_INT(&iat, "iat", time(NULL));
  jwt_value_t iss;
  jwt_set_SET_STR(&iss, "iss", "Wikigen-Auth");
  jwt_value_t aud;
  jwt_set_SET_STR(&aud, "aud", "Wikigen-Client");
  jwt_value_t exp;
  jwt_set_SET_INT(&exp, "exp", time(NULL) + 28800);

  jwt_builder_claim_set(builder, &user_name);
  jwt_builder_claim_set(builder, &avatar);
  jwt_builder_claim_set(builder, &exp);
  jwt_builder_claim_set(builder, &iat);
  jwt_builder_claim_set(builder, &iss);
  jwt_builder_claim_set(builder, &aud);

  char *jwt_secret = get_jwks();

  jwk_set_t *keys = jwks_create_strn(jwt_secret, strlen(jwt_secret));
  const jwk_item_t *key = jwks_item_get(keys, 0);

  jwt_builder_setkey(builder, JWT_ALG_HS256, key);

  char *generated_token = jwt_builder_generate(builder);
  jwt_builder_free(builder);

  strncpy(out, generated_token, 1024);
  out[1024 - 1] = '\0';
  free(generated_token);

  return ERROR_CONTEXT(OK, "OK");
}

static int verify_callback(jwt_t *jwt, jwt_config_t *config) {
  Payload *payload = (Payload *)config->ctx;
  jwt_value_t jval;

  jwt_set_GET_STR(&jval, "user_name");
  if (jwt_claim_get(jwt, &jval) == JWT_VALUE_ERR_NONE && jval.str_val != NULL) {
    strncpy(payload->user_name, jval.str_val, sizeof(payload->user_name));
    payload->user_name[sizeof(payload->user_name) - 1] = '\0';
  }

  jwt_set_GET_STR(&jval, "avatar");
  if (jwt_claim_get(jwt, &jval) == JWT_VALUE_ERR_NONE && jval.str_val != NULL) {
    strncpy(payload->avatar, jval.str_val, sizeof(payload->avatar));
    payload->avatar[sizeof(payload->avatar) - 1] = '\0';
  }

  return 0;
}

ErrorContext verify_jwt(char *token, Payload *out) {
  jwt_t *jwt = NULL;
  jwt_checker_t *checker = jwt_checker_new();

  if (checker == NULL) {
    LOG_ERROR("Failed to create JWT checker");
    return ERROR_CONTEXT(ERROR, "Failed to create JWT checker");
  }

  char *jwt_secret = get_jwks();

  jwk_set_t *keys = jwks_create_strn(jwt_secret, strlen(jwt_secret));
  const jwk_item_t *key = jwks_item_get(keys, 0);

  jwt_checker_setkey(checker, JWT_ALG_HS256, key);
  jwt_checker_setcb(checker, verify_callback, out);

  int is_valid = jwt_checker_verify(checker, token);
  jwt_checker_free(checker);

  if (is_valid != 0) {
    return ERROR_CONTEXT(INVALID_TOKEN, "JWT verification failed!");
  }
  free(jwt_secret);

  return ERROR_CONTEXT(OK, "OK");
}
