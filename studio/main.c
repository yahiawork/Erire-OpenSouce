#include <windows.h>
#include <objbase.h>

#include "app.h"
#include "debug_log.h"
#include "license_guard.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command) {
    HRESULT co_result;
    int result;
    char license_error[4096] = {0};

    (void) previous;
    (void) command_line;
    studio_debug_log_init_from_module("erire_studio_debug.log");
    studio_debug_log_write("WinMain entered");
    studio_debug_log_writef("show_command=%d", show_command);

    if (!er_license_guard_require(ER_LICENSE_APP_STUDIO, license_error, sizeof(license_error))) {
        studio_debug_log_writef("license guard rejected startup: %s", license_error);
        MessageBoxA(NULL, license_error[0] ? license_error : "Product key verification is required.", "Erire Studio activation required", MB_OK | MB_ICONERROR);
        return 1;
    }
    studio_debug_log_write("license guard accepted startup");

    co_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    studio_debug_log_writef("CoInitializeEx returned 0x%08lX", (unsigned long) co_result);

    result = studio_app_run(instance, show_command);
    studio_debug_log_writef("studio_app_run returned %d", result);

    if (SUCCEEDED(co_result)) {
        CoUninitialize();
        studio_debug_log_write("CoUninitialize completed");
    } else {
        studio_debug_log_write("CoUninitialize skipped because CoInitializeEx did not succeed");
    }
    studio_debug_log_write("WinMain exiting");
    return result;
}
