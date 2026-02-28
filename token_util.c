#include "token_util.h"
#include "log.h"

char *get_jwks() {
  FILE *file = fopen("JWKS.json", "r");
  if (file == NULL) {
    printf("Failed to open JWKS.json file\n");
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
    }

    strcat(jwks, line);
  }

  fclose(file);
  return jwks;
}

char *create_jwt(struct Payload *payload) {
  jwt_builder_t *builder = jwt_builder_new();

  if (builder == NULL) {
    LOG_ERROR("Failed to create JWT builder");
    return NULL;
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

  char *token = jwt_builder_generate(builder);
  jwt_builder_free(builder);

  return token;
}

static int verify_callback(jwt_t *jwt, jwt_config_t *config) {
  struct Payload *payload = (struct Payload *)config->ctx;
  jwt_value_t jval;

  jwt_set_GET_STR(&jval, "user_name");
  if (jwt_claim_get(jwt, &jval) == JWT_VALUE_ERR_NONE && jval.str_val != NULL) {
    payload->user_name = strdup(jval.str_val);
  }

  jwt_set_GET_STR(&jval, "avatar");
  if (jwt_claim_get(jwt, &jval) == JWT_VALUE_ERR_NONE && jval.str_val != NULL) {
    payload->avatar = strdup(jval.str_val);
  }

  return 0;
}

struct Payload *verify_jwt(char *token) {
  jwt_t *jwt = NULL;
  jwt_checker_t *checker = jwt_checker_new();

  if (checker == NULL) {
    LOG_ERROR("Failed to create JWT checker");
    return NULL;
  }

  struct Payload *payload = malloc(sizeof(struct Payload));
  payload->user_name = NULL;
  payload->avatar = NULL;

  char *jwt_secret = get_jwks();

  jwk_set_t *keys = jwks_create_strn(jwt_secret, strlen(jwt_secret));
  const jwk_item_t *key = jwks_item_get(keys, 0);

  jwt_checker_setkey(checker, JWT_ALG_HS256, key);
  jwt_checker_setcb(checker, verify_callback, payload);

  int is_valid = jwt_checker_verify(checker, token);
  jwt_checker_free(checker);

  if (is_valid != 0) {
    printf("JWT verification failed!\n");
    free(payload->user_name);
    free(payload->avatar);
    free(payload);
    return NULL;
  }

  return payload;
}
