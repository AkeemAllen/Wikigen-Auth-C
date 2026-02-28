// logging is AI generated
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_ERROR = 2
} LogLevel;

int  log_init(const char *filepath);  /* returns 0 on success, -1 on failure */
void log_close(void);
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);

#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif
