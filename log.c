// Logging is AI generated
#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

int log_init(const char *filepath) {
  pthread_mutex_lock(&g_log_mutex);
  g_log_file = fopen(filepath, "a");
  if (!g_log_file) {
    pthread_mutex_unlock(&g_log_mutex);
    return -1;
  }
  setvbuf(g_log_file, NULL, _IOLBF, 0);
  pthread_mutex_unlock(&g_log_mutex);
  return 0;
}

void log_close(void) {
  pthread_mutex_lock(&g_log_mutex);
  if (g_log_file) {
    fflush(g_log_file);
    fclose(g_log_file);
    g_log_file = NULL;
  }
  pthread_mutex_unlock(&g_log_mutex);
}

void log_write(LogLevel level, const char *file, int line, const char *fmt,
               ...) {
  const char *level_str;
  switch (level) {
  case LOG_LEVEL_DEBUG:
    level_str = "DEBUG";
    break;
  case LOG_LEVEL_INFO:
    level_str = "INFO ";
    break;
  case LOG_LEVEL_ERROR:
    level_str = "ERROR";
    break;
  default:
    level_str = "?????";
    break;
  }

  time_t now = time(NULL);
  struct tm tm_buf;
  localtime_r(&now, &tm_buf);
  char ts[20];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

  const char *basename = strrchr(file, '/');
  basename = basename ? basename + 1 : file;

  pthread_mutex_lock(&g_log_mutex);

  if (g_log_file) {
    fprintf(g_log_file, "[%s] [%s] %s:%d | ", ts, level_str, basename, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);
    fputc('\n', g_log_file);
  }

  if (level == LOG_LEVEL_ERROR) {
    fprintf(stderr, "[%s] [%s] %s:%d | ", ts, level_str, basename, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
  }

  pthread_mutex_unlock(&g_log_mutex);
}
