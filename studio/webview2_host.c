#include "webview2_host.h"

#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_log.h"
#include "fileio.h"

typedef HRESULT (STDAPICALLTYPE *StudioCreateEnvironmentFn)(
    PCWSTR browser_executable_folder,
    PCWSTR user_data_folder,
    ICoreWebView2EnvironmentOptions *environment_options,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environment_created_handler
);

static const IID STUDIO_IID_ICOREWEBVIEW2_CREATE_ENVIRONMENT_COMPLETED_HANDLER =
    {0x4e8a3389, 0xc9d8, 0x4bd2, {0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d}};
static const IID STUDIO_IID_ICOREWEBVIEW2_CREATE_CONTROLLER_COMPLETED_HANDLER =
    {0x6c4819f3, 0xc9b7, 0x4260, {0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c}};
static const IID STUDIO_IID_ICOREWEBVIEW2_WEB_MESSAGE_RECEIVED_HANDLER =
    {0x57213f19, 0x00e6, 0x49fa, {0x8e, 0x07, 0x89, 0x8e, 0xa0, 0x1e, 0xcb, 0xd2}};

typedef struct StudioEnvHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    ULONG ref_count;
    StudioWebViewHost *host;
} StudioEnvHandler;

typedef struct StudioControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    ULONG ref_count;
    StudioWebViewHost *host;
} StudioControllerHandler;

typedef struct StudioMessageHandler {
    ICoreWebView2WebMessageReceivedEventHandler iface;
    ULONG ref_count;
    StudioWebViewHost *host;
} StudioMessageHandler;

static HMODULE studio_webview_load_loader_from_paths(const char *browser_dir) {
    wchar_t wide_path[STUDIO_MAX_PATH];
    char loader_path[STUDIO_MAX_PATH];
    char app_dir[STUDIO_MAX_PATH];

    if (browser_dir && browser_dir[0] != '\0') {
        er_path_dirname(browser_dir, app_dir, sizeof(app_dir));
        studio_join_path(app_dir, "WebView2Loader.dll", loader_path, sizeof(loader_path));
        studio_debug_log_writef("trying WebView2 loader at %s", loader_path);
        if (studio_utf8_to_wide(loader_path, wide_path, sizeof(wide_path) / sizeof(wide_path[0]))) {
            HMODULE app_loader = LoadLibraryW(wide_path);
            if (app_loader) {
                studio_debug_log_writef("loaded WebView2 loader from %s", loader_path);
                return app_loader;
            }
        }

        studio_join_path(browser_dir, "WebView2Loader.dll", loader_path, sizeof(loader_path));
        studio_debug_log_writef("trying WebView2 loader at %s", loader_path);
        if (studio_utf8_to_wide(loader_path, wide_path, sizeof(wide_path) / sizeof(wide_path[0]))) {
            HMODULE local_loader = LoadLibraryW(wide_path);
            if (local_loader) {
                studio_debug_log_writef("loaded WebView2 loader from %s", loader_path);
                return local_loader;
            }
        }
    }
    studio_debug_log_write("trying system WebView2Loader.dll");
    return LoadLibraryW(L"WebView2Loader.dll");
}

static HRESULT STDMETHODCALLTYPE studio_env_query_interface(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    REFIID riid,
    void **object
) {
    if (!object) {
        return E_POINTER;
    }
    *object = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &STUDIO_IID_ICOREWEBVIEW2_CREATE_ENVIRONMENT_COMPLETED_HANDLER)) {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE studio_env_add_ref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self) {
    StudioEnvHandler *handler = (StudioEnvHandler *) self;
    return (ULONG) InterlockedIncrement((LONG *) &handler->ref_count);
}

static ULONG STDMETHODCALLTYPE studio_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self) {
    StudioEnvHandler *handler = (StudioEnvHandler *) self;
    ULONG value = (ULONG) InterlockedDecrement((LONG *) &handler->ref_count);
    if (value == 0) {
        free(handler);
    }
    return value;
}

static HRESULT STDMETHODCALLTYPE studio_controller_query_interface(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    REFIID riid,
    void **object
) {
    if (!object) {
        return E_POINTER;
    }
    *object = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &STUDIO_IID_ICOREWEBVIEW2_CREATE_CONTROLLER_COMPLETED_HANDLER)) {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE studio_controller_add_ref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self) {
    StudioControllerHandler *handler = (StudioControllerHandler *) self;
    return (ULONG) InterlockedIncrement((LONG *) &handler->ref_count);
}

static ULONG STDMETHODCALLTYPE studio_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self) {
    StudioControllerHandler *handler = (StudioControllerHandler *) self;
    ULONG value = (ULONG) InterlockedDecrement((LONG *) &handler->ref_count);
    if (value == 0) {
        free(handler);
    }
    return value;
}

static HRESULT STDMETHODCALLTYPE studio_message_query_interface(
    ICoreWebView2WebMessageReceivedEventHandler *self,
    REFIID riid,
    void **object
) {
    if (!object) {
        return E_POINTER;
    }
    *object = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &STUDIO_IID_ICOREWEBVIEW2_WEB_MESSAGE_RECEIVED_HANDLER)) {
        *object = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE studio_message_add_ref(ICoreWebView2WebMessageReceivedEventHandler *self) {
    StudioMessageHandler *handler = (StudioMessageHandler *) self;
    return (ULONG) InterlockedIncrement((LONG *) &handler->ref_count);
}

static ULONG STDMETHODCALLTYPE studio_message_release(ICoreWebView2WebMessageReceivedEventHandler *self) {
    StudioMessageHandler *handler = (StudioMessageHandler *) self;
    ULONG value = (ULONG) InterlockedDecrement((LONG *) &handler->ref_count);
    if (value == 0) {
        free(handler);
    }
    return value;
}

static HRESULT STDMETHODCALLTYPE studio_message_invoke(
    ICoreWebView2WebMessageReceivedEventHandler *self,
    ICoreWebView2 *sender,
    ICoreWebView2WebMessageReceivedEventArgs *args
) {
    StudioMessageHandler *handler = (StudioMessageHandler *) self;
    LPWSTR wide_message = NULL;
    char utf8[65536];

    (void) sender;
    if (!handler || !handler->host || !handler->host->on_message) {
        studio_debug_log_write("web message ignored because host/callback is missing");
        return S_OK;
    }
    if (FAILED(args->lpVtbl->TryGetWebMessageAsString(args, &wide_message)) || !wide_message) {
        studio_debug_log_write("TryGetWebMessageAsString failed");
        return S_OK;
    }
    if (studio_wide_to_utf8(wide_message, utf8, sizeof(utf8))) {
        handler->host->on_message(handler->host->user_data, utf8);
    }
    CoTaskMemFree(wide_message);
    return S_OK;
}

static const ICoreWebView2WebMessageReceivedEventHandlerVtbl studio_message_vtbl = {
    studio_message_query_interface,
    studio_message_add_ref,
    studio_message_release,
    studio_message_invoke
};

static void studio_webview_show_startup_error_page(StudioWebViewHost *host, const char *path, HRESULT hr) {
    char html[8192];
    wchar_t wide_html[8192];
    HRESULT navigate_result;
    const char *log_path;

    if (!host || !host->webview) {
        return;
    }
    log_path = studio_debug_log_path();
    snprintf(
        html,
        sizeof(html),
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<title>Erire Studio</title>"
        "<style>"
        "body{margin:0;background:#0f172a;color:#e2e8f0;font:14px Segoe UI, sans-serif;}"
        ".wrap{padding:24px 28px;}"
        "h1{margin:0 0 12px;font-size:20px;}"
        "p{margin:0 0 10px;color:#94a3b8;}"
        "pre{margin:16px 0 0;padding:14px;background:#111827;border:1px solid #334155;"
        "border-radius:8px;white-space:pre-wrap;word-break:break-word;}"
        "</style></head><body><div class=\"wrap\">"
        "<h1>Erire Studio startup error</h1>"
        "<p>WebView2 started, but the frontend file could not be opened.</p>"
        "<pre>Navigate HRESULT: 0x%08lX\nFrontend: %s\nLog: %s</pre>"
        "</div></body></html>",
        (unsigned long) hr,
        path ? path : "(null)",
        log_path ? log_path : "(none)"
    );
    if (!studio_utf8_to_wide(html, wide_html, sizeof(wide_html) / sizeof(wide_html[0]))) {
        studio_debug_log_write("failed to convert startup error page to UTF-16");
        return;
    }
    navigate_result = host->webview->lpVtbl->NavigateToString(host->webview, wide_html);
    if (FAILED(navigate_result)) {
        studio_debug_log_write_hresult("NavigateToString(startup error page)", navigate_result);
    } else {
        studio_debug_log_write("startup error page displayed");
    }
}

static void studio_webview_apply_settings(StudioWebViewHost *host) {
    ICoreWebView2Settings *settings = NULL;
    EventRegistrationToken token;
    StudioMessageHandler *message_handler;

    if (!host->webview) {
        studio_debug_log_write("studio_webview_apply_settings skipped because webview is null");
        return;
    }
    if (SUCCEEDED(host->webview->lpVtbl->get_Settings(host->webview, &settings)) && settings) {
        settings->lpVtbl->put_IsScriptEnabled(settings, TRUE);
        settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings, TRUE);
        settings->lpVtbl->put_IsWebMessageEnabled(settings, TRUE);
        settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings, FALSE);
        settings->lpVtbl->put_AreDevToolsEnabled(settings, FALSE);
        settings->lpVtbl->put_IsStatusBarEnabled(settings, FALSE);
        settings->lpVtbl->Release(settings);
        studio_debug_log_write("WebView2 settings applied");
    }

    message_handler = (StudioMessageHandler *) calloc(1, sizeof(*message_handler));
    if (!message_handler) {
        return;
    }
    message_handler->iface.lpVtbl = &studio_message_vtbl;
    message_handler->ref_count = 1;
    message_handler->host = host;
    token.value = 0;
    host->webview->lpVtbl->add_WebMessageReceived(host->webview, &message_handler->iface, &token);
    studio_debug_log_write("WebView2 message handler registered");
    message_handler->iface.lpVtbl->Release(&message_handler->iface);
}

void studio_webview_host_resize(StudioWebViewHost *host) {
    RECT bounds;
    HRESULT hr;

    if (!host || !host->controller || !host->parent) {
        return;
    }
    GetClientRect(host->parent, &bounds);
    hr = host->controller->lpVtbl->put_Bounds(host->controller, bounds);
    if (FAILED(hr)) {
        studio_debug_log_write_hresult("put_Bounds", hr);
    }
}

void studio_webview_host_notify_parent_window_position_changed(StudioWebViewHost *host) {
    HRESULT hr;

    if (!host || !host->controller) {
        return;
    }
    hr = host->controller->lpVtbl->NotifyParentWindowPositionChanged(host->controller);
    if (FAILED(hr)) {
        studio_debug_log_write_hresult("NotifyParentWindowPositionChanged", hr);
    }
}

static HRESULT STDMETHODCALLTYPE studio_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    HRESULT result,
    ICoreWebView2Controller *controller
) {
    StudioControllerHandler *handler = (StudioControllerHandler *) self;
    StudioWebViewHost *host = handler ? handler->host : NULL;

    if (FAILED(result) || !host || !controller) {
        studio_debug_log_write_hresult("CreateCoreWebView2Controller callback", result);
        MessageBoxA(
            host ? host->parent : NULL,
            "WebView2 controller creation failed. Check erire_studio_debug.log for details.",
            "Erire Studio",
            MB_OK | MB_ICONERROR
        );
        return result;
    }
    studio_debug_log_write("CreateCoreWebView2Controller callback succeeded");
    controller->lpVtbl->AddRef(controller);
    host->controller = controller;
    if (FAILED(controller->lpVtbl->get_CoreWebView2(controller, &host->webview)) || !host->webview) {
        studio_debug_log_write("get_CoreWebView2 failed");
        MessageBoxA(host->parent, "WebView2 core interface acquisition failed.", "Erire Studio", MB_OK | MB_ICONERROR);
        return E_FAIL;
    }
    controller->lpVtbl->put_IsVisible(controller, TRUE);
    studio_webview_host_resize(host);
    studio_webview_apply_settings(host);
    host->ready = true;
    studio_debug_log_write("WebView2 host marked ready");
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl studio_controller_vtbl = {
    studio_controller_query_interface,
    studio_controller_add_ref,
    studio_controller_release,
    studio_controller_invoke
};

static HRESULT STDMETHODCALLTYPE studio_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    HRESULT result,
    ICoreWebView2Environment *environment
) {
    StudioEnvHandler *handler = (StudioEnvHandler *) self;
    StudioWebViewHost *host = handler ? handler->host : NULL;
    StudioControllerHandler *controller_handler;

    if (FAILED(result) || !host || !environment) {
        studio_debug_log_write_hresult("CreateCoreWebView2Environment callback", result);
        MessageBoxA(
            host ? host->parent : NULL,
            "WebView2 environment creation failed. Check erire_studio_debug.log for details.",
            "Erire Studio",
            MB_OK | MB_ICONERROR
        );
        return result;
    }
    studio_debug_log_write("CreateCoreWebView2Environment callback succeeded");
    environment->lpVtbl->AddRef(environment);
    host->environment = environment;
    studio_debug_log_writef("environment stored; parent hwnd=0x%p", host->parent);

    controller_handler = (StudioControllerHandler *) calloc(1, sizeof(*controller_handler));
    if (!controller_handler) {
        studio_debug_log_write("controller handler allocation failed");
        return E_OUTOFMEMORY;
    }
    studio_debug_log_write("controller handler allocated");
    controller_handler->iface.lpVtbl = &studio_controller_vtbl;
    controller_handler->ref_count = 1;
    controller_handler->host = host;

    studio_debug_log_write("calling CreateCoreWebView2Controller");
    result = environment->lpVtbl->CreateCoreWebView2Controller(environment, host->parent, &controller_handler->iface);
    if (FAILED(result)) {
        studio_debug_log_write_hresult("CreateCoreWebView2Controller", result);
        MessageBoxA(host->parent, "Could not start WebView2 controller creation.", "Erire Studio", MB_OK | MB_ICONERROR);
    } else {
        studio_debug_log_write("CreateCoreWebView2Controller dispatched");
    }
    controller_handler->iface.lpVtbl->Release(&controller_handler->iface);
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl studio_env_vtbl = {
    studio_env_query_interface,
    studio_env_add_ref,
    studio_env_release,
    studio_env_invoke
};

bool studio_webview_host_init(
    StudioWebViewHost *host,
    HWND parent,
    const char *browser_dir,
    const char *user_data_dir,
    StudioWebViewMessageHandler on_message,
    void *user_data
) {
    StudioCreateEnvironmentFn create_environment;
    FARPROC create_environment_proc;
    StudioEnvHandler *handler;
    HRESULT result;
    wchar_t wide_browser_dir[STUDIO_MAX_PATH];
    wchar_t wide_user_data[STUDIO_MAX_PATH];
    const wchar_t *browser_dir_arg = NULL;

    studio_debug_log_write("studio_webview_host_init entered");
    memset(host, 0, sizeof(*host));
    host->parent = parent;
    host->on_message = on_message;
    host->user_data = user_data;
    if (browser_dir) {
        strncpy(host->browser_dir, browser_dir, sizeof(host->browser_dir) - 1);
        host->browser_dir[sizeof(host->browser_dir) - 1] = '\0';
    }
    strncpy(host->user_data_dir, user_data_dir, sizeof(host->user_data_dir) - 1);
    host->user_data_dir[sizeof(host->user_data_dir) - 1] = '\0';

    host->loader = studio_webview_load_loader_from_paths(host->browser_dir);
    if (!host->loader) {
        studio_debug_log_write_win32_error("LoadLibraryW(WebView2Loader.dll)", GetLastError());
        return false;
    }
    studio_debug_log_write("WebView2 loader loaded");
    create_environment_proc = GetProcAddress(host->loader, "CreateCoreWebView2EnvironmentWithOptions");
    memcpy(&create_environment, &create_environment_proc, sizeof(create_environment));
    if (!create_environment) {
        studio_debug_log_write_win32_error("GetProcAddress(CreateCoreWebView2EnvironmentWithOptions)", GetLastError());
        FreeLibrary(host->loader);
        host->loader = NULL;
        return false;
    }

    handler = (StudioEnvHandler *) calloc(1, sizeof(*handler));
    if (!handler) {
        FreeLibrary(host->loader);
        host->loader = NULL;
        return false;
    }
    handler->iface.lpVtbl = &studio_env_vtbl;
    handler->ref_count = 1;
    handler->host = host;

    if (host->browser_dir[0] != '\0' &&
        GetFileAttributesA(host->browser_dir) != INVALID_FILE_ATTRIBUTES &&
        (GetFileAttributesA(host->browser_dir) & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        studio_utf8_to_wide(host->browser_dir, wide_browser_dir, sizeof(wide_browser_dir) / sizeof(wide_browser_dir[0]))) {
        browser_dir_arg = wide_browser_dir;
    }
    if (browser_dir_arg) {
        studio_debug_log_writef("using fixed runtime browser_dir=%s", host->browser_dir);
    } else {
        studio_debug_log_write("browser_dir unavailable; falling back to installed runtime");
    }
    studio_utf8_to_wide(user_data_dir, wide_user_data, sizeof(wide_user_data) / sizeof(wide_user_data[0]));
    result = create_environment(browser_dir_arg, wide_user_data, NULL, &handler->iface);
    studio_debug_log_writef("CreateCoreWebView2EnvironmentWithOptions returned 0x%08lX", (unsigned long) result);
    handler->iface.lpVtbl->Release(&handler->iface);
    if (FAILED(result)) {
        studio_debug_log_write_hresult("CreateCoreWebView2EnvironmentWithOptions", result);
        FreeLibrary(host->loader);
        host->loader = NULL;
        return false;
    }
    return true;
}

bool studio_webview_host_navigate_file(StudioWebViewHost *host, const char *path) {
    char url[STUDIO_MAX_PATH * 2];
    wchar_t wide_url[STUDIO_MAX_PATH * 2];
    DWORD attributes;
    HRESULT hr;

    if (!host || !host->ready || !host->webview) {
        studio_debug_log_write("studio_webview_host_navigate_file rejected because host is not ready");
        return false;
    }
    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        studio_debug_log_writef("frontend file is missing or invalid: %s", path ? path : "(null)");
        studio_webview_show_startup_error_page(host, path, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        return false;
    }
    if (!studio_path_file_url(path, url, sizeof(url))) {
        studio_debug_log_writef("studio_path_file_url failed for %s", path ? path : "(null)");
        return false;
    }
    if (!studio_utf8_to_wide(url, wide_url, sizeof(wide_url) / sizeof(wide_url[0]))) {
        studio_debug_log_writef("studio_utf8_to_wide failed for navigate url=%s", url);
        return false;
    }
    hr = host->webview->lpVtbl->Navigate(host->webview, wide_url);
    if (FAILED(hr)) {
        studio_debug_log_writef("Navigate failed for %s", url);
        studio_debug_log_write_hresult("Navigate(file URL)", hr);
        studio_webview_show_startup_error_page(host, path, hr);
        return false;
    }
    studio_debug_log_writef("Navigate succeeded for %s", url);
    return true;
}

bool studio_webview_host_post_json(StudioWebViewHost *host, const char *json) {
    wchar_t wide_message[65536];

    if (!host || !host->ready || !host->webview || !json) {
        return false;
    }
    if (!studio_utf8_to_wide(json, wide_message, sizeof(wide_message) / sizeof(wide_message[0]))) {
        studio_debug_log_write("studio_utf8_to_wide failed while posting JSON");
        return false;
    }
    if (!SUCCEEDED(host->webview->lpVtbl->PostWebMessageAsString(host->webview, wide_message))) {
        studio_debug_log_write("PostWebMessageAsString failed");
        return false;
    }
    return true;
}

void studio_webview_host_dispose(StudioWebViewHost *host) {
    if (!host) {
        return;
    }
    studio_debug_log_write("studio_webview_host_dispose");
    if (host->webview) {
        host->webview->lpVtbl->Release(host->webview);
        host->webview = NULL;
    }
    if (host->controller) {
        host->controller->lpVtbl->Close(host->controller);
        host->controller->lpVtbl->Release(host->controller);
        host->controller = NULL;
    }
    if (host->environment) {
        host->environment->lpVtbl->Release(host->environment);
        host->environment = NULL;
    }
    if (host->loader) {
        FreeLibrary(host->loader);
        host->loader = NULL;
    }
    host->ready = false;
}
