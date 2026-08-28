#include "fileio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

bool er_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

bool er_directory_exists(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
#else
    struct stat info;
    if (stat(path, &info) != 0) {
        return false;
    }
    return S_ISDIR(info.st_mode);
#endif
}


unsigned long long er_file_last_write_time(const char *path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER time_value;

    if (!path || path[0] == '\0') {
        return 0ull;
    }

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return 0ull;
    }

    time_value.LowPart = data.ftLastWriteTime.dwLowDateTime;
    time_value.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return (unsigned long long) time_value.QuadPart;
#else
    struct stat info;

    if (!path || path[0] == '\0') {
        return 0ull;
    }

    if (stat(path, &info) != 0) {
        return 0ull;
    }

    return (unsigned long long) info.st_mtime;
#endif
}

bool er_file_read_all(const char *path, char **out_data, size_t *out_size, ErError *error) {
    FILE *file;
    long length;
    size_t read_size;
    char *buffer;

    *out_data = NULL;
    if (out_size) {
        *out_size = 0;
    }

    file = fopen(path, "rb");
    if (!file) {
        er_error_set(error, 0, 0, "Could not open file: %s", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not seek file: %s", path);
        return false;
    }

    length = ftell(file);
    if (length < 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not get file length: %s", path);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not rewind file: %s", path);
        return false;
    }

    buffer = (char *) malloc((size_t) length + 1);
    if (!buffer) {
        fclose(file);
        er_error_set(error, 0, 0, "Out of memory while reading: %s", path);
        return false;
    }

    read_size = fread(buffer, 1, (size_t) length, file);
    fclose(file);
    if (read_size != (size_t) length) {
        free(buffer);
        er_error_set(error, 0, 0, "Could not fully read: %s", path);
        return false;
    }

    buffer[length] = '\0';
    *out_data = buffer;
    if (out_size) {
        *out_size = (size_t) length;
    }
    return true;
}

bool er_file_write_all(const char *path, const void *data, size_t size, ErError *error) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        er_error_set(error, 0, 0, "Could not open output file: %s", path);
        return false;
    }

    if (size > 0 && fwrite(data, 1, size, file) != size) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not write output file: %s", path);
        return false;
    }

    fclose(file);
    return true;
}

bool er_file_copy(const char *source_path, const char *dest_path, ErError *error) {
    char *data = NULL;
    size_t size = 0;

    if (!er_file_read_all(source_path, &data, &size, error)) {
        return false;
    }

    if (!er_file_write_all(dest_path, data, size, error)) {
        free(data);
        return false;
    }

    free(data);
    return true;
}

bool er_path_has_extension(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return false;
    }
#ifdef _WIN32
    return _stricmp(dot, extension) == 0;
#else
    return strcmp(dot, extension) == 0;
#endif
}

void er_path_basename_without_extension(const char *path, char *out, size_t out_capacity) {
    const char *base = path;
    const char *slash = strrchr(path, '\\');
    const char *slash2 = strrchr(path, '/');
    const char *dot;
    size_t len;

    if (slash && slash + 1 > base) {
        base = slash + 1;
    }
    if (slash2 && slash2 + 1 > base) {
        base = slash2 + 1;
    }

    dot = strrchr(base, '.');
    len = dot ? (size_t) (dot - base) : strlen(base);
    if (len + 1 > out_capacity) {
        len = out_capacity - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

void er_path_join(const char *left, const char *right, char *out, size_t out_capacity) {
    size_t left_len = strlen(left);
    int needs_sep = left_len > 0 && left[left_len - 1] != '\\' && left[left_len - 1] != '/';
    snprintf(out, out_capacity, "%s%s%s", left, needs_sep ? "\\" : "", right);
}

void er_path_dirname(const char *path, char *out, size_t out_capacity) {
    const char *last = strrchr(path, '\\');
    const char *last2 = strrchr(path, '/');
    size_t len;

    if (last2 && (!last || last2 > last)) {
        last = last2;
    }
    if (!last) {
        snprintf(out, out_capacity, ".");
        return;
    }

    len = (size_t) (last - path);
    if (len + 1 > out_capacity) {
        len = out_capacity - 1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
}

static bool er_make_one_directory(const char *path) {
    if (!path || path[0] == '\0') {
        return true;
    }
#ifdef _WIN32
    size_t len = strlen(path);
    if (len == 2 && path[1] == ':') {
        return true;
    }
    if (len == 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return true;
    }
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return mkdir(path, 0755) == 0 || access(path, F_OK) == 0;
#endif
}

bool er_directory_create_recursive(const char *path, ErError *error) {
    char buffer[1024];
    size_t len;
    size_t i;

    len = strlen(path);
    if (len >= sizeof(buffer)) {
        er_error_set(error, 0, 0, "Path is too long: %s", path);
        return false;
    }

    memcpy(buffer, path, len + 1);
    for (i = 1; i < len; ++i) {
        if (buffer[i] == '\\' || buffer[i] == '/') {
            char saved = buffer[i];
            buffer[i] = '\0';
            if (buffer[0] != '\0' && !er_make_one_directory(buffer)) {
                er_error_set(error, 0, 0, "Could not create directory: %s", buffer);
                return false;
            }
            buffer[i] = saved;
        }
    }

    if (!er_make_one_directory(buffer)) {
        er_error_set(error, 0, 0, "Could not create directory: %s", buffer);
        return false;
    }
    return true;
}


bool er_get_current_module_path(char *out, size_t out_capacity, ErError *error) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, out, (DWORD) out_capacity);
    if (len == 0 || len >= out_capacity) {
        er_error_set(error, 0, 0, "Could not determine current executable path");
        return false;
    }
    return true;
#else
    (void) out;
    (void) out_capacity;
    er_error_set(error, 0, 0, "Current executable path is only implemented on Windows");
    return false;
#endif
}
