#include "app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge.h"
#include "debug_log.h"
#include "fileio.h"
#include "json.h"
#include "outline.h"

enum {
    IDM_FILE_NEW_PROJECT = 1001,
    IDM_FILE_OPEN_PROJECT,
    IDM_FILE_OPEN_FILE,
    IDM_FILE_SAVE,
    IDM_FILE_SAVE_AS,
    IDM_FILE_SAVE_ALL,
    IDM_FILE_CLOSE_PROJECT,
    IDM_FILE_EXIT,
    IDM_EDIT_UNDO,
    IDM_EDIT_REDO,
    IDM_EDIT_CUT,
    IDM_EDIT_COPY,
    IDM_EDIT_PASTE,
    IDM_EDIT_SELECT_ALL,
    IDM_EDIT_FIND,
    IDM_EDIT_REPLACE,
    IDM_VIEW_TOGGLE_EXPLORER,
    IDM_VIEW_TOGGLE_OUTLINE,
    IDM_VIEW_TOGGLE_BOTTOM,
    IDM_VIEW_RESET_LAYOUT,
    IDM_RUN_PROJECT,
    IDM_RUN_PYTHON,
    IDM_RUN_STOP,
    IDM_BUILD_CHECK,
    IDM_BUILD_PROJECT,
    IDM_TOOLS_RELOAD_PROJECT,
    IDM_TOOLS_OPEN_PROJECT_FOLDER,
    IDM_TOOLS_REFRESH_SYMBOLS,
    IDM_TOOLS_CLEAR_OUTPUT,
    IDM_SETTINGS_OPEN,
    IDM_HELP_ABOUT,
    STUDIO_WM_BOOTSTRAP_WEBVIEW = WM_APP + 10
};

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif

#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

typedef HRESULT (WINAPI *StudioDwmSetWindowAttributeFn)(HWND hwnd, DWORD attribute, LPCVOID value, DWORD value_size);

static const char *studio_kind_label(StudioProcessKind kind) {
    switch (kind) {
        case STUDIO_PROCESS_RUN: return "run";
        case STUDIO_PROCESS_CHECK: return "check";
        case STUDIO_PROCESS_BUILD: return "build";
        case STUDIO_PROCESS_PYTHON: return "python";
        case STUDIO_PROCESS_TERMINAL: return "terminal";
        default: return "idle";
    }
}

static StudioApp *studio_app_from_window(HWND hwnd) {
    return (StudioApp *) GetWindowLongPtrA(hwnd, GWLP_USERDATA);
}

static void studio_app_webview_message(void *user_data, const char *message) {
    StudioApp *app = (StudioApp *) user_data;
    studio_app_dispatch_message(app, message);
}

static void studio_app_apply_window_chrome(HWND hwnd) {
    HMODULE dwmapi;
    StudioDwmSetWindowAttributeFn set_window_attribute;
    FARPROC set_window_attribute_proc;
    BOOL dark_mode = TRUE;
    DWORD caption_color = RGB(10, 10, 12);
    DWORD text_color = RGB(232, 238, 244);
    DWORD border_color = RGB(18, 22, 29);
    int corner_preference = 1;

    if (!hwnd) {
        return;
    }

    dwmapi = LoadLibraryA("dwmapi.dll");
    if (!dwmapi) {
        return;
    }
    set_window_attribute_proc = GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    memcpy(&set_window_attribute, &set_window_attribute_proc, sizeof(set_window_attribute));
    if (!set_window_attribute) {
        FreeLibrary(dwmapi);
        return;
    }

    set_window_attribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode, (DWORD) sizeof(dark_mode));
    set_window_attribute(hwnd, DWMWA_CAPTION_COLOR, &caption_color, (DWORD) sizeof(caption_color));
    set_window_attribute(hwnd, DWMWA_TEXT_COLOR, &text_color, (DWORD) sizeof(text_color));
    set_window_attribute(hwnd, DWMWA_BORDER_COLOR, &border_color, (DWORD) sizeof(border_color));
    set_window_attribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_preference, (DWORD) sizeof(corner_preference));
    FreeLibrary(dwmapi);
}

static void studio_app_finish_runner(StudioApp *app) {
    studio_debug_log_write("studio_app_finish_runner entered");
    if (app->runner.thread_handle) {
        WaitForSingleObject(app->runner.thread_handle, 2000);
        CloseHandle(app->runner.thread_handle);
        app->runner.thread_handle = NULL;
    }
    if (app->runner.process_handle) {
        CloseHandle(app->runner.process_handle);
        app->runner.process_handle = NULL;
    }
    app->runner.running = false;
    app->runner.active_kind = STUDIO_PROCESS_NONE;
}

bool studio_app_post_json(StudioApp *app, const char *type, const char *payload_json) {
    StudioJsonBuilder builder;
    char *json;
    bool ok;

    studio_json_builder_init(&builder);
    studio_json_builder_append(&builder, "{\"type\":");
    studio_json_builder_append_escaped(&builder, type);
    studio_json_builder_append(&builder, ",\"payload\":");
    studio_json_builder_append(&builder, payload_json ? payload_json : "{}");
    studio_json_builder_append(&builder, "}");
    json = studio_json_builder_take(&builder);
    ok = studio_webview_host_post_json(&app->webview, json);
    if (!ok) {
        studio_debug_log_writef("studio_app_post_json failed for type=%s", type ? type : "(null)");
    }
    free(json);
    return ok;
}

bool studio_app_post_error(StudioApp *app, const char *message) {
    StudioJsonBuilder payload;
    char *json;
    bool ok;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"message\":");
    studio_json_builder_append_escaped(&payload, message);
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_debug_log_writef("studio_app_post_error: %s", message ? message : "(null)");
    ok = studio_app_post_json(app, "app:error", json);
    free(json);
    return ok;
}

bool studio_app_open_project(StudioApp *app, const char *path) {
    ErError error;

    studio_debug_log_writef("studio_app_open_project path=%s", path ? path : "(null)");
    er_error_clear(&error);
    if (!studio_project_open(&app->project, path, &error)) {
        studio_debug_log_writef("studio_project_open failed: %s", error.message);
        studio_app_post_error(app, error.message);
        return false;
    }
    studio_debug_log_writef("project opened name=%s root=%s entry=%s", app->project.name, app->project.root_path, app->project.entry_file);
    strncpy(app->settings.last_project, app->project.root_path, sizeof(app->settings.last_project) - 1);
    app->settings.last_project[sizeof(app->settings.last_project) - 1] = '\0';
    studio_settings_push_recent(&app->settings, app->project.root_path);
    studio_settings_save(&app->settings);
    studio_app_refresh_project(app);
    if (app->project.entry_file[0] != '\0') {
        studio_app_open_file(app, app->project.entry_file);
    }
    return true;
}

bool studio_app_refresh_project(StudioApp *app) {
    StudioJsonBuilder payload;
    char *project_tree;
    char *json;

    if (!app->project.is_open) {
        studio_debug_log_write("studio_app_refresh_project with no open project");
        studio_app_post_json(app, "project:loaded", "{\"open\":false}");
        studio_app_post_json(app, "project:tree", "{\"entries\":[]}");
        return true;
    }

    project_tree = studio_project_tree_json(&app->project);
    if (!project_tree) {
        studio_debug_log_write("studio_project_tree_json returned null");
        return false;
    }
    studio_debug_log_writef("studio_app_refresh_project sending tree for %s", app->project.root_path);

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"open\":true,\"name\":");
    studio_json_builder_append_escaped(&payload, app->project.name);
    studio_json_builder_append(&payload, ",\"root\":");
    studio_json_builder_append_escaped(&payload, app->project.root_path);
    studio_json_builder_append(&payload, ",\"entry\":");
    studio_json_builder_append_escaped(&payload, app->project.entry_file);
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "project:loaded", json);
    free(json);

    studio_app_post_json(app, "project:tree", project_tree);
    free(project_tree);
    return true;
}

bool studio_app_open_file(StudioApp *app, const char *path) {
    ErError error;
    char *content = NULL;
    char *outline_json = NULL;
    char name[STUDIO_MAX_NAME];
    const char *base = path;
    StudioJsonBuilder payload;
    char *json;

    studio_debug_log_writef("studio_app_open_file path=%s", path ? path : "(null)");
    er_error_clear(&error);
    if (!studio_project_read_file(path, &content, NULL, &error)) {
        studio_debug_log_writef("studio_project_read_file failed: %s", error.message);
        studio_app_post_error(app, error.message);
        return false;
    }

    if (strrchr(path, '\\')) {
        base = strrchr(path, '\\') + 1;
    }
    if (strrchr(path, '/')) {
        const char *alt = strrchr(path, '/') + 1;
        if (alt > base) {
            base = alt;
        }
    }
    strncpy(name, base, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{");
    studio_json_builder_append(&payload, "\"path\":");
    studio_json_builder_append_escaped(&payload, path);
    studio_json_builder_append(&payload, ",\"name\":");
    studio_json_builder_append_escaped(&payload, name);
    studio_json_builder_append(&payload, ",\"language\":");
    studio_json_builder_append_escaped(&payload, studio_language_from_path(path));
    studio_json_builder_append(&payload, ",\"content\":");
    studio_json_builder_append_escaped(&payload, content ? content : "");
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "file:content", json);
    free(json);

    outline_json = studio_outline_extract_json(path, content);
    if (outline_json) {
        studio_app_post_json(app, "outline:data", outline_json);
        free(outline_json);
    }

    studio_app_set_active_file(app, path);
    studio_debug_log_writef("studio_app_open_file completed path=%s", path);
    free(content);
    return true;
}

bool studio_app_send_settings(StudioApp *app) {
    StudioJsonBuilder payload;
    char *json;
    size_t index;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{");
    studio_json_builder_append(&payload, "\"erireExe\":");
    studio_json_builder_append_escaped(&payload, app->settings.erire_exe);
    studio_json_builder_append(&payload, ",\"pythonExe\":");
    studio_json_builder_append_escaped(&payload, app->settings.python_exe);
    studio_json_builder_append(&payload, ",\"fontName\":");
    studio_json_builder_append_escaped(&payload, app->settings.font_name);
    studio_json_builder_append(&payload, ",\"themeName\":");
    studio_json_builder_append_escaped(&payload, app->settings.theme_name);
    studio_json_builder_appendf(&payload, ",\"fontSize\":%d", app->settings.font_size);
    studio_json_builder_appendf(&payload, ",\"autosave\":%s", app->settings.autosave ? "true" : "false");
    studio_json_builder_appendf(&payload, ",\"softwareRendering\":%s", app->settings.software_rendering ? "true" : "false");
    studio_json_builder_appendf(&payload, ",\"showExplorer\":%s", app->settings.show_explorer ? "true" : "false");
    studio_json_builder_appendf(&payload, ",\"showOutline\":%s", app->settings.show_outline ? "true" : "false");
    studio_json_builder_appendf(&payload, ",\"showBottom\":%s", app->settings.show_bottom ? "true" : "false");
    studio_json_builder_append(&payload, ",\"lastProject\":");
    studio_json_builder_append_escaped(&payload, app->settings.last_project);
    studio_json_builder_append(&payload, ",\"recentProjects\":[");
    for (index = 0; index < STUDIO_RECENT_PROJECTS; ++index) {
        if (index > 0) {
            studio_json_builder_append(&payload, ",");
        }
        studio_json_builder_append_escaped(&payload, app->settings.recent_projects[index]);
    }
    studio_json_builder_append(&payload, "]}");
    json = studio_json_builder_take(&payload);
    studio_debug_log_write("studio_app_send_settings");
    studio_app_post_json(app, "settings:data", json);
    free(json);
    return true;
}

void studio_app_set_active_file(StudioApp *app, const char *path) {
    if (!path) {
        app->active_file[0] = '\0';
        studio_debug_log_write("studio_app_set_active_file cleared");
        return;
    }
    strncpy(app->active_file, path, sizeof(app->active_file) - 1);
    app->active_file[sizeof(app->active_file) - 1] = '\0';
    studio_debug_log_writef("studio_app_set_active_file path=%s", app->active_file);
}

void studio_app_send_process_state(StudioApp *app, const char *state, StudioProcessKind kind) {
    StudioJsonBuilder payload;
    char *json;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"state\":");
    studio_json_builder_append_escaped(&payload, state);
    studio_json_builder_append(&payload, ",\"kind\":");
    studio_json_builder_append_escaped(&payload, studio_kind_label(kind));
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "process:state", json);
    free(json);
}

static bool studio_app_start_command(StudioApp *app, StudioProcessKind kind, const char *exe, const char *arguments, const char *working_directory) {
    ErError error;

    studio_debug_log_writef(
        "studio_app_start_command kind=%s exe=%s args=%s cwd=%s",
        studio_kind_label(kind),
        exe ? exe : "(null)",
        arguments ? arguments : "",
        working_directory ? working_directory : ""
    );
    er_error_clear(&error);
    if (!studio_runner_start(&app->runner, kind, exe, arguments, working_directory, &error)) {
        studio_debug_log_writef("studio_runner_start failed: %s", error.message);
        studio_app_post_error(app, error.message);
        return false;
    }
    studio_app_send_process_state(app, "running", kind);
    return true;
}

static void studio_app_run_project(StudioApp *app) {
    char args[STUDIO_MAX_PATH * 2];

    if (!app->project.is_open) {
        studio_app_post_error(app, "No project is open");
        return;
    }
    snprintf(args, sizeof(args), "--run \"%s\"", app->project.entry_file);
    studio_app_start_command(app, STUDIO_PROCESS_RUN, app->settings.erire_exe, args, app->project.root_path);
}

static void studio_app_check_project(StudioApp *app) {
    char args[STUDIO_MAX_PATH * 2];

    if (!app->project.is_open) {
        studio_app_post_error(app, "No project is open");
        return;
    }
    snprintf(args, sizeof(args), "--check \"%s\"", app->project.entry_file);
    studio_app_start_command(app, STUDIO_PROCESS_CHECK, app->settings.erire_exe, args, app->project.root_path);
}

static void studio_app_build_project(StudioApp *app) {
    char args[STUDIO_MAX_PATH * 3];
    char output[STUDIO_MAX_PATH];

    if (!app->project.is_open) {
        studio_app_post_error(app, "No project is open");
        return;
    }
    studio_join_path(app->project.root_path, "build", output, sizeof(output));
    studio_ensure_directory(output);
    studio_join_path(output, "app.exe", output, sizeof(output));
    snprintf(args, sizeof(args), "--build \"%s\" \"%s\"", app->project.entry_file, output);
    studio_app_start_command(app, STUDIO_PROCESS_BUILD, app->settings.erire_exe, args, app->project.root_path);
}

static void studio_app_run_python(StudioApp *app) {
    char args[STUDIO_MAX_PATH * 2];
    char working_dir[STUDIO_MAX_PATH];

    if (!app->active_file[0] || strcmp(studio_language_from_path(app->active_file), "python") != 0) {
        studio_app_post_error(app, "Active file is not a Python file");
        return;
    }
    er_path_dirname(app->active_file, working_dir, sizeof(working_dir));
    snprintf(args, sizeof(args), "\"%s\"", app->active_file);
    studio_app_start_command(app, STUDIO_PROCESS_PYTHON, app->settings.python_exe, args, working_dir);
}

static void studio_app_post_command(StudioApp *app, const char *command_id) {
    StudioJsonBuilder payload;
    char *json;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"id\":");
    studio_json_builder_append_escaped(&payload, command_id);
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "command:execute", json);
    free(json);
}

static void studio_app_handle_native_command(StudioApp *app, UINT command_id) {
    char path[STUDIO_MAX_PATH];

    studio_debug_log_writef("native command id=%u", (unsigned) command_id);
    switch (command_id) {
        case IDM_FILE_NEW_PROJECT: studio_app_post_command(app, "newProject"); break;
        case IDM_FILE_OPEN_PROJECT:
            if (studio_pick_folder(app->hwnd, path, sizeof(path))) {
                studio_app_open_project(app, path);
            }
            break;
        case IDM_FILE_OPEN_FILE:
            if (studio_open_file_dialog(app->hwnd, path, sizeof(path))) {
                studio_app_open_file(app, path);
            }
            break;
        case IDM_FILE_SAVE: studio_app_post_command(app, "save"); break;
        case IDM_FILE_SAVE_AS: studio_app_post_command(app, "saveAs"); break;
        case IDM_FILE_SAVE_ALL: studio_app_post_command(app, "saveAll"); break;
        case IDM_FILE_CLOSE_PROJECT:
            studio_project_close(&app->project);
            app->settings.last_project[0] = '\0';
            studio_settings_save(&app->settings);
            studio_app_refresh_project(app);
            break;
        case IDM_FILE_EXIT: DestroyWindow(app->hwnd); break;
        case IDM_EDIT_UNDO: studio_app_post_command(app, "undo"); break;
        case IDM_EDIT_REDO: studio_app_post_command(app, "redo"); break;
        case IDM_EDIT_CUT: studio_app_post_command(app, "cut"); break;
        case IDM_EDIT_COPY: studio_app_post_command(app, "copy"); break;
        case IDM_EDIT_PASTE: studio_app_post_command(app, "paste"); break;
        case IDM_EDIT_SELECT_ALL: studio_app_post_command(app, "selectAll"); break;
        case IDM_EDIT_FIND: studio_app_post_command(app, "find"); break;
        case IDM_EDIT_REPLACE: studio_app_post_command(app, "replace"); break;
        case IDM_VIEW_TOGGLE_EXPLORER: studio_app_post_command(app, "toggleExplorer"); break;
        case IDM_VIEW_TOGGLE_OUTLINE: studio_app_post_command(app, "toggleOutline"); break;
        case IDM_VIEW_TOGGLE_BOTTOM: studio_app_post_command(app, "toggleBottom"); break;
        case IDM_VIEW_RESET_LAYOUT: studio_app_post_command(app, "resetLayout"); break;
        case IDM_RUN_PROJECT: studio_app_run_project(app); break;
        case IDM_RUN_PYTHON: studio_app_run_python(app); break;
        case IDM_RUN_STOP: studio_runner_stop(&app->runner); break;
        case IDM_BUILD_CHECK: studio_app_check_project(app); break;
        case IDM_BUILD_PROJECT: studio_app_build_project(app); break;
        case IDM_TOOLS_RELOAD_PROJECT: studio_app_refresh_project(app); break;
        case IDM_TOOLS_OPEN_PROJECT_FOLDER:
            if (app->project.is_open) {
                ShellExecuteA(app->hwnd, "open", app->project.root_path, NULL, NULL, SW_SHOWNORMAL);
            }
            break;
        case IDM_TOOLS_REFRESH_SYMBOLS: studio_app_post_command(app, "refreshSymbols"); break;
        case IDM_TOOLS_CLEAR_OUTPUT: studio_app_post_command(app, "clearOutput"); break;
        case IDM_SETTINGS_OPEN: studio_app_post_command(app, "openSettings"); break;
        case IDM_HELP_ABOUT:
            MessageBoxA(
                app->hwnd,
                "erire.studio - 1.0.4 2026 - Yahia Saad\nCreated and developed by FirstStandStudio (Yahia Saad)\nWebsite: https://firststandstudio.github.io",
                "About Erire Studio",
                MB_OK | MB_ICONINFORMATION
            );
            break;
    }
}

static void studio_app_initialize_webview(StudioApp *app) {
    char local_appdata[STUDIO_MAX_PATH];
    char browser_dir[STUDIO_MAX_PATH];
    char user_data_dir[STUDIO_MAX_PATH];

    if (app->settings.software_rendering) {
        SetEnvironmentVariableA(
            "WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
            "--disable-gpu --disable-gpu-compositing --disable-features=CalculateNativeWinOcclusion"
        );
        studio_debug_log_write("WebView2 software rendering mode enabled");
    } else {
        SetEnvironmentVariableA("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", NULL);
        studio_debug_log_write("WebView2 hardware rendering mode enabled");
    }

    studio_join_path(app->module_dir, "webview2", browser_dir, sizeof(browser_dir));
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, local_appdata) == S_OK) {
        studio_join_path(local_appdata, "Erire Studio", user_data_dir, sizeof(user_data_dir));
        studio_join_path(
            user_data_dir,
            app->settings.software_rendering ? "WebView2Safe" : "WebView2",
            user_data_dir,
            sizeof(user_data_dir)
        );
    } else {
        studio_join_path(
            app->module_dir,
            app->settings.software_rendering ? "webview2_data_safe" : "webview2_data",
            user_data_dir,
            sizeof(user_data_dir)
        );
    }
    studio_ensure_directory(user_data_dir);
    studio_debug_log_writef("studio_app_initialize_webview browser_dir=%s", browser_dir);
    studio_debug_log_writef("studio_app_initialize_webview user_data_dir=%s", user_data_dir);

    if (!studio_webview_host_init(&app->webview, app->hwnd, browser_dir, user_data_dir, studio_app_webview_message, app)) {
        studio_debug_log_write("studio_webview_host_init returned false");
        MessageBoxA(
            app->hwnd,
            "Could not initialize WebView2.\n\n"
            "Erire Studio tried:\n"
            "- .\\WebView2Loader.dll\n"
            "- .\\webview2\\WebView2Loader.dll\n"
            "- system WebView2Loader.dll\n\n"
            "The local webview2 runtime may be invalid, architecture may not match x64, or WebView2Loader.dll is missing.",
            "Erire Studio",
            MB_OK | MB_ICONERROR
        );
        return;
    }

    studio_debug_log_write("studio_webview_host_init started successfully");
    SetTimer(app->hwnd, 1, 120, NULL);
    studio_debug_log_write("startup timer started");
}

static void studio_app_try_finish_startup_timer(StudioApp *app) {
    if (!app) {
        return;
    }
    if (app->webview_navigated && app->startup_project_restored) {
        KillTimer(app->hwnd, 1);
        studio_debug_log_write("startup timer stopped");
    }
}

void studio_app_dispatch_message(StudioApp *app, const char *json) {
    studio_debug_log_write("studio_app_dispatch_message");
    studio_bridge_dispatch(app, json);
}

static LRESULT CALLBACK studio_app_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    StudioApp *app = studio_app_from_window(hwnd);

    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTA *create = (CREATESTRUCTA *) lparam;
            studio_debug_log_write("WM_NCCREATE");
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR) create->lpCreateParams);
            return TRUE;
        }
        case WM_CREATE: {
            app = studio_app_from_window(hwnd);
            studio_debug_log_write("WM_CREATE");
            app->hwnd = hwnd;
            studio_runner_init(&app->runner, hwnd);
            PostMessageA(hwnd, STUDIO_WM_BOOTSTRAP_WEBVIEW, 0, 0);
            studio_debug_log_write("posted STUDIO_WM_BOOTSTRAP_WEBVIEW");
            return 0;
        }
        case STUDIO_WM_BOOTSTRAP_WEBVIEW:
            if (app) {
                studio_debug_log_write("STUDIO_WM_BOOTSTRAP_WEBVIEW");
                studio_app_initialize_webview(app);
            }
            return 0;
        case WM_SIZE:
            if (app) {
                studio_webview_host_resize(&app->webview);
                studio_webview_host_notify_parent_window_position_changed(&app->webview);
            }
            return 0;
        case WM_MOVE:
        case WM_MOVING:
        case WM_WINDOWPOSCHANGED:
        case WM_DPICHANGED:
            if (app) {
                studio_webview_host_notify_parent_window_position_changed(&app->webview);
            }
            break;
        case WM_TIMER:
            if (!app) {
                return 0;
            }
            if (wparam == 1 && app->webview.ready && !app->webview_navigated) {
                studio_debug_log_writef("timer navigating frontend=%s", app->frontend_index);
                if (studio_webview_host_navigate_file(&app->webview, app->frontend_index)) {
                    app->webview_navigated = true;
                    studio_app_try_finish_startup_timer(app);
                }
            }
            if (wparam == 1 && app->frontend_ready && !app->startup_project_restored) {
                studio_debug_log_write("frontend ready; restoring startup state");
                app->startup_project_restored = true;
                studio_app_send_settings(app);
                if (app->settings.last_project[0] != '\0') {
                    studio_debug_log_writef("restoring last project %s", app->settings.last_project);
                    studio_app_open_project(app, app->settings.last_project);
                }
                studio_app_try_finish_startup_timer(app);
            }
            return 0;
        case WM_COMMAND:
            if (app) {
                studio_app_handle_native_command(app, LOWORD(wparam));
            }
            return 0;
        case STUDIO_WM_RUNNER_OUTPUT:
            if (app) {
                StudioRunnerChunk *chunk = (StudioRunnerChunk *) lparam;
                StudioJsonBuilder payload;
                char *json;

                studio_json_builder_init(&payload);
                studio_json_builder_append(&payload, "{\"kind\":");
                studio_json_builder_append_escaped(&payload, studio_kind_label(chunk->kind));
                studio_json_builder_append(&payload, ",\"stream\":");
                studio_json_builder_append_escaped(&payload, chunk->stream);
                studio_json_builder_append(&payload, ",\"text\":");
                studio_json_builder_append_escaped(&payload, chunk->text ? chunk->text : "");
                studio_json_builder_append(&payload, "}");
                json = studio_json_builder_take(&payload);
                studio_debug_log_writef("runner output kind=%s bytes=%u", studio_kind_label(chunk->kind), (unsigned) (chunk->text ? strlen(chunk->text) : 0));
                if (chunk->kind == STUDIO_PROCESS_BUILD || chunk->kind == STUDIO_PROCESS_CHECK) {
                    studio_app_post_json(app, "build:output", json);
                } else if (chunk->kind != STUDIO_PROCESS_TERMINAL) {
                    studio_app_post_json(app, "run:output", json);
                }
                studio_app_post_json(app, "terminal:output", json);
                free(json);
                free(chunk->text);
                free(chunk);
            }
            return 0;
        case STUDIO_WM_RUNNER_EXIT:
            if (app) {
                StudioRunnerExit *exit_message = (StudioRunnerExit *) lparam;
                StudioJsonBuilder payload;
                char *json;

                studio_json_builder_init(&payload);
                studio_json_builder_append(&payload, "{\"kind\":");
                studio_json_builder_append_escaped(&payload, studio_kind_label(exit_message->kind));
                studio_json_builder_appendf(&payload, ",\"exitCode\":%lu}", (unsigned long) exit_message->exit_code);
                json = studio_json_builder_take(&payload);
                studio_debug_log_writef("runner exit kind=%s exit_code=%lu", studio_kind_label(exit_message->kind), (unsigned long) exit_message->exit_code);
                studio_app_post_json(app, "status:update", json);
                free(json);
                studio_app_send_process_state(app, "idle", STUDIO_PROCESS_NONE);
                studio_app_finish_runner(app);
                free(exit_message);
            }
            return 0;
        case WM_DESTROY:
            if (app) {
                studio_debug_log_write("WM_DESTROY");
                KillTimer(hwnd, 1);
                studio_settings_save(&app->settings);
                studio_runner_dispose(&app->runner);
                studio_webview_host_dispose(&app->webview);
            }
            studio_debug_log_write("PostQuitMessage");
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int studio_app_run(HINSTANCE instance, int show_command) {
    WNDCLASSEXA window_class;
    MSG message;
    StudioApp app;
    HWND hwnd;
    BOOL message_result;
    int register_result;
    HICON icon_big;
    HICON icon_small;

    memset(&app, 0, sizeof(app));
    app.instance = instance;
    studio_debug_log_write("studio_app_run entered");
    studio_settings_load(&app.settings);
    studio_debug_log_writef("settings loaded last_project=%s", app.settings.last_project);
    studio_module_directory(app.module_dir, sizeof(app.module_dir));
    studio_join_path(app.module_dir, "studio\\frontend\\index.html", app.frontend_index, sizeof(app.frontend_index));
    studio_debug_log_writef("module_dir=%s", app.module_dir);
    studio_debug_log_writef("frontend_index=%s", app.frontend_index);

    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = studio_app_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    icon_big = (HICON) LoadImageA(instance, MAKEINTRESOURCEA(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    icon_small = (HICON) LoadImageA(instance, MAKEINTRESOURCEA(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    window_class.hIcon = icon_big ? icon_big : LoadIcon(instance, MAKEINTRESOURCEA(1));
    window_class.hIconSm = icon_small ? icon_small : window_class.hIcon;
    window_class.lpszClassName = "ErireStudioWindowClass";
    window_class.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    register_result = RegisterClassExA(&window_class);
    if (register_result == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        studio_debug_log_write_win32_error("RegisterClassExA", GetLastError());
        MessageBoxA(NULL, "RegisterClassExA failed for Erire Studio.", "Erire Studio", MB_OK | MB_ICONERROR);
        return 1;
    }
    studio_debug_log_write("RegisterClassExA succeeded");

    studio_debug_log_write("calling CreateWindowExA");
    hwnd = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "erire.studio - 1.0.4 2026 - Yahia Saad",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1540,
        940,
        NULL,
        NULL,
        instance,
        &app
    );
    if (!hwnd) {
        studio_debug_log_write_win32_error("CreateWindowExA", GetLastError());
        MessageBoxA(NULL, "CreateWindowExA failed for Erire Studio.", "Erire Studio", MB_OK | MB_ICONERROR);
        return 1;
    }
    studio_debug_log_writef("CreateWindowExA succeeded hwnd=0x%p", hwnd);
    SetWindowTextA(hwnd, "erire.studio - 1.0.4 2026 - Yahia Saad");
    if (icon_big) {
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon_big);
    }
    if (icon_small) {
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon_small);
    }
    studio_app_apply_window_chrome(hwnd);

    ShowWindow(hwnd, show_command == 0 ? SW_SHOWNORMAL : show_command);
    UpdateWindow(hwnd);
    studio_debug_log_write("window shown and updated");

    studio_debug_log_write("message loop entered");
    while ((message_result = GetMessageA(&message, NULL, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    if (message_result == -1) {
        studio_debug_log_write_win32_error("GetMessageA", GetLastError());
        MessageBoxA(NULL, "GetMessageA failed in Erire Studio.", "Erire Studio", MB_OK | MB_ICONERROR);
        return 1;
    }
    studio_debug_log_writef("message loop exited wParam=%ld", (long) message.wParam);
    return (int) message.wParam;
}
