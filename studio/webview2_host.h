#ifndef ERIRE_STUDIO_WEBVIEW2_HOST_H
#define ERIRE_STUDIO_WEBVIEW2_HOST_H

#include <stdbool.h>
#include <windows.h>

#include "util.h"
#include "webview2_minimal.h"

typedef void (*StudioWebViewMessageHandler)(void *user_data, const char *message);

typedef struct StudioWebViewHost {
    HWND parent;
    HMODULE loader;
    ICoreWebView2Environment *environment;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *webview;
    StudioWebViewMessageHandler on_message;
    void *user_data;
    bool ready;
    char browser_dir[STUDIO_MAX_PATH];
    char user_data_dir[STUDIO_MAX_PATH];
} StudioWebViewHost;

bool studio_webview_host_init(
    StudioWebViewHost *host,
    HWND parent,
    const char *browser_dir,
    const char *user_data_dir,
    StudioWebViewMessageHandler on_message,
    void *user_data
);
void studio_webview_host_resize(StudioWebViewHost *host);
void studio_webview_host_notify_parent_window_position_changed(StudioWebViewHost *host);
bool studio_webview_host_navigate_file(StudioWebViewHost *host, const char *path);
bool studio_webview_host_post_json(StudioWebViewHost *host, const char *json);
void studio_webview_host_dispose(StudioWebViewHost *host);

#endif
