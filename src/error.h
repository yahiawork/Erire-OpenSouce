#ifndef ERIRE_ERROR_H
#define ERIRE_ERROR_H

#include <stdbool.h>
#include <stdio.h>

typedef struct ErError {
    int line;
    int column;
    char message[512];
} ErError;

void er_error_clear(ErError *error);
void er_error_set(ErError *error, int line, int column, const char *fmt, ...);
bool er_error_has(const ErError *error);
void er_error_print(FILE *out, const char *source_name, const ErError *error);

#endif
