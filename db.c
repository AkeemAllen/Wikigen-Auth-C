#include "db.h"

libsql_database_t db;
libsql_connection_t conn;

bool init_db() {
  libsql_setup((libsql_config_t){0});

  char *db_url = get_env_value("TURSO_DATABASE_URL");
  char *auth_token = get_env_value("TURSO_AUTH_TOKEN");

  db = libsql_database_init(
      (libsql_database_desc_t){.url = db_url, .auth_token = auth_token});

  if (db.err) {
    fprintf(stderr, "Failed to initialize database: %s\n",
            libsql_error_message(db.err));
    return false;
  }

  conn = libsql_database_connect(db);
  if (conn.err) {
    fprintf(stderr, "Failed to connect to database: %s\n",
            libsql_error_message(conn.err));
    return false;
  }

  return true;
}

libsql_connection_t get_db_connection() { return conn; }
