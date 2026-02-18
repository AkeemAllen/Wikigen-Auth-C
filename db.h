#ifndef DB_H
#define DB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "libsql.h"
#include "env.h"

bool init_db();
libsql_connection_t get_db_connection();

#endif
