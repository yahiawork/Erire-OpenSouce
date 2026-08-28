#ifndef ERIRE_RUNTIME_H
#define ERIRE_RUNTIME_H

#include <stdbool.h>

#include "error.h"

bool er_runtime_check_file(const char *path, ErError *error);
bool er_runtime_check_source(const char *source_name, const char *source, ErError *error);
int er_runtime_run_file(const char *path, ErError *error);
int er_runtime_run_file_live(const char *path, unsigned int poll_ms, ErError *error);
int er_runtime_run_source(const char *source_name, const char *source, ErError *error);

#endif
