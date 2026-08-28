#ifndef ERIRE_STUDIO_APP_H
#define ERIRE_STUDIO_APP_H

#include <stdbool.h>
#include <windows.h>

#include "project.h"
#include "runner.h"
#include "settings.h"
#include "webview2_host.h"

typedef struct StudioApp {
    HINSTANCE instance;
    HWND hwnd;
    StudioSettings settings;
    StudioProject project;
    StudioRunner runner;
    StudioWebViewHost webview;
    bool frontend_ready;
    bool webview_navigated;
    bool startup_project_restored;
    char module_dir[STUDIO_MAX_PATH];
    char frontend_index[STUDIO_MAX_PATH];
    char active_file[STUDIO_MAX_PATH];
} StudioApp;

int studio_app_run(HINSTANCE instance, int show_command);
void studio_app_dispatch_message(StudioApp *app, const char *json);
bool studio_app_post_json(StudioApp *app, const char *type, const char *payload_json);
bool studio_app_post_error(StudioApp *app, const char *message);
bool studio_app_open_project(StudioApp *app, const char *path);
bool studio_app_refresh_project(StudioApp *app);
bool studio_app_open_file(StudioApp *app, const char *path);
bool studio_app_send_settings(StudioApp *app);
void studio_app_set_active_file(StudioApp *app, const char *path);
void studio_app_send_process_state(StudioApp *app, const char *state, StudioProcessKind kind);

#endif
