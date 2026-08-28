#ifndef ERIRE_STUDIO_UTIL_H
#define ERIRE_STUDIO_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

#define STUDIO_MAX_PATH 1024
#define STUDIO_MAX_NAME 128
#define STUDIO_RECENT_PROJECTS 8

bool studio_utf8_to_wide(const char *input, wchar_t *output, size_t output_capacity);
bool studio_wide_to_utf8(const wchar_t *input, char *output, size_t output_capacity);
bool studio_module_directory(char *out, size_t out_capacity);
void studio_join_path(const char *left, const char *right, char *out, size_t out_capacity);
void studio_normalize_slashes(char *path);
bool studio_path_file_url(const char *path, char *out, size_t out_capacity);
bool studio_ensure_directory(const char *path);
bool studio_pick_folder(HWND owner, char *out_path, size_t out_capacity);
bool studio_open_file_dialog(HWND owner, char *out_path, size_t out_capacity);
bool studio_save_file_dialog(HWND owner, const char *suggested_name, char *out_path, size_t out_capacity);
const char *studio_language_from_path(const char *path);

#endif
