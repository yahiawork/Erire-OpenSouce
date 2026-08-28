#include "windows.h"

#include "launcher.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <commdlg.h>
#include <windows.h>
#endif

#include "error.h"
#include "fileio.h"
#include "packager.h"
#include "runtime.h"

#ifdef _WIN32

#define ER_LAUNCHER_ID_EDIT_PATH 2001
#define ER_LAUNCHER_ID_BUTTON_BROWSE 2002
#define ER_LAUNCHER_ID_BUTTON_RUN 2003
#define ER_LAUNCHER_ID_STATUS 2004

typedef struct LauncherState {
    HWND hwnd;
    HWND edit_path;
    HWND status;
    char selected_path[MAX_PATH];
} LauncherState;

static char g_launch_after_close[MAX_PATH];

static void er_launcher_set_status(LauncherState *state, const char *text) {
    SetWindowTextA(state->status, text ? text : "");
}

static void er_launcher_pick_file(LauncherState *state) {
    OPENFILENAMEA dialog;
    char file_buffer[MAX_PATH];

    memset(&dialog, 0, sizeof(dialog));
    memset(file_buffer, 0, sizeof(file_buffer));

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = state->hwnd;
    dialog.lpstrFilter = "Erire Files (*.er)\0*.er\0All Files\0*.*\0";
    dialog.lpstrFile = file_buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    dialog.lpstrTitle = "Open Erire App";

    if (GetOpenFileNameA(&dialog)) {
        strncpy(state->selected_path, file_buffer, sizeof(state->selected_path) - 1);
        SetWindowTextA(state->edit_path, state->selected_path);
        er_launcher_set_status(state, "Ready to run.");
    }
}

static void er_launcher_request_run(LauncherState *state) {
    GetWindowTextA(state->edit_path, state->selected_path, (int) sizeof(state->selected_path));
    if (state->selected_path[0] == '\0') {
        er_launcher_set_status(state, "Choose an .er file first.");
        return;
    }
    if (!er_path_has_extension(state->selected_path, ".er")) {
        er_launcher_set_status(state, "Selected file must use the .er extension.");
        return;
    }
    strncpy(g_launch_after_close, state->selected_path, sizeof(g_launch_after_close) - 1);
    DestroyWindow(state->hwnd);
}

static LRESULT CALLBACK er_launcher_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    LauncherState *state = (LauncherState *) GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTA *create = (CREATESTRUCTA *) lparam;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) create->lpCreateParams);
            return TRUE;
        }
        case WM_CREATE:
            return 0;
        case WM_COMMAND:
            if (!state) {
                break;
            }
            switch (LOWORD(wparam)) {
                case ER_LAUNCHER_ID_BUTTON_BROWSE:
                    er_launcher_pick_file(state);
                    return 0;
                case ER_LAUNCHER_ID_BUTTON_RUN:
                    er_launcher_request_run(state);
                    return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static bool er_launcher_create_window(LauncherState *state) {
    WNDCLASSEXA wc;
    HFONT font;
    HINSTANCE instance = GetModuleHandleA(NULL);

    memset(state, 0, sizeof(*state));
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = er_launcher_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "ErireRunnerWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(1));
    wc.hIconSm = LoadIconA(instance, MAKEINTRESOURCEA(1));

    RegisterClassExA(&wc);

    state->hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "ErireRunner",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        560,
        260,
        NULL,
        NULL,
        instance,
        state
    );

    if (!state->hwnd) {
        return false;
    }

    SetWindowTextA(state->hwnd, "ErireRunner");

    font = CreateFontA(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    CreateWindowExA(0, "STATIC", "Open an Erire app and run it without a terminal.",
        WS_CHILD | WS_VISIBLE, 20, 18, 500, 24, state->hwnd, NULL, instance, NULL);

    state->edit_path = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 60, 380, 28,
        state->hwnd, (HMENU) (INT_PTR) ER_LAUNCHER_ID_EDIT_PATH, instance, NULL);

    CreateWindowExA(0, "BUTTON", "Browse",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 410, 60, 110, 28,
        state->hwnd, (HMENU) (INT_PTR) ER_LAUNCHER_ID_BUTTON_BROWSE, instance, NULL);

    CreateWindowExA(0, "BUTTON", "Run",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 410, 100, 110, 34,
        state->hwnd, (HMENU) (INT_PTR) ER_LAUNCHER_ID_BUTTON_RUN, instance, NULL);

    state->status = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "Choose an .er file.",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        20, 100, 380, 90, state->hwnd, (HMENU) (INT_PTR) ER_LAUNCHER_ID_STATUS, instance, NULL);

    SendMessageA(state->edit_path, WM_SETFONT, (WPARAM) font, TRUE);
    SendMessageA(state->status, WM_SETFONT, (WPARAM) font, TRUE);

    ShowWindow(state->hwnd, SW_SHOWDEFAULT);
    UpdateWindow(state->hwnd);
    return true;
}

static int er_launcher_open_ui(void) {
    LauncherState state;
    MSG msg;
    ErError error;

    memset(g_launch_after_close, 0, sizeof(g_launch_after_close));

    if (!er_launcher_create_window(&state)) {
        MessageBoxA(NULL, "Could not create ErireRunner window.", "ErireRunner", MB_ICONERROR | MB_OK);
        return 1;
    }

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (g_launch_after_close[0] == '\0') {
        return 0;
    }

    er_error_clear(&error);
    if (er_runtime_run_file(g_launch_after_close, &error) != 0) {
        MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
        return 1;
    }

    return 0;
}

int erire_launcher_main(int argc, char **argv) {
    char exe_path[1024];
    char *embedded_source = NULL;
    size_t embedded_size = 0;
    ErPackagedApp packaged_app;
    ErError error;

    er_error_clear(&error);
    memset(&packaged_app, 0, sizeof(packaged_app));

    if (!er_get_current_module_path(exe_path, sizeof(exe_path), &error)) {
        MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
        return 1;
    }

    er_error_clear(&error);
    if (er_packager_extract_embedded_app(exe_path, &packaged_app, &error)) {
        if (er_runtime_run_file(packaged_app.entry_path, &error) != 0) {
            MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
            er_packaged_app_free(&packaged_app);
            return 1;
        }
        er_packaged_app_free(&packaged_app);
        return 0;
    } else if (er_error_has(&error)) {
        MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
        return 1;
    }

    er_error_clear(&error);
    if (er_packager_extract_embedded_source(exe_path, &embedded_source, &embedded_size, &error)) {
        (void) embedded_size;
        if (er_runtime_run_source(exe_path, embedded_source, &error) != 0) {
            MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
            free(embedded_source);
            return 1;
        }
        free(embedded_source);
        return 0;
    } else if (er_error_has(&error)) {
        MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (argc > 1) {
        er_error_clear(&error);
        if (er_runtime_run_file(argv[1], &error) != 0) {
            MessageBoxA(NULL, error.message, "ErireRunner", MB_ICONERROR | MB_OK);
            return 1;
        }
        return 0;
    }

    return er_launcher_open_ui();
}

int main(int argc, char **argv) {
    return erire_launcher_main(argc, argv);
}

#else
int erire_launcher_main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    return 1;
}

int main(int argc, char **argv) {
    return erire_launcher_main(argc, argv);
}
#endif
