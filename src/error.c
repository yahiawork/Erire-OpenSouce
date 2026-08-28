#include "error.h"

#include <stdarg.h>
#include <stdio.h>

void er_error_clear(ErError *error) {
    if (!error) {
        return;
    }
    error->line = 0;
    error->column = 0;
    error->message[0] = '\0';
}

void er_error_set(ErError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error) {
        return;
    }

    error->line = line;
    error->column = column;

    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

bool er_error_has(const ErError *error) {
    return error && error->message[0] != '\0';
}

void er_error_print(FILE *out, const char *source_name, const ErError *error) {
    if (!out || !er_error_has(error)) {
        return;
    }

    if (source_name && source_name[0] != '\0' && error->line > 0 && error->column > 0) {
        fprintf(out, "\033[1;31merror\033[0m \033[90m[%s:%d:%d]\033[0m: %s\n", source_name, error->line, error->column, error->message);
        return;
    }

    if (source_name && source_name[0] != '\0') {
        fprintf(out, "\033[1;31merror\033[0m \033[90m[%s]\033[0m: %s\n", source_name, error->message);
        return;
    }

    fprintf(out, "\033[1;31merror\033[0m: %s\n", error->message);
}

