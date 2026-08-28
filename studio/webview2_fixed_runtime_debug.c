#define UNICODE
#define _UNICODE

#include <windows.h>
#include <objbase.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "webview2_minimal.h"

#define WV2_SHOW_CHECKPOINTS 1
#define WV2_PATH_CAPACITY 1024

typedef HRESULT (STDAPICALLTYPE *CreateCoreWebView2EnvironmentWithOptionsFn)(
    PCWSTR browser_executable_folder,
    PCWSTR user_data_folder,
    ICoreWebView2EnvironmentOptions *environment_options,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environment_created_handler
);

typedef struct DebugApp {
    HINSTANCE instance;
    HWND hwnd;
    HMODULE loader;
    ICoreWebView2Environment *environment;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *webview;
    wchar_t exe_dir[WV2_PATH_CAPACITY];
    wchar_t runtime_dir[WV2_PATH_CAPACITY];
    wchar_t user_data_dir[WV2_PATH_CAPACITY];
} DebugApp;

typedef struct EnvCompletedHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    ULONG ref_count;
} EnvCompletedHandler;

typedef struct ControllerCompletedHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    ULONG ref_count;
} ControllerCompletedHandler;

static DebugApp g_app;

static void checkpoint_box(const wchar_t *message) {
#if WV2_SHOW_CHECKPOINTS
    MessageBoxW(NULL, message, L"WebView2 Debug Checkpoint", MB_OK | MB_ICONINFORMATION);
#else
    (void) message;
#endif
}

static void show_error_box(const wchar_t *title, const wchar_t *message) {
    MessageBoxW(NULL, message, title, MB_OK | MB_ICONERROR);
}

static void show_hresult_box(const wchar_t *title, HRESULT hr, const wchar_t *context) {
    wchar_t buffer[2048];
    wchar_t system_message[1024];
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD size = FormatMessageW(flags, NULL, (DWORD) hr, 0, system_message, (DWORD) (sizeof(system_message) / sizeof(system_message[0])), NULL);

    if (size == 0) {
        swprintf(system_message, sizeof(system_message) / sizeof(system_message[0]), L"No system message available.");
    }

    swprintf(
        buffer,
        sizeof(buffer) / sizeof(buffer[0]),
        L"%ls\n\nHRESULT: 0x%08lX\n\n%ls",
        context,
        (unsigned long) hr,
        system_message
    );
    show_error_box(title, buffer);
}

static bool file_exists_w(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool directory_exists_w(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void join_path_w(const wchar_t *left, const wchar_t *right, wchar_t *out, size_t out_capacity) {
    size_t left_length = wcslen(left);
    bool needs_slash = left_length > 0 && left[left_length - 1] != L'\\' && left[left_length - 1] != L'/';

    swprintf(out, out_capacity, L"%ls%ls%ls", left, needs_slash ? L"\\" : L"", right);
}

static bool get_module_directory(wchar_t *out, size_t out_capacity) {
    DWORD length = GetModuleFileNameW(NULL, out, (DWORD) out_capacity);
    wchar_t *last_slash;

    if (length == 0 || length >= out_capacity) {
        return false;
    }
    last_slash = wcsrchr(out, L'\\');
    if (!last_slash) {
        last_slash = wcsrchr(out, L'/');
    }
    if (!last_slash) {
        return false;
    }
    *last_slash = L'\0';
    return true;
}

static void resize_controller_to_window(void) {
    RECT bounds;

    if (!g_app.controller || !g_app.hwnd) {
        return;
    }
    GetClientRect(g_app.hwnd, &bounds);
    g_app.controller->lpVtbl->put_Bounds(g_app.controller, bounds);
}

static void cleanup_webview(void) {
    if (g_app.webview) {
        g_app.webview->lpVtbl->Release(g_app.webview);
        g_app.webview = NULL;
    }
    if (g_app.controller) {
        g_app.controller->lpVtbl->Close(g_app.controller);
        g_app.controller->lpVtbl->Release(g_app.controller);
        g_app.controller = NULL;
    }
    if (g_app.environment) {
        g_app.environment->lpVtbl->Release(g_app.environment);
        g_app.environment = NULL;
    }
    if (g_app.loader) {
        FreeLibrary(g_app.loader);
        g_app.loader = NULL;
    }
}

static HMODULE load_webview2_loader(void) {
    wchar_t loader_path[WV2_PATH_CAPACITY];

    join_path_w(g_app.exe_dir, L"WebView2Loader.dll", loader_path, sizeof(loader_path) / sizeof(loader_path[0]));
    if (file_exists_w(loader_path)) {
        HMODULE loader = LoadLibraryW(loader_path);
        if (loader) {
            return loader;
        }
        show_hresult_box(L"WebView2 Loader", HRESULT_FROM_WIN32(GetLastError()), L"Found .\\WebView2Loader.dll but LoadLibraryW failed.");
        return NULL;
    }

    join_path_w(g_app.runtime_dir, L"WebView2Loader.dll", loader_path, sizeof(loader_path) / sizeof(loader_path[0]));
    if (file_exists_w(loader_path)) {
        HMODULE loader = LoadLibraryW(loader_path);
        if (loader) {
            return loader;
        }
        show_hresult_box(L"WebView2 Loader", HRESULT_FROM_WIN32(GetLastError()), L"Found .\\webview2\\WebView2Loader.dll but LoadLibraryW failed.");
        return NULL;
    }

    return LoadLibraryW(L"WebView2Loader.dll");
}

static HRESULT STDMETHODCALLTYPE env_query_interface(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    REFIID riid,
    void **object
) {
    (void) riid;
    if (!object) {
        return E_POINTER;
    }
    *object = self;
    self->lpVtbl->AddRef(self);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE env_add_ref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self) {
    EnvCompletedHandler *handler = (EnvCompletedHandler *) self;
    return (ULONG) InterlockedIncrement((LONG *) &handler->ref_count);
}

static ULONG STDMETHODCALLTYPE env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self) {
    EnvCompletedHandler *handler = (EnvCompletedHandler *) self;
    ULONG value = (ULONG) InterlockedDecrement((LONG *) &handler->ref_count);
    if (value == 0) {
        free(handler);
    }
    return value;
}

static HRESULT STDMETHODCALLTYPE controller_query_interface(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    REFIID riid,
    void **object
) {
    (void) riid;
    if (!object) {
        return E_POINTER;
    }
    *object = self;
    self->lpVtbl->AddRef(self);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE controller_add_ref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self) {
    ControllerCompletedHandler *handler = (ControllerCompletedHandler *) self;
    return (ULONG) InterlockedIncrement((LONG *) &handler->ref_count);
}

static ULONG STDMETHODCALLTYPE controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self) {
    ControllerCompletedHandler *handler = (ControllerCompletedHandler *) self;
    ULONG value = (ULONG) InterlockedDecrement((LONG *) &handler->ref_count);
    if (value == 0) {
        free(handler);
    }
    return value;
}

static HRESULT STDMETHODCALLTYPE controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    HRESULT error_code,
    ICoreWebView2Controller *controller
) {
    ICoreWebView2Settings *settings = NULL;
    const wchar_t *html =
        L"<html><body style='background:#11161d;color:#e6edf3;font-family:Segoe UI;padding:24px'>"
        L"<h2>WebView2 Fixed Runtime OK</h2>"
        L"<p>The Win32 window, message loop, and async WebView2 callbacks are working.</p>"
        L"</body></html>";
    HRESULT hr;

    (void) self;
    checkpoint_box(L"Checkpoint: controller callback invoked");

    if (FAILED(error_code) || !controller) {
        show_hresult_box(L"WebView2 Controller", error_code, L"CreateCoreWebView2Controller callback failed.");
        DestroyWindow(g_app.hwnd);
        return error_code;
    }

    g_app.controller = controller;
    hr = controller->lpVtbl->get_CoreWebView2(controller, &g_app.webview);
    if (FAILED(hr) || !g_app.webview) {
        show_hresult_box(L"WebView2 Controller", hr, L"get_CoreWebView2 failed.");
        DestroyWindow(g_app.hwnd);
        return hr;
    }

    hr = controller->lpVtbl->put_IsVisible(controller, TRUE);
    if (FAILED(hr)) {
        show_hresult_box(L"WebView2 Controller", hr, L"put_IsVisible failed.");
        DestroyWindow(g_app.hwnd);
        return hr;
    }

    resize_controller_to_window();

    hr = g_app.webview->lpVtbl->get_Settings(g_app.webview, &settings);
    if (FAILED(hr) || !settings) {
        show_hresult_box(L"WebView2 Settings", hr, L"get_Settings failed.");
        DestroyWindow(g_app.hwnd);
        return hr;
    }

    settings->lpVtbl->put_IsScriptEnabled(settings, TRUE);
    settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings, TRUE);
    settings->lpVtbl->put_IsWebMessageEnabled(settings, TRUE);
    settings->lpVtbl->Release(settings);

    hr = g_app.webview->lpVtbl->NavigateToString(g_app.webview, html);
    if (FAILED(hr)) {
        show_hresult_box(L"WebView2 Navigate", hr, L"NavigateToString failed.");
        DestroyWindow(g_app.hwnd);
        return hr;
    }

    checkpoint_box(L"Checkpoint: WebView2 initialized and NavigateToString succeeded");
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    HRESULT error_code,
    ICoreWebView2Environment *environment
) {
    ControllerCompletedHandler *controller_handler;
    HRESULT hr;

    (void) self;
    checkpoint_box(L"Checkpoint: environment callback invoked");

    if (FAILED(error_code) || !environment) {
        show_hresult_box(L"WebView2 Environment", error_code, L"CreateCoreWebView2EnvironmentWithOptions callback failed.");
        DestroyWindow(g_app.hwnd);
        return error_code;
    }

    g_app.environment = environment;

    controller_handler = (ControllerCompletedHandler *) calloc(1, sizeof(*controller_handler));
    if (!controller_handler) {
        show_error_box(L"Out of Memory", L"Could not allocate controller callback handler.");
        DestroyWindow(g_app.hwnd);
        return E_OUTOFMEMORY;
    }

    static const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl controller_vtbl = {
        controller_query_interface,
        controller_add_ref,
        controller_release,
        controller_invoke
    };

    controller_handler->iface.lpVtbl = &controller_vtbl;
    controller_handler->ref_count = 1;

    hr = environment->lpVtbl->CreateCoreWebView2Controller(environment, g_app.hwnd, &controller_handler->iface);
    controller_handler->iface.lpVtbl->Release(&controller_handler->iface);

    if (FAILED(hr)) {
        show_hresult_box(L"WebView2 Environment", hr, L"CreateCoreWebView2Controller call failed immediately.");
        DestroyWindow(g_app.hwnd);
        return hr;
    }

    checkpoint_box(L"Checkpoint: CreateCoreWebView2Controller returned S_OK");
    return S_OK;
}

static bool start_webview2_fixed_runtime(void) {
    wchar_t runtime_exe[WV2_PATH_CAPACITY];
    wchar_t loader_context[WV2_PATH_CAPACITY];
    FARPROC proc_address;
    CreateCoreWebView2EnvironmentWithOptionsFn create_environment;
    EnvCompletedHandler *env_handler;
    HRESULT hr;

    checkpoint_box(L"Checkpoint: start_webview2_fixed_runtime entered");

    if (!get_module_directory(g_app.exe_dir, sizeof(g_app.exe_dir) / sizeof(g_app.exe_dir[0]))) {
        show_error_box(L"Startup Error", L"Could not resolve the executable directory.");
        return false;
    }

    join_path_w(g_app.exe_dir, L"webview2", g_app.runtime_dir, sizeof(g_app.runtime_dir) / sizeof(g_app.runtime_dir[0]));
    join_path_w(g_app.exe_dir, L"webview2_userdata", g_app.user_data_dir, sizeof(g_app.user_data_dir) / sizeof(g_app.user_data_dir[0]));
    CreateDirectoryW(g_app.user_data_dir, NULL);

    if (!directory_exists_w(g_app.runtime_dir)) {
        show_error_box(L"WebView2 Runtime", L"Fixed Version runtime folder not found.\nExpected: .\\webview2");
        return false;
    }

    join_path_w(g_app.runtime_dir, L"msedgewebview2.exe", runtime_exe, sizeof(runtime_exe) / sizeof(runtime_exe[0]));
    if (!file_exists_w(runtime_exe)) {
        show_error_box(L"WebView2 Runtime", L"msedgewebview2.exe was not found inside .\\webview2");
        return false;
    }

    checkpoint_box(L"Checkpoint: Fixed Version runtime folder looks valid");

    g_app.loader = load_webview2_loader();
    if (!g_app.loader) {
        swprintf(
            loader_context,
            sizeof(loader_context) / sizeof(loader_context[0]),
            L"Could not load WebView2Loader.dll.\n\nTried:\n- .\\WebView2Loader.dll\n- .\\webview2\\WebView2Loader.dll\n- system search path"
        );
        show_error_box(L"WebView2 Loader", loader_context);
        return false;
    }

    checkpoint_box(L"Checkpoint: WebView2Loader.dll loaded");

    proc_address = GetProcAddress(g_app.loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!proc_address) {
        show_error_box(L"WebView2 Loader", L"CreateCoreWebView2EnvironmentWithOptions was not found in WebView2Loader.dll");
        return false;
    }
    memcpy(&create_environment, &proc_address, sizeof(create_environment));

    env_handler = (EnvCompletedHandler *) calloc(1, sizeof(*env_handler));
    if (!env_handler) {
        show_error_box(L"Out of Memory", L"Could not allocate environment callback handler.");
        return false;
    }

    static const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl env_vtbl = {
        env_query_interface,
        env_add_ref,
        env_release,
        env_invoke
    };

    env_handler->iface.lpVtbl = &env_vtbl;
    env_handler->ref_count = 1;

    hr = create_environment(g_app.runtime_dir, g_app.user_data_dir, NULL, &env_handler->iface);
    env_handler->iface.lpVtbl->Release(&env_handler->iface);

    if (FAILED(hr)) {
        show_hresult_box(
            L"WebView2 Environment",
            hr,
            L"CreateCoreWebView2EnvironmentWithOptions failed immediately.\n\nThis usually means the fixed runtime path is wrong, the loader/runtime architecture does not match x64, or the runtime cannot be started from that folder."
        );
        return false;
    }

    checkpoint_box(L"Checkpoint: CreateCoreWebView2EnvironmentWithOptions returned S_OK");
    return true;
}

static LRESULT CALLBACK debug_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            checkpoint_box(L"Checkpoint: WM_CREATE");
            if (!start_webview2_fixed_runtime()) {
                return -1;
            }
            return 0;
        case WM_SIZE:
            resize_controller_to_window();
            return 0;
        case WM_DESTROY:
            cleanup_webview();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command) {
    WNDCLASSW window_class;
    HWND hwnd;
    MSG message;
    HRESULT hr;
    BOOL message_result;

    (void) previous;
    (void) command_line;

    checkpoint_box(L"Checkpoint: WinMain entered");

    memset(&g_app, 0, sizeof(g_app));
    g_app.instance = instance;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        show_hresult_box(L"COM Initialization", hr, L"CoInitializeEx failed.");
        return 1;
    }

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = debug_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.lpszClassName = L"ErireStudioWebView2DebugWindow";
    window_class.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);

    if (!RegisterClassW(&window_class)) {
        show_hresult_box(L"RegisterClassW", HRESULT_FROM_WIN32(GetLastError()), L"RegisterClassW failed.");
        CoUninitialize();
        return 1;
    }

    checkpoint_box(L"Checkpoint: RegisterClassW succeeded");

    hwnd = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"Erire Studio WebView2 Fixed Runtime Debug",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        800,
        NULL,
        NULL,
        instance,
        NULL
    );
    if (!hwnd) {
        show_hresult_box(L"CreateWindowExW", HRESULT_FROM_WIN32(GetLastError()), L"CreateWindowExW failed.");
        CoUninitialize();
        return 1;
    }

    g_app.hwnd = hwnd;
    ShowWindow(hwnd, show_command == 0 ? SW_SHOWNORMAL : show_command);
    UpdateWindow(hwnd);

    checkpoint_box(L"Checkpoint: main window created and shown");

    for (;;) {
        message_result = GetMessageW(&message, NULL, 0, 0);
        if (message_result == -1) {
            show_hresult_box(L"GetMessageW", HRESULT_FROM_WIN32(GetLastError()), L"GetMessageW failed.");
            cleanup_webview();
            CoUninitialize();
            return 1;
        }
        if (message_result == 0) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CoUninitialize();
    return (int) message.wParam;
}
