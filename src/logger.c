#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

int global_verbosity = VERBOSITY_DEFAULT;

void log_verbose(const char *format, ...) {
    if (global_verbosity >= VERBOSITY_VERBOSE) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fflush(stderr);
    }
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}

void log_default(const char *format, ...) {
    if (global_verbosity == VERBOSITY_DEFAULT) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fflush(stdout);
    }
    else if (global_verbosity >= VERBOSITY_VERBOSE) {
        fprintf(stderr, "[DEBUG] ");
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fflush(stderr);
    }

}
