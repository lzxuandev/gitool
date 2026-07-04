#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

#define VERBOSITY_QUIET   0
#define VERBOSITY_DEFAULT 1
#define VERBOSITY_VERBOSE 2

extern int global_verbosity;

void log_verbose(const char *format, ...);
void log_error(const char *format, ...);
void log_default(const char *format, ...);

#define PRINT_VERBOSE(fmt, ...) log_verbose("[DEBUG] " fmt, ##__VA_ARGS__)
#define PRINT_ERROR(fmt, ...)   log_error("[ERROR] " fmt, ##__VA_ARGS__)
#define PRINT(fmt, ...)    log_default(fmt, ##__VA_ARGS__)

#endif
