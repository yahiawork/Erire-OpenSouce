#define COBJMACROS

#include "ui.h"
#include "fileio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <commctrl.h>
#include <exdisp.h>
#include <ole2.h>
#include <oleidl.h>
#include <olectl.h>
#include <oleauto.h>
#include <winreg.h>
#include <wincodec.h>
#include <windowsx.h>

static const char *ER_UI_CLASS_NAME = "ErireAppWindow";
static IWICImagingFactory *er_ui_wic_factory = NULL;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif

#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_DEFAULT 0
#define DWMWCP_DONOTROUND 1
#define DWMWCP_ROUND 2
#define DWMWCP_ROUNDSMALL 3
#endif

#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_AUTO 0
#define DWMSBT_NONE 1
#define DWMSBT_MAINWINDOW 2
#define DWMSBT_TRANSIENTWINDOW 3
#define DWMSBT_TABBEDWINDOW 4
#endif

typedef HRESULT (WINAPI *ErUiDwmSetWindowAttributeFn)(HWND hwnd, DWORD attribute, LPCVOID value, DWORD value_size);
typedef BOOL (WINAPI *ErUiSetProcessDpiAwarenessContextFn)(HANDLE value);
typedef BOOL (WINAPI *ErUiSetProcessDPIAwareFn)(void);
typedef struct EventRegistrationToken {
    INT64 value;
} EventRegistrationToken;

typedef struct ICoreWebView2 ICoreWebView2;
typedef struct ICoreWebView2Controller ICoreWebView2Controller;
typedef struct ICoreWebView2Environment ICoreWebView2Environment;
typedef struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
typedef struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
typedef struct ICoreWebView2ExecuteScriptCompletedHandler ICoreWebView2ExecuteScriptCompletedHandler;

typedef struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
        REFIID riid,
        void **object
    );
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self);
    HRESULT (STDMETHODCALLTYPE *Invoke)(
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
        HRESULT error_code,
        ICoreWebView2Environment *result
    );
} ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl;

struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *lpVtbl;
};

typedef struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
        REFIID riid,
        void **object
    );
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self);
    HRESULT (STDMETHODCALLTYPE *Invoke)(
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
        HRESULT error_code,
        ICoreWebView2Controller *result
    );
} ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl;

struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *lpVtbl;
};

typedef struct ICoreWebView2ExecuteScriptCompletedHandlerVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ICoreWebView2ExecuteScriptCompletedHandler *self,
        REFIID riid,
        void **object
    );
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2ExecuteScriptCompletedHandler *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2ExecuteScriptCompletedHandler *self);
    HRESULT (STDMETHODCALLTYPE *Invoke)(
        ICoreWebView2ExecuteScriptCompletedHandler *self,
        HRESULT error_code,
        LPCWSTR result_object_as_json
    );
} ICoreWebView2ExecuteScriptCompletedHandlerVtbl;

struct ICoreWebView2ExecuteScriptCompletedHandler {
    const ICoreWebView2ExecuteScriptCompletedHandlerVtbl *lpVtbl;
};

typedef struct ICoreWebView2EnvironmentVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Environment *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Environment *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Environment *self);
    HRESULT (STDMETHODCALLTYPE *add_NewBrowserVersionAvailable)(
        ICoreWebView2Environment *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *CreateCoreWebView2Controller)(
        ICoreWebView2Environment *self,
        HWND parent_window,
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *handler
    );
    HRESULT (STDMETHODCALLTYPE *CreateWebResourceResponse)(
        ICoreWebView2Environment *self,
        IStream *content,
        int status_code,
        LPCWSTR reason_phrase,
        LPCWSTR headers,
        void **response
    );
    HRESULT (STDMETHODCALLTYPE *get_BrowserVersionString)(ICoreWebView2Environment *self, LPWSTR *version_info);
    HRESULT (STDMETHODCALLTYPE *remove_NewBrowserVersionAvailable)(
        ICoreWebView2Environment *self,
        EventRegistrationToken token
    );
} ICoreWebView2EnvironmentVtbl;

struct ICoreWebView2Environment {
    const ICoreWebView2EnvironmentVtbl *lpVtbl;
};

typedef struct ICoreWebView2ControllerVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Controller *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Controller *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Controller *self);
    HRESULT (STDMETHODCALLTYPE *add_AcceleratorKeyPressed)(
        ICoreWebView2Controller *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_GotFocus)(
        ICoreWebView2Controller *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_LostFocus)(
        ICoreWebView2Controller *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_MoveFocusRequested)(
        ICoreWebView2Controller *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_ZoomFactorChanged)(
        ICoreWebView2Controller *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *Close)(ICoreWebView2Controller *self);
    HRESULT (STDMETHODCALLTYPE *get_Bounds)(ICoreWebView2Controller *self, RECT *bounds);
    HRESULT (STDMETHODCALLTYPE *get_CoreWebView2)(ICoreWebView2Controller *self, ICoreWebView2 **core_webview2);
    HRESULT (STDMETHODCALLTYPE *get_IsVisible)(ICoreWebView2Controller *self, BOOL *is_visible);
    HRESULT (STDMETHODCALLTYPE *get_ParentWindow)(ICoreWebView2Controller *self, HWND *parent_window);
    HRESULT (STDMETHODCALLTYPE *get_ZoomFactor)(ICoreWebView2Controller *self, double *zoom_factor);
    HRESULT (STDMETHODCALLTYPE *MoveFocus)(ICoreWebView2Controller *self, int reason);
    HRESULT (STDMETHODCALLTYPE *NotifyParentWindowPositionChanged)(ICoreWebView2Controller *self);
    HRESULT (STDMETHODCALLTYPE *put_Bounds)(ICoreWebView2Controller *self, RECT bounds);
    HRESULT (STDMETHODCALLTYPE *put_IsVisible)(ICoreWebView2Controller *self, BOOL is_visible);
    HRESULT (STDMETHODCALLTYPE *put_ParentWindow)(ICoreWebView2Controller *self, HWND parent_window);
    HRESULT (STDMETHODCALLTYPE *put_ZoomFactor)(ICoreWebView2Controller *self, double zoom_factor);
    HRESULT (STDMETHODCALLTYPE *remove_AcceleratorKeyPressed)(
        ICoreWebView2Controller *self,
        EventRegistrationToken token
    );
    HRESULT (STDMETHODCALLTYPE *remove_GotFocus)(ICoreWebView2Controller *self, EventRegistrationToken token);
    HRESULT (STDMETHODCALLTYPE *remove_LostFocus)(ICoreWebView2Controller *self, EventRegistrationToken token);
    HRESULT (STDMETHODCALLTYPE *remove_MoveFocusRequested)(
        ICoreWebView2Controller *self,
        EventRegistrationToken token
    );
    HRESULT (STDMETHODCALLTYPE *remove_ZoomFactorChanged)(
        ICoreWebView2Controller *self,
        EventRegistrationToken token
    );
    HRESULT (STDMETHODCALLTYPE *SetBoundsAndZoomFactor)(
        ICoreWebView2Controller *self,
        RECT bounds,
        double zoom_factor
    );
} ICoreWebView2ControllerVtbl;

struct ICoreWebView2Controller {
    const ICoreWebView2ControllerVtbl *lpVtbl;
};

typedef struct ICoreWebView2Vtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2 *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2 *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2 *self);
    HRESULT (STDMETHODCALLTYPE *add_ContainsFullScreenElementChanged)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_ContentLoading)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_DocumentTitleChanged)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_FrameNavigationCompleted)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_FrameNavigationStarting)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_HistoryChanged)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_NavigationCompleted)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_NavigationStarting)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_NewWindowRequested)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_PermissionRequested)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_ProcessFailed)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_ScriptDialogOpening)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_SourceChanged)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_WebMessageReceived)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_WebResourceRequested)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *add_WindowCloseRequested)(
        ICoreWebView2 *self, IUnknown *event_handler, EventRegistrationToken *token
    );
    HRESULT (STDMETHODCALLTYPE *AddHostObjectToScript)(ICoreWebView2 *self, LPCWSTR name, VARIANT *object);
    HRESULT (STDMETHODCALLTYPE *AddScriptToExecuteOnDocumentCreated)(
        ICoreWebView2 *self, LPCWSTR javascript, IUnknown *handler
    );
    HRESULT (STDMETHODCALLTYPE *AddWebResourceRequestedFilter)(
        ICoreWebView2 *self, LPCWSTR uri, int resource_context
    );
    HRESULT (STDMETHODCALLTYPE *CallDevToolsProtocolMethod)(
        ICoreWebView2 *self, LPCWSTR method_name, LPCWSTR params_as_json, IUnknown *handler
    );
    HRESULT (STDMETHODCALLTYPE *CapturePreview)(
        ICoreWebView2 *self, int image_format, IStream *image_stream, IUnknown *handler
    );
    HRESULT (STDMETHODCALLTYPE *ExecuteScript)(
        ICoreWebView2 *self,
        LPCWSTR javascript,
        ICoreWebView2ExecuteScriptCompletedHandler *handler
    );
    HRESULT (STDMETHODCALLTYPE *get_BrowserProcessId)(ICoreWebView2 *self, UINT32 *process_id);
    HRESULT (STDMETHODCALLTYPE *get_CanGoBack)(ICoreWebView2 *self, BOOL *can_go_back);
    HRESULT (STDMETHODCALLTYPE *get_CanGoForward)(ICoreWebView2 *self, BOOL *can_go_forward);
    HRESULT (STDMETHODCALLTYPE *get_ContainsFullScreenElement)(ICoreWebView2 *self, BOOL *contains_full_screen);
    HRESULT (STDMETHODCALLTYPE *get_DocumentTitle)(ICoreWebView2 *self, LPWSTR *title);
    HRESULT (STDMETHODCALLTYPE *get_Settings)(ICoreWebView2 *self, IUnknown **settings);
    HRESULT (STDMETHODCALLTYPE *get_Source)(ICoreWebView2 *self, LPWSTR *uri);
    HRESULT (STDMETHODCALLTYPE *GetDevToolsProtocolEventReceiver)(
        ICoreWebView2 *self, LPCWSTR event_name, IUnknown **receiver
    );
    HRESULT (STDMETHODCALLTYPE *GoBack)(ICoreWebView2 *self);
    HRESULT (STDMETHODCALLTYPE *GoForward)(ICoreWebView2 *self);
    HRESULT (STDMETHODCALLTYPE *Navigate)(ICoreWebView2 *self, LPCWSTR uri);
    HRESULT (STDMETHODCALLTYPE *NavigateToString)(ICoreWebView2 *self, LPCWSTR html_content);
    HRESULT (STDMETHODCALLTYPE *OpenDevToolsWindow)(ICoreWebView2 *self);
    HRESULT (STDMETHODCALLTYPE *PostWebMessageAsJson)(ICoreWebView2 *self, LPCWSTR web_message_as_json);
    HRESULT (STDMETHODCALLTYPE *PostWebMessageAsString)(ICoreWebView2 *self, LPCWSTR web_message_as_string);
    HRESULT (STDMETHODCALLTYPE *Reload)(ICoreWebView2 *self);
} ICoreWebView2Vtbl;

struct ICoreWebView2 {
    const ICoreWebView2Vtbl *lpVtbl;
};

typedef HRESULT (STDAPICALLTYPE *ErUiCreateWebViewEnvironmentWithOptionsInternalFn)(
    LPCWSTR browser_executable_folder,
    LPCWSTR user_data_folder,
    IUnknown *environment_options,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environment_created_handler
);

typedef enum ErUiWebViewBackendKind {
    ER_UI_WEBVIEW_BACKEND_LEGACY,
    ER_UI_WEBVIEW_BACKEND_WEBVIEW2
} ErUiWebViewBackendKind;

typedef struct ErUiPlatformWebView {
    ErUiWebViewBackendKind backend_kind;
    IOleClientSite client_site;
    IOleInPlaceSite in_place_site;
    IOleInPlaceFrame in_place_frame;
    ULONG ref_count;
    ErUiApp *app;
    ErUiNode *node;
    HMODULE modern_loader;
    wchar_t *modern_runtime_dir;
    wchar_t *modern_user_data_dir;
    ICoreWebView2Environment *modern_environment;
    ICoreWebView2Controller *modern_controller;
    ICoreWebView2 *modern_webview;
    IOleObject *ole_object;
    IWebBrowser2 *browser;
    IOleInPlaceObject *in_place_object;
    IStorage *storage;
    ILockBytes *lock_bytes;
} ErUiPlatformWebView;

#define ER_UI_WEBVIEW_FROM_CLIENT_SITE(iface) \
    ((ErUiPlatformWebView *) ((char *) (iface) - offsetof(ErUiPlatformWebView, client_site)))
#define ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface) \
    ((ErUiPlatformWebView *) ((char *) (iface) - offsetof(ErUiPlatformWebView, in_place_site)))
#define ER_UI_WEBVIEW_FROM_INPLACE_FRAME(iface) \
    ((ErUiPlatformWebView *) ((char *) (iface) - offsetof(ErUiPlatformWebView, in_place_frame)))

static char *er_ui_dup(const char *text) {
    char *copy;
    size_t len;

    if (!text) {
        return NULL;
    }

    len = strlen(text);
    copy = (char *) malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static void er_ui_copy_executable_name(char *buffer, size_t buffer_size) {
    char module_path[MAX_PATH];
    const char *file_name;

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (GetModuleFileNameA(NULL, module_path, (DWORD) sizeof(module_path)) == 0) {
        return;
    }

    file_name = strrchr(module_path, '\\');
    file_name = file_name ? file_name + 1 : module_path;
    strncpy(buffer, file_name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
}

static void er_ui_write_ie_feature_flag(const char *feature_name, const char *exe_name, DWORD value) {
    char key_path[256];
    HKEY key = NULL;

    if (!feature_name || !exe_name || exe_name[0] == '\0') {
        return;
    }

    snprintf(
        key_path,
        sizeof(key_path),
        "Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\%s",
        feature_name
    );

    if (RegCreateKeyExA(
            HKEY_CURRENT_USER,
            key_path,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            NULL,
            &key,
            NULL
        ) != ERROR_SUCCESS) {
        return;
    }

    RegSetValueExA(key, exe_name, 0, REG_DWORD, (const BYTE *) &value, sizeof(value));
    RegCloseKey(key);
}

static void er_ui_enable_modern_legacy_webview_features(void) {
    char exe_name[MAX_PATH];

    er_ui_copy_executable_name(exe_name, sizeof(exe_name));
    if (exe_name[0] == '\0') {
        return;
    }

    /* Push the old WebBrowser control into IE11 edge mode and enable
       hardware-assisted rendering features where possible. */
    er_ui_write_ie_feature_flag("FEATURE_BROWSER_EMULATION", exe_name, 11001u);
    er_ui_write_ie_feature_flag("FEATURE_GPU_RENDERING", exe_name, 1u);
    er_ui_write_ie_feature_flag("FEATURE_96DPI_PIXEL", exe_name, 1u);
    er_ui_write_ie_feature_flag("FEATURE_AJAX_CONNECTIONEVENTS", exe_name, 1u);
    er_ui_write_ie_feature_flag("FEATURE_DISABLE_NAVIGATION_SOUNDS", exe_name, 1u);
}

static void er_ui_enable_modern_dpi_mode(void) {
    HMODULE user32;
    ErUiSetProcessDpiAwarenessContextFn set_dpi_awareness_context;
    ErUiSetProcessDPIAwareFn set_process_dpi_aware;

    user32 = LoadLibraryA("user32.dll");
    if (!user32) {
        return;
    }

    set_dpi_awareness_context = (ErUiSetProcessDpiAwarenessContextFn) GetProcAddress(
        user32,
        "SetProcessDpiAwarenessContext"
    );
    if (set_dpi_awareness_context) {
        set_dpi_awareness_context((HANDLE) -4);
        FreeLibrary(user32);
        return;
    }

    set_process_dpi_aware = (ErUiSetProcessDPIAwareFn) GetProcAddress(user32, "SetProcessDPIAware");
    if (set_process_dpi_aware) {
        set_process_dpi_aware();
    }

    FreeLibrary(user32);
}

static void er_ui_enable_modern_window_chrome(HWND hwnd) {
    HMODULE dwmapi;
    ErUiDwmSetWindowAttributeFn set_window_attribute;
    BOOL dark_mode = TRUE;
    int corner_preference = DWMWCP_ROUND;
    int backdrop_type = DWMSBT_MAINWINDOW;
    COLORREF caption_color = RGB(24, 32, 41);
    COLORREF text_color = RGB(248, 250, 252);

    if (!hwnd) {
        return;
    }

    dwmapi = LoadLibraryA("dwmapi.dll");
    if (!dwmapi) {
        return;
    }

    set_window_attribute = (ErUiDwmSetWindowAttributeFn) GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    if (!set_window_attribute) {
        FreeLibrary(dwmapi);
        return;
    }

    set_window_attribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode, (DWORD) sizeof(dark_mode));
    set_window_attribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_preference, (DWORD) sizeof(corner_preference));
    set_window_attribute(hwnd, DWMWA_CAPTION_COLOR, &caption_color, (DWORD) sizeof(caption_color));
    set_window_attribute(hwnd, DWMWA_TEXT_COLOR, &text_color, (DWORD) sizeof(text_color));
    set_window_attribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop_type, (DWORD) sizeof(backdrop_type));
    FreeLibrary(dwmapi);
}

static wchar_t *er_ui_utf8_to_wide(const char *text) {
    int count;
    wchar_t *buffer;

    if (!text) {
        return NULL;
    }

    count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (count <= 0) {
        count = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
        if (count <= 0) {
            return NULL;
        }
        buffer = (wchar_t *) malloc((size_t) count * sizeof(wchar_t));
        if (!buffer) {
            return NULL;
        }
        MultiByteToWideChar(CP_ACP, 0, text, -1, buffer, count);
        return buffer;
    }

    buffer = (wchar_t *) malloc((size_t) count * sizeof(wchar_t));
    if (!buffer) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, count);
    return buffer;
}

static wchar_t *er_ui_wide_dup(const wchar_t *text) {
    size_t len;
    wchar_t *copy;

    if (!text) {
        return NULL;
    }

    len = wcslen(text);
    copy = (wchar_t *) malloc((len + 1) * sizeof(wchar_t));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, (len + 1) * sizeof(wchar_t));
    return copy;
}

static int er_ui_compare_version_component(const wchar_t **left, const wchar_t **right) {
    unsigned long left_value = 0;
    unsigned long right_value = 0;

    while (**left >= L'0' && **left <= L'9') {
        left_value = (left_value * 10u) + (unsigned long) (**left - L'0');
        (*left)++;
    }
    while (**right >= L'0' && **right <= L'9') {
        right_value = (right_value * 10u) + (unsigned long) (**right - L'0');
        (*right)++;
    }

    if (left_value < right_value) {
        return -1;
    }
    if (left_value > right_value) {
        return 1;
    }
    return 0;
}

static int er_ui_compare_version_text(const wchar_t *left, const wchar_t *right) {
    const wchar_t *left_cursor = left ? left : L"";
    const wchar_t *right_cursor = right ? right : L"";
    int part_compare;

    while (*left_cursor != L'\0' || *right_cursor != L'\0') {
        part_compare = er_ui_compare_version_component(&left_cursor, &right_cursor);
        if (part_compare != 0) {
            return part_compare;
        }

        if (*left_cursor == L'.') {
            left_cursor++;
        }
        if (*right_cursor == L'.') {
            right_cursor++;
        }
    }

    return 0;
}

static wchar_t *er_ui_join_wide_path3(const wchar_t *a, const wchar_t *b, const wchar_t *c) {
    size_t len_a = a ? wcslen(a) : 0;
    size_t len_b = b ? wcslen(b) : 0;
    size_t len_c = c ? wcslen(c) : 0;
    size_t total = len_a + len_b + len_c + 3;
    wchar_t *buffer = (wchar_t *) calloc(total, sizeof(wchar_t));

    if (!buffer) {
        return NULL;
    }

    if (len_a > 0) {
        wcscpy(buffer, a);
    }
    if (len_b > 0) {
        if (len_a > 0 && buffer[wcslen(buffer) - 1] != L'\\') {
            wcscat(buffer, L"\\");
        }
        wcscat(buffer, b);
    }
    if (len_c > 0) {
        if (wcslen(buffer) > 0 && buffer[wcslen(buffer) - 1] != L'\\') {
            wcscat(buffer, L"\\");
        }
        wcscat(buffer, c);
    }

    return buffer;
}

static void er_ui_ensure_directory_recursive(const wchar_t *path) {
    wchar_t *mutable_path;
    wchar_t *cursor;

    if (!path || path[0] == L'\0') {
        return;
    }

    mutable_path = er_ui_wide_dup(path);
    if (!mutable_path) {
        return;
    }

    cursor = mutable_path;
    if (cursor[0] != L'\0' && cursor[1] == L':') {
        cursor += 2;
    }

    for (; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'/') {
            wchar_t saved = *cursor;
            *cursor = L'\0';
            if (mutable_path[0] != L'\0') {
                CreateDirectoryW(mutable_path, NULL);
            }
            *cursor = saved;
        }
    }

    CreateDirectoryW(mutable_path, NULL);
    free(mutable_path);
}

static wchar_t *er_ui_find_local_webview_runtime_dir(void) {
    char module_path[MAX_PATH];
    char module_dir[MAX_PATH];
    char runtime_dir_utf8[MAX_PATH];
    char loader_utf8[MAX_PATH];
    wchar_t *runtime_dir_wide;
    DWORD attributes;
    ErError error;

    er_error_clear(&error);
    if (!er_get_current_module_path(module_path, sizeof(module_path), &error)) {
        return NULL;
    }

    er_path_dirname(module_path, module_dir, sizeof(module_dir));
    er_path_join(module_dir, "webview2", runtime_dir_utf8, sizeof(runtime_dir_utf8));
    er_path_join(runtime_dir_utf8,
#ifdef _WIN64
        "EBWebView\\x64\\EmbeddedBrowserWebView.dll",
#else
        "EBWebView\\x86\\EmbeddedBrowserWebView.dll",
#endif
        loader_utf8,
        sizeof(loader_utf8)
    );

    runtime_dir_wide = er_ui_utf8_to_wide(runtime_dir_utf8);
    if (!runtime_dir_wide) {
        return NULL;
    }

    {
        wchar_t *loader_wide = er_ui_utf8_to_wide(loader_utf8);
        if (!loader_wide) {
            free(runtime_dir_wide);
            return NULL;
        }
        attributes = GetFileAttributesW(loader_wide);
        free(loader_wide);
    }

    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        free(runtime_dir_wide);
        return NULL;
    }

    return runtime_dir_wide;
}

static wchar_t *er_ui_find_edge_webview_runtime_dir(void) {
    WIN32_FIND_DATAW find_data;
    HANDLE search;
    wchar_t pattern[MAX_PATH];
    wchar_t best_version[MAX_PATH];
    wchar_t *best_path = NULL;
    wchar_t *local_runtime;

    local_runtime = er_ui_find_local_webview_runtime_dir();
    if (local_runtime) {
        return local_runtime;
    }

    swprintf(
        pattern,
        sizeof(pattern) / sizeof(pattern[0]),
        L"%ls\\*",
        L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application"
    );

    best_version[0] = L'\0';
    search = FindFirstFileW(pattern, &find_data);
    if (search == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    do {
        wchar_t *candidate_dir;
        wchar_t *candidate_dll;
        DWORD attributes;

        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        candidate_dir = er_ui_join_wide_path3(
            L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application",
            find_data.cFileName,
#ifdef _WIN64
            L"EBWebView\\x64\\EmbeddedBrowserWebView.dll"
#else
            L"EBWebView\\x86\\EmbeddedBrowserWebView.dll"
#endif
        );
        if (!candidate_dir) {
            continue;
        }

        candidate_dll = candidate_dir;
        attributes = GetFileAttributesW(candidate_dll);
        free(candidate_dll);
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }

        if (best_version[0] == L'\0' || er_ui_compare_version_text(find_data.cFileName, best_version) > 0) {
            free(best_path);
            best_path = er_ui_join_wide_path3(
                L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application",
                find_data.cFileName,
                NULL
            );
            if (best_path) {
                wcsncpy(best_version, find_data.cFileName, (sizeof(best_version) / sizeof(best_version[0])) - 1);
                best_version[(sizeof(best_version) / sizeof(best_version[0])) - 1] = L'\0';
            }
        }
    } while (FindNextFileW(search, &find_data));

    FindClose(search);
    return best_path;
}

static wchar_t *er_ui_build_webview2_user_data_dir(void) {
    wchar_t temp_path[MAX_PATH];
    wchar_t exe_name[MAX_PATH];
    char exe_name_utf8[MAX_PATH];
    wchar_t *wide_name = NULL;
    wchar_t *base_dir = NULL;
    wchar_t *full_dir = NULL;

    if (GetTempPathW((DWORD) (sizeof(temp_path) / sizeof(temp_path[0])), temp_path) == 0) {
        return NULL;
    }

    er_ui_copy_executable_name(exe_name_utf8, sizeof(exe_name_utf8));
    if (exe_name_utf8[0] == '\0') {
        strcpy(exe_name_utf8, "erire");
    }

    wide_name = er_ui_utf8_to_wide(exe_name_utf8);
    if (!wide_name) {
        return NULL;
    }

    swprintf(exe_name, sizeof(exe_name) / sizeof(exe_name[0]), L"%ls", wide_name);
    free(wide_name);

    base_dir = er_ui_join_wide_path3(temp_path, L"ErireWebView2", NULL);
    full_dir = er_ui_join_wide_path3(base_dir, exe_name, NULL);
    er_ui_ensure_directory_recursive(base_dir);
    er_ui_ensure_directory_recursive(full_dir);
    free(base_dir);
    return full_dir;
}

static bool er_ui_wic_factory_get(IWICImagingFactory **out_factory, ErError *error) {
    HRESULT hr;

    if (!out_factory) {
        er_error_set(error, 0, 0, "Invalid WIC factory output pointer");
        return false;
    }

    if (!er_ui_wic_factory) {
        hr = CoCreateInstance(
            &CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory,
            (void **) &er_ui_wic_factory
        );
        if (FAILED(hr) || !er_ui_wic_factory) {
            er_error_set(error, 0, 0, "Could not initialize the Windows Imaging Component");
            return false;
        }
    }

    *out_factory = er_ui_wic_factory;
    return true;
}

static bool er_ui_load_bitmap_from_path(
    const char *path,
    HBITMAP *out_bitmap,
    int *out_width,
    int *out_height,
    ErError *error
) {
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    wchar_t *wide_path = NULL;
    UINT width = 0;
    UINT height = 0;
    BITMAPINFO bitmap_info;
    void *pixels = NULL;
    HBITMAP bitmap = NULL;
    HRESULT hr;
    bool ok = false;

    if (!path || path[0] == '\0' || !out_bitmap || !out_width || !out_height) {
        er_error_set(error, 0, 0, "Image path and outputs are required");
        return false;
    }

    if (!er_ui_wic_factory_get(&factory, error)) {
        return false;
    }

    wide_path = er_ui_utf8_to_wide(path);
    if (!wide_path) {
        er_error_set(error, 0, 0, "Could not convert image path");
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory,
        wide_path,
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );
    if (FAILED(hr) || !decoder) {
        er_error_set(error, 0, 0, "Could not open image file: %s", path);
        goto cleanup;
    }

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame) {
        er_error_set(error, 0, 0, "Could not decode the first frame of image: %s", path);
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || !converter) {
        er_error_set(error, 0, 0, "Could not create image format converter");
        goto cleanup;
    }

    hr = IWICFormatConverter_Initialize(
        converter,
        (IWICBitmapSource *) frame,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not convert image to a renderable format");
        goto cleanup;
    }

    hr = IWICBitmapSource_GetSize((IWICBitmapSource *) converter, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        er_error_set(error, 0, 0, "Could not read image dimensions");
        goto cleanup;
    }

    memset(&bitmap_info, 0, sizeof(bitmap_info));
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = (LONG) width;
    bitmap_info.bmiHeader.biHeight = -(LONG) height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection(NULL, &bitmap_info, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!bitmap || !pixels) {
        er_error_set(error, 0, 0, "Could not allocate image bitmap");
        goto cleanup;
    }

    hr = IWICBitmapSource_CopyPixels(
        (IWICBitmapSource *) converter,
        NULL,
        width * 4u,
        width * height * 4u,
        (BYTE *) pixels
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not copy image pixels");
        goto cleanup;
    }

    *out_bitmap = bitmap;
    *out_width = (int) width;
    *out_height = (int) height;
    bitmap = NULL;
    ok = true;

cleanup:
    if (bitmap) {
        DeleteObject(bitmap);
    }
    if (converter) {
        IWICFormatConverter_Release(converter);
    }
    if (frame) {
        IWICBitmapFrameDecode_Release(frame);
    }
    if (decoder) {
        IWICBitmapDecoder_Release(decoder);
    }
    free(wide_path);
    return ok;
}

static HICON er_ui_create_icon_from_bitmap(HBITMAP bitmap, int width, int height) {
    ICONINFO icon_info;
    HBITMAP mask_bitmap;
    HICON icon;

    if (!bitmap || width <= 0 || height <= 0) {
        return NULL;
    }

    mask_bitmap = CreateBitmap(width, height, 1, 1, NULL);
    if (!mask_bitmap) {
        return NULL;
    }

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.fIcon = TRUE;
    icon_info.hbmColor = bitmap;
    icon_info.hbmMask = mask_bitmap;
    icon = CreateIconIndirect(&icon_info);
    DeleteObject(mask_bitmap);
    return icon;
}

static void er_ui_destroy_node_assets(ErUiNode *node) {
    if (!node) {
        return;
    }

    if (node->image_bitmap) {
        DeleteObject(node->image_bitmap);
        node->image_bitmap = NULL;
    }
    if (node->icon_handle) {
        DestroyIcon(node->icon_handle);
        node->icon_handle = NULL;
    }
}

static bool er_ui_load_icon_from_path(const char *path, HICON *out_icon, ErError *error) {
    HBITMAP bitmap = NULL;
    HICON icon = NULL;
    int width = 0;
    int height = 0;

    if (!path || path[0] == '\0' || !out_icon) {
        er_error_set(error, 0, 0, "Icon path is required");
        return false;
    }

    if (!er_ui_load_bitmap_from_path(path, &bitmap, &width, &height, error)) {
        return false;
    }

    icon = er_ui_create_icon_from_bitmap(bitmap, width, height);
    DeleteObject(bitmap);
    if (!icon) {
        er_error_set(error, 0, 0, "Could not create icon from image");
        return false;
    }

    *out_icon = icon;
    return true;
}

static bool er_ui_node_load_visual_assets(ErUiNode *node, ErError *error) {
    const char *path = NULL;

    if (!node) {
        return false;
    }

    er_ui_destroy_node_assets(node);

    if (node->kind == ER_UI_NODE_IMAGE && node->asset_path && node->asset_path[0] != '\0') {
        path = node->asset_path;
    } else if (node->icon_path && node->icon_path[0] != '\0') {
        path = node->icon_path;
    }

    if (!path) {
        return true;
    }

    if (!er_ui_load_bitmap_from_path(path, &node->image_bitmap, &node->image_pixel_width, &node->image_pixel_height, error)) {
        return false;
    }

    node->icon_handle = er_ui_create_icon_from_bitmap(node->image_bitmap, node->image_pixel_width, node->image_pixel_height);
    return true;
}

static void er_ui_compute_fit_rect(
    const RECT *bounds,
    int source_width,
    int source_height,
    ErUiImageFit fit,
    RECT *out_rect
) {
    int bounds_width;
    int bounds_height;
    double scale_x;
    double scale_y;
    double scale;
    int draw_width;
    int draw_height;

    if (!bounds || !out_rect || source_width <= 0 || source_height <= 0) {
        if (out_rect && bounds) {
            *out_rect = *bounds;
        }
        return;
    }

    *out_rect = *bounds;
    bounds_width = bounds->right - bounds->left;
    bounds_height = bounds->bottom - bounds->top;
    if (bounds_width <= 0 || bounds_height <= 0) {
        return;
    }

    if (fit == ER_UI_IMAGE_FIT_STRETCH) {
        return;
    }

    scale_x = (double) bounds_width / (double) source_width;
    scale_y = (double) bounds_height / (double) source_height;

    if (fit == ER_UI_IMAGE_FIT_COVER) {
        scale = scale_x > scale_y ? scale_x : scale_y;
    } else if (fit == ER_UI_IMAGE_FIT_CENTER) {
        scale = 1.0;
    } else {
        scale = scale_x < scale_y ? scale_x : scale_y;
    }

    draw_width = (int) ((double) source_width * scale + 0.5);
    draw_height = (int) ((double) source_height * scale + 0.5);
    if (draw_width < 1) {
        draw_width = 1;
    }
    if (draw_height < 1) {
        draw_height = 1;
    }

    out_rect->left = bounds->left + ((bounds_width - draw_width) / 2);
    out_rect->top = bounds->top + ((bounds_height - draw_height) / 2);
    out_rect->right = out_rect->left + draw_width;
    out_rect->bottom = out_rect->top + draw_height;
}

static void er_ui_draw_bitmap_in_rect(HDC hdc, HBITMAP bitmap, int width, int height, const RECT *dest_rect) {
    HDC memory_dc;
    HBITMAP old_bitmap;
    BLENDFUNCTION blend;
    int draw_width;
    int draw_height;

    if (!hdc || !bitmap || !dest_rect || width <= 0 || height <= 0) {
        return;
    }

    draw_width = dest_rect->right - dest_rect->left;
    draw_height = dest_rect->bottom - dest_rect->top;
    if (draw_width <= 0 || draw_height <= 0) {
        return;
    }

    memory_dc = CreateCompatibleDC(hdc);
    if (!memory_dc) {
        return;
    }

    old_bitmap = (HBITMAP) SelectObject(memory_dc, bitmap);
    memset(&blend, 0, sizeof(blend));
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(
        hdc,
        dest_rect->left,
        dest_rect->top,
        draw_width,
        draw_height,
        memory_dc,
        0,
        0,
        width,
        height,
        blend
    );
    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
}

static RECT er_ui_node_rect(const ErUiNode *node);
static RECT er_ui_webview_rect(const ErUiNode *node);
static ULONG er_ui_webview_release_ref(ErUiPlatformWebView *webview);
static void er_ui_webview_destroy(ErUiPlatformWebView *webview);
static void er_ui_animation_capture_origin(ErUiNode *node);
static void er_ui_animation_tick_app(ErUiApp *app);

typedef struct ErUiWebView2CreateState {
    HANDLE event_handle;
    ErUiPlatformWebView *webview;
    HRESULT result;
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *controller_handler_iface;
} ErUiWebView2CreateState;

typedef struct ErUiWebView2EnvironmentHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    ULONG ref_count;
    ErUiWebView2CreateState *state;
} ErUiWebView2EnvironmentHandler;

typedef struct ErUiWebView2ControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    ULONG ref_count;
    ErUiWebView2CreateState *state;
} ErUiWebView2ControllerHandler;

typedef struct ErUiWebView2ExecuteHandler {
    ICoreWebView2ExecuteScriptCompletedHandler iface;
    ULONG ref_count;
} ErUiWebView2ExecuteHandler;

static ULONG STDMETHODCALLTYPE er_ui_webview2_env_handler_add_ref(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self
) {
    ErUiWebView2EnvironmentHandler *handler = (ErUiWebView2EnvironmentHandler *) self;
    return ++handler->ref_count;
}

static ULONG STDMETHODCALLTYPE er_ui_webview2_env_handler_release(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self
) {
    ErUiWebView2EnvironmentHandler *handler = (ErUiWebView2EnvironmentHandler *) self;
    ULONG count = handler->ref_count > 0 ? --handler->ref_count : 0;

    if (count == 0) {
        free(handler);
    }
    return count;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview2_env_handler_query_interface(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    REFIID riid,
    void **object
) {
    (void) riid;

    if (!object) {
        return E_POINTER;
    }

    *object = self;
    er_ui_webview2_env_handler_add_ref(self);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview2_controller_handler_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    HRESULT error_code,
    ICoreWebView2Controller *result
);

static HRESULT STDMETHODCALLTYPE er_ui_webview2_env_handler_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    HRESULT error_code,
    ICoreWebView2Environment *result
) {
    ErUiWebView2EnvironmentHandler *handler = (ErUiWebView2EnvironmentHandler *) self;
    ErUiPlatformWebView *webview = handler && handler->state ? handler->state->webview : NULL;

    if (!handler || !handler->state) {
        return E_FAIL;
    }

    handler->state->result = error_code;
    if (FAILED(error_code) || !result || !webview) {
        SetEvent(handler->state->event_handle);
        return S_OK;
    }

    result->lpVtbl->AddRef(result);
    webview->modern_environment = result;

    error_code = result->lpVtbl->CreateCoreWebView2Controller(
        result,
        webview->app ? webview->app->hwnd : NULL,
        handler->state->controller_handler_iface
    );
    handler->state->result = error_code;
    if (FAILED(error_code)) {
        SetEvent(handler->state->event_handle);
    }

    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl er_ui_webview2_env_handler_vtbl = {
    er_ui_webview2_env_handler_query_interface,
    er_ui_webview2_env_handler_add_ref,
    er_ui_webview2_env_handler_release,
    er_ui_webview2_env_handler_invoke
};

static ULONG STDMETHODCALLTYPE er_ui_webview2_controller_handler_add_ref(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self
) {
    ErUiWebView2ControllerHandler *handler = (ErUiWebView2ControllerHandler *) self;
    return ++handler->ref_count;
}

static ULONG STDMETHODCALLTYPE er_ui_webview2_controller_handler_release(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self
) {
    ErUiWebView2ControllerHandler *handler = (ErUiWebView2ControllerHandler *) self;
    ULONG count = handler->ref_count > 0 ? --handler->ref_count : 0;

    if (count == 0) {
        free(handler);
    }
    return count;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview2_controller_handler_query_interface(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    REFIID riid,
    void **object
) {
    (void) riid;

    if (!object) {
        return E_POINTER;
    }

    *object = self;
    er_ui_webview2_controller_handler_add_ref(self);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview2_controller_handler_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    HRESULT error_code,
    ICoreWebView2Controller *result
) {
    ErUiWebView2ControllerHandler *handler = (ErUiWebView2ControllerHandler *) self;
    ErUiPlatformWebView *webview = handler && handler->state ? handler->state->webview : NULL;
    RECT rect;

    if (!handler || !handler->state) {
        return E_FAIL;
    }

    handler->state->result = error_code;
    if (FAILED(error_code) || !result || !webview) {
        SetEvent(handler->state->event_handle);
        return S_OK;
    }

    result->lpVtbl->AddRef(result);
    webview->modern_controller = result;

    error_code = result->lpVtbl->get_CoreWebView2(result, &webview->modern_webview);
    handler->state->result = error_code;
    if (FAILED(error_code) || !webview->modern_webview) {
        SetEvent(handler->state->event_handle);
        return S_OK;
    }

    rect = er_ui_webview_rect(webview->node);
    result->lpVtbl->put_Bounds(result, rect);
    result->lpVtbl->put_IsVisible(result, webview->node && webview->node->visible ? TRUE : FALSE);

    SetEvent(handler->state->event_handle);
    return S_OK;
}

static const ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl er_ui_webview2_controller_handler_vtbl = {
    er_ui_webview2_controller_handler_query_interface,
    er_ui_webview2_controller_handler_add_ref,
    er_ui_webview2_controller_handler_release,
    er_ui_webview2_controller_handler_invoke
};

static ULONG STDMETHODCALLTYPE er_ui_webview2_execute_handler_add_ref(ICoreWebView2ExecuteScriptCompletedHandler *self) {
    ErUiWebView2ExecuteHandler *handler = (ErUiWebView2ExecuteHandler *) self;
    return ++handler->ref_count;
}

static ULONG STDMETHODCALLTYPE er_ui_webview2_execute_handler_release(ICoreWebView2ExecuteScriptCompletedHandler *self) {
    ErUiWebView2ExecuteHandler *handler = (ErUiWebView2ExecuteHandler *) self;
    ULONG count = handler->ref_count > 0 ? --handler->ref_count : 0;

    if (count == 0) {
        free(handler);
    }
    return count;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview2_execute_handler_query_interface(
    ICoreWebView2ExecuteScriptCompletedHandler *self,
    REFIID riid,
    void **object
) {
    (void) riid;

    if (!object) {
        return E_POINTER;
    }

    *object = self;
    er_ui_webview2_execute_handler_add_ref(self);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview2_execute_handler_invoke(
    ICoreWebView2ExecuteScriptCompletedHandler *self,
    HRESULT error_code,
    LPCWSTR result_object_as_json
) {
    (void) self;
    (void) error_code;
    (void) result_object_as_json;
    return S_OK;
}

static const ICoreWebView2ExecuteScriptCompletedHandlerVtbl er_ui_webview2_execute_handler_vtbl = {
    er_ui_webview2_execute_handler_query_interface,
    er_ui_webview2_execute_handler_add_ref,
    er_ui_webview2_execute_handler_release,
    er_ui_webview2_execute_handler_invoke
};

static bool er_ui_webview2_wait_for_completion(HANDLE event_handle) {
    DWORD wait_result;
    MSG msg;

    if (!event_handle) {
        return false;
    }

    for (;;) {
        wait_result = MsgWaitForMultipleObjects(1, &event_handle, FALSE, 15000, QS_ALLINPUT);
        if (wait_result == WAIT_OBJECT_0) {
            return true;
        }
        if (wait_result == WAIT_TIMEOUT || wait_result == WAIT_FAILED) {
            return false;
        }

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
}

static bool er_ui_webview2_update_runtime_path(ErUiPlatformWebView *webview, ErError *error) {
    wchar_t *runtime_dir;
    wchar_t *loader_path;

    if (!webview) {
        return false;
    }

    runtime_dir = er_ui_find_edge_webview_runtime_dir();
    if (!runtime_dir) {
        er_error_set(error, 0, 0, "Could not find Microsoft Edge WebView runtime");
        return false;
    }

    loader_path = er_ui_join_wide_path3(
        runtime_dir,
#ifdef _WIN64
        L"EBWebView\\x64",
#else
        L"EBWebView\\x86",
#endif
        L"EmbeddedBrowserWebView.dll"
    );
    if (!loader_path) {
        free(runtime_dir);
        er_error_set(error, 0, 0, "Could not build WebView2 runtime path");
        return false;
    }

    if (GetFileAttributesW(loader_path) == INVALID_FILE_ATTRIBUTES) {
        free(loader_path);
        free(runtime_dir);
        er_error_set(error, 0, 0, "EmbeddedBrowserWebView.dll is missing from the runtime");
        return false;
    }

    webview->modern_runtime_dir = runtime_dir;
    free(loader_path);
    return true;
}

static bool er_ui_webview2_load_module(ErUiPlatformWebView *webview, ErError *error) {
    wchar_t *loader_path;

    if (!webview || !webview->modern_runtime_dir) {
        er_error_set(error, 0, 0, "WebView2 runtime path is not initialized");
        return false;
    }

    loader_path = er_ui_join_wide_path3(
        webview->modern_runtime_dir,
#ifdef _WIN64
        L"EBWebView\\x64",
#else
        L"EBWebView\\x86",
#endif
        L"EmbeddedBrowserWebView.dll"
    );
    if (!loader_path) {
        er_error_set(error, 0, 0, "Could not build EmbeddedBrowserWebView path");
        return false;
    }

    SetDllDirectoryW(webview->modern_runtime_dir);
    webview->modern_loader = LoadLibraryW(loader_path);
    SetDllDirectoryW(NULL);
    free(loader_path);

    if (!webview->modern_loader) {
        er_error_set(error, 0, 0, "Could not load EmbeddedBrowserWebView.dll");
        return false;
    }

    return true;
}

static bool er_ui_webview2_update_rect(ErUiPlatformWebView *webview) {
    RECT rect;

    if (!webview || !webview->modern_controller || !webview->node) {
        return false;
    }

    rect = er_ui_webview_rect(webview->node);
    return SUCCEEDED(webview->modern_controller->lpVtbl->put_Bounds(webview->modern_controller, rect));
}

static bool er_ui_webview2_set_visible(ErUiPlatformWebView *webview, bool visible) {
    if (!webview || !webview->modern_controller) {
        return false;
    }

    return SUCCEEDED(webview->modern_controller->lpVtbl->put_IsVisible(
        webview->modern_controller,
        visible ? TRUE : FALSE
    ));
}

static bool er_ui_webview2_navigate(ErUiPlatformWebView *webview, const char *url, ErError *error) {
    wchar_t *wide_url;
    HRESULT hr;

    if (!webview || !webview->modern_webview) {
        er_error_set(error, 0, 0, "WebView2 backend is not initialized");
        return false;
    }

    wide_url = er_ui_utf8_to_wide(url && url[0] != '\0' ? url : "about:blank");
    if (!wide_url) {
        er_error_set(error, 0, 0, "Could not convert WebView2 URL");
        return false;
    }

    hr = webview->modern_webview->lpVtbl->Navigate(webview->modern_webview, wide_url);
    free(wide_url);
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "WebView2 navigation failed");
        return false;
    }

    return true;
}

static bool er_ui_webview2_run_script(ErUiPlatformWebView *webview, const char *script, ErError *error) {
    ErUiWebView2ExecuteHandler *handler;
    wchar_t *wide_script;
    HRESULT hr;

    if (!webview || !webview->modern_webview) {
        er_error_set(error, 0, 0, "WebView2 backend is not initialized");
        return false;
    }

    wide_script = er_ui_utf8_to_wide(script ? script : "");
    if (!wide_script) {
        er_error_set(error, 0, 0, "Could not convert WebView2 script text");
        return false;
    }

    handler = (ErUiWebView2ExecuteHandler *) calloc(1, sizeof(*handler));
    if (!handler) {
        free(wide_script);
        er_error_set(error, 0, 0, "Out of memory while preparing WebView2 script handler");
        return false;
    }

    handler->iface.lpVtbl = &er_ui_webview2_execute_handler_vtbl;
    handler->ref_count = 1;

    hr = webview->modern_webview->lpVtbl->ExecuteScript(webview->modern_webview, wide_script, &handler->iface);
    free(wide_script);
    if (FAILED(hr)) {
        handler->iface.lpVtbl->Release(&handler->iface);
        er_error_set(error, 0, 0, "WebView2 script execution failed");
        return false;
    }

    return true;
}

static ErUiPlatformWebView *er_ui_webview2_create(ErUiApp *app, ErUiNode *node, ErError *error) {
    ErUiPlatformWebView *webview;
    ErUiCreateWebViewEnvironmentWithOptionsInternalFn create_environment;
    ErUiWebView2CreateState state;
    ErUiWebView2EnvironmentHandler *environment_handler = NULL;
    ErUiWebView2ControllerHandler *controller_handler = NULL;
    wchar_t *initial_url = NULL;
    HRESULT hr;

    webview = (ErUiPlatformWebView *) calloc(1, sizeof(ErUiPlatformWebView));
    if (!webview) {
        er_error_set(error, 0, 0, "Out of memory while creating WebView2 backend");
        return NULL;
    }

    webview->backend_kind = ER_UI_WEBVIEW_BACKEND_WEBVIEW2;
    webview->ref_count = 1;
    webview->app = app;
    webview->node = node;

    if (!er_ui_webview2_update_runtime_path(webview, error)) {
        er_ui_webview_release_ref(webview);
        return NULL;
    }

    if (!er_ui_webview2_load_module(webview, error)) {
        er_ui_webview_destroy(webview);
        return NULL;
    }

    create_environment = (ErUiCreateWebViewEnvironmentWithOptionsInternalFn) GetProcAddress(
        webview->modern_loader,
        "CreateWebViewEnvironmentWithOptionsInternal"
    );
    if (!create_environment) {
        er_error_set(error, 0, 0, "Could not locate CreateWebViewEnvironmentWithOptionsInternal");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    webview->modern_user_data_dir = er_ui_build_webview2_user_data_dir();
    if (!webview->modern_user_data_dir) {
        er_error_set(error, 0, 0, "Could not prepare WebView2 user data directory");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    memset(&state, 0, sizeof(state));
    state.event_handle = CreateEventW(NULL, TRUE, FALSE, NULL);
    state.webview = webview;
    state.result = E_FAIL;
    if (!state.event_handle) {
        er_error_set(error, 0, 0, "Could not create WebView2 initialization event");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    controller_handler = (ErUiWebView2ControllerHandler *) calloc(1, sizeof(*controller_handler));
    environment_handler = (ErUiWebView2EnvironmentHandler *) calloc(1, sizeof(*environment_handler));
    if (!controller_handler || !environment_handler) {
        CloseHandle(state.event_handle);
        free(controller_handler);
        free(environment_handler);
        er_error_set(error, 0, 0, "Out of memory while preparing WebView2 callbacks");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    controller_handler->iface.lpVtbl = &er_ui_webview2_controller_handler_vtbl;
    controller_handler->ref_count = 1;
    controller_handler->state = &state;
    state.controller_handler_iface = &controller_handler->iface;

    environment_handler->iface.lpVtbl = &er_ui_webview2_env_handler_vtbl;
    environment_handler->ref_count = 1;
    environment_handler->state = &state;

    hr = create_environment(
        webview->modern_runtime_dir,
        webview->modern_user_data_dir,
        NULL,
        &environment_handler->iface
    );
    if (FAILED(hr)) {
        CloseHandle(state.event_handle);
        controller_handler->iface.lpVtbl->Release(&controller_handler->iface);
        environment_handler->iface.lpVtbl->Release(&environment_handler->iface);
        er_error_set(error, 0, 0, "WebView2 environment creation failed to start");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    if (!er_ui_webview2_wait_for_completion(state.event_handle)) {
        CloseHandle(state.event_handle);
        controller_handler->iface.lpVtbl->Release(&controller_handler->iface);
        environment_handler->iface.lpVtbl->Release(&environment_handler->iface);
        er_error_set(error, 0, 0, "Timed out while waiting for WebView2 initialization");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    CloseHandle(state.event_handle);
    controller_handler->iface.lpVtbl->Release(&controller_handler->iface);
    environment_handler->iface.lpVtbl->Release(&environment_handler->iface);

    if (FAILED(state.result) || !webview->modern_controller || !webview->modern_webview) {
        er_error_set(error, 0, 0, "WebView2 initialization did not complete successfully");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    initial_url = er_ui_utf8_to_wide(node && node->text && node->text[0] != '\0' ? node->text : "about:blank");
    if (!initial_url) {
        er_error_set(error, 0, 0, "Could not convert initial WebView2 URL");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    hr = webview->modern_webview->lpVtbl->Navigate(webview->modern_webview, initial_url);
    free(initial_url);
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not navigate initial WebView2 page");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    er_ui_webview2_set_visible(webview, node && node->visible);
    er_ui_webview2_update_rect(webview);
    return webview;
}

static ULONG er_ui_webview_add_ref(ErUiPlatformWebView *webview) {
    return ++webview->ref_count;
}

static ULONG er_ui_webview_release_ref(ErUiPlatformWebView *webview) {
    ULONG count;

    if (!webview) {
        return 0;
    }

    count = --webview->ref_count;
    if (count == 0) {
        free(webview);
    }
    return count;
}

static HRESULT er_ui_webview_query_interface(ErUiPlatformWebView *webview, REFIID riid, void **out_object) {
    if (!out_object) {
        return E_POINTER;
    }

    *out_object = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleClientSite)) {
        *out_object = &webview->client_site;
    } else if (IsEqualIID(riid, &IID_IOleWindow) || IsEqualIID(riid, &IID_IOleInPlaceSite)) {
        *out_object = &webview->in_place_site;
    } else if (IsEqualIID(riid, &IID_IOleInPlaceFrame) || IsEqualIID(riid, &IID_IOleInPlaceUIWindow)) {
        *out_object = &webview->in_place_frame;
    } else {
        return E_NOINTERFACE;
    }

    er_ui_webview_add_ref(webview);
    return S_OK;
}

static RECT er_ui_webview_rect(const ErUiNode *node) {
    RECT rect;
    int inset;

    rect = er_ui_node_rect(node);
    if (!node) {
        return rect;
    }

    inset = node->padding > 0 ? node->padding : 0;
    rect.left += inset;
    rect.right -= inset;
    rect.top += inset;
    rect.bottom -= inset;

    if (rect.right - rect.left < 32 || rect.bottom - rect.top < 32) {
        return er_ui_node_rect(node);
    }

    return rect;
}

static void er_ui_webview_sync_hwnd(ErUiPlatformWebView *webview) {
    HWND hwnd = NULL;

    if (!webview || webview->backend_kind != ER_UI_WEBVIEW_BACKEND_LEGACY ||
        !webview->in_place_object || !webview->node) {
        return;
    }

    if (SUCCEEDED(IOleInPlaceObject_GetWindow(webview->in_place_object, &hwnd))) {
        webview->node->hwnd = hwnd;
    }
}

static void er_ui_webview_update_rect(ErUiPlatformWebView *webview) {
    RECT rect;

    if (!webview) {
        return;
    }

    if (webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        er_ui_webview2_update_rect(webview);
        return;
    }

    if (!webview->in_place_object || !webview->node) {
        return;
    }

    rect = er_ui_webview_rect(webview->node);
    IOleInPlaceObject_SetObjectRects(webview->in_place_object, &rect, &rect);
    er_ui_webview_sync_hwnd(webview);
    if (webview->node->hwnd && IsWindow(webview->node->hwnd)) {
        MoveWindow(
            webview->node->hwnd,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            TRUE
        );
    }
}

static void er_ui_webview_set_visible(ErUiPlatformWebView *webview, bool visible) {
    if (!webview) {
        return;
    }

    if (webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        er_ui_webview2_set_visible(webview, visible);
        return;
    }

    if (webview->browser) {
        IWebBrowser2_put_Visible(webview->browser, visible ? VARIANT_TRUE : VARIANT_FALSE);
    }
    if (webview->node && webview->node->hwnd && IsWindow(webview->node->hwnd)) {
        ShowWindow(webview->node->hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

static bool er_ui_webview_navigate(ErUiPlatformWebView *webview, const char *url, ErError *error) {
    wchar_t *wide_url;
    VARIANT target;
    VARIANT empty;
    HRESULT hr;

    if (webview && webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        return er_ui_webview2_navigate(webview, url, error);
    }

    if (!webview || !webview->browser) {
        er_error_set(error, 0, 0, "Webview backend is not initialized");
        return false;
    }

    wide_url = er_ui_utf8_to_wide(url && url[0] != '\0' ? url : "about:blank");
    if (!wide_url) {
        er_error_set(error, 0, 0, "Could not convert webview URL");
        return false;
    }

    VariantInit(&target);
    VariantInit(&empty);
    V_VT(&target) = VT_BSTR;
    V_BSTR(&target) = SysAllocString(wide_url);
    free(wide_url);
    if (!V_BSTR(&target)) {
        er_error_set(error, 0, 0, "Out of memory while navigating webview");
        return false;
    }

    hr = IWebBrowser2_Navigate2(webview->browser, &target, &empty, &empty, &empty, &empty);
    VariantClear(&target);
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Webview navigation failed");
        return false;
    }

    return true;
}

static bool er_ui_dispatch_get_property(
    IDispatch *dispatch,
    const wchar_t *name,
    VARIANT *out_result,
    ErError *error
) {
    OLECHAR *names[1];
    DISPID dispid;
    DISPPARAMS params;
    HRESULT hr;

    if (!dispatch || !name || !out_result) {
        er_error_set(error, 0, 0, "Invalid COM dispatch property request");
        return false;
    }

    names[0] = (OLECHAR *) name;
    hr = IDispatch_GetIDsOfNames(dispatch, &IID_NULL, names, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not resolve browser property");
        return false;
    }

    memset(&params, 0, sizeof(params));
    VariantInit(out_result);
    hr = IDispatch_Invoke(
        dispatch,
        dispid,
        &IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &params,
        out_result,
        NULL,
        NULL
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not read browser property");
        return false;
    }

    return true;
}

static bool er_ui_dispatch_invoke_method(
    IDispatch *dispatch,
    const wchar_t *name,
    VARIANT *args,
    UINT arg_count,
    VARIANT *out_result,
    ErError *error
) {
    OLECHAR *names[1];
    DISPID dispid;
    DISPPARAMS params;
    HRESULT hr;

    if (!dispatch || !name) {
        er_error_set(error, 0, 0, "Invalid COM dispatch method request");
        return false;
    }

    names[0] = (OLECHAR *) name;
    hr = IDispatch_GetIDsOfNames(dispatch, &IID_NULL, names, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not resolve browser method");
        return false;
    }

    memset(&params, 0, sizeof(params));
    params.rgvarg = args;
    params.cArgs = arg_count;

    if (out_result) {
        VariantInit(out_result);
    }

    hr = IDispatch_Invoke(
        dispatch,
        dispid,
        &IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &params,
        out_result,
        NULL,
        NULL
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not invoke browser method");
        return false;
    }

    return true;
}

static bool er_ui_webview_run_script_internal(ErUiPlatformWebView *webview, const char *script, ErError *error) {
    IDispatch *document_dispatch = NULL;
    IDispatch *window_dispatch = NULL;
    VARIANT window_result;
    VARIANT invoke_result;
    VARIANT args[2];
    wchar_t *wide_script = NULL;
    HRESULT hr;
    bool ok = false;

    VariantInit(&window_result);
    VariantInit(&invoke_result);
    VariantInit(&args[0]);
    VariantInit(&args[1]);

    if (webview && webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        return er_ui_webview2_run_script(webview, script, error);
    }

    if (!webview || !webview->browser) {
        er_error_set(error, 0, 0, "Webview backend is not initialized");
        return false;
    }

    hr = IWebBrowser2_get_Document(webview->browser, &document_dispatch);
    if (FAILED(hr) || !document_dispatch) {
        er_error_set(error, 0, 0, "Could not access browser document");
        goto cleanup;
    }

    if (!er_ui_dispatch_get_property(document_dispatch, L"parentWindow", &window_result, error)) {
        goto cleanup;
    }
    if (V_VT(&window_result) != VT_DISPATCH || !V_DISPATCH(&window_result)) {
        er_error_set(error, 0, 0, "Browser window does not expose script execution");
        goto cleanup;
    }

    window_dispatch = V_DISPATCH(&window_result);
    IDispatch_AddRef(window_dispatch);

    wide_script = er_ui_utf8_to_wide(script ? script : "");
    if (!wide_script) {
        er_error_set(error, 0, 0, "Could not convert script text");
        goto cleanup;
    }

    V_VT(&args[0]) = VT_BSTR;
    V_BSTR(&args[0]) = SysAllocString(L"JavaScript");
    V_VT(&args[1]) = VT_BSTR;
    V_BSTR(&args[1]) = SysAllocString(wide_script);
    free(wide_script);
    wide_script = NULL;

    if (!V_BSTR(&args[0]) || !V_BSTR(&args[1])) {
        er_error_set(error, 0, 0, "Out of memory while preparing script execution");
        goto cleanup;
    }

    if (!er_ui_dispatch_invoke_method(window_dispatch, L"execScript", args, 2, &invoke_result, error)) {
        goto cleanup;
    }

    VariantClear(&invoke_result);
    ok = true;

cleanup:
    if (wide_script) {
        free(wide_script);
    }
    VariantClear(&args[0]);
    VariantClear(&args[1]);
    VariantClear(&window_result);
    if (window_dispatch) {
        IDispatch_Release(window_dispatch);
    }
    if (document_dispatch) {
        IDispatch_Release(document_dispatch);
    }
    return ok;
}

static void er_ui_webview_destroy(ErUiPlatformWebView *webview) {
    if (!webview) {
        return;
    }

    if (webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        if (webview->modern_controller) {
            webview->modern_controller->lpVtbl->Close(webview->modern_controller);
        }
        if (webview->modern_webview) {
            webview->modern_webview->lpVtbl->Release(webview->modern_webview);
            webview->modern_webview = NULL;
        }
        if (webview->modern_controller) {
            webview->modern_controller->lpVtbl->Release(webview->modern_controller);
            webview->modern_controller = NULL;
        }
        if (webview->modern_environment) {
            webview->modern_environment->lpVtbl->Release(webview->modern_environment);
            webview->modern_environment = NULL;
        }
        if (webview->modern_loader) {
            FreeLibrary(webview->modern_loader);
            webview->modern_loader = NULL;
        }
        free(webview->modern_runtime_dir);
        webview->modern_runtime_dir = NULL;
        free(webview->modern_user_data_dir);
        webview->modern_user_data_dir = NULL;
        er_ui_webview_release_ref(webview);
        return;
    }

    er_ui_webview_add_ref(webview);

    if (webview->browser) {
        IWebBrowser2_Stop(webview->browser);
    }
    if (webview->ole_object) {
        IOleObject_SetClientSite(webview->ole_object, NULL);
        IOleObject_Close(webview->ole_object, OLECLOSE_NOSAVE);
    }
    if (webview->in_place_object) {
        IOleInPlaceObject_Release(webview->in_place_object);
        webview->in_place_object = NULL;
    }
    if (webview->browser) {
        IWebBrowser2_Release(webview->browser);
        webview->browser = NULL;
    }
    if (webview->ole_object) {
        IOleObject_Release(webview->ole_object);
        webview->ole_object = NULL;
    }
    if (webview->storage) {
        IStorage_Release(webview->storage);
        webview->storage = NULL;
    }
    if (webview->lock_bytes) {
        ILockBytes_Release(webview->lock_bytes);
        webview->lock_bytes = NULL;
    }
    er_ui_webview_release_ref(webview);
    er_ui_webview_release_ref(webview);
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_query_interface(IOleClientSite *iface, REFIID riid, void **out_object) {
    return er_ui_webview_query_interface(ER_UI_WEBVIEW_FROM_CLIENT_SITE(iface), riid, out_object);
}

static ULONG STDMETHODCALLTYPE er_ui_webview_client_add_ref(IOleClientSite *iface) {
    return er_ui_webview_add_ref(ER_UI_WEBVIEW_FROM_CLIENT_SITE(iface));
}

static ULONG STDMETHODCALLTYPE er_ui_webview_client_release(IOleClientSite *iface) {
    return er_ui_webview_release_ref(ER_UI_WEBVIEW_FROM_CLIENT_SITE(iface));
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_save_object(IOleClientSite *iface) {
    (void) iface;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_get_moniker(
    IOleClientSite *iface,
    DWORD assign,
    DWORD which_moniker,
    IMoniker **out_moniker
) {
    (void) iface;
    (void) assign;
    (void) which_moniker;
    if (out_moniker) {
        *out_moniker = NULL;
    }
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_get_container(IOleClientSite *iface, IOleContainer **out_container) {
    (void) iface;
    if (out_container) {
        *out_container = NULL;
    }
    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_show_object(IOleClientSite *iface) {
    (void) iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_on_show_window(IOleClientSite *iface, BOOL show) {
    (void) iface;
    (void) show;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_client_request_new_object_layout(IOleClientSite *iface) {
    (void) iface;
    return E_NOTIMPL;
}

static const IOleClientSiteVtbl er_ui_webview_client_site_vtbl = {
    er_ui_webview_client_query_interface,
    er_ui_webview_client_add_ref,
    er_ui_webview_client_release,
    er_ui_webview_client_save_object,
    er_ui_webview_client_get_moniker,
    er_ui_webview_client_get_container,
    er_ui_webview_client_show_object,
    er_ui_webview_client_on_show_window,
    er_ui_webview_client_request_new_object_layout
};

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_query_interface(IOleInPlaceSite *iface, REFIID riid, void **out_object) {
    return er_ui_webview_query_interface(ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface), riid, out_object);
}

static ULONG STDMETHODCALLTYPE er_ui_webview_inplace_add_ref(IOleInPlaceSite *iface) {
    return er_ui_webview_add_ref(ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface));
}

static ULONG STDMETHODCALLTYPE er_ui_webview_inplace_release(IOleInPlaceSite *iface) {
    return er_ui_webview_release_ref(ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface));
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_get_window(IOleInPlaceSite *iface, HWND *out_hwnd) {
    ErUiPlatformWebView *webview = ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface);

    if (!out_hwnd) {
        return E_POINTER;
    }

    *out_hwnd = webview->app ? webview->app->hwnd : NULL;
    return *out_hwnd ? S_OK : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_context_sensitive_help(IOleInPlaceSite *iface, BOOL enter_mode) {
    (void) iface;
    (void) enter_mode;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_can_activate(IOleInPlaceSite *iface) {
    (void) iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_on_activate(IOleInPlaceSite *iface) {
    (void) iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_on_ui_activate(IOleInPlaceSite *iface) {
    (void) iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_get_window_context(
    IOleInPlaceSite *iface,
    IOleInPlaceFrame **out_frame,
    IOleInPlaceUIWindow **out_doc,
    LPRECT out_pos_rect,
    LPRECT out_clip_rect,
    LPOLEINPLACEFRAMEINFO out_frame_info
) {
    ErUiPlatformWebView *webview = ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface);
    RECT rect;

    if (!out_frame || !out_pos_rect || !out_clip_rect || !out_frame_info) {
        return E_POINTER;
    }

    rect = er_ui_webview_rect(webview->node);
    *out_frame = &webview->in_place_frame;
    IOleInPlaceFrame_AddRef(*out_frame);
    if (out_doc) {
        *out_doc = NULL;
    }
    *out_pos_rect = rect;
    *out_clip_rect = rect;
    memset(out_frame_info, 0, sizeof(*out_frame_info));
    out_frame_info->cb = sizeof(*out_frame_info);
    out_frame_info->fMDIApp = FALSE;
    out_frame_info->hwndFrame = webview->app ? webview->app->hwnd : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_scroll(IOleInPlaceSite *iface, SIZE scroll_extent) {
    (void) iface;
    (void) scroll_extent;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_on_ui_deactivate(IOleInPlaceSite *iface, BOOL undoable) {
    (void) iface;
    (void) undoable;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_on_deactivate(IOleInPlaceSite *iface) {
    (void) iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_discard_undo(IOleInPlaceSite *iface) {
    (void) iface;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_deactivate_and_undo(IOleInPlaceSite *iface) {
    (void) iface;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_inplace_on_pos_rect_change(IOleInPlaceSite *iface, LPCRECT new_rect) {
    ErUiPlatformWebView *webview = ER_UI_WEBVIEW_FROM_INPLACE_SITE(iface);

    (void) new_rect;
    er_ui_webview_update_rect(webview);
    return S_OK;
}

static const IOleInPlaceSiteVtbl er_ui_webview_inplace_site_vtbl = {
    er_ui_webview_inplace_query_interface,
    er_ui_webview_inplace_add_ref,
    er_ui_webview_inplace_release,
    er_ui_webview_inplace_get_window,
    er_ui_webview_inplace_context_sensitive_help,
    er_ui_webview_inplace_can_activate,
    er_ui_webview_inplace_on_activate,
    er_ui_webview_inplace_on_ui_activate,
    er_ui_webview_inplace_get_window_context,
    er_ui_webview_inplace_scroll,
    er_ui_webview_inplace_on_ui_deactivate,
    er_ui_webview_inplace_on_deactivate,
    er_ui_webview_inplace_discard_undo,
    er_ui_webview_inplace_deactivate_and_undo,
    er_ui_webview_inplace_on_pos_rect_change
};

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_query_interface(IOleInPlaceFrame *iface, REFIID riid, void **out_object) {
    return er_ui_webview_query_interface(ER_UI_WEBVIEW_FROM_INPLACE_FRAME(iface), riid, out_object);
}

static ULONG STDMETHODCALLTYPE er_ui_webview_frame_add_ref(IOleInPlaceFrame *iface) {
    return er_ui_webview_add_ref(ER_UI_WEBVIEW_FROM_INPLACE_FRAME(iface));
}

static ULONG STDMETHODCALLTYPE er_ui_webview_frame_release(IOleInPlaceFrame *iface) {
    return er_ui_webview_release_ref(ER_UI_WEBVIEW_FROM_INPLACE_FRAME(iface));
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_get_window(IOleInPlaceFrame *iface, HWND *out_hwnd) {
    ErUiPlatformWebView *webview = ER_UI_WEBVIEW_FROM_INPLACE_FRAME(iface);

    if (!out_hwnd) {
        return E_POINTER;
    }

    *out_hwnd = webview->app ? webview->app->hwnd : NULL;
    return *out_hwnd ? S_OK : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_context_sensitive_help(IOleInPlaceFrame *iface, BOOL enter_mode) {
    (void) iface;
    (void) enter_mode;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_get_border(IOleInPlaceFrame *iface, LPRECT out_rect) {
    (void) iface;
    (void) out_rect;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_request_border_space(IOleInPlaceFrame *iface, LPCBORDERWIDTHS border_widths) {
    (void) iface;
    (void) border_widths;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_set_border_space(IOleInPlaceFrame *iface, LPCBORDERWIDTHS border_widths) {
    (void) iface;
    (void) border_widths;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_set_active_object(
    IOleInPlaceFrame *iface,
    IOleInPlaceActiveObject *active_object,
    LPCOLESTR object_name
) {
    (void) iface;
    (void) active_object;
    (void) object_name;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_insert_menus(
    IOleInPlaceFrame *iface,
    HMENU menu,
    LPOLEMENUGROUPWIDTHS widths
) {
    (void) iface;
    (void) menu;
    (void) widths;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_set_menu(
    IOleInPlaceFrame *iface,
    HMENU menu,
    HOLEMENU shared_menu,
    HWND active_object_hwnd
) {
    (void) iface;
    (void) menu;
    (void) shared_menu;
    (void) active_object_hwnd;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_remove_menus(IOleInPlaceFrame *iface, HMENU menu) {
    (void) iface;
    (void) menu;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_set_status_text(IOleInPlaceFrame *iface, LPCOLESTR status_text) {
    (void) iface;
    (void) status_text;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_enable_modeless(IOleInPlaceFrame *iface, BOOL enable) {
    (void) iface;
    (void) enable;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE er_ui_webview_frame_translate_accelerator(IOleInPlaceFrame *iface, LPMSG msg, WORD id) {
    (void) iface;
    (void) msg;
    (void) id;
    return E_NOTIMPL;
}

static const IOleInPlaceFrameVtbl er_ui_webview_inplace_frame_vtbl = {
    er_ui_webview_frame_query_interface,
    er_ui_webview_frame_add_ref,
    er_ui_webview_frame_release,
    er_ui_webview_frame_get_window,
    er_ui_webview_frame_context_sensitive_help,
    er_ui_webview_frame_get_border,
    er_ui_webview_frame_request_border_space,
    er_ui_webview_frame_set_border_space,
    er_ui_webview_frame_set_active_object,
    er_ui_webview_frame_insert_menus,
    er_ui_webview_frame_set_menu,
    er_ui_webview_frame_remove_menus,
    er_ui_webview_frame_set_status_text,
    er_ui_webview_frame_enable_modeless,
    er_ui_webview_frame_translate_accelerator
};

static ErUiPlatformWebView *er_ui_webview_create(ErUiApp *app, ErUiNode *node, ErError *error) {
    ErUiPlatformWebView *webview;
    RECT rect;
    HRESULT hr;

    er_error_clear(error);
    webview = er_ui_webview2_create(app, node, error);
    if (webview) {
        return webview;
    }

    er_error_clear(error);
    webview = (ErUiPlatformWebView *) calloc(1, sizeof(ErUiPlatformWebView));
    if (!webview) {
        er_error_set(error, 0, 0, "Out of memory while creating webview");
        return NULL;
    }

    webview->backend_kind = ER_UI_WEBVIEW_BACKEND_LEGACY;
    webview->client_site.lpVtbl = &er_ui_webview_client_site_vtbl;
    webview->in_place_site.lpVtbl = &er_ui_webview_inplace_site_vtbl;
    webview->in_place_frame.lpVtbl = &er_ui_webview_inplace_frame_vtbl;
    webview->ref_count = 1;
    webview->app = app;
    webview->node = node;

    hr = CreateILockBytesOnHGlobal(NULL, TRUE, &webview->lock_bytes);
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not allocate webview storage");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    hr = StgCreateDocfileOnILockBytes(
        webview->lock_bytes,
        STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
        0,
        &webview->storage
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not create webview document storage");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    hr = OleCreate(
        &CLSID_WebBrowser,
        &IID_IOleObject,
        OLERENDER_DRAW,
        NULL,
        &webview->client_site,
        webview->storage,
        (void **) &webview->ole_object
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not create embedded webview control");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    IOleObject_SetHostNames(webview->ole_object, L"Erire", L"Erire WebView");
    OleSetContainedObject((IUnknown *) webview->ole_object, TRUE);

    rect = er_ui_webview_rect(node);
    hr = IOleObject_DoVerb(
        webview->ole_object,
        OLEIVERB_SHOW,
        NULL,
        &webview->client_site,
        0,
        app->hwnd,
        &rect
    );
    if (FAILED(hr)) {
        er_error_set(error, 0, 0, "Could not activate embedded webview");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    hr = IOleObject_QueryInterface(webview->ole_object, &IID_IWebBrowser2, (void **) &webview->browser);
    if (FAILED(hr) || !webview->browser) {
        er_error_set(error, 0, 0, "Could not access browser automation interface");
        er_ui_webview_destroy(webview);
        return NULL;
    }
    IWebBrowser2_put_Silent(webview->browser, VARIANT_TRUE);

    hr = IOleObject_QueryInterface(webview->ole_object, &IID_IOleInPlaceObject, (void **) &webview->in_place_object);
    if (FAILED(hr) || !webview->in_place_object) {
        er_error_set(error, 0, 0, "Could not access webview window host");
        er_ui_webview_destroy(webview);
        return NULL;
    }

    er_ui_webview_update_rect(webview);
    if (!er_ui_webview_navigate(webview, node->text ? node->text : "about:blank", error)) {
        er_ui_webview_destroy(webview);
        return NULL;
    }
    er_ui_webview_set_visible(webview, node->visible);
    return webview;
}

static COLORREF er_ui_color(unsigned int rgb) {
    return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

static HFONT er_ui_create_font(int point_size) {
    HDC screen;
    int pixels;

    if (point_size <= 0) {
        point_size = 16;
    }

    screen = GetDC(NULL);
    pixels = -MulDiv(point_size, GetDeviceCaps(screen, LOGPIXELSY), 72);
    ReleaseDC(NULL, screen);

    return CreateFontA(
        pixels,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI Variable Text"
    );
}

static int er_ui_max_int(int left, int right) {
    return left > right ? left : right;
}

static int er_ui_scale_layout_value(int base_value, double scale, int minimum_value) {
    int scaled;

    if (base_value <= 0) {
        return base_value;
    }

    scaled = (int) ((base_value * scale) + 0.5);
    if (scaled < minimum_value) {
        scaled = minimum_value;
    }
    return scaled;
}

static void er_ui_refresh_node_font(ErUiNode *node) {
    HFONT new_font;

    if (!node) {
        return;
    }

    new_font = er_ui_create_font(node->font_size);
    if (!new_font) {
        return;
    }

    if (node->font) {
        DeleteObject(node->font);
    }
    node->font = new_font;

    if (node->hwnd) {
        SendMessageA(node->hwnd, WM_SETFONT, (WPARAM) node->font, TRUE);
    }
}

static void er_ui_capture_node_layout_base(ErUiNode *node) {
    if (!node) {
        return;
    }

    node->base_x = node->x;
    node->base_y = node->y;
    node->base_w = node->w;
    node->base_h = node->h;
    node->base_font_size = node->font_size;
    node->base_icon_size = node->icon_size;
    node->base_padding = node->padding;
    node->base_border_width = node->border_width;
    node->base_border_radius = node->border_radius;
    node->base_shadow_size = node->shadow_size;
}

static void er_ui_rescale_node_layout(ErUiApp *app, ErUiNode *node) {
    double scale_x;
    double scale_y;
    double uniform_scale;

    if (!app || !node || app->design_client_w <= 0 || app->design_client_h <= 0) {
        return;
    }

    scale_x = (double) er_ui_max_int(1, app->client_w) / (double) app->design_client_w;
    scale_y = (double) er_ui_max_int(1, app->client_h) / (double) app->design_client_h;
    uniform_scale = scale_x < scale_y ? scale_x : scale_y;

    node->x = er_ui_scale_layout_value(node->base_x, scale_x, 0);
    node->y = er_ui_scale_layout_value(node->base_y, scale_y, 0);
    node->w = er_ui_scale_layout_value(node->base_w, scale_x, 1);
    node->h = er_ui_scale_layout_value(node->base_h, scale_y, 1);
    node->font_size = er_ui_scale_layout_value(node->base_font_size, uniform_scale, 9);
    node->icon_size = er_ui_scale_layout_value(node->base_icon_size, uniform_scale, node->base_icon_size > 0 ? 10 : 0);
    node->padding = er_ui_scale_layout_value(node->base_padding, uniform_scale, node->base_padding > 0 ? 1 : 0);
    node->border_width = er_ui_scale_layout_value(node->base_border_width, uniform_scale, node->base_border_width > 0 ? 1 : 0);
    node->border_radius = er_ui_scale_layout_value(node->base_border_radius, uniform_scale, node->base_border_radius > 0 ? 1 : 0);
    node->shadow_size = er_ui_scale_layout_value(node->base_shadow_size, uniform_scale, node->base_shadow_size > 0 ? 1 : 0);

    er_ui_refresh_node_font(node);
    if (node->animation_active) {
        er_ui_animation_capture_origin(node);
    }
}

static int er_ui_clamp_channel(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static unsigned int er_ui_adjust_rgb(unsigned int rgb, int delta) {
    int red = er_ui_clamp_channel((int) ((rgb >> 16) & 0xffu) + delta);
    int green = er_ui_clamp_channel((int) ((rgb >> 8) & 0xffu) + delta);
    int blue = er_ui_clamp_channel((int) (rgb & 0xffu) + delta);
    return ((unsigned int) red << 16) | ((unsigned int) green << 8) | (unsigned int) blue;
}

static unsigned int er_ui_mix_rgb(unsigned int start_rgb, unsigned int end_rgb, int numerator, int denominator) {
    int start_red = (int) ((start_rgb >> 16) & 0xffu);
    int start_green = (int) ((start_rgb >> 8) & 0xffu);
    int start_blue = (int) (start_rgb & 0xffu);
    int end_red = (int) ((end_rgb >> 16) & 0xffu);
    int end_green = (int) ((end_rgb >> 8) & 0xffu);
    int end_blue = (int) (end_rgb & 0xffu);
    int red;
    int green;
    int blue;

    if (denominator <= 0) {
        return start_rgb;
    }

    numerator = numerator < 0 ? 0 : (numerator > denominator ? denominator : numerator);
    red = start_red + (((end_red - start_red) * numerator) / denominator);
    green = start_green + (((end_green - start_green) * numerator) / denominator);
    blue = start_blue + (((end_blue - start_blue) * numerator) / denominator);
    return ((unsigned int) red << 16) | ((unsigned int) green << 8) | (unsigned int) blue;
}

static unsigned int er_ui_blend_rgb(unsigned int base_rgb, unsigned int overlay_rgb, int alpha) {
    int base_red = (int) ((base_rgb >> 16) & 0xffu);
    int base_green = (int) ((base_rgb >> 8) & 0xffu);
    int base_blue = (int) (base_rgb & 0xffu);
    int overlay_red = (int) ((overlay_rgb >> 16) & 0xffu);
    int overlay_green = (int) ((overlay_rgb >> 8) & 0xffu);
    int overlay_blue = (int) (overlay_rgb & 0xffu);
    int red;
    int green;
    int blue;

    alpha = er_ui_clamp_channel(alpha);
    red = ((base_red * (255 - alpha)) + (overlay_red * alpha) + 127) / 255;
    green = ((base_green * (255 - alpha)) + (overlay_green * alpha) + 127) / 255;
    blue = ((base_blue * (255 - alpha)) + (overlay_blue * alpha) + 127) / 255;
    return ((unsigned int) red << 16) | ((unsigned int) green << 8) | (unsigned int) blue;
}

static void er_ui_theme_init_dark(ErUiTheme *theme) {
    memset(theme, 0, sizeof(*theme));
    theme->window_bg_rgb = 0x0f172a;
    theme->text_rgb = 0xe5e7eb;
    theme->muted_text_rgb = 0x94a3b8;
    theme->surface_rgb = 0x111827;
    theme->surface_hover_rgb = 0x172033;
    theme->surface_active_rgb = 0x0f1727;
    theme->surface_border_rgb = 0x334155;
    theme->accent_rgb = 0x2563eb;
    theme->accent_hover_rgb = 0x3b82f6;
    theme->accent_active_rgb = 0x1d4ed8;
    theme->accent_text_rgb = 0xf8fafc;
    theme->input_bg_rgb = 0x111827;
    theme->input_border_rgb = 0x475569;
    theme->focus_rgb = 0x60a5fa;
    theme->shadow_rgb = 0x020617;
    theme->highlight_rgb = 0xcbd5e1;
}

static RECT er_ui_node_rect(const ErUiNode *node) {
    RECT rect;

    rect.left = node->x;
    rect.top = node->y;
    rect.right = node->x + node->w;
    rect.bottom = node->y + node->h;
    return rect;
}

static void er_ui_inset_rect(RECT *rect, int horizontal, int vertical) {
    rect->left += horizontal;
    rect->right -= horizontal;
    rect->top += vertical;
    rect->bottom -= vertical;

    if (rect->right < rect->left) {
        rect->right = rect->left;
    }
    if (rect->bottom < rect->top) {
        rect->bottom = rect->top;
    }
}

static bool er_ui_rect_contains(const RECT *rect, int x, int y) {
    return x >= rect->left && x < rect->right && y >= rect->top && y < rect->bottom;
}

static char *er_ui_dup_window_text(HWND hwnd) {
    int length;
    char *buffer;

    if (!hwnd) {
        return NULL;
    }

    length = GetWindowTextLengthA(hwnd);
    buffer = (char *) malloc((size_t) length + 1);
    if (!buffer) {
        return NULL;
    }

    GetWindowTextA(hwnd, buffer, length + 1);
    return buffer;
}

static ErUiNode *er_ui_find_node_by_hwnd(ErUiApp *app, HWND hwnd) {
    size_t i;

    for (i = 0; i < app->node_count; ++i) {
        if (app->nodes[i].hwnd == hwnd) {
            return &app->nodes[i];
        }
    }
    return NULL;
}

static ErUiNode *er_ui_find_node_by_id(ErUiApp *app, const char *id) {
    size_t i;

    if (!id) {
        return NULL;
    }
    for (i = 0; i < app->node_count; ++i) {
        if (app->nodes[i].id && strcmp(app->nodes[i].id, id) == 0) {
            return &app->nodes[i];
        }
    }
    return NULL;
}

static ErUiNode *er_ui_find_webview_node_by_id(ErUiApp *app, const char *id) {
    ErUiNode *node = er_ui_find_node_by_id(app, id);

    if (!node || node->kind != ER_UI_NODE_WEBVIEW || !node->webview) {
        return NULL;
    }

    if (node->webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2 && node->webview->modern_webview) {
        return node;
    }

    if (node->webview->backend_kind == ER_UI_WEBVIEW_BACKEND_LEGACY && node->webview->browser) {
        return node;
    }

    return NULL;
}

static ErUiNode *er_ui_find_node_by_control_id(ErUiApp *app, int control_id) {
    size_t i;

    if (!app || control_id == 0) {
        return NULL;
    }
    for (i = 0; i < app->node_count; ++i) {
        if (app->nodes[i].control_id == control_id) {
            return &app->nodes[i];
        }
    }
    return NULL;
}

static ErUiTimer *er_ui_find_timer_by_id(ErUiApp *app, unsigned int timer_id) {
    size_t i;

    if (!app || timer_id == 0) {
        return NULL;
    }

    for (i = 0; i < app->timer_count; ++i) {
        if (app->timers[i].timer_id == timer_id) {
            return &app->timers[i];
        }
    }

    return NULL;
}

static ErUiTimer *er_ui_find_timer_by_os_id(ErUiApp *app, unsigned int os_timer_id) {
    size_t i;

    if (!app || os_timer_id == 0) {
        return NULL;
    }

    for (i = 0; i < app->timer_count; ++i) {
        if (app->timers[i].os_timer_id == os_timer_id) {
            return &app->timers[i];
        }
    }

    return NULL;
}

static bool er_ui_grow_timers(ErUiApp *app, ErError *error) {
    size_t new_capacity;
    ErUiTimer *new_items;

    if (!app) {
        er_error_set(error, 0, 0, "UI timer storage requires an app");
        return false;
    }

    if (app->timer_count < app->timer_capacity) {
        return true;
    }

    new_capacity = app->timer_capacity == 0 ? 8 : app->timer_capacity * 2;
    new_items = (ErUiTimer *) realloc(app->timers, new_capacity * sizeof(ErUiTimer));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing UI timer list");
        return false;
    }

    app->timers = new_items;
    app->timer_capacity = new_capacity;
    return true;
}

static bool er_ui_grow_shortcuts(ErUiApp *app, ErError *error) {
    size_t new_capacity;
    ErUiShortcut *new_items;

    if (!app) {
        er_error_set(error, 0, 0, "UI shortcut storage requires an app");
        return false;
    }

    if (app->shortcut_count < app->shortcut_capacity) {
        return true;
    }

    new_capacity = app->shortcut_capacity == 0 ? 8 : app->shortcut_capacity * 2;
    new_items = (ErUiShortcut *) realloc(app->shortcuts, new_capacity * sizeof(ErUiShortcut));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing UI shortcut list");
        return false;
    }

    app->shortcuts = new_items;
    app->shortcut_capacity = new_capacity;
    return true;
}

static char *er_ui_trim_ascii(char *text) {
    char *end;

    if (!text) {
        return text;
    }

    while (*text && isspace((unsigned char) *text)) {
        text++;
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char) end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

static bool er_ui_ascii_equals(const char *left, const char *right) {
    unsigned char lc;
    unsigned char rc;

    if (!left || !right) {
        return false;
    }

    while (*left && *right) {
        lc = (unsigned char) *left++;
        rc = (unsigned char) *right++;
        if (toupper(lc) != toupper(rc)) {
            return false;
        }
    }
    return *left == '\0' && *right == '\0';
}

static unsigned int er_ui_shortcut_key_from_name(const char *name) {
    size_t len;

    if (!name) {
        return 0;
    }

    len = strlen(name);
    if (len == 1) {
        unsigned char c = (unsigned char) name[0];
        if (isalpha(c)) {
            return (unsigned int) toupper(c);
        }
        if (isdigit(c)) {
            return (unsigned int) c;
        }
    }

    if ((name[0] == 'F' || name[0] == 'f') && len >= 2 && len <= 3) {
        int number = atoi(name + 1);
        if (number >= 1 && number <= 12) {
            return (unsigned int) (VK_F1 + (number - 1));
        }
    }

    if (er_ui_ascii_equals(name, "Enter") || er_ui_ascii_equals(name, "Return")) return VK_RETURN;
    if (er_ui_ascii_equals(name, "Space")) return VK_SPACE;
    if (er_ui_ascii_equals(name, "Tab")) return VK_TAB;
    if (er_ui_ascii_equals(name, "Esc") || er_ui_ascii_equals(name, "Escape")) return VK_ESCAPE;
    if (er_ui_ascii_equals(name, "Delete") || er_ui_ascii_equals(name, "Del")) return VK_DELETE;
    if (er_ui_ascii_equals(name, "Backspace")) return VK_BACK;
    if (er_ui_ascii_equals(name, "Left")) return VK_LEFT;
    if (er_ui_ascii_equals(name, "Right")) return VK_RIGHT;
    if (er_ui_ascii_equals(name, "Up")) return VK_UP;
    if (er_ui_ascii_equals(name, "Down")) return VK_DOWN;
    return 0;
}

static bool er_ui_parse_shortcut_combo(const char *combo, ErUiShortcut *out_shortcut, ErError *error) {
    char *copy;
    char *token;
    bool has_key = false;

    if (!combo || combo[0] == '\0') {
        er_error_set(error, 0, 0, "Shortcut combo cannot be empty");
        return false;
    }
    if (!out_shortcut) {
        er_error_set(error, 0, 0, "Shortcut target storage is missing");
        return false;
    }

    memset(out_shortcut, 0, sizeof(*out_shortcut));
    copy = er_ui_dup(combo);
    if (!copy) {
        er_error_set(error, 0, 0, "Out of memory while parsing shortcut");
        return false;
    }

    token = strtok(copy, "+");
    while (token) {
        char *part = er_ui_trim_ascii(token);
        unsigned int key;

        if (part[0] == '\0') {
            free(copy);
            er_error_set(error, 0, 0, "Shortcut combo contains an empty segment");
            return false;
        }

        if (er_ui_ascii_equals(part, "Ctrl") || er_ui_ascii_equals(part, "Control")) {
            out_shortcut->ctrl = true;
        } else if (er_ui_ascii_equals(part, "Alt")) {
            out_shortcut->alt = true;
        } else if (er_ui_ascii_equals(part, "Shift")) {
            out_shortcut->shift = true;
        } else {
            key = er_ui_shortcut_key_from_name(part);
            if (key == 0) {
                free(copy);
                er_error_set(error, 0, 0, "Unsupported shortcut key '%s'", part);
                return false;
            }
            if (has_key) {
                free(copy);
                er_error_set(error, 0, 0, "Shortcut combo can only contain one key");
                return false;
            }
            out_shortcut->virtual_key = key;
            has_key = true;
        }

        token = strtok(NULL, "+");
    }

    free(copy);
    if (!has_key) {
        er_error_set(error, 0, 0, "Shortcut combo must include a key");
        return false;
    }
    return true;
}

static void er_ui_sync_input_text(ErUiNode *node) {
    char *text;

    if (!node || node->kind != ER_UI_NODE_INPUT || !node->hwnd) {
        return;
    }

    text = er_ui_dup_window_text(node->hwnd);
    if (!text) {
        return;
    }

    free(node->text);
    node->text = text;
}

static bool er_ui_node_is_on_current_page(const ErUiApp *app, const ErUiNode *node) {
    if (!app || !node || !app->current_page || !node->page) {
        return true;
    }
    return strcmp(app->current_page, node->page) == 0;
}

static void er_ui_update_node_visibility(ErUiApp *app, ErUiNode *node) {
    if (!app || !node) {
        return;
    }

    node->visible = er_ui_node_is_on_current_page(app, node);
    if (node->webview) {
        er_ui_webview_set_visible(node->webview, node->visible);
        er_ui_webview_update_rect(node->webview);
    }
    if (node->hwnd) {
        ShowWindow(node->hwnd, node->visible ? SW_SHOW : SW_HIDE);
    }
}

static void er_ui_invalidate_node(ErUiApp *app, ErUiNode *node) {
    RECT rect;
    int inflate;

    if (!app || !app->hwnd || !node) {
        return;
    }

    rect = er_ui_node_rect(node);
    inflate = er_ui_max_int(4, node->shadow_size > 0 ? node->shadow_size + 4 : 4);
    InflateRect(&rect, inflate, inflate);
    InvalidateRect(app->hwnd, &rect, FALSE);
}

static void er_ui_set_hovered_control(ErUiApp *app, int control_id) {
    ErUiNode *old_node;
    ErUiNode *new_node;

    if (!app || app->hovered_control_id == control_id) {
        return;
    }

    old_node = er_ui_find_node_by_control_id(app, app->hovered_control_id);
    if (old_node) {
        old_node->hovered = false;
        er_ui_invalidate_node(app, old_node);
    }

    app->hovered_control_id = control_id;
    new_node = er_ui_find_node_by_control_id(app, control_id);
    if (new_node) {
        new_node->hovered = true;
        er_ui_invalidate_node(app, new_node);
    }
}

static void er_ui_set_pressed_control(ErUiApp *app, int control_id) {
    ErUiNode *old_node;
    ErUiNode *new_node;

    if (!app || app->pressed_control_id == control_id) {
        return;
    }

    old_node = er_ui_find_node_by_control_id(app, app->pressed_control_id);
    if (old_node) {
        old_node->pressed = false;
        er_ui_invalidate_node(app, old_node);
    }

    app->pressed_control_id = control_id;
    new_node = er_ui_find_node_by_control_id(app, control_id);
    if (new_node) {
        new_node->pressed = true;
        er_ui_invalidate_node(app, new_node);
    }
}

static void er_ui_set_focused_control(ErUiApp *app, int control_id) {
    ErUiNode *old_node;
    ErUiNode *new_node;

    if (!app || app->focused_control_id == control_id) {
        return;
    }

    old_node = er_ui_find_node_by_control_id(app, app->focused_control_id);
    if (old_node) {
        old_node->focused = false;
        er_ui_invalidate_node(app, old_node);
    }

    app->focused_control_id = control_id;
    new_node = er_ui_find_node_by_control_id(app, control_id);
    if (new_node) {
        new_node->focused = true;
        er_ui_invalidate_node(app, new_node);
    }
}

static void er_ui_begin_mouse_tracking(ErUiApp *app) {
    TRACKMOUSEEVENT event;

    if (!app || app->tracking_mouse) {
        return;
    }

    memset(&event, 0, sizeof(event));
    event.cbSize = sizeof(event);
    event.dwFlags = TME_LEAVE;
    event.hwndTrack = app->hwnd;
    if (TrackMouseEvent(&event)) {
        app->tracking_mouse = true;
    }
}

static bool er_ui_node_accepts_pointer(const ErUiNode *node) {
    return node &&
           node->visible &&
           node->custom_drawn &&
           (node->kind == ER_UI_NODE_INPUT || node->kind == ER_UI_NODE_BUTTON || node->on_click != NULL);
}

static ErUiNode *er_ui_hit_test(ErUiApp *app, int x, int y) {
    size_t i;

    if (!app) {
        return NULL;
    }

    for (i = app->node_count; i > 0; --i) {
        ErUiNode *node = &app->nodes[i - 1];
        RECT rect;

        if (!er_ui_node_accepts_pointer(node)) {
            continue;
        }

        rect = er_ui_node_rect(node);
        if (er_ui_rect_contains(&rect, x, y)) {
            return node;
        }
    }

    return NULL;
}

static unsigned int er_ui_node_base_bg(const ErUiApp *app, const ErUiNode *node) {
    if (node->has_bg_color) {
        return node->bg_color_rgb;
    }

    switch (node->kind) {
        case ER_UI_NODE_BUTTON:
            return app->theme.accent_rgb;
        case ER_UI_NODE_INPUT:
            return app->theme.input_bg_rgb;
        case ER_UI_NODE_IMAGE:
        case ER_UI_NODE_WEBVIEW:
        case ER_UI_NODE_BOX:
            return app->theme.surface_rgb;
        case ER_UI_NODE_TEXT:
        default:
            return app->background_rgb;
    }
}

static unsigned int er_ui_node_effective_bg(const ErUiApp *app, const ErUiNode *node) {
    unsigned int bg = er_ui_node_base_bg(app, node);

    if (node->kind == ER_UI_NODE_BUTTON) {
        if (node->pressed) {
            return er_ui_adjust_rgb(bg, -18);
        }
        if (node->hovered) {
            return er_ui_adjust_rgb(bg, 10);
        }
    } else if (node->kind == ER_UI_NODE_INPUT) {
        if (node->hovered && !node->focused) {
            return er_ui_adjust_rgb(bg, 6);
        }
    } else if ((node->kind == ER_UI_NODE_IMAGE || node->kind == ER_UI_NODE_WEBVIEW || node->kind == ER_UI_NODE_BOX) && node->hovered) {
        return er_ui_adjust_rgb(bg, 4);
    }

    return bg;
}

static unsigned int er_ui_node_effective_bg_alt(const ErUiApp *app, const ErUiNode *node, unsigned int base_bg) {
    (void) app;

    if (node->has_bg_alt_color) {
        return node->bg_alt_color_rgb;
    }

    switch (node->kind) {
        case ER_UI_NODE_BUTTON:
            return base_bg;
        case ER_UI_NODE_INPUT:
            return base_bg;
        case ER_UI_NODE_IMAGE:
        case ER_UI_NODE_WEBVIEW:
            return base_bg;
        case ER_UI_NODE_BOX:
            return base_bg;
        case ER_UI_NODE_TEXT:
        default:
            return base_bg;
    }
}

static unsigned int er_ui_node_effective_shadow(const ErUiApp *app, const ErUiNode *node) {
    if (node->has_shadow_color) {
        return node->shadow_color_rgb;
    }
    return app->theme.shadow_rgb;
}

static unsigned int er_ui_node_effective_text(const ErUiApp *app, const ErUiNode *node) {
    if (node->has_text_color) {
        return node->text_color_rgb;
    }

    switch (node->kind) {
        case ER_UI_NODE_BUTTON:
            return app->theme.accent_text_rgb;
        case ER_UI_NODE_IMAGE:
        case ER_UI_NODE_WEBVIEW:
            return app->theme.muted_text_rgb;
        default:
            return app->theme.text_rgb;
    }
}

static unsigned int er_ui_node_effective_border(const ErUiApp *app, const ErUiNode *node) {
    unsigned int border;

    if (node->focused) {
        return app->theme.focus_rgb;
    }

    if (node->has_border_color) {
        border = node->border_color_rgb;
    } else {
        switch (node->kind) {
            case ER_UI_NODE_BUTTON:
                border = app->theme.accent_rgb;
                break;
            case ER_UI_NODE_INPUT:
                border = app->theme.input_border_rgb;
                break;
            default:
                border = app->theme.surface_border_rgb;
                break;
        }
    }

    if (node->hovered && node->kind != ER_UI_NODE_TEXT) {
        border = er_ui_adjust_rgb(border, 10);
    }

    return border;
}

static UINT er_ui_alignment_flags(ErUiTextAlign align) {
    switch (align) {
        case ER_UI_TEXT_ALIGN_CENTER:
            return DT_CENTER;
        case ER_UI_TEXT_ALIGN_END:
            return DT_RIGHT;
        case ER_UI_TEXT_ALIGN_START:
        case ER_UI_TEXT_ALIGN_DEFAULT:
        default:
            return DT_LEFT;
    }
}

static void er_ui_draw_round_surface(
    HDC hdc,
    const RECT *rect,
    bool fill,
    unsigned int fill_rgb,
    int border_width,
    unsigned int border_rgb,
    int radius
);

static void er_ui_fill_vertical_gradient(HDC hdc, const RECT *rect, unsigned int top_rgb, unsigned int bottom_rgb, int radius) {
    HRGN clip_region;
    HBRUSH brush;
    int saved_dc;
    int height;
    int y;

    if (!rect || rect->right <= rect->left || rect->bottom <= rect->top) {
        return;
    }

    clip_region = radius > 0
        ? CreateRoundRectRgn(rect->left, rect->top, rect->right + 1, rect->bottom + 1, radius * 2, radius * 2)
        : CreateRectRgn(rect->left, rect->top, rect->right, rect->bottom);
    if (!clip_region) {
        return;
    }

    saved_dc = SaveDC(hdc);
    SelectClipRgn(hdc, clip_region);
    brush = (HBRUSH) GetStockObject(DC_BRUSH);
    height = er_ui_max_int(1, rect->bottom - rect->top);

    for (y = rect->top; y < rect->bottom; ++y) {
        RECT line = { rect->left, y, rect->right, y + 1 };
        int step = height > 1 ? y - rect->top : 0;
        int max_step = height > 1 ? height - 1 : 1;
        unsigned int mixed = er_ui_mix_rgb(top_rgb, bottom_rgb, step, max_step);
        SetDCBrushColor(hdc, er_ui_color(mixed));
        FillRect(hdc, &line, brush);
    }

    RestoreDC(hdc, saved_dc);
    DeleteObject(clip_region);
}

static void er_ui_draw_shadow(
    HDC hdc,
    const ErUiApp *app,
    const RECT *rect,
    int radius,
    int shadow_size,
    unsigned int shadow_rgb
) {
    int layer;

    if (!app || !rect || shadow_size <= 0) {
        return;
    }

    for (layer = shadow_size; layer >= 1; --layer) {
        RECT shadow_rect = *rect;
        int blur = layer / 2;
        int alpha = 8 + ((shadow_size - layer + 1) * 32) / er_ui_max_int(1, shadow_size);
        unsigned int blended = er_ui_blend_rgb(app->background_rgb, shadow_rgb, alpha);

        InflateRect(&shadow_rect, blur, blur);
        OffsetRect(&shadow_rect, 0, layer);
        er_ui_draw_round_surface(
            hdc,
            &shadow_rect,
            true,
            blended,
            0,
            blended,
            radius > 0 ? radius + blur : 0
        );
    }
}

static void er_ui_draw_surface_highlight(HDC hdc, const RECT *rect, int radius, unsigned int bg_rgb, unsigned int highlight_rgb) {
    RECT highlight = *rect;

    highlight.left += 1;
    highlight.right -= 1;
    highlight.top += 1;
    highlight.bottom = er_ui_max_int(highlight.top + 2, highlight.top + ((rect->bottom - rect->top) / 4));
    if (highlight.bottom <= highlight.top || highlight.right <= highlight.left) {
        return;
    }

    er_ui_fill_vertical_gradient(
        hdc,
        &highlight,
        er_ui_blend_rgb(bg_rgb, highlight_rgb, 24),
        er_ui_blend_rgb(bg_rgb, highlight_rgb, 4),
        radius > 2 ? radius - 2 : 0
    );
}

static void er_ui_draw_box_style_overlay(
    HDC hdc,
    const ErUiNode *node,
    const RECT *rect,
    unsigned int bg_rgb,
    unsigned int accent_rgb
) {
    if (!node || node->kind != ER_UI_NODE_BOX) {
        return;
    }

    if (node->surface_style == ER_UI_SURFACE_STYLE_CARD) {
        RECT strip = *rect;
        strip.left += 1;
        strip.right -= 1;
        strip.top += 1;
        strip.bottom = er_ui_max_int(strip.top + 4, strip.top + ((rect->bottom - rect->top) / 7));
        er_ui_fill_vertical_gradient(
            hdc,
            &strip,
            er_ui_blend_rgb(bg_rgb, accent_rgb, 54),
            er_ui_blend_rgb(bg_rgb, accent_rgb, 10),
            node->border_radius > 2 ? node->border_radius - 2 : 0
        );
        return;
    }

    if (node->surface_style == ER_UI_SURFACE_STYLE_PANEL) {
        RECT rail = *rect;
        rail.left += 6;
        rail.right = er_ui_max_int(rail.left + 4, rail.left + ((rect->right - rect->left) / 36));
        rail.top += 10;
        rail.bottom -= 10;
        if (rail.right > rail.left && rail.bottom > rail.top) {
            er_ui_draw_round_surface(
                hdc,
                &rail,
                true,
                er_ui_blend_rgb(bg_rgb, accent_rgb, 72),
                0,
                accent_rgb,
                3
            );
        }
    }
}

static void er_ui_draw_round_surface(
    HDC hdc,
    const RECT *rect,
    bool fill,
    unsigned int fill_rgb,
    int border_width,
    unsigned int border_rgb,
    int radius
) {
    HBRUSH brush = fill ? CreateSolidBrush(er_ui_color(fill_rgb)) : NULL;
    HPEN pen = border_width > 0 ? CreatePen(PS_SOLID, border_width, er_ui_color(border_rgb)) : NULL;
    HGDIOBJ old_brush = SelectObject(hdc, fill ? brush : GetStockObject(HOLLOW_BRUSH));
    HGDIOBJ old_pen = SelectObject(hdc, border_width > 0 ? pen : GetStockObject(NULL_PEN));

    if (radius > 0) {
        RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom, radius * 2, radius * 2);
    } else {
        Rectangle(hdc, rect->left, rect->top, rect->right, rect->bottom);
    }

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    if (brush) {
        DeleteObject(brush);
    }
    if (pen) {
        DeleteObject(pen);
    }
}

static void er_ui_draw_focus_ring(HDC hdc, const RECT *rect, int radius, unsigned int rgb) {
    HPEN pen = CreatePen(PS_DOT, 1, er_ui_color(rgb));
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    RECT ring = *rect;

    InflateRect(&ring, -2, -2);

    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    if (radius > 0) {
        RoundRect(hdc, ring.left, ring.top, ring.right, ring.bottom, er_ui_max_int(4, (radius - 2) * 2), er_ui_max_int(4, (radius - 2) * 2));
    } else {
        Rectangle(hdc, ring.left, ring.top, ring.right, ring.bottom);
    }
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void er_ui_draw_text_in_rect(HDC hdc, const ErUiNode *node, const RECT *rect, unsigned int text_rgb, bool single_line, bool wrap) {
    wchar_t *wide_text;
    HGDIOBJ old_font;
    UINT flags = DT_NOPREFIX | DT_EDITCONTROL | er_ui_alignment_flags(node->text_align);

    if (!node->text || node->text[0] == '\0') {
        return;
    }

    wide_text = er_ui_utf8_to_wide(node->text);
    if (!wide_text) {
        return;
    }

    old_font = SelectObject(hdc, node->font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, er_ui_color(text_rgb));

    if (single_line) {
        flags |= DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
    } else if (wrap) {
        flags |= DT_WORDBREAK;
    } else {
        flags |= DT_TOP;
    }

    DrawTextW(hdc, wide_text, -1, (RECT *) rect, flags);

    SelectObject(hdc, old_font);
    free(wide_text);
}

static int er_ui_measure_text_height(const ErUiNode *node, int width, bool single_line, bool wrap) {
    HDC screen;
    HFONT font;
    HGDIOBJ old_font;
    wchar_t *wide_text;
    RECT rect;
    UINT flags = DT_NOPREFIX | DT_EDITCONTROL | er_ui_alignment_flags(node->text_align) | DT_CALCRECT;
    int height = 0;

    if (!node || !node->text || node->text[0] == '\0') {
        return 0;
    }

    wide_text = er_ui_utf8_to_wide(node->text);
    if (!wide_text) {
        return 0;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = er_ui_max_int(1, width);
    rect.bottom = 0;

    if (single_line) {
        flags |= DT_SINGLELINE;
    } else if (wrap) {
        flags |= DT_WORDBREAK;
    } else {
        flags |= DT_TOP;
    }

    screen = GetDC(NULL);
    font = er_ui_create_font(node->font_size);
    old_font = SelectObject(screen, font);
    DrawTextW(screen, wide_text, -1, &rect, flags);
    height = er_ui_max_int(0, rect.bottom - rect.top);
    SelectObject(screen, old_font);
    DeleteObject(font);
    ReleaseDC(NULL, screen);
    free(wide_text);
    return height;
}

static int er_ui_content_vertical_padding(const ErUiNode *node) {
    if (!node) {
        return 0;
    }

    switch (node->kind) {
        case ER_UI_NODE_BUTTON:
            return er_ui_max_int(8, node->padding / 2);
        case ER_UI_NODE_INPUT:
            if (node->multiline) {
                return er_ui_max_int(8, node->padding - 4);
            }
            return er_ui_max_int(7, node->padding - 5);
        default:
            return node->padding;
    }
}

static void er_ui_position_input_control(ErUiNode *node) {
    int inner_x;
    int inner_y;
    int inner_w;
    int inner_h;
    int horizontal_margin;
    RECT format_rect;

    if (!node || !node->hwnd) {
        return;
    }

    inner_x = node->x + node->padding;
    inner_y = node->y + er_ui_content_vertical_padding(node);
    inner_w = er_ui_max_int(32, node->w - (node->padding * 2));
    inner_h = er_ui_max_int(24, node->h - (er_ui_content_vertical_padding(node) * 2));
    MoveWindow(node->hwnd, inner_x, inner_y, inner_w, inner_h, TRUE);

    horizontal_margin = node->multiline
        ? er_ui_max_int(10, node->padding / 2)
        : er_ui_max_int(8, node->padding / 2);
    SendMessageA(
        node->hwnd,
        EM_SETMARGINS,
        EC_LEFTMARGIN | EC_RIGHTMARGIN,
        MAKELPARAM(horizontal_margin, horizontal_margin)
    );

    if (node->multiline) {
        GetClientRect(node->hwnd, &format_rect);
        format_rect.left += horizontal_margin;
        format_rect.top += 8;
        format_rect.right -= horizontal_margin;
        format_rect.bottom -= 8;
        if (format_rect.right < format_rect.left + 8) {
            format_rect.right = format_rect.left + 8;
        }
        if (format_rect.bottom < format_rect.top + 8) {
            format_rect.bottom = format_rect.top + 8;
        }
        SendMessageA(node->hwnd, EM_SETRECTNP, 0, (LPARAM) &format_rect);
        InvalidateRect(node->hwnd, NULL, TRUE);
    }
}

static void er_ui_position_webview_control(ErUiNode *node) {
    if (!node || !node->webview) {
        return;
    }
    er_ui_webview_update_rect(node->webview);
}

static unsigned long er_ui_tick_now(void) {
    return (unsigned long) GetTickCount();
}

static double er_ui_abs_double(double value) {
    return value < 0.0 ? -value : value;
}

static double er_ui_fractional(double value) {
    long whole = (long) value;
    double fraction = value - (double) whole;

    if (fraction < 0.0) {
        fraction += 1.0;
    }
    return fraction;
}

static double er_ui_triangle_wave(double progress) {
    progress = er_ui_fractional(progress);
    if (progress < 0.25) {
        return progress * 4.0;
    }
    if (progress < 0.75) {
        return 2.0 - (progress * 4.0);
    }
    return (progress * 4.0) - 4.0;
}

static int er_ui_round_double(double value) {
    return value >= 0.0 ? (int) (value + 0.5) : (int) (value - 0.5);
}

static ErUiAnimationProperty er_ui_animation_property_from_text(const char *property_text) {
    if (!property_text || property_text[0] == '\0') {
        return ER_UI_ANIMATION_PROPERTY_NONE;
    }
    if (strcmp(property_text, "x") == 0) {
        return ER_UI_ANIMATION_PROPERTY_X;
    }
    if (strcmp(property_text, "y") == 0) {
        return ER_UI_ANIMATION_PROPERTY_Y;
    }
    if (strcmp(property_text, "w") == 0) {
        return ER_UI_ANIMATION_PROPERTY_W;
    }
    if (strcmp(property_text, "h") == 0) {
        return ER_UI_ANIMATION_PROPERTY_H;
    }
    if (strcmp(property_text, "shadow") == 0) {
        return ER_UI_ANIMATION_PROPERTY_SHADOW;
    }
    return ER_UI_ANIMATION_PROPERTY_NONE;
}

static double er_ui_node_get_animation_property(const ErUiNode *node, ErUiAnimationProperty property) {
    if (!node) {
        return 0.0;
    }

    switch (property) {
        case ER_UI_ANIMATION_PROPERTY_X:
            return (double) node->x;
        case ER_UI_ANIMATION_PROPERTY_Y:
            return (double) node->y;
        case ER_UI_ANIMATION_PROPERTY_W:
            return (double) node->w;
        case ER_UI_ANIMATION_PROPERTY_H:
            return (double) node->h;
        case ER_UI_ANIMATION_PROPERTY_SHADOW:
            return (double) node->shadow_size;
        case ER_UI_ANIMATION_PROPERTY_NONE:
        default:
            return 0.0;
    }
}

static void er_ui_node_set_animation_property(ErUiNode *node, ErUiAnimationProperty property, double value) {
    int rounded;

    if (!node) {
        return;
    }

    rounded = er_ui_round_double(value);
    switch (property) {
        case ER_UI_ANIMATION_PROPERTY_X:
            node->x = rounded;
            break;
        case ER_UI_ANIMATION_PROPERTY_Y:
            node->y = rounded;
            break;
        case ER_UI_ANIMATION_PROPERTY_W:
            node->w = er_ui_max_int(1, rounded);
            break;
        case ER_UI_ANIMATION_PROPERTY_H:
            node->h = er_ui_max_int(1, rounded);
            break;
        case ER_UI_ANIMATION_PROPERTY_SHADOW:
            node->shadow_size = rounded < 0 ? 0 : rounded;
            break;
        case ER_UI_ANIMATION_PROPERTY_NONE:
        default:
            break;
    }
}

static void er_ui_animation_capture_origin(ErUiNode *node) {
    if (!node) {
        return;
    }

    node->animation_origin_x = node->x;
    node->animation_origin_y = node->y;
    node->animation_origin_w = node->w;
    node->animation_origin_h = node->h;
    node->animation_origin_shadow = node->shadow_size;
    if (node->animation_property != ER_UI_ANIMATION_PROPERTY_NONE) {
        node->animation_key_base = er_ui_node_get_animation_property(node, node->animation_property);
    } else {
        node->animation_key_base = 0.0;
    }
    node->animation_start_tick = er_ui_tick_now();
}

static void er_ui_animation_reset_node(ErUiNode *node) {
    if (!node) {
        return;
    }

    node->x = node->animation_origin_x;
    node->y = node->animation_origin_y;
    node->w = er_ui_max_int(1, node->animation_origin_w);
    node->h = er_ui_max_int(1, node->animation_origin_h);
    node->shadow_size = node->animation_origin_shadow < 0 ? 0 : node->animation_origin_shadow;
}

static void er_ui_animation_apply_keyframe_value(ErUiNode *node, double normalized_progress) {
    size_t index;
    double offset_value;

    if (!node || node->animation_keyframe_count == 0) {
        return;
    }

    if (normalized_progress <= node->animation_keyframe_times[0]) {
        offset_value = node->animation_keyframe_values[0];
        er_ui_node_set_animation_property(
            node,
            node->animation_property,
            node->animation_key_base + offset_value
        );
        return;
    }

    for (index = 1; index < node->animation_keyframe_count; ++index) {
        double left_time = node->animation_keyframe_times[index - 1];
        double right_time = node->animation_keyframe_times[index];
        double left_value = node->animation_keyframe_values[index - 1];
        double right_value = node->animation_keyframe_values[index];
        double local_progress;
        double mixed_value;

        if (normalized_progress > right_time) {
            continue;
        }

        if (right_time <= left_time) {
            mixed_value = right_value;
        } else {
            local_progress = (normalized_progress - left_time) / (right_time - left_time);
            mixed_value = left_value + ((right_value - left_value) * local_progress);
        }

        er_ui_node_set_animation_property(
            node,
            node->animation_property,
            node->animation_key_base + mixed_value
        );
        return;
    }

    offset_value = node->animation_keyframe_values[node->animation_keyframe_count - 1];
    er_ui_node_set_animation_property(
        node,
        node->animation_property,
        node->animation_key_base + offset_value
    );
}

static bool er_ui_animation_tick_node(ErUiApp *app, ErUiNode *node, unsigned long now_tick) {
    double duration_ms;
    double elapsed_ms;
    double normalized_progress;
    double wave;
    bool finished;

    (void) app;

    if (!node || !node->animation_active || node->animation_duration_ms <= 0) {
        return false;
    }

    duration_ms = (double) node->animation_duration_ms;
    elapsed_ms = (double) (now_tick - node->animation_start_tick);
    normalized_progress = elapsed_ms / duration_ms;
    finished = !node->animation_loop && normalized_progress >= 1.0;

    if (node->animation_loop) {
        normalized_progress = er_ui_fractional(normalized_progress);
    } else if (normalized_progress > 1.0) {
        normalized_progress = 1.0;
    } else if (normalized_progress < 0.0) {
        normalized_progress = 0.0;
    }

    switch (node->animation_kind) {
        case ER_UI_ANIMATION_PRESET_FLOAT:
            er_ui_animation_reset_node(node);
            wave = er_ui_triangle_wave(normalized_progress);
            node->y = node->animation_origin_y + er_ui_round_double(wave * node->animation_amplitude);
            break;
        case ER_UI_ANIMATION_PRESET_DRIFT:
            er_ui_animation_reset_node(node);
            wave = er_ui_triangle_wave(normalized_progress);
            node->x = node->animation_origin_x + er_ui_round_double(wave * node->animation_amplitude);
            break;
        case ER_UI_ANIMATION_PRESET_PULSE: {
            double pulse = er_ui_abs_double(er_ui_triangle_wave(normalized_progress));
            double scale = 1.0 + (pulse * node->animation_amplitude);
            int new_w;
            int new_h;

            er_ui_animation_reset_node(node);
            new_w = er_ui_max_int(1, er_ui_round_double((double) node->animation_origin_w * scale));
            new_h = er_ui_max_int(1, er_ui_round_double((double) node->animation_origin_h * scale));
            node->x = node->animation_origin_x - ((new_w - node->animation_origin_w) / 2);
            node->y = node->animation_origin_y - ((new_h - node->animation_origin_h) / 2);
            node->w = new_w;
            node->h = new_h;
            break;
        }
        case ER_UI_ANIMATION_OSCILLATE:
            wave = er_ui_triangle_wave(normalized_progress);
            er_ui_node_set_animation_property(
                node,
                node->animation_property,
                node->animation_key_base + (wave * node->animation_amplitude)
            );
            break;
        case ER_UI_ANIMATION_KEYFRAMES:
            er_ui_animation_apply_keyframe_value(node, normalized_progress);
            break;
        case ER_UI_ANIMATION_NONE:
        default:
            return false;
    }

    if (finished) {
        if (node->animation_kind == ER_UI_ANIMATION_KEYFRAMES) {
            er_ui_animation_apply_keyframe_value(node, 1.0);
        } else {
            er_ui_animation_reset_node(node);
        }
        node->animation_active = false;
    }

    if (node->kind == ER_UI_NODE_INPUT && node->hwnd) {
        er_ui_position_input_control(node);
    } else if (node->kind == ER_UI_NODE_WEBVIEW && node->webview) {
        er_ui_position_webview_control(node);
    }

    return true;
}

static void er_ui_animation_tick_app(ErUiApp *app) {
    size_t i;
    bool changed = false;
    unsigned long now_tick;

    if (!app || !app->hwnd) {
        return;
    }

    now_tick = er_ui_tick_now();
    for (i = 0; i < app->node_count; ++i) {
        if (er_ui_animation_tick_node(app, &app->nodes[i], now_tick)) {
            changed = true;
        }
    }

    if (changed) {
        InvalidateRect(app->hwnd, NULL, FALSE);
    }
}

static void er_ui_animation_stop_node(ErUiNode *node) {
    if (!node) {
        return;
    }

    if (node->animation_active) {
        er_ui_animation_reset_node(node);
    }
    node->animation_active = false;
    node->animation_kind = ER_UI_ANIMATION_NONE;
    node->animation_property = ER_UI_ANIMATION_PROPERTY_NONE;
    node->animation_keyframe_count = 0;
}

static void er_ui_trim_in_place(char *text) {
    char *start;
    char *end;

    if (!text) {
        return;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char) *start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char) end[-1])) {
        end--;
    }
    *end = '\0';
}

static bool er_ui_parse_keyframe_text(ErUiNode *node, const char *frames_text, ErError *error) {
    char *mutable_text;
    char *cursor;
    size_t count = 0;
    double previous_time = -1.0;

    if (!node || !frames_text || frames_text[0] == '\0') {
        er_error_set(error, 0, 0, "Keyframes text is required");
        return false;
    }

    mutable_text = er_ui_dup(frames_text);
    if (!mutable_text) {
        er_error_set(error, 0, 0, "Out of memory while parsing keyframes");
        return false;
    }

    cursor = mutable_text;
    while (*cursor != '\0' && count < (sizeof(node->animation_keyframe_times) / sizeof(node->animation_keyframe_times[0]))) {
        char *segment = cursor;
        char *separator = strchr(segment, '|');
        char *colon;
        double time_value;
        double frame_value;

        if (separator) {
            *separator = '\0';
            cursor = separator + 1;
        } else {
            cursor = segment + strlen(segment);
        }

        er_ui_trim_in_place(segment);
        if (segment[0] == '\0') {
            continue;
        }

        colon = strchr(segment, ':');
        if (!colon) {
            free(mutable_text);
            er_error_set(error, 0, 0, "Invalid keyframe segment '%s'", segment);
            return false;
        }

        *colon = '\0';
        er_ui_trim_in_place(segment);
        er_ui_trim_in_place(colon + 1);
        time_value = strtod(segment, NULL);
        frame_value = strtod(colon + 1, NULL);
        if (time_value < 0.0 || time_value > 1.0) {
            free(mutable_text);
            er_error_set(error, 0, 0, "Keyframe time must stay between 0 and 1");
            return false;
        }
        if (time_value < previous_time) {
            free(mutable_text);
            er_error_set(error, 0, 0, "Keyframe times must be in ascending order");
            return false;
        }

        node->animation_keyframe_times[count] = time_value;
        node->animation_keyframe_values[count] = frame_value;
        previous_time = time_value;
        count++;
    }

    free(mutable_text);

    if (count == 0) {
        er_error_set(error, 0, 0, "At least one keyframe is required");
        return false;
    }

    node->animation_keyframe_count = count;
    return true;
}

static int er_ui_box_default_padding(ErUiSurfaceStyle style) {
    switch (style) {
        case ER_UI_SURFACE_STYLE_CARD:
            return 22;
        case ER_UI_SURFACE_STYLE_PANEL:
            return 20;
        case ER_UI_SURFACE_STYLE_BOX:
        case ER_UI_SURFACE_STYLE_DEFAULT:
        default:
            return 18;
    }
}

static int er_ui_box_default_radius(ErUiSurfaceStyle style) {
    switch (style) {
        case ER_UI_SURFACE_STYLE_CARD:
            return 24;
        case ER_UI_SURFACE_STYLE_PANEL:
            return 14;
        case ER_UI_SURFACE_STYLE_BOX:
        case ER_UI_SURFACE_STYLE_DEFAULT:
        default:
            return 16;
    }
}

static int er_ui_box_default_shadow(ErUiSurfaceStyle style) {
    switch (style) {
        case ER_UI_SURFACE_STYLE_CARD:
            return 6;
        case ER_UI_SURFACE_STYLE_PANEL:
            return 2;
        case ER_UI_SURFACE_STYLE_BOX:
        case ER_UI_SURFACE_STYLE_DEFAULT:
        default:
            return 4;
    }
}

static void er_ui_apply_named_preset(ErUiApp *app, ErUiNode *node) {
    if (!app || !node || !node->preset || node->preset[0] == '\0') {
        return;
    }

    if ((strcmp(node->preset, "title") == 0 || strcmp(node->preset, "heroTitle") == 0) &&
        node->kind == ER_UI_NODE_TEXT) {
        if (node->font_size <= 16) {
            node->font_size = strcmp(node->preset, "heroTitle") == 0 ? 36 : 30;
        }
        if (node->padding < 0) {
            node->padding = 4;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = 0xf8fafc;
        }
        if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
            node->text_align = ER_UI_TEXT_ALIGN_START;
        }
        return;
    }

    if ((strcmp(node->preset, "muted") == 0 || strcmp(node->preset, "caption") == 0) &&
        node->kind == ER_UI_NODE_TEXT) {
        if (node->font_size <= 16) {
            node->font_size = strcmp(node->preset, "caption") == 0 ? 12 : 14;
        }
        if (node->padding < 0) {
            node->padding = 4;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.muted_text_rgb;
        }
        if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
            node->text_align = ER_UI_TEXT_ALIGN_START;
        }
        return;
    }

    if (strcmp(node->preset, "overline") == 0 && node->kind == ER_UI_NODE_TEXT) {
        if (node->font_size <= 16) {
            node->font_size = 12;
        }
        if (node->padding < 0) {
            node->padding = 2;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = 0x7dd3fc;
        }
        if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
            node->text_align = ER_UI_TEXT_ALIGN_START;
        }
        return;
    }

    if (strcmp(node->preset, "primary") == 0 && node->kind == ER_UI_NODE_BUTTON) {
        if (node->padding < 0) {
            node->padding = 14;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 14;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 3;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = app->theme.accent_rgb;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.accent_active_rgb;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.accent_text_rgb;
        }
        if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
            node->text_align = ER_UI_TEXT_ALIGN_CENTER;
        }
        return;
    }

    if (strcmp(node->preset, "secondary") == 0 && node->kind == ER_UI_NODE_BUTTON) {
        if (node->padding < 0) {
            node->padding = 13;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 13;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 2;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = app->theme.surface_rgb;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.surface_border_rgb;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.text_rgb;
        }
        if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
            node->text_align = ER_UI_TEXT_ALIGN_CENTER;
        }
        return;
    }

    if (strcmp(node->preset, "ghost") == 0 && node->kind == ER_UI_NODE_BUTTON) {
        if (node->padding < 0) {
            node->padding = 12;
        }
        if (node->border_width < 0) {
            node->border_width = 0;
        }
        if (node->border_radius < 0) {
            node->border_radius = 12;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 0;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.highlight_rgb, 10);
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.text_rgb;
        }
        return;
    }

    if (strcmp(node->preset, "success") == 0 && node->kind == ER_UI_NODE_BUTTON) {
        if (node->padding < 0) {
            node->padding = 14;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 14;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 3;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = 0x16a34a;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = 0x166534;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = 0xf0fdf4;
        }
        return;
    }

    if (strcmp(node->preset, "danger") == 0 && node->kind == ER_UI_NODE_BUTTON) {
        if (node->padding < 0) {
            node->padding = 14;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 14;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 3;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = 0xdc2626;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = 0x991b1b;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = 0xfef2f2;
        }
        return;
    }

    if ((strcmp(node->preset, "search") == 0 || strcmp(node->preset, "toolbarField") == 0) &&
        node->kind == ER_UI_NODE_INPUT) {
        if (node->padding < 0) {
            node->padding = strcmp(node->preset, "search") == 0 ? 15 : 14;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = strcmp(node->preset, "search") == 0 ? 24 : 14;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 0;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = app->theme.input_bg_rgb;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.input_border_rgb;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.text_rgb;
        }
        return;
    }

    if (strcmp(node->preset, "toolbar") == 0 && node->kind == ER_UI_NODE_BOX) {
        if (node->surface_style == ER_UI_SURFACE_STYLE_DEFAULT) {
            node->surface_style = ER_UI_SURFACE_STYLE_PANEL;
        }
        if (node->padding < 0) {
            node->padding = 20;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 22;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 2;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = app->theme.surface_rgb;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.surface_border_rgb;
        }
        return;
    }

    if (strcmp(node->preset, "sidebar") == 0 && node->kind == ER_UI_NODE_BOX) {
        if (node->surface_style == ER_UI_SURFACE_STYLE_DEFAULT) {
            node->surface_style = ER_UI_SURFACE_STYLE_PANEL;
        }
        if (node->padding < 0) {
            node->padding = 18;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 20;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 2;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.surface_rgb, 92);
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.surface_border_rgb;
        }
        return;
    }

    if ((strcmp(node->preset, "card") == 0 || strcmp(node->preset, "metric") == 0) &&
        node->kind == ER_UI_NODE_BOX) {
        if (node->surface_style == ER_UI_SURFACE_STYLE_DEFAULT || node->surface_style == ER_UI_SURFACE_STYLE_BOX) {
            node->surface_style = ER_UI_SURFACE_STYLE_CARD;
        }
        if (node->padding < 0) {
            node->padding = strcmp(node->preset, "metric") == 0 ? 24 : 22;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = strcmp(node->preset, "metric") == 0 ? 26 : 22;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = strcmp(node->preset, "metric") == 0 ? 8 : 6;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = app->theme.surface_rgb;
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.surface_border_rgb;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.text_rgb;
        }
        return;
    }

    if (strcmp(node->preset, "statusBar") == 0 && node->kind == ER_UI_NODE_BOX) {
        if (node->surface_style == ER_UI_SURFACE_STYLE_DEFAULT) {
            node->surface_style = ER_UI_SURFACE_STYLE_PANEL;
        }
        if (node->padding < 0) {
            node->padding = 14;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 18;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 2;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.surface_rgb, 78);
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.surface_border_rgb;
        }
        return;
    }

    if ((strcmp(node->preset, "browser") == 0 || strcmp(node->preset, "webviewShell") == 0) &&
        node->kind == ER_UI_NODE_WEBVIEW) {
        if (node->padding < 0) {
            node->padding = 10;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 24;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 24;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.surface_rgb, 84);
        }
        if (!node->has_bg_alt_color) {
            node->has_bg_alt_color = true;
            node->bg_alt_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.surface_hover_rgb, 90);
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = app->theme.surface_border_rgb;
        }
        if (!node->has_text_color) {
            node->has_text_color = true;
            node->text_color_rgb = app->theme.muted_text_rgb;
        }
        return;
    }

    if (strcmp(node->preset, "glass") == 0 &&
        (node->kind == ER_UI_NODE_BOX || node->kind == ER_UI_NODE_WEBVIEW)) {
        if (node->kind == ER_UI_NODE_BOX &&
            (node->surface_style == ER_UI_SURFACE_STYLE_DEFAULT || node->surface_style == ER_UI_SURFACE_STYLE_BOX)) {
            node->surface_style = ER_UI_SURFACE_STYLE_CARD;
        }
        if (node->padding < 0) {
            node->padding = 20;
        }
        if (node->border_width < 0) {
            node->border_width = 1;
        }
        if (node->border_radius < 0) {
            node->border_radius = 24;
        }
        if (node->shadow_size < 0) {
            node->shadow_size = 18;
        }
        if (!node->has_bg_color) {
            node->has_bg_color = true;
            node->bg_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.highlight_rgb, 18);
        }
        if (!node->has_bg_alt_color) {
            node->has_bg_alt_color = true;
            node->bg_alt_color_rgb = er_ui_blend_rgb(app->background_rgb, app->theme.highlight_rgb, 10);
        }
        if (!node->has_border_color) {
            node->has_border_color = true;
            node->border_color_rgb = er_ui_blend_rgb(app->theme.surface_border_rgb, app->theme.highlight_rgb, 48);
        }
        return;
    }
}

static void er_ui_apply_node_defaults(ErUiApp *app, ErUiNode *node) {
    int measured_height;
    int content_width;
    bool multi_line;

    if (!node || !app) {
        return;
    }

    if (node->font_size <= 0) {
        node->font_size = 16;
    }

    er_ui_apply_named_preset(app, node);

    switch (node->kind) {
        case ER_UI_NODE_TEXT:
            if (node->padding < 0) {
                node->padding = 2;
            }
            if (node->border_width < 0) {
                node->border_width = node->has_border_color ? 1 : 0;
            }
            if (node->border_radius < 0) {
                node->border_radius = 0;
            }
            if (node->shadow_size < 0) {
                node->shadow_size = 0;
            }
            if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
                node->text_align = ER_UI_TEXT_ALIGN_START;
            }
            if (node->w <= 0) {
                node->w = 120;
            }
            multi_line = strchr(node->text ? node->text : "", '\n') != NULL ||
                         node->h > node->font_size + (node->padding * 2) + 10;
            content_width = er_ui_max_int(32, node->w - (node->padding * 2));
            measured_height = er_ui_measure_text_height(node, content_width, !multi_line, multi_line);
            node->h = er_ui_max_int(node->h, measured_height + (node->padding * 2) + (multi_line ? 4 : 2));
            node->custom_drawn = true;
            break;
        case ER_UI_NODE_BUTTON:
            if (node->icon_size <= 0) {
                node->icon_size = 20;
            }
            if (node->image_fit != ER_UI_IMAGE_FIT_COVER &&
                node->image_fit != ER_UI_IMAGE_FIT_STRETCH &&
                node->image_fit != ER_UI_IMAGE_FIT_CENTER) {
                node->image_fit = ER_UI_IMAGE_FIT_CONTAIN;
            }
            if (node->padding < 0) {
                node->padding = 16;
            }
            if (node->border_width < 0) {
                node->border_width = 1;
            }
            if (node->border_radius < 0) {
                node->border_radius = 14;
            }
            if (node->shadow_size < 0) {
                node->shadow_size = 2;
            }
            if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
                node->text_align = ER_UI_TEXT_ALIGN_CENTER;
            }
            if (node->w <= 0) {
                node->w = 108;
            }
            measured_height = er_ui_measure_text_height(node, er_ui_max_int(32, node->w - (node->padding * 2)), true, false);
            node->h = er_ui_max_int(node->h, measured_height + (er_ui_content_vertical_padding(node) * 2) + 10);
            node->custom_drawn = true;
            node->focusable = true;
            break;
        case ER_UI_NODE_INPUT:
            if (node->padding < 0) {
                node->padding = 14;
            }
            if (node->border_width < 0) {
                node->border_width = 1;
            }
            if (node->border_radius < 0) {
                node->border_radius = 12;
            }
            if (node->shadow_size < 0) {
                node->shadow_size = 0;
            }
            if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
                node->text_align = ER_UI_TEXT_ALIGN_START;
            }
            if (node->w <= 0) {
                node->w = 180;
            }
            if (node->multiline) {
                node->h = er_ui_max_int(node->h, er_ui_max_int(110, (node->font_size * 4) + (er_ui_content_vertical_padding(node) * 2) + 18));
            } else {
                node->h = er_ui_max_int(node->h, node->font_size + (er_ui_content_vertical_padding(node) * 2) + 12);
            }
            node->custom_drawn = true;
            node->focusable = true;
            break;
        case ER_UI_NODE_IMAGE:
            if (node->icon_size <= 0) {
                node->icon_size = 32;
            }
            if (node->image_fit != ER_UI_IMAGE_FIT_COVER &&
                node->image_fit != ER_UI_IMAGE_FIT_STRETCH &&
                node->image_fit != ER_UI_IMAGE_FIT_CENTER) {
                node->image_fit = ER_UI_IMAGE_FIT_CONTAIN;
            }
            if (node->padding < 0) {
                node->padding = 18;
            }
            if (node->border_width < 0) {
                node->border_width = 1;
            }
            if (node->border_radius < 0) {
                node->border_radius = 16;
            }
            if (node->shadow_size < 0) {
                node->shadow_size = 12;
            }
            if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
                node->text_align = ER_UI_TEXT_ALIGN_CENTER;
            }
            if (node->w <= 0) {
                node->w = 180;
            }
            content_width = er_ui_max_int(40, node->w - (node->padding * 2));
            measured_height = er_ui_measure_text_height(node, content_width, false, true);
            node->h = er_ui_max_int(node->h, er_ui_max_int(140, measured_height + (node->padding * 2) + 12));
            node->custom_drawn = true;
            node->focusable = node->on_click != NULL;
            break;
        case ER_UI_NODE_WEBVIEW:
            if (node->padding < 0) {
                node->padding = 10;
            }
            if (node->border_width < 0) {
                node->border_width = 1;
            }
            if (node->border_radius < 0) {
                node->border_radius = 18;
            }
            if (node->shadow_size < 0) {
                node->shadow_size = 6;
            }
            if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
                node->text_align = ER_UI_TEXT_ALIGN_START;
            }
            if (node->w <= 0) {
                node->w = 260;
            }
            if (node->h <= 0) {
                node->h = 180;
            }
            node->custom_drawn = true;
            node->focusable = false;
            break;
        case ER_UI_NODE_BOX:
            if (node->padding < 0) {
                node->padding = er_ui_box_default_padding(node->surface_style);
            }
            if (node->border_width < 0) {
                node->border_width = 1;
            }
            if (node->border_radius < 0) {
                node->border_radius = er_ui_box_default_radius(node->surface_style);
            }
            if (node->shadow_size < 0) {
                node->shadow_size = er_ui_box_default_shadow(node->surface_style);
            }
            if (node->text_align == ER_UI_TEXT_ALIGN_DEFAULT) {
                node->text_align = node->kind == ER_UI_NODE_BOX ? ER_UI_TEXT_ALIGN_START : ER_UI_TEXT_ALIGN_CENTER;
            }
            if (node->w <= 0) {
                node->w = 180;
            }
            content_width = er_ui_max_int(40, node->w - (node->padding * 2));
            measured_height = er_ui_measure_text_height(node, content_width, false, true);
            node->h = er_ui_max_int(node->h, er_ui_max_int(84, measured_height + (node->padding * 2) + 12));
            node->custom_drawn = true;
            node->focusable = node->on_click != NULL;
            break;
    }
}

static void er_ui_draw_node(HDC hdc, ErUiApp *app, ErUiNode *node) {
    RECT outer;
    RECT clip_rect;
    RECT content;
    unsigned int bg;
    unsigned int bg_alt;
    unsigned int border;
    unsigned int shadow;
    unsigned int text;
    bool draw_surface;
    bool multi_line;
    int vertical_padding;
    int saved_dc;

    if (!node || !node->visible) {
        return;
    }

    outer = er_ui_node_rect(node);
    clip_rect = outer;
    if (node->shadow_size > 0) {
        InflateRect(&clip_rect, node->shadow_size + 4, node->shadow_size + 4);
    }
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, clip_rect.left, clip_rect.top, clip_rect.right, clip_rect.bottom);

    bg = er_ui_node_effective_bg(app, node);
    bg_alt = er_ui_node_effective_bg_alt(app, node, bg);
    border = er_ui_node_effective_border(app, node);
    shadow = er_ui_node_effective_shadow(app, node);
    text = er_ui_node_effective_text(app, node);
    draw_surface = node->kind != ER_UI_NODE_TEXT ||
                   node->has_bg_color ||
                   node->has_bg_alt_color ||
                   node->border_width > 0 ||
                   node->shadow_size > 0;

    if (draw_surface) {
        er_ui_draw_shadow(hdc, app, &outer, node->border_radius, node->shadow_size, shadow);
        if (node->has_bg_alt_color) {
            er_ui_fill_vertical_gradient(hdc, &outer, bg_alt, bg, node->border_radius);
            er_ui_draw_surface_highlight(hdc, &outer, node->border_radius, bg, app->theme.highlight_rgb);
            er_ui_draw_box_style_overlay(hdc, node, &outer, bg, border);
        } else {
            er_ui_draw_round_surface(hdc, &outer, true, bg, 0, bg, node->border_radius);
        }
        if (node->border_width > 0) {
            er_ui_draw_round_surface(hdc, &outer, false, bg, node->border_width, border, node->border_radius);
        }
    }

    if (node->focused && node->kind != ER_UI_NODE_INPUT) {
        er_ui_draw_focus_ring(hdc, &outer, node->border_radius, app->theme.focus_rgb);
    }

    if (node->kind == ER_UI_NODE_INPUT) {
        if (node->focused) {
            er_ui_draw_focus_ring(hdc, &outer, node->border_radius, app->theme.focus_rgb);
        }
        RestoreDC(hdc, saved_dc);
        return;
    }

    content = outer;
    vertical_padding = er_ui_content_vertical_padding(node);
    er_ui_inset_rect(&content, node->padding, vertical_padding);

    if (node->kind == ER_UI_NODE_BUTTON && node->pressed) {
        OffsetRect(&content, 0, 1);
    }

    multi_line = node->kind == ER_UI_NODE_BOX ||
                 (node->kind == ER_UI_NODE_TEXT &&
                  (strchr(node->text ? node->text : "", '\n') != NULL ||
                   node->h > node->font_size + (node->padding * 2) + 10));

    switch (node->kind) {
        case ER_UI_NODE_TEXT:
            er_ui_draw_text_in_rect(hdc, node, &content, text, !multi_line, multi_line);
            break;
        case ER_UI_NODE_BUTTON:
            if (node->image_bitmap) {
                RECT icon_rect = content;
                RECT text_rect = content;
                int icon_size = node->icon_size > 0 ? node->icon_size : er_ui_max_int(16, node->h - (vertical_padding * 2) - 6);

                if (node->text && node->text[0] != '\0') {
                    icon_rect.left = content.left + 4;
                    icon_rect.top = content.top + ((content.bottom - content.top - icon_size) / 2);
                    icon_rect.right = icon_rect.left + icon_size;
                    icon_rect.bottom = icon_rect.top + icon_size;
                    text_rect.left = icon_rect.right + 10;
                } else {
                    icon_rect.left = content.left + ((content.right - content.left - icon_size) / 2);
                    icon_rect.top = content.top + ((content.bottom - content.top - icon_size) / 2);
                    icon_rect.right = icon_rect.left + icon_size;
                    icon_rect.bottom = icon_rect.top + icon_size;
                }

                er_ui_draw_bitmap_in_rect(
                    hdc,
                    node->image_bitmap,
                    node->image_pixel_width,
                    node->image_pixel_height,
                    &icon_rect
                );
                if (node->text && node->text[0] != '\0') {
                    er_ui_draw_text_in_rect(hdc, node, &text_rect, text, true, false);
                }
            } else {
                er_ui_draw_text_in_rect(hdc, node, &content, text, true, false);
            }
            break;
        case ER_UI_NODE_IMAGE:
            if (node->image_bitmap) {
                RECT image_rect = content;
                RECT caption_rect = content;

                if (node->text && node->text[0] != '\0') {
                    int caption_height = er_ui_measure_text_height(node, content.right - content.left, false, true);
                    if (caption_height < 22) {
                        caption_height = 22;
                    }
                    image_rect.bottom -= caption_height + 8;
                    caption_rect.top = image_rect.bottom + 8;
                }

                er_ui_compute_fit_rect(
                    &image_rect,
                    node->image_pixel_width,
                    node->image_pixel_height,
                    node->image_fit,
                    &image_rect
                );
                er_ui_draw_bitmap_in_rect(
                    hdc,
                    node->image_bitmap,
                    node->image_pixel_width,
                    node->image_pixel_height,
                    &image_rect
                );

                if (node->text && node->text[0] != '\0') {
                    er_ui_draw_text_in_rect(hdc, node, &caption_rect, text, false, true);
                }
            } else {
                er_ui_draw_text_in_rect(hdc, node, &content, text, false, true);
            }
            break;
        case ER_UI_NODE_BOX:
            er_ui_draw_text_in_rect(hdc, node, &content, text, false, true);
            break;
        case ER_UI_NODE_WEBVIEW:
            if (!node->webview) {
                er_ui_draw_text_in_rect(hdc, node, &content, text, false, true);
            }
            break;
        case ER_UI_NODE_INPUT:
            break;
    }

    RestoreDC(hdc, saved_dc);
}

static void er_ui_activate_node(ErUiApp *app, int control_id) {
    ErUiNode *node = er_ui_find_node_by_control_id(app, control_id);
    if (node && node->on_click) {
        node->on_click(app, node, node->on_click_user_data);
    }
}

static bool er_ui_handle_shortcut(ErUiApp *app, unsigned int virtual_key) {
    size_t i;
    bool ctrl_down;
    bool alt_down;
    bool shift_down;

    if (!app || virtual_key == 0) {
        return false;
    }

    ctrl_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    alt_down = (GetKeyState(VK_MENU) & 0x8000) != 0;
    shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    for (i = 0; i < app->shortcut_count; ++i) {
        ErUiShortcut *shortcut = &app->shortcuts[i];
        ErUiNode *target;

        if (shortcut->virtual_key != virtual_key ||
            shortcut->ctrl != ctrl_down ||
            shortcut->alt != alt_down ||
            shortcut->shift != shift_down) {
            continue;
        }

        target = er_ui_find_node_by_id(app, shortcut->target_id);
        if (!target || !target->visible || !target->on_click) {
            continue;
        }

        app->last_click_x = target->x + (target->w / 2);
        app->last_click_y = target->y + (target->h / 2);
        er_ui_set_pressed_control(app, target->control_id);
        er_ui_activate_node(app, target->control_id);
        er_ui_set_pressed_control(app, 0);
        return true;
    }

    return false;
}

static void er_ui_focus_relative(ErUiApp *app, int direction) {
    size_t i;
    size_t start_index = 0;
    bool found = false;

    if (!app || app->node_count == 0) {
        return;
    }

    for (i = 0; i < app->node_count; ++i) {
        if (app->nodes[i].control_id == app->focused_control_id) {
            start_index = i;
            found = true;
            break;
        }
    }

    for (i = 1; i <= app->node_count; ++i) {
        size_t index;
        ErUiNode *node;

        if (direction < 0) {
            index = found
                ? (start_index + app->node_count - (i % app->node_count)) % app->node_count
                : app->node_count - i;
        } else {
            index = found
                ? (start_index + i) % app->node_count
                : i - 1;
        }

        node = &app->nodes[index];
        if (!node->visible || !node->focusable || !node->custom_drawn || node->kind == ER_UI_NODE_INPUT) {
            continue;
        }

        SetFocus(app->hwnd);
        er_ui_set_focused_control(app, node->control_id);
        return;
    }
}

static LRESULT CALLBACK er_ui_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    ErUiApp *app = (ErUiApp *) GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTA *create = (CREATESTRUCTA *) lparam;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) create->lpCreateParams);
            return TRUE;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE: {
            size_t i;
            int client_w;
            int client_h;

            if (!app) {
                break;
            }

            client_w = LOWORD(lparam);
            client_h = HIWORD(lparam);
            app->client_w = client_w > 0 ? client_w : 1;
            app->client_h = client_h > 0 ? client_h : 1;

            for (i = 0; i < app->node_count; ++i) {
                er_ui_rescale_node_layout(app, &app->nodes[i]);
                if (app->nodes[i].kind == ER_UI_NODE_INPUT && app->nodes[i].hwnd) {
                    er_ui_position_input_control(&app->nodes[i]);
                } else if (app->nodes[i].kind == ER_UI_NODE_WEBVIEW && app->nodes[i].webview) {
                    er_ui_position_webview_control(&app->nodes[i]);
                }
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_TIMER:
            if (app) {
                if (wparam == 1u) {
                    er_ui_animation_tick_app(app);
                    return 0;
                }
                {
                    ErUiTimer *timer = er_ui_find_timer_by_os_id(app, (unsigned int) wparam);
                    if (timer && timer->callback) {
                        timer->callback(app, timer->timer_id, timer->user_data);
                        return 0;
                    }
                }
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT client;
            HDC hdc;
            HDC memory_dc;
            HBITMAP bitmap;
            HGDIOBJ old_bitmap;
            size_t i;
            int width;
            int height;

            if (!app) {
                break;
            }

            hdc = BeginPaint(hwnd, &paint);
            GetClientRect(hwnd, &client);
            width = er_ui_max_int(1, client.right - client.left);
            height = er_ui_max_int(1, client.bottom - client.top);

            memory_dc = CreateCompatibleDC(hdc);
            bitmap = CreateCompatibleBitmap(hdc, width, height);
            old_bitmap = SelectObject(memory_dc, bitmap);
            FillRect(memory_dc, &client, app->background_brush);

            for (i = 0; i < app->node_count; ++i) {
                if (app->nodes[i].visible && app->nodes[i].custom_drawn) {
                    er_ui_draw_node(memory_dc, app, &app->nodes[i]);
                }
            }

            BitBlt(hdc, 0, 0, width, height, memory_dc, 0, 0, SRCCOPY);
            SelectObject(memory_dc, old_bitmap);
            DeleteObject(bitmap);
            DeleteDC(memory_dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_COMMAND: {
            int control_id = LOWORD(wparam);
            int code = HIWORD(wparam);
            ErUiNode *node;

            if (!app) {
                break;
            }

            node = er_ui_find_node_by_control_id(app, control_id);
            if (!node) {
                break;
            }

            if (code == EN_SETFOCUS) {
                er_ui_set_focused_control(app, 0);
                node->focused = true;
                er_ui_invalidate_node(app, node);
                return 0;
            }
            if (code == EN_KILLFOCUS) {
                node->focused = false;
                er_ui_invalidate_node(app, node);
                return 0;
            }
            if (code == EN_CHANGE) {
                er_ui_sync_input_text(node);
                if (node->on_change) {
                    node->on_change(app, node, node->on_change_user_data);
                }
                er_ui_invalidate_node(app, node);
                return 0;
            }
            break;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC) wparam;
            HWND child = (HWND) lparam;
            ErUiNode *node = app ? er_ui_find_node_by_hwnd(app, child) : NULL;

            if (!node) {
                break;
            }

            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, er_ui_color(er_ui_node_effective_text(app, node)));
            SetBkColor(hdc, er_ui_color(er_ui_node_base_bg(app, node)));
            return (LRESULT) node->brush;
        }
        case WM_SETCURSOR:
            if (app && LOWORD(lparam) == HTCLIENT) {
                ErUiNode *hovered = er_ui_find_node_by_control_id(app, app->hovered_control_id);
                if (hovered && hovered->kind == ER_UI_NODE_INPUT) {
                    SetCursor(app->ibeam_cursor ? app->ibeam_cursor : LoadCursor(NULL, IDC_IBEAM));
                    return TRUE;
                }
                if (hovered && (hovered->kind == ER_UI_NODE_BUTTON || hovered->on_click != NULL)) {
                    SetCursor(app->hand_cursor ? app->hand_cursor : LoadCursor(NULL, IDC_HAND));
                    return TRUE;
                }
                SetCursor(app->arrow_cursor ? app->arrow_cursor : LoadCursor(NULL, IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_MOUSEMOVE:
            if (app) {
                ErUiNode *hit;
                app->last_mouse_x = GET_X_LPARAM(lparam);
                app->last_mouse_y = GET_Y_LPARAM(lparam);

                er_ui_begin_mouse_tracking(app);
                hit = er_ui_hit_test(app, app->last_mouse_x, app->last_mouse_y);
                er_ui_set_hovered_control(app, hit ? hit->control_id : 0);
                return 0;
            }
            break;
        case WM_MOUSELEAVE:
            if (app) {
                app->tracking_mouse = false;
                er_ui_set_hovered_control(app, 0);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            if (app) {
                ErUiNode *hit;
                app->last_mouse_x = GET_X_LPARAM(lparam);
                app->last_mouse_y = GET_Y_LPARAM(lparam);
                app->last_click_x = app->last_mouse_x;
                app->last_click_y = app->last_mouse_y;
                hit = er_ui_hit_test(app, app->last_mouse_x, app->last_mouse_y);

                if (!hit) {
                    er_ui_set_focused_control(app, 0);
                    break;
                }

                if (hit->kind == ER_UI_NODE_INPUT && hit->hwnd) {
                    SetFocus(hit->hwnd);
                    return 0;
                }

                if (hit->focusable) {
                    SetFocus(hwnd);
                    er_ui_set_focused_control(app, hit->control_id);
                }

                if (hit->on_click || hit->kind == ER_UI_NODE_BUTTON) {
                    er_ui_set_pressed_control(app, hit->control_id);
                    SetCapture(hwnd);
                }
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (app) {
                ErUiNode *hit;
                int pressed_control_id = app->pressed_control_id;
                app->last_mouse_x = GET_X_LPARAM(lparam);
                app->last_mouse_y = GET_Y_LPARAM(lparam);
                app->last_click_x = app->last_mouse_x;
                app->last_click_y = app->last_mouse_y;
                hit = er_ui_hit_test(app, app->last_mouse_x, app->last_mouse_y);

                if (GetCapture() == hwnd) {
                    ReleaseCapture();
                }

                er_ui_set_pressed_control(app, 0);
                if (hit && pressed_control_id != 0 && hit->control_id == pressed_control_id) {
                    er_ui_activate_node(app, pressed_control_id);
                } else if (hit && pressed_control_id != 0 && hit->on_click) {
                    ErUiNode *pressed = er_ui_find_node_by_control_id(app, pressed_control_id);
                    if (pressed &&
                        pressed->kind == ER_UI_NODE_BOX &&
                        hit->kind == ER_UI_NODE_BOX &&
                        hit->control_id != pressed_control_id) {
                        er_ui_activate_node(app, hit->control_id);
                    }
                }
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (app) {
                if (er_ui_handle_shortcut(app, (unsigned int) wparam)) {
                    return 0;
                }

                if (wparam == VK_TAB) {
                    er_ui_focus_relative(app, GetKeyState(VK_SHIFT) < 0 ? -1 : 1);
                    return 0;
                }

                if ((wparam == VK_RETURN || wparam == VK_SPACE) && app->focused_control_id != 0) {
                    int control_id = app->focused_control_id;
                    ErUiNode *node = er_ui_find_node_by_control_id(app, control_id);
                    if (node && node->on_click) {
                        node->pressed = true;
                        er_ui_invalidate_node(app, node);
                        er_ui_activate_node(app, control_id);
                        node = er_ui_find_node_by_control_id(app, control_id);
                        if (node) {
                            node->pressed = false;
                            er_ui_invalidate_node(app, node);
                        }
                        return 0;
                    }
                }
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static bool er_ui_register_class(HINSTANCE instance, ErError *error) {
    static bool registered = false;
    WNDCLASSA wc;

    if (registered) {
        return true;
    }

    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = er_ui_window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = ER_UI_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    if (!RegisterClassA(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            er_error_set(error, 0, 0, "Could not register window class");
            return false;
        }
    }

    registered = true;
    return true;
}

static bool er_ui_grow_nodes(ErUiApp *app, ErError *error) {
    size_t new_capacity;
    ErUiNode *new_nodes;
    size_t i;

    if (app->node_count < app->node_capacity) {
        return true;
    }

    new_capacity = app->node_capacity == 0 ? 16 : app->node_capacity * 2;
    new_nodes = (ErUiNode *) realloc(app->nodes, new_capacity * sizeof(ErUiNode));
    if (!new_nodes) {
        er_error_set(error, 0, 0, "Out of memory while growing UI node list");
        return false;
    }

    for (i = 0; i < app->node_count; ++i) {
        if (new_nodes[i].webview) {
            new_nodes[i].webview->node = &new_nodes[i];
            new_nodes[i].webview->app = app;
        }
    }

    app->nodes = new_nodes;
    app->node_capacity = new_capacity;
    return true;
}

bool er_ui_app_init(
    ErUiApp *app,
    const char *title,
    int x,
    int y,
    int w,
    int h,
    bool resizable,
    ErError *error
) {
    INITCOMMONCONTROLSEX controls;
    HRESULT ole_result;
    const char *effective_title = (title && title[0] != '\0') ? title : "Erire";
    DWORD window_style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

    memset(app, 0, sizeof(*app));
    er_error_clear(error);

    app->instance = GetModuleHandleA(NULL);
    app->title = er_ui_dup(effective_title);
    app->x = x;
    app->y = y;
    app->w = w > 0 ? w : 800;
    app->h = h > 0 ? h : 520;
    app->client_w = app->w;
    app->client_h = app->h;
    app->design_client_w = app->w;
    app->design_client_h = app->h;
    app->resizable = resizable;
    er_ui_theme_init_dark(&app->theme);
    app->background_rgb = app->theme.window_bg_rgb;
    app->background_brush = CreateSolidBrush(er_ui_color(app->background_rgb));
    app->next_control_id = 1000;
    app->current_page = er_ui_dup("main");
    app->arrow_cursor = LoadCursor(NULL, IDC_ARROW);
    app->hand_cursor = LoadCursor(NULL, IDC_HAND);
    app->ibeam_cursor = LoadCursor(NULL, IDC_IBEAM);

    er_ui_enable_modern_dpi_mode();

    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);
    er_ui_enable_modern_legacy_webview_features();

    ole_result = OleInitialize(NULL);
    if (FAILED(ole_result)) {
        er_error_set(error, 0, 0, "Could not initialize OLE services");
        return false;
    }
    app->ole_initialized = true;

    if (!er_ui_register_class(app->instance, error)) {
        OleUninitialize();
        app->ole_initialized = false;
        return false;
    }

    if (!resizable) {
        window_style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    app->hwnd = CreateWindowExA(
        0,
        ER_UI_CLASS_NAME,
        effective_title,
        window_style,
        app->x,
        app->y,
        app->w,
        app->h,
        NULL,
        NULL,
        app->instance,
        app
    );

    if (!app->hwnd) {
        er_error_set(error, 0, 0, "Could not create Erire app window");
        OleUninitialize();
        app->ole_initialized = false;
        return false;
    }

    SetWindowTextA(app->hwnd, effective_title);

    {
        RECT client;
        GetClientRect(app->hwnd, &client);
        app->client_w = er_ui_max_int(1, client.right - client.left);
        app->client_h = er_ui_max_int(1, client.bottom - client.top);
        app->design_client_w = app->client_w;
        app->design_client_h = app->client_h;
    }

    app->next_timer_os_id = 2u;

    SetTimer(app->hwnd, 1u, 16u, NULL);

    if (GetFileAttributesA("assets\\brand\\erire-logo.png") != INVALID_FILE_ATTRIBUTES) {
        ErError icon_error;
        er_error_clear(&icon_error);
        er_ui_app_set_icon(app, "assets\\brand\\erire-logo.png", &icon_error);
    }

    er_ui_enable_modern_window_chrome(app->hwnd);
    return true;
}

void er_ui_app_destroy(ErUiApp *app) {
    size_t i;

    if (!app) {
        return;
    }

    if (app->hwnd) {
        KillTimer(app->hwnd, 1u);
        for (i = 0; i < app->timer_count; ++i) {
            if (app->timers[i].os_timer_id != 0) {
                KillTimer(app->hwnd, app->timers[i].os_timer_id);
            }
        }
    }

    for (i = 0; i < app->node_count; ++i) {
        if (app->nodes[i].webview) {
            er_ui_webview_destroy(app->nodes[i].webview);
            app->nodes[i].webview = NULL;
            app->nodes[i].hwnd = NULL;
        } else if (app->nodes[i].hwnd && IsWindow(app->nodes[i].hwnd)) {
            DestroyWindow(app->nodes[i].hwnd);
        }
        DeleteObject(app->nodes[i].font);
        DeleteObject(app->nodes[i].brush);
        er_ui_destroy_node_assets(&app->nodes[i]);
        free(app->nodes[i].id);
        free(app->nodes[i].page);
        free(app->nodes[i].text);
        free(app->nodes[i].preset);
        free(app->nodes[i].asset_path);
        free(app->nodes[i].icon_path);
    }

    free(app->nodes);
    free(app->timers);
    for (i = 0; i < app->shortcut_count; ++i) {
        free(app->shortcuts[i].target_id);
    }
    free(app->shortcuts);
    free(app->title);
    free(app->icon_path);
    free(app->current_page);

    if (app->background_brush) {
        DeleteObject(app->background_brush);
    }
    if (app->window_icon_small) {
        DestroyIcon(app->window_icon_small);
    }
    if (app->window_icon_large) {
        DestroyIcon(app->window_icon_large);
    }

    if (app->ole_initialized) {
        OleUninitialize();
    }

    if (er_ui_wic_factory) {
        IWICImagingFactory_Release(er_ui_wic_factory);
        er_ui_wic_factory = NULL;
    }

    memset(app, 0, sizeof(*app));
}

void er_ui_app_set_title(ErUiApp *app, const char *title) {
    const char *effective_title;

    if (!app) {
        return;
    }

    effective_title = (title && title[0] != '\0')
        ? title
        : ((app->title && app->title[0] != '\0') ? app->title : "Erire");

    if (app->title != effective_title) {
        free(app->title);
        app->title = er_ui_dup(effective_title);
    }

    if (app->hwnd && effective_title) {
        SetWindowTextA(app->hwnd, effective_title);
    }
}

bool er_ui_app_set_icon(ErUiApp *app, const char *path, ErError *error) {
    HICON icon = NULL;

    if (!app || !path || path[0] == '\0') {
        er_error_set(error, 0, 0, "Window icon path is required");
        return false;
    }

    if (!er_ui_load_icon_from_path(path, &icon, error)) {
        return false;
    }

    if (app->window_icon_small) {
        DestroyIcon(app->window_icon_small);
    }
    if (app->window_icon_large) {
        DestroyIcon(app->window_icon_large);
    }

    app->window_icon_small = CopyIcon(icon);
    app->window_icon_large = icon;
    free(app->icon_path);
    app->icon_path = er_ui_dup(path);

    if (app->hwnd) {
        SendMessageA(app->hwnd, WM_SETICON, ICON_SMALL, (LPARAM) app->window_icon_small);
        SendMessageA(app->hwnd, WM_SETICON, ICON_BIG, (LPARAM) app->window_icon_large);
    }

    return true;
}

void er_ui_app_set_background(ErUiApp *app, unsigned int rgb) {
    if (!app) {
        return;
    }

    app->background_rgb = rgb;
    app->theme.window_bg_rgb = rgb;
    if (app->background_brush) {
        DeleteObject(app->background_brush);
    }
    app->background_brush = CreateSolidBrush(er_ui_color(rgb));
    if (app->hwnd) {
        InvalidateRect(app->hwnd, NULL, TRUE);
    }
}

ErUiNode *er_ui_app_find_node(ErUiApp *app, const char *id) {
    return er_ui_find_node_by_id(app, id);
}

ErUiNode *er_ui_app_add_node(ErUiApp *app, const ErUiNodeSpec *spec, ErError *error) {
    ErUiNode *node;
    wchar_t *wide_hint;

    if (!er_ui_grow_nodes(app, error)) {
        return NULL;
    }

    if (spec->id && spec->id[0] != '\0' && er_ui_find_node_by_id(app, spec->id)) {
        er_error_set(error, 0, 0, "Duplicate element id '%s'", spec->id);
        return NULL;
    }

    node = &app->nodes[app->node_count];
    memset(node, 0, sizeof(*node));
    node->control_id = app->next_control_id++;
    node->kind = spec->kind;
    node->id = er_ui_dup((spec->id && spec->id[0] != '\0') ? spec->id : NULL);
    node->page = er_ui_dup((spec->page && spec->page[0] != '\0') ? spec->page : NULL);
    node->text = er_ui_dup(spec->text ? spec->text : "");
    node->preset = er_ui_dup((spec->preset && spec->preset[0] != '\0') ? spec->preset : NULL);
    node->asset_path = er_ui_dup((spec->asset_path && spec->asset_path[0] != '\0') ? spec->asset_path : NULL);
    node->icon_path = er_ui_dup((spec->icon_path && spec->icon_path[0] != '\0') ? spec->icon_path : NULL);
    node->x = spec->x;
    node->y = spec->y;
    node->w = spec->w > 0 ? spec->w : 120;
    node->h = spec->h > 0 ? spec->h : 40;
    node->font_size = spec->font_size > 0 ? spec->font_size : 16;
    node->icon_size = spec->icon_size;
    node->padding = spec->padding;
    node->border_width = spec->border_width;
    node->border_radius = spec->border_radius;
    node->surface_style = spec->surface_style;
    node->text_align = spec->text_align;
    node->image_fit = spec->image_fit;
    node->multiline = spec->multiline;
    node->read_only = spec->read_only;
    node->has_text_color = spec->has_text_color;
    node->text_color_rgb = spec->text_color_rgb;
    node->has_bg_color = spec->has_bg_color;
    node->bg_color_rgb = spec->bg_color_rgb;
    node->has_bg_alt_color = spec->has_bg_alt_color;
    node->bg_alt_color_rgb = spec->bg_alt_color_rgb;
    node->has_border_color = spec->has_border_color;
    node->border_color_rgb = spec->border_color_rgb;
    node->shadow_size = spec->shadow_size;
    node->has_shadow_color = spec->has_shadow_color;
    node->shadow_color_rgb = spec->shadow_color_rgb;
    node->on_click = spec->on_click;
    node->on_click_user_data = spec->on_click_user_data;
    node->on_change = spec->on_change;
    node->on_change_user_data = spec->on_change_user_data;

    er_ui_apply_node_defaults(app, node);
    er_ui_capture_node_layout_base(node);
    er_ui_rescale_node_layout(app, node);
    node->visible = er_ui_node_is_on_current_page(app, node);
    if (!node->font) {
        node->font = er_ui_create_font(node->font_size);
    }

    if ((node->kind == ER_UI_NODE_IMAGE || node->icon_path) && !er_ui_node_load_visual_assets(node, error)) {
        DeleteObject(node->font);
        free(node->id);
        free(node->page);
        free(node->text);
        free(node->preset);
        free(node->asset_path);
        free(node->icon_path);
        memset(node, 0, sizeof(*node));
        return NULL;
    }

    if (node->kind == ER_UI_NODE_INPUT) {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT;
        unsigned int input_bg = node->has_bg_color ? node->bg_color_rgb : app->theme.input_bg_rgb;

        if (node->multiline) {
            style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
        } else {
            style |= ES_AUTOHSCROLL;
        }
        if (node->read_only) {
            style |= ES_READONLY;
        }

        node->brush = CreateSolidBrush(er_ui_color(input_bg));
        node->hwnd = CreateWindowExA(
            0,
            "EDIT",
            node->text ? node->text : "",
            style,
            node->x,
            node->y,
            node->w,
            node->h,
            app->hwnd,
            (HMENU) (INT_PTR) node->control_id,
            app->instance,
            NULL
        );

        if (!node->hwnd) {
            DeleteObject(node->font);
            DeleteObject(node->brush);
            er_ui_destroy_node_assets(node);
            free(node->id);
            free(node->page);
            free(node->text);
            free(node->preset);
            free(node->asset_path);
            free(node->icon_path);
            memset(node, 0, sizeof(*node));
            er_error_set(error, 0, 0, "Could not create input control");
            return NULL;
        }

        SendMessageA(node->hwnd, WM_SETFONT, (WPARAM) node->font, TRUE);
        SendMessageA(node->hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
        er_ui_position_input_control(node);

        if (spec->hint && spec->hint[0] != '\0') {
            wide_hint = er_ui_utf8_to_wide(spec->hint);
            if (wide_hint) {
                SendMessageW(node->hwnd, EM_SETCUEBANNER, 0, (LPARAM) wide_hint);
                free(wide_hint);
            }
        }

        if (!node->visible) {
            ShowWindow(node->hwnd, SW_HIDE);
        }
    } else if (node->kind == ER_UI_NODE_WEBVIEW) {
        node->webview = er_ui_webview_create(app, node, error);
        if (!node->webview) {
            DeleteObject(node->font);
            er_ui_destroy_node_assets(node);
            free(node->id);
            free(node->page);
            free(node->text);
            free(node->preset);
            free(node->asset_path);
            free(node->icon_path);
            memset(node, 0, sizeof(*node));
            return NULL;
        }
    }

    app->node_count++;
    if (app->hwnd) {
        er_ui_invalidate_node(app, node);
    }
    return node;
}

bool er_ui_app_remove_node(ErUiApp *app, const char *id, ErError *error) {
    size_t i;
    ErUiNode *node;

    if (!app || !id || id[0] == '\0') {
        er_error_set(error, 0, 0, "UI remove target id is required");
        return false;
    }

    for (i = 0; i < app->node_count; ++i) {
        if (app->nodes[i].id && strcmp(app->nodes[i].id, id) == 0) {
            break;
        }
    }

    if (i >= app->node_count) {
        er_error_set(error, 0, 0, "Could not find node '%s' for removal", id);
        return false;
    }

    node = &app->nodes[i];
    er_ui_invalidate_node(app, node);

    if (app->hovered_control_id == node->control_id) {
        er_ui_set_hovered_control(app, 0);
    }
    if (app->pressed_control_id == node->control_id) {
        er_ui_set_pressed_control(app, 0);
    }
    if (app->focused_control_id == node->control_id) {
        er_ui_set_focused_control(app, 0);
    }

    if (node->webview) {
        er_ui_webview_destroy(node->webview);
        node->webview = NULL;
        node->hwnd = NULL;
    } else if (node->hwnd && IsWindow(node->hwnd)) {
        DestroyWindow(node->hwnd);
        node->hwnd = NULL;
    }

    DeleteObject(node->font);
    DeleteObject(node->brush);
    er_ui_destroy_node_assets(node);
    free(node->id);
    free(node->page);
    free(node->text);
    free(node->preset);
    free(node->asset_path);
    free(node->icon_path);
    memset(node, 0, sizeof(*node));

    if (i + 1 < app->node_count) {
        app->nodes[i] = app->nodes[app->node_count - 1];
        memset(&app->nodes[app->node_count - 1], 0, sizeof(app->nodes[app->node_count - 1]));
    }
    app->node_count--;

    if (app->hwnd) {
        InvalidateRect(app->hwnd, NULL, TRUE);
    }
    return true;
}

void er_ui_app_show_page(ErUiApp *app, const char *page) {
    size_t i;

    if (!app) {
        return;
    }

    free(app->current_page);
    app->current_page = er_ui_dup(page ? page : "main");

    for (i = 0; i < app->node_count; ++i) {
        er_ui_update_node_visibility(app, &app->nodes[i]);
    }

    if (app->hovered_control_id != 0) {
        ErUiNode *hovered = er_ui_find_node_by_control_id(app, app->hovered_control_id);
        if (!hovered || !er_ui_node_is_on_current_page(app, hovered)) {
            er_ui_set_hovered_control(app, 0);
        }
    }
    if (app->pressed_control_id != 0) {
        ErUiNode *pressed = er_ui_find_node_by_control_id(app, app->pressed_control_id);
        if (!pressed || !er_ui_node_is_on_current_page(app, pressed)) {
            er_ui_set_pressed_control(app, 0);
        }
    }
    if (app->focused_control_id != 0) {
        ErUiNode *focused = er_ui_find_node_by_control_id(app, app->focused_control_id);
        if (!focused || !er_ui_node_is_on_current_page(app, focused)) {
            er_ui_set_focused_control(app, 0);
        }
    }

    if (app->hwnd) {
        InvalidateRect(app->hwnd, NULL, TRUE);
    }
}

bool er_ui_app_set_text(ErUiApp *app, const char *id, const char *text) {
    ErUiNode *node = er_ui_find_node_by_id(app, id);
    ErError error;

    if (!node) {
        return false;
    }
    free(node->text);
    node->text = er_ui_dup(text ? text : "");
    if (!node->text) {
        return false;
    }
    if (node->kind == ER_UI_NODE_WEBVIEW && node->webview) {
        er_error_clear(&error);
        if (!er_ui_webview_navigate(node->webview, node->text, &error)) {
            return false;
        }
        er_ui_webview_update_rect(node->webview);
        return true;
    }
    if (node->hwnd) {
        SetWindowTextA(node->hwnd, node->text ? node->text : "");
    }
    er_ui_invalidate_node(app, node);
    return true;
}

bool er_ui_app_set_image(ErUiApp *app, const char *id, const char *path, ErError *error) {
    ErUiNode *node;
    char *copy = NULL;

    if (!app || !id || id[0] == '\0') {
        er_error_set(error, 0, 0, "UI image target id is required");
        return false;
    }

    node = er_ui_find_node_by_id(app, id);
    if (!node) {
        er_error_set(error, 0, 0, "Could not find image target '%s'", id);
        return false;
    }

    if (node->kind != ER_UI_NODE_IMAGE && node->kind != ER_UI_NODE_BUTTON) {
        er_error_set(error, 0, 0, "Node '%s' does not support dynamic image updates", id);
        return false;
    }

    copy = er_ui_dup(path ? path : "");
    if (path && path[0] != '\0' && !copy) {
        er_error_set(error, 0, 0, "Out of memory while updating node image");
        return false;
    }

    er_ui_invalidate_node(app, node);
    if (node->kind == ER_UI_NODE_IMAGE) {
        free(node->asset_path);
        node->asset_path = copy;
    } else {
        free(node->icon_path);
        node->icon_path = copy;
    }

    if (!er_ui_node_load_visual_assets(node, error)) {
        return false;
    }

    er_ui_invalidate_node(app, node);
    return true;
}

bool er_ui_app_set_bounds(ErUiApp *app, const char *id, int x, int y, int w, int h, ErError *error) {
    ErUiNode *node;

    if (!app || !id || id[0] == '\0') {
        er_error_set(error, 0, 0, "UI bounds target id is required");
        return false;
    }

    node = er_ui_find_node_by_id(app, id);
    if (!node) {
        er_error_set(error, 0, 0, "Could not find bounds target '%s'", id);
        return false;
    }

    er_ui_invalidate_node(app, node);
    node->base_x = x;
    node->base_y = y;
    node->base_w = w > 0 ? w : 1;
    node->base_h = h > 0 ? h : 1;
    er_ui_rescale_node_layout(app, node);

    if (node->kind == ER_UI_NODE_INPUT && node->hwnd) {
        er_ui_position_input_control(node);
    } else if (node->kind == ER_UI_NODE_WEBVIEW && node->webview) {
        er_ui_position_webview_control(node);
    }

    er_ui_invalidate_node(app, node);
    return true;
}

bool er_ui_app_set_timer(
    ErUiApp *app,
    unsigned int timer_id,
    unsigned int interval_ms,
    ErUiTimerCallback callback,
    void *user_data,
    ErError *error
) {
    ErUiTimer *timer;

    if (!app || !app->hwnd) {
        er_error_set(error, 0, 0, "UI timers require an initialized app window");
        return false;
    }
    if (timer_id == 0) {
        er_error_set(error, 0, 0, "Timer id must be greater than zero");
        return false;
    }
    if (interval_ms == 0) {
        er_error_set(error, 0, 0, "Timer interval must be greater than zero");
        return false;
    }
    if (!callback) {
        er_error_set(error, 0, 0, "Timer callback is required");
        return false;
    }

    timer = er_ui_find_timer_by_id(app, timer_id);
    if (!timer) {
        if (!er_ui_grow_timers(app, error)) {
            return false;
        }
        timer = &app->timers[app->timer_count++];
        memset(timer, 0, sizeof(*timer));
        timer->timer_id = timer_id;
        if (app->next_timer_os_id < 2u) {
            app->next_timer_os_id = 2u;
        }
        timer->os_timer_id = app->next_timer_os_id++;
    } else if (timer->os_timer_id != 0) {
        KillTimer(app->hwnd, timer->os_timer_id);
    }

    timer->interval_ms = interval_ms;
    timer->callback = callback;
    timer->user_data = user_data;

    if (SetTimer(app->hwnd, timer->os_timer_id, timer->interval_ms, NULL) == 0) {
        er_error_set(error, 0, 0, "Could not start UI timer %u", timer_id);
        return false;
    }

    return true;
}

void er_ui_app_clear_timer(ErUiApp *app, unsigned int timer_id) {
    size_t i;

    if (!app || timer_id == 0) {
        return;
    }

    for (i = 0; i < app->timer_count; ++i) {
        if (app->timers[i].timer_id == timer_id) {
            if (app->hwnd && app->timers[i].os_timer_id != 0) {
                KillTimer(app->hwnd, app->timers[i].os_timer_id);
            }
            if (i + 1 < app->timer_count) {
                app->timers[i] = app->timers[app->timer_count - 1];
            }
            app->timer_count--;
            return;
        }
    }
}

bool er_ui_app_add_shortcut(ErUiApp *app, const char *combo, const char *target_id, ErError *error) {
    ErUiShortcut shortcut;
    char *target_copy;
    size_t i;

    if (!app) {
        er_error_set(error, 0, 0, "UI shortcuts require an initialized app");
        return false;
    }
    if (!target_id || target_id[0] == '\0') {
        er_error_set(error, 0, 0, "Shortcut target id cannot be empty");
        return false;
    }
    if (!er_ui_parse_shortcut_combo(combo, &shortcut, error)) {
        return false;
    }

    target_copy = er_ui_dup(target_id);
    if (!target_copy) {
        er_error_set(error, 0, 0, "Out of memory while storing shortcut target");
        return false;
    }

    for (i = 0; i < app->shortcut_count; ++i) {
        ErUiShortcut *existing = &app->shortcuts[i];
        if (existing->virtual_key == shortcut.virtual_key &&
            existing->ctrl == shortcut.ctrl &&
            existing->alt == shortcut.alt &&
            existing->shift == shortcut.shift) {
            free(existing->target_id);
            existing->target_id = target_copy;
            return true;
        }
    }

    if (!er_ui_grow_shortcuts(app, error)) {
        free(target_copy);
        return false;
    }

    shortcut.target_id = target_copy;
    app->shortcuts[app->shortcut_count++] = shortcut;
    return true;
}

bool er_ui_app_webview_navigate(ErUiApp *app, const char *id, const char *url) {
    ErUiNode *node = er_ui_find_webview_node_by_id(app, id);
    ErError error;

    if (!node) {
        return false;
    }

    free(node->text);
    node->text = er_ui_dup(url ? url : "");
    if (!node->text) {
        return false;
    }

    er_error_clear(&error);
    if (!er_ui_webview_navigate(node->webview, node->text, &error)) {
        return false;
    }

    er_ui_webview_update_rect(node->webview);
    er_ui_invalidate_node(app, node);
    return true;
}

bool er_ui_app_webview_back(ErUiApp *app, const char *id) {
    ErUiNode *node = er_ui_find_webview_node_by_id(app, id);
    (void) app;
    if (!node) {
        return false;
    }
    if (node->webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        return node->webview->modern_webview &&
            SUCCEEDED(node->webview->modern_webview->lpVtbl->GoBack(node->webview->modern_webview));
    }
    return SUCCEEDED(IWebBrowser2_GoBack(node->webview->browser));
}

bool er_ui_app_webview_forward(ErUiApp *app, const char *id) {
    ErUiNode *node = er_ui_find_webview_node_by_id(app, id);
    (void) app;
    if (!node) {
        return false;
    }
    if (node->webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        return node->webview->modern_webview &&
            SUCCEEDED(node->webview->modern_webview->lpVtbl->GoForward(node->webview->modern_webview));
    }
    return SUCCEEDED(IWebBrowser2_GoForward(node->webview->browser));
}

bool er_ui_app_webview_reload(ErUiApp *app, const char *id) {
    ErUiNode *node = er_ui_find_webview_node_by_id(app, id);
    (void) app;
    if (!node) {
        return false;
    }
    if (node->webview->backend_kind == ER_UI_WEBVIEW_BACKEND_WEBVIEW2) {
        return node->webview->modern_webview &&
            SUCCEEDED(node->webview->modern_webview->lpVtbl->Reload(node->webview->modern_webview));
    }
    return SUCCEEDED(IWebBrowser2_Refresh(node->webview->browser));
}

bool er_ui_app_webview_run_script(ErUiApp *app, const char *id, const char *script) {
    ErUiNode *node = er_ui_find_webview_node_by_id(app, id);
    ErError error;
    (void) app;

    if (!node) {
        return false;
    }

    er_error_clear(&error);
    return er_ui_webview_run_script_internal(node->webview, script, &error);
}

bool er_ui_app_animation_play(
    ErUiApp *app,
    const char *id,
    const char *preset,
    int duration_ms,
    bool loop,
    ErError *error
) {
    ErUiNode *node = er_ui_find_node_by_id(app, id);

    if (!node) {
        er_error_set(error, 0, 0, "Could not find animation target '%s'", id ? id : "");
        return false;
    }

    er_ui_animation_stop_node(node);
    if (!preset || preset[0] == '\0') {
        er_error_set(error, 0, 0, "Animation preset is required");
        return false;
    }

    if (strcmp(preset, "float") == 0) {
        node->animation_kind = ER_UI_ANIMATION_PRESET_FLOAT;
        node->animation_amplitude = 10.0;
    } else if (strcmp(preset, "drift") == 0 || strcmp(preset, "driftX") == 0) {
        node->animation_kind = ER_UI_ANIMATION_PRESET_DRIFT;
        node->animation_amplitude = 12.0;
    } else if (strcmp(preset, "pulse") == 0 || strcmp(preset, "breathe") == 0) {
        node->animation_kind = ER_UI_ANIMATION_PRESET_PULSE;
        node->animation_amplitude = 0.045;
    } else {
        er_error_set(error, 0, 0, "Unknown animation preset '%s'", preset);
        return false;
    }

    node->animation_duration_ms = duration_ms > 0 ? duration_ms : 1800;
    node->animation_loop = loop;
    node->animation_active = true;
    node->animation_property = ER_UI_ANIMATION_PROPERTY_NONE;
    node->animation_keyframe_count = 0;
    er_ui_animation_capture_origin(node);
    if (app && app->hwnd) {
        InvalidateRect(app->hwnd, NULL, FALSE);
    }
    return true;
}

bool er_ui_app_animation_oscillate(
    ErUiApp *app,
    const char *id,
    const char *property,
    double amplitude,
    int duration_ms,
    bool loop,
    ErError *error
) {
    ErUiNode *node = er_ui_find_node_by_id(app, id);
    ErUiAnimationProperty animation_property;

    if (!node) {
        er_error_set(error, 0, 0, "Could not find animation target '%s'", id ? id : "");
        return false;
    }

    animation_property = er_ui_animation_property_from_text(property);
    if (animation_property == ER_UI_ANIMATION_PROPERTY_NONE) {
        er_error_set(error, 0, 0, "Unsupported animation property '%s'", property ? property : "");
        return false;
    }

    er_ui_animation_stop_node(node);
    node->animation_kind = ER_UI_ANIMATION_OSCILLATE;
    node->animation_property = animation_property;
    node->animation_amplitude = amplitude;
    node->animation_duration_ms = duration_ms > 0 ? duration_ms : 1800;
    node->animation_loop = loop;
    node->animation_active = true;
    node->animation_keyframe_count = 0;
    er_ui_animation_capture_origin(node);
    if (app && app->hwnd) {
        InvalidateRect(app->hwnd, NULL, FALSE);
    }
    return true;
}

bool er_ui_app_animation_keyframes(
    ErUiApp *app,
    const char *id,
    const char *property,
    const char *frames_text,
    int duration_ms,
    bool loop,
    ErError *error
) {
    ErUiNode *node = er_ui_find_node_by_id(app, id);
    ErUiAnimationProperty animation_property;

    if (!node) {
        er_error_set(error, 0, 0, "Could not find animation target '%s'", id ? id : "");
        return false;
    }

    animation_property = er_ui_animation_property_from_text(property);
    if (animation_property == ER_UI_ANIMATION_PROPERTY_NONE) {
        er_error_set(error, 0, 0, "Unsupported animation property '%s'", property ? property : "");
        return false;
    }

    er_ui_animation_stop_node(node);
    node->animation_kind = ER_UI_ANIMATION_KEYFRAMES;
    node->animation_property = animation_property;
    node->animation_duration_ms = duration_ms > 0 ? duration_ms : 1800;
    node->animation_loop = loop;
    node->animation_active = true;
    er_ui_animation_capture_origin(node);
    if (!er_ui_parse_keyframe_text(node, frames_text, error)) {
        node->animation_active = false;
        node->animation_kind = ER_UI_ANIMATION_NONE;
        node->animation_property = ER_UI_ANIMATION_PROPERTY_NONE;
        return false;
    }
    if (app && app->hwnd) {
        InvalidateRect(app->hwnd, NULL, FALSE);
    }
    return true;
}

bool er_ui_app_animation_stop(ErUiApp *app, const char *id, ErError *error) {
    ErUiNode *node = er_ui_find_node_by_id(app, id);

    if (!node) {
        er_error_set(error, 0, 0, "Could not find animation target '%s'", id ? id : "");
        return false;
    }

    er_ui_animation_stop_node(node);
    if (node->kind == ER_UI_NODE_INPUT && node->hwnd) {
        er_ui_position_input_control(node);
    } else if (node->kind == ER_UI_NODE_WEBVIEW && node->webview) {
        er_ui_position_webview_control(node);
    }
    if (app && app->hwnd) {
        InvalidateRect(app->hwnd, NULL, FALSE);
    }
    return true;
}

char *er_ui_node_dup_text(ErUiNode *node) {
    if (!node) {
        return NULL;
    }

    if (node->kind == ER_UI_NODE_INPUT && node->hwnd) {
        return er_ui_dup_window_text(node->hwnd);
    }

    return er_ui_dup(node->text ? node->text : "");
}

int er_ui_app_run(ErUiApp *app) {
    MSG msg;

    if (!app || !app->hwnd) {
        return 1;
    }

    ShowWindow(app->hwnd, SW_SHOWNORMAL);
    UpdateWindow(app->hwnd);
    InvalidateRect(app->hwnd, NULL, TRUE);
    RedrawWindow(
        app->hwnd,
        NULL,
        NULL,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW
    );
    UpdateWindow(app->hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int) msg.wParam;
}

#else
bool er_ui_app_init(
    ErUiApp *app,
    const char *title,
    int x,
    int y,
    int w,
    int h,
    bool resizable,
    ErError *error
) {
    (void) app;
    (void) title;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) resizable;
    er_error_set(error, 0, 0, "Erire runtime currently targets Windows first");
    return false;
}

void er_ui_app_destroy(ErUiApp *app) { (void) app; }
void er_ui_app_set_title(ErUiApp *app, const char *title) { (void) app; (void) title; }
bool er_ui_app_set_icon(ErUiApp *app, const char *path, ErError *error) {
    (void) app;
    (void) path;
    er_error_set(error, 0, 0, "Erire window icons currently target Windows first");
    return false;
}
void er_ui_app_set_background(ErUiApp *app, unsigned int rgb) { (void) app; (void) rgb; }
ErUiNode *er_ui_app_find_node(ErUiApp *app, const char *id) {
    (void) app;
    (void) id;
    return NULL;
}
ErUiNode *er_ui_app_add_node(ErUiApp *app, const ErUiNodeSpec *spec, ErError *error) {
    (void) app;
    (void) spec;
    er_error_set(error, 0, 0, "Erire runtime currently targets Windows first");
    return NULL;
}
bool er_ui_app_remove_node(ErUiApp *app, const char *id, ErError *error) {
    (void) app;
    (void) id;
    er_error_set(error, 0, 0, "Erire live node removal currently targets Windows first");
    return false;
}
void er_ui_app_show_page(ErUiApp *app, const char *page) { (void) app; (void) page; }
bool er_ui_app_set_text(ErUiApp *app, const char *id, const char *text) {
    (void) app;
    (void) id;
    (void) text;
    return false;
}
bool er_ui_app_set_image(ErUiApp *app, const char *id, const char *path, ErError *error) {
    (void) app;
    (void) id;
    (void) path;
    er_error_set(error, 0, 0, "Erire image updates currently target Windows first");
    return false;
}
bool er_ui_app_set_bounds(ErUiApp *app, const char *id, int x, int y, int w, int h, ErError *error) {
    (void) app;
    (void) id;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    er_error_set(error, 0, 0, "Erire dynamic layout updates currently target Windows first");
    return false;
}
bool er_ui_app_set_timer(
    ErUiApp *app,
    unsigned int timer_id,
    unsigned int interval_ms,
    ErUiTimerCallback callback,
    void *user_data,
    ErError *error
) { 
    (void) app;
    (void) timer_id;
    (void) interval_ms;
    (void) callback;
    (void) user_data;
    er_error_set(error, 0, 0, "Erire timers currently target Windows first");
    return false;
}
void er_ui_app_clear_timer(ErUiApp *app, unsigned int timer_id) {
    (void) app;
    (void) timer_id;
}
bool er_ui_app_add_shortcut(ErUiApp *app, const char *combo, const char *target_id, ErError *error) {
    (void) app;
    (void) combo;
    (void) target_id;
    er_error_set(error, 0, 0, "Erire shortcuts currently target Windows first");
    return false;
}
bool er_ui_app_webview_navigate(ErUiApp *app, const char *id, const char *url) {
    (void) app;
    (void) id;
    (void) url;
    return false;
}
bool er_ui_app_webview_back(ErUiApp *app, const char *id) {
    (void) app;
    (void) id;
    return false;
}
bool er_ui_app_webview_forward(ErUiApp *app, const char *id) {
    (void) app;
    (void) id;
    return false;
}
bool er_ui_app_webview_reload(ErUiApp *app, const char *id) {
    (void) app;
    (void) id;
    return false;
}
bool er_ui_app_webview_run_script(ErUiApp *app, const char *id, const char *script) {
    (void) app;
    (void) id;
    (void) script;
    return false;
}
bool er_ui_app_animation_play(
    ErUiApp *app,
    const char *id,
    const char *preset,
    int duration_ms,
    bool loop,
    ErError *error
) {
    (void) app;
    (void) id;
    (void) preset;
    (void) duration_ms;
    (void) loop;
    er_error_set(error, 0, 0, "Erire animations currently target Windows first");
    return false;
}
bool er_ui_app_animation_oscillate(
    ErUiApp *app,
    const char *id,
    const char *property,
    double amplitude,
    int duration_ms,
    bool loop,
    ErError *error
) {
    (void) app;
    (void) id;
    (void) property;
    (void) amplitude;
    (void) duration_ms;
    (void) loop;
    er_error_set(error, 0, 0, "Erire animations currently target Windows first");
    return false;
}
bool er_ui_app_animation_keyframes(
    ErUiApp *app,
    const char *id,
    const char *property,
    const char *frames_text,
    int duration_ms,
    bool loop,
    ErError *error
) { 
    (void) app;
    (void) id;
    (void) property;
    (void) frames_text;
    (void) duration_ms;
    (void) loop;
    er_error_set(error, 0, 0, "Erire animations currently target Windows first");
    return false;
}
bool er_ui_app_animation_stop(ErUiApp *app, const char *id, ErError *error) {
    (void) app;
    (void) id;
    er_error_set(error, 0, 0, "Erire animations currently target Windows first");
    return false;
}
char *er_ui_node_dup_text(ErUiNode *node) { (void) node; return NULL; }
int er_ui_app_run(ErUiApp *app) { (void) app; return 1; }
#endif
