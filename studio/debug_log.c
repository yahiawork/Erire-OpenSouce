#include "debug_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char g_studio_debug_log_path[MAX_PATH];

static void studio_debug_log_timestamp(char *out, size_t out_capacity) {
    SYSTEMTIME now;

    GetLocalTime(&now);
    snprintf(
        out,
        out_capacity,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        (unsigned) now.wYear,
        (unsigned) now.wMonth,
        (unsigned) now.wDay,
        (unsigned) now.wHour,
        (unsigned) now.wMinute,
        (unsigned) now.wSecond,
        (unsigned) now.wMilliseconds
    );
}

static bool studio_debug_log_ensure_path(char *out_path, size_t out_capacity, const char *filename) {
    DWORD length;
    char *last_slash;

    length = GetModuleFileNameA(NULL, out_path, (DWORD) out_capacity);
    if (length == 0 || length >= out_capacity) {
        return false;
    }

    last_slash = strrchr(out_path, '\\');
    if (!last_slash) {
        last_slash = strrchr(out_path, '/');
    }
    if (!last_slash) {
        return false;
    }
    last_slash[1] = '\0';
    strncat(out_path, filename, out_capacity - strlen(out_path) - 1);
    return true;
}

bool studio_debug_log_init_from_module(const char *filename) {
    if (!filename || filename[0] == '\0') {
        return false;
    }
    if (!studio_debug_log_ensure_path(g_studio_debug_log_path, sizeof(g_studio_debug_log_path), filename)) {
        return false;
    }
    studio_debug_log_reset();
    studio_debug_log_writef("log initialized: %s", g_studio_debug_log_path);
    return true;
}

void studio_debug_log_reset(void) {
    FILE *file;

    if (g_studio_debug_log_path[0] == '\0') {
        return;
    }
    file = fopen(g_studio_debug_log_path, "wb");
    if (!file) {
        return;
    }
    fclose(file);
}

void studio_debug_log_write(const char *message) {
    FILE *file;
    char timestamp[64];
    char line[4096];

    if (g_studio_debug_log_path[0] == '\0' || !message) {
        return;
    }

    studio_debug_log_timestamp(timestamp, sizeof(timestamp));
    snprintf(
        line,
        sizeof(line),
        "[%s][tid=%lu] %s\r\n",
        timestamp,
        (unsigned long) GetCurrentThreadId(),
        message
    );

    OutputDebugStringA(line);

    file = fopen(g_studio_debug_log_path, "ab");
    if (!file) {
        return;
    }
    fwrite(line, 1, strlen(line), file);
    fclose(file);
}

void studio_debug_log_writef(const char *fmt, ...) {
    va_list args;
    char buffer[3072];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    studio_debug_log_write(buffer);
}

void studio_debug_log_write_win32_error(const char *context, DWORD error_code) {
    char system_message[1024];
    char buffer[2048];
    DWORD size;

    size = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        0,
        system_message,
        (DWORD) sizeof(system_message),
        NULL
    );
    if (size == 0) {
        snprintf(system_message, sizeof(system_message), "No system message available.");
    }
    snprintf(buffer, sizeof(buffer), "%s failed: Win32=0x%08lX (%s)", context, (unsigned long) error_code, system_message);
    studio_debug_log_write(buffer);
}

void studio_debug_log_write_hresult(const char *context, HRESULT hr) {
    studio_debug_log_writef("%s failed: HRESULT=0x%08lX", context, (unsigned long) hr);
}

const char *studio_debug_log_path(void) {
    return g_studio_debug_log_path;
}
