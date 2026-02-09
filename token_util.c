#include "token_util.h"

char* get_env_value(const char* key) {
    FILE* file = fopen(".env", "r");
    if (file == NULL) {
        return NULL;
    }

    char line[512];
    size_t key_len = strlen(key);
    char* value = NULL;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            char* val_start = line + key_len + 1;
            
            while (*val_start == ' ' || *val_start == '\t') {
                val_start++;
            }

            value = malloc(strlen(val_start) + 1);
            if (value != NULL) {
                strcpy(value, val_start);
            }
            break;
        }
    }

    fclose(file);
    return value;
}

char *create_jwt(struct Payload *payload) {
  jwt_builder_t *builder = jwt_builder_new();

  if (builder == NULL) {
    printf("Failed to create JWT builder\n");
    return NULL;
  }

  jwt_value_t user_name;
  jwt_set_SET_STR(&user_name, "user_name", payload->user_name);
  jwt_value_t avatar;
  jwt_set_SET_STR(&avatar, "avatar", payload->avatar);
  jwt_value_t exp;
  jwt_set_SET_INT(&exp, "exp", time(NULL) + 28800);
  
  jwt_builder_claim_set(builder, &user_name);
  jwt_builder_claim_set(builder, &avatar);
  jwt_builder_claim_set(builder, &exp);
  //char *jwt_secret = get_env_value("JWT_SECRET");

  //jwt_builder_setkey(builder, JWT_ALG_HS256, (unsigned char *)jwt_secret, strlen(jwt_secret));

  char *token = jwt_builder_generate(builder);
  jwt_builder_free(builder);
  printf("JWT: %s\n", token);

  return token;
}

struct Payload *verify_jwt(char *token) {
  jwt_t *jwt = NULL;
  jwt_checker_t *checker = jwt_checker_new();

  if (checker == NULL) {
    printf("Failed to create JWT checker\n");
    return NULL;
  }

  // char *jwt_secret = get_env_value("JWT_SECRET");
  int is_valid = jwt_checker_verify(checker, token);

  if (is_valid != 0) {
    printf("JWT verification failed!\n");
    return NULL;
  }

  struct Payload *payload = malloc(sizeof(struct Payload));

  payload->user_name = jwt_checker_claim_get(checker, JWT_CLAIM_EXP);
  payload->avatar = "";
  printf("User Name: %s\n", payload->user_name);
  jwt_checker_free(checker);

  return payload;
}
