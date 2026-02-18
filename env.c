#include "env.h"

char* get_env_value(const char* key) {
    FILE* file = fopen(".env", "r");
    if (file == NULL) {
        printf("Failed to open .env file\n");
        return NULL;
    }

    char line[1024];
    size_t key_len = strlen(key);
    char* value = NULL;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
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
