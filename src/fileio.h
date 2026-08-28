#ifndef ERIRE_FILEIO_H
#define ERIRE_FILEIO_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

bool er_file_exists(const char *path);
bool er_directory_exists(const char *path);
unsigned long long er_file_last_write_time(const char *path);
bool er_file_read_all(const char *path, char **out_data, size_t *out_size, ErError *error);
bool er_file_write_all(const char *path, const void *data, size_t size, ErError *error);
bool er_file_copy(const char *source_path, const char *dest_path, ErError *error);
bool er_path_has_extension(const char *path, const char *extension);
void er_path_basename_without_extension(const char *path, char *out, size_t out_capacity);
void er_path_join(const char *left, const char *right, char *out, size_t out_capacity);
void er_path_dirname(const char *path, char *out, size_t out_capacity);
bool er_directory_create_recursive(const char *path, ErError *error);
bool er_get_current_module_path(char *out, size_t out_capacity, ErError *error);

#endif
