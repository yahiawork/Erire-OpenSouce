#ifndef ERIRE_STUDIO_DEBUG_LOG_H
#define ERIRE_STUDIO_DEBUG_LOG_H

#include <windows.h>
#include <stdbool.h>

bool studio_debug_log_init_from_module(const char *filename);
void studio_debug_log_reset(void);
void studio_debug_log_write(const char *message);
void studio_debug_log_writef(const char *fmt, ...);
void studio_debug_log_write_win32_error(const char *context, DWORD error_code);
void studio_debug_log_write_hresult(const char *context, HRESULT hr);
const char *studio_debug_log_path(void);

#endif
