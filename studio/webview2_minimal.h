#ifndef ERIRE_STUDIO_WEBVIEW2_MINIMAL_H
#define ERIRE_STUDIO_WEBVIEW2_MINIMAL_H

#include <windows.h>
#include <oleauto.h>

typedef struct EventRegistrationToken {
    INT64 value;
} EventRegistrationToken;

typedef struct ICoreWebView2 ICoreWebView2;
typedef struct ICoreWebView2Controller ICoreWebView2Controller;
typedef struct ICoreWebView2Environment ICoreWebView2Environment;
typedef struct ICoreWebView2Settings ICoreWebView2Settings;
typedef struct ICoreWebView2WebMessageReceivedEventArgs ICoreWebView2WebMessageReceivedEventArgs;
typedef IUnknown ICoreWebView2EnvironmentOptions;

typedef struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
typedef struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
typedef struct ICoreWebView2WebMessageReceivedEventHandler
    ICoreWebView2WebMessageReceivedEventHandler;

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

typedef struct ICoreWebView2WebMessageReceivedEventHandlerVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ICoreWebView2WebMessageReceivedEventHandler *self,
        REFIID riid,
        void **object
    );
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2WebMessageReceivedEventHandler *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2WebMessageReceivedEventHandler *self);
    HRESULT (STDMETHODCALLTYPE *Invoke)(
        ICoreWebView2WebMessageReceivedEventHandler *self,
        ICoreWebView2 *sender,
        ICoreWebView2WebMessageReceivedEventArgs *args
    );
} ICoreWebView2WebMessageReceivedEventHandlerVtbl;

struct ICoreWebView2WebMessageReceivedEventHandler {
    const ICoreWebView2WebMessageReceivedEventHandlerVtbl *lpVtbl;
};

typedef struct ICoreWebView2EnvironmentVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Environment *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Environment *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Environment *self);
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
    HRESULT (STDMETHODCALLTYPE *add_NewBrowserVersionAvailable)(
        ICoreWebView2Environment *self,
        IUnknown *event_handler,
        EventRegistrationToken *token
    );
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
    HRESULT (STDMETHODCALLTYPE *get_IsVisible)(ICoreWebView2Controller *self, BOOL *is_visible);
    HRESULT (STDMETHODCALLTYPE *put_IsVisible)(ICoreWebView2Controller *self, BOOL is_visible);
    HRESULT (STDMETHODCALLTYPE *get_Bounds)(ICoreWebView2Controller *self, RECT *bounds);
    HRESULT (STDMETHODCALLTYPE *put_Bounds)(ICoreWebView2Controller *self, RECT bounds);
    HRESULT (STDMETHODCALLTYPE *get_ZoomFactor)(ICoreWebView2Controller *self, double *zoom_factor);
    HRESULT (STDMETHODCALLTYPE *put_ZoomFactor)(ICoreWebView2Controller *self, double zoom_factor);
    void *add_ZoomFactorChanged;
    void *remove_ZoomFactorChanged;
    void *SetBoundsAndZoomFactor;
    void *MoveFocus;
    void *add_MoveFocusRequested;
    void *remove_MoveFocusRequested;
    void *add_GotFocus;
    void *remove_GotFocus;
    void *add_LostFocus;
    void *remove_LostFocus;
    void *add_AcceleratorKeyPressed;
    void *remove_AcceleratorKeyPressed;
    HRESULT (STDMETHODCALLTYPE *get_ParentWindow)(ICoreWebView2Controller *self, HWND *parent_window);
    HRESULT (STDMETHODCALLTYPE *put_ParentWindow)(ICoreWebView2Controller *self, HWND parent_window);
    HRESULT (STDMETHODCALLTYPE *NotifyParentWindowPositionChanged)(ICoreWebView2Controller *self);
    HRESULT (STDMETHODCALLTYPE *Close)(ICoreWebView2Controller *self);
    HRESULT (STDMETHODCALLTYPE *get_CoreWebView2)(ICoreWebView2Controller *self, ICoreWebView2 **core_webview2);
} ICoreWebView2ControllerVtbl;

struct ICoreWebView2Controller {
    const ICoreWebView2ControllerVtbl *lpVtbl;
};

typedef struct ICoreWebView2SettingsVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Settings *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Settings *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Settings *self);
    HRESULT (STDMETHODCALLTYPE *get_IsScriptEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_IsScriptEnabled)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_IsWebMessageEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_IsWebMessageEnabled)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_AreDefaultScriptDialogsEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_AreDefaultScriptDialogsEnabled)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_IsStatusBarEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_IsStatusBarEnabled)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_AreDevToolsEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_AreDevToolsEnabled)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_AreDefaultContextMenusEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_AreDefaultContextMenusEnabled)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_AreHostObjectsAllowed)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_AreHostObjectsAllowed)(ICoreWebView2Settings *self, BOOL value);
    HRESULT (STDMETHODCALLTYPE *get_IsZoomControlEnabled)(ICoreWebView2Settings *self, BOOL *value);
    HRESULT (STDMETHODCALLTYPE *put_IsZoomControlEnabled)(ICoreWebView2Settings *self, BOOL value);
    void *get_IsBuiltInErrorPageEnabled;
    void *put_IsBuiltInErrorPageEnabled;
} ICoreWebView2SettingsVtbl;

struct ICoreWebView2Settings {
    const ICoreWebView2SettingsVtbl *lpVtbl;
};

typedef struct ICoreWebView2WebMessageReceivedEventArgsVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2WebMessageReceivedEventArgs *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2WebMessageReceivedEventArgs *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2WebMessageReceivedEventArgs *self);
    HRESULT (STDMETHODCALLTYPE *get_Source)(ICoreWebView2WebMessageReceivedEventArgs *self, LPWSTR *value);
    HRESULT (STDMETHODCALLTYPE *get_WebMessageAsJson)(ICoreWebView2WebMessageReceivedEventArgs *self, LPWSTR *value);
    HRESULT (STDMETHODCALLTYPE *TryGetWebMessageAsString)(ICoreWebView2WebMessageReceivedEventArgs *self, LPWSTR *value);
} ICoreWebView2WebMessageReceivedEventArgsVtbl;

struct ICoreWebView2WebMessageReceivedEventArgs {
    const ICoreWebView2WebMessageReceivedEventArgsVtbl *lpVtbl;
};

typedef struct ICoreWebView2Vtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2 *self, REFIID riid, void **object);
    ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2 *self);
    ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2 *self);
    HRESULT (STDMETHODCALLTYPE *get_Settings)(ICoreWebView2 *self, ICoreWebView2Settings **settings);
    HRESULT (STDMETHODCALLTYPE *get_Source)(ICoreWebView2 *self, LPWSTR *uri);
    HRESULT (STDMETHODCALLTYPE *Navigate)(ICoreWebView2 *self, LPCWSTR uri);
    HRESULT (STDMETHODCALLTYPE *NavigateToString)(ICoreWebView2 *self, LPCWSTR html_content);
    void *add_NavigationStarting;
    void *remove_NavigationStarting;
    void *add_ContentLoading;
    void *remove_ContentLoading;
    void *add_SourceChanged;
    void *remove_SourceChanged;
    void *add_HistoryChanged;
    void *remove_HistoryChanged;
    void *add_NavigationCompleted;
    void *remove_NavigationCompleted;
    void *add_FrameNavigationStarting;
    void *remove_FrameNavigationStarting;
    void *add_FrameNavigationCompleted;
    void *remove_FrameNavigationCompleted;
    void *add_ScriptDialogOpening;
    void *remove_ScriptDialogOpening;
    void *add_PermissionRequested;
    void *remove_PermissionRequested;
    void *add_ProcessFailed;
    void *remove_ProcessFailed;
    void *AddScriptToExecuteOnDocumentCreated;
    void *RemoveScriptToExecuteOnDocumentCreated;
    void *ExecuteScript;
    void *CapturePreview;
    HRESULT (STDMETHODCALLTYPE *Reload)(ICoreWebView2 *self);
    HRESULT (STDMETHODCALLTYPE *PostWebMessageAsJson)(ICoreWebView2 *self, LPCWSTR web_message_as_json);
    HRESULT (STDMETHODCALLTYPE *PostWebMessageAsString)(ICoreWebView2 *self, LPCWSTR web_message_as_string);
    HRESULT (STDMETHODCALLTYPE *add_WebMessageReceived)(
        ICoreWebView2 *self,
        ICoreWebView2WebMessageReceivedEventHandler *event_handler,
        EventRegistrationToken *token
    );
    void *remove_WebMessageReceived;
    void *CallDevToolsProtocolMethod;
    void *get_BrowserProcessId;
    void *get_CanGoBack;
    void *get_CanGoForward;
    void *GoBack;
    void *GoForward;
    void *GetDevToolsProtocolEventReceiver;
    void *Stop;
    void *add_NewWindowRequested;
    void *remove_NewWindowRequested;
    void *add_DocumentTitleChanged;
    void *remove_DocumentTitleChanged;
    void *get_DocumentTitle;
    void *AddHostObjectToScript;
    void *RemoveHostObjectFromScript;
    void *OpenDevToolsWindow;
    void *add_ContainsFullScreenElementChanged;
    void *remove_ContainsFullScreenElementChanged;
    void *get_ContainsFullScreenElement;
    void *add_WebResourceRequested;
    void *remove_WebResourceRequested;
    void *AddWebResourceRequestedFilter;
    void *RemoveWebResourceRequestedFilter;
    void *add_WindowCloseRequested;
    void *remove_WindowCloseRequested;
} ICoreWebView2Vtbl;

struct ICoreWebView2 {
    const ICoreWebView2Vtbl *lpVtbl;
};

#endif
