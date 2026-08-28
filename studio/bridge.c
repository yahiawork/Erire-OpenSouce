#include "bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "debug_log.h"
#include "fileio.h"
#include "json.h"
#include "outline.h"
#include "templates.h"

#include <ctype.h>
#include <shellapi.h>

static bool studio_bridge_save_file(StudioApp *app, const char *path, const char *content) {
    ErError error;
    char *outline_json;
    StudioJsonBuilder payload;
    char *json;

    studio_debug_log_writef("bridge save file path=%s", path ? path : "(null)");
    er_error_clear(&error);
    if (!studio_project_write_file(path, content, &error)) {
        studio_debug_log_writef("studio_project_write_file failed: %s", error.message);
        return studio_app_post_error(app, error.message);
    }

    studio_json_builder_init(&payload);

    // fix.build.c

    studio_json_builder_append(&payload, "{\"path\":");
    studio_json_builder_append_escaped(&payload, path);
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "file:saved", json);
    free(json);

    outline_json = studio_outline_extract_json(path, content);
    if (outline_json) {
        studio_app_post_json(app, "outline:data", outline_json);
        free(outline_json);
    }
    return true;
}

static bool studio_bridge_start_process(StudioApp *app, StudioProcessKind kind, const char *exe_path, const char *arguments, const char *working_dir) {
    ErError error;

    studio_debug_log_writef(
        "bridge start process kind=%d exe=%s args=%s cwd=%s",
        (int) kind,
        exe_path ? exe_path : "(null)",
        arguments ? arguments : "",
        working_dir ? working_dir : ""
    );
    er_error_clear(&error);
    if (!studio_runner_start(&app->runner, kind, exe_path, arguments, working_dir, &error)) {
        studio_debug_log_writef("bridge process start failed: %s", error.message);
        return studio_app_post_error(app, error.message);
    }
    studio_app_send_process_state(app, "running", kind);
    return true;
}

static void studio_bridge_run_project(StudioApp *app) {
    char args[STUDIO_MAX_PATH * 2];

    if (!app->project.is_open) {
        studio_app_post_error(app, "No project is open");
        return;
    }
    snprintf(args, sizeof(args), "--run \"%s\"", app->project.entry_file);
    studio_bridge_start_process(app, STUDIO_PROCESS_RUN, app->settings.erire_exe, args, app->project.root_path);
}

static void studio_bridge_check_project(StudioApp *app) {
    char args[STUDIO_MAX_PATH * 2];

    if (!app->project.is_open) {
        studio_app_post_error(app, "No project is open");
        return;
    }
    snprintf(args, sizeof(args), "--check \"%s\"", app->project.entry_file);
    studio_bridge_start_process(app, STUDIO_PROCESS_CHECK, app->settings.erire_exe, args, app->project.root_path);
}

static void studio_bridge_build_project(StudioApp *app) {
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
    studio_bridge_start_process(app, STUDIO_PROCESS_BUILD, app->settings.erire_exe, args, app->project.root_path);
}

static void studio_bridge_run_python(StudioApp *app, const char *path) {
    char args[STUDIO_MAX_PATH * 2];
    char working_dir[STUDIO_MAX_PATH];
    const char *target = path && path[0] != '\0' ? path : app->active_file;

    if (!target || target[0] == '\0' || strcmp(studio_language_from_path(target), "python") != 0) {
        studio_app_post_error(app, "No active Python file selected");
        return;
    }
    er_path_dirname(target, working_dir, sizeof(working_dir));
    studio_app_set_active_file(app, target);
    snprintf(args, sizeof(args), "\"%s\"", target);
    studio_bridge_start_process(app, STUDIO_PROCESS_PYTHON, app->settings.python_exe, args, working_dir);
}

static void studio_bridge_send_dialog_result(StudioApp *app, const char *target, const char *path) {
    StudioJsonBuilder payload;
    char *json;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"target\":");
    studio_json_builder_append_escaped(&payload, target ? target : "");
    studio_json_builder_append(&payload, ",\"path\":");
    studio_json_builder_append_escaped(&payload, path ? path : "");
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "dialog:selected", json);
    free(json);
}

static void studio_bridge_send_terminal_cwd(StudioApp *app, const char *cwd, const char *message, const char *kind) {
    StudioJsonBuilder payload;
    char *json;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"cwd\":");
    studio_json_builder_append_escaped(&payload, cwd ? cwd : "");
    if (message) {
        studio_json_builder_append(&payload, ",\"message\":");
        studio_json_builder_append_escaped(&payload, message);
    }
    if (kind) {
        studio_json_builder_append(&payload, ",\"kind\":");
        studio_json_builder_append_escaped(&payload, kind);
    }
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, "terminal:cwd", json);
    free(json);
}

static void studio_bridge_default_terminal_cwd(StudioApp *app, char *out_path, size_t out_capacity) {
    if (!out_path || out_capacity == 0) {
        return;
    }
    out_path[0] = '\0';
    if (app->project.is_open && app->project.root_path[0] != '\0') {
        strncpy(out_path, app->project.root_path, out_capacity - 1);
        out_path[out_capacity - 1] = '\0';
        return;
    }
    if (app->active_file[0] != '\0') {
        er_path_dirname(app->active_file, out_path, out_capacity);
        if (out_path[0] != '\0') {
            return;
        }
    }
    if (GetCurrentDirectoryA((DWORD) out_capacity, out_path) == 0 || out_path[0] == '\0') {
        strncpy(out_path, app->module_dir, out_capacity - 1);
        out_path[out_capacity - 1] = '\0';
    }
}

static void studio_bridge_strip_surrounding_quotes(char *text) {
    size_t length;

    if (!text) {
        return;
    }
    length = strlen(text);
    if (length >= 2 && ((text[0] == '"' && text[length - 1] == '"') || (text[0] == '\'' && text[length - 1] == '\''))) {
        memmove(text, text + 1, length - 2);
        text[length - 2] = '\0';
    }
}

static bool studio_bridge_path_is_absolute(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    if ((path[0] == '\\' && path[1] == '\\') || (path[0] == '/' && path[1] == '/')) {
        return true;
    }
    return isalpha((unsigned char) path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

static bool studio_bridge_resolve_directory(
    const char *base_directory,
    const char *requested_directory,
    char *out_path,
    size_t out_capacity,
    ErError *error
) {
    char requested[STUDIO_MAX_PATH];
    char candidate[STUDIO_MAX_PATH];
    DWORD attributes;

    if (!out_path || out_capacity == 0) {
        er_error_set(error, 0, 0, "Invalid terminal path buffer");
        return false;
    }

    requested[0] = '\0';
    if (requested_directory && requested_directory[0] != '\0') {
        strncpy(requested, requested_directory, sizeof(requested) - 1);
        requested[sizeof(requested) - 1] = '\0';
        studio_bridge_strip_surrounding_quotes(requested);
    }

    if (requested[0] == '\0' || strcmp(requested, ".") == 0) {
        strncpy(candidate, base_directory, sizeof(candidate) - 1);
        candidate[sizeof(candidate) - 1] = '\0';
    } else if (studio_bridge_path_is_absolute(requested)) {
        strncpy(candidate, requested, sizeof(candidate) - 1);
        candidate[sizeof(candidate) - 1] = '\0';
    } else {
        studio_join_path(base_directory, requested, candidate, sizeof(candidate));
    }

    if (GetFullPathNameA(candidate, (DWORD) out_capacity, out_path, NULL) == 0) {
        er_error_set(error, 0, 0, "Could not resolve directory '%s'", requested[0] != '\0' ? requested : candidate);
        return false;
    }

    attributes = GetFileAttributesA(out_path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        er_error_set(error, 0, 0, "Directory does not exist: %s", out_path);
        return false;
    }
    return true;
}

static void studio_bridge_change_terminal_directory(StudioApp *app, const char *cwd, const char *requested_path) {
    char base_directory[STUDIO_MAX_PATH];
    char resolved_path[STUDIO_MAX_PATH];
    ErError error;

    studio_bridge_default_terminal_cwd(app, base_directory, sizeof(base_directory));
    er_error_clear(&error);
    if (cwd && cwd[0] != '\0' && !studio_bridge_resolve_directory(base_directory, cwd, base_directory, sizeof(base_directory), &error)) {
        studio_app_post_error(app, error.message);
        return;
    }
    er_error_clear(&error);
    if (!studio_bridge_resolve_directory(base_directory, requested_path, resolved_path, sizeof(resolved_path), &error)) {
        studio_app_post_error(app, error.message);
        return;
    }
    studio_bridge_send_terminal_cwd(app, resolved_path, requested_path && requested_path[0] != '\0' ? resolved_path : NULL, "success");
}

static void studio_bridge_execute_terminal_command(StudioApp *app, const char *cwd, const char *command) {
    char working_directory[STUDIO_MAX_PATH];
    char system_directory[STUDIO_MAX_PATH];
    char shell_exe[STUDIO_MAX_PATH];
    char arguments[4096];
    ErError error;

    if (!command || command[0] == '\0') {
        return;
    }

    studio_bridge_default_terminal_cwd(app, working_directory, sizeof(working_directory));
    er_error_clear(&error);
    if (cwd && cwd[0] != '\0' && !studio_bridge_resolve_directory(working_directory, cwd, working_directory, sizeof(working_directory), &error)) {
        studio_app_post_error(app, error.message);
        return;
    }

    if (GetSystemDirectoryA(system_directory, (UINT) sizeof(system_directory)) == 0) {
        strncpy(system_directory, "C:\\Windows\\System32", sizeof(system_directory) - 1);
        system_directory[sizeof(system_directory) - 1] = '\0';
    }
    studio_join_path(system_directory, "cmd.exe", shell_exe, sizeof(shell_exe));
    snprintf(arguments, sizeof(arguments), "/d /s /c %s", command);
    studio_bridge_start_process(app, STUDIO_PROCESS_TERMINAL, shell_exe, arguments, working_directory);
}

static bool studio_bridge_path_matches_or_is_child(const char *path, const char *prefix) {
    size_t prefix_length;

    if (!path || !prefix) {
        return false;
    }
    prefix_length = strlen(prefix);
    if (_stricmp(path, prefix) == 0) {
        return true;
    }
    if (_strnicmp(path, prefix, prefix_length) != 0) {
        return false;
    }
    return path[prefix_length] == '\\' || path[prefix_length] == '/';
}

static void studio_bridge_send_entry_event(
    StudioApp *app,
    const char *type,
    const char *path,
    const char *new_path,
    const char *kind
) {
    StudioJsonBuilder payload;
    char *json;

    studio_json_builder_init(&payload);
    studio_json_builder_append(&payload, "{\"path\":");
    studio_json_builder_append_escaped(&payload, path ? path : "");
    if (new_path) {
        studio_json_builder_append(&payload, ",\"newPath\":");
        studio_json_builder_append_escaped(&payload, new_path);
    }
    studio_json_builder_append(&payload, ",\"kind\":");
    studio_json_builder_append_escaped(&payload, kind ? kind : "file");
    studio_json_builder_append(&payload, "}");
    json = studio_json_builder_take(&payload);
    studio_app_post_json(app, type, json);
    free(json);
}

void studio_bridge_dispatch(struct StudioApp *app, const char *json) {
    char type[128];
    char path[STUDIO_MAX_PATH];
    char content[262144];
    char location[STUDIO_MAX_PATH];
    char name[STUDIO_MAX_NAME];
    char template_name[64];
    char target[64];
    char old_path[STUDIO_MAX_PATH];
    char new_path[STUDIO_MAX_PATH];
    bool bool_value;
    int int_value;

    if (!studio_json_get_string(json, "type", type, sizeof(type))) {
        studio_app_post_error(app, "Received bridge message without a type");
        return;
    }
    studio_debug_log_writef("bridge message type=%s", type);

    if (strcmp(type, "app:ready") == 0) {
        char terminal_cwd[STUDIO_MAX_PATH];

        app->frontend_ready = true;
        studio_debug_log_write("frontend marked ready");
        studio_app_send_process_state(app, "idle", STUDIO_PROCESS_NONE);
        studio_bridge_default_terminal_cwd(app, terminal_cwd, sizeof(terminal_cwd));
        studio_bridge_send_terminal_cwd(app, terminal_cwd, NULL, "info");
        return;
    }

    if (strcmp(type, "app:newProject") == 0) {
        char new_root[STUDIO_MAX_PATH];
        ErError error;

        name[0] = '\0';
        location[0] = '\0';
        template_name[0] = '\0';
        studio_json_get_string(json, "name", name, sizeof(name));
        studio_json_get_string(json, "location", location, sizeof(location));
        studio_json_get_string(json, "template", template_name, sizeof(template_name));
        if (location[0] == '\0' && !studio_pick_folder(app->hwnd, location, sizeof(location))) {
            return;
        }
        er_error_clear(&error);
        if (!studio_templates_create_project(name, location, template_name, new_root, sizeof(new_root), &error)) {
            studio_app_post_error(app, error.message);
            return;
        }
        studio_app_open_project(app, new_root);
        return;
    }

    if (strcmp(type, "app:openProject") == 0) {
        path[0] = '\0';
        if (!studio_json_get_string(json, "path", path, sizeof(path)) || path[0] == '\0') {
            if (!studio_pick_folder(app->hwnd, path, sizeof(path))) {
                return;
            }
        }
        studio_app_open_project(app, path);
        return;
    }

    if (strcmp(type, "app:openFile") == 0) {
        path[0] = '\0';
        if (!studio_json_get_string(json, "path", path, sizeof(path)) || path[0] == '\0') {
            if (!studio_open_file_dialog(app->hwnd, path, sizeof(path))) {
                return;
            }
        }
        studio_app_open_file(app, path);
        return;
    }

    if (strcmp(type, "app:exit") == 0) {
        DestroyWindow(app->hwnd);
        return;
    }

    if (strcmp(type, "app:openExternal") == 0) {
        path[0] = '\0';
        if (studio_json_get_string(json, "url", path, sizeof(path)) && path[0] != '\0') {
            ShellExecuteA(app->hwnd, "open", path, NULL, NULL, SW_SHOWNORMAL);
        }
        return;
    }

    if (strcmp(type, "app:saveFile") == 0) {
        path[0] = '\0';
        content[0] = '\0';
        studio_json_get_string(json, "path", path, sizeof(path));
        studio_json_get_string(json, "content", content, sizeof(content));
        if (path[0] == '\0') {
            studio_app_post_error(app, "Save request is missing a path");
            return;
        }
        studio_bridge_save_file(app, path, content);
        return;
    }

    if (strcmp(type, "app:saveFileAs") == 0) {
        path[0] = '\0';
        old_path[0] = '\0';
        content[0] = '\0';
        studio_json_get_string(json, "path", old_path, sizeof(old_path));
        studio_json_get_string(json, "content", content, sizeof(content));
        if (!studio_save_file_dialog(app->hwnd, old_path, path, sizeof(path))) {
            return;
        }
        if (studio_bridge_save_file(app, path, content)) {
            studio_app_set_active_file(app, path);
        }
        return;
    }

    if (strcmp(type, "project:refresh") == 0) {
        studio_app_refresh_project(app);
        return;
    }

    if (strcmp(type, "project:renameEntry") == 0) {
        ErError error;
        DWORD attributes;

        path[0] = '\0';
        name[0] = '\0';
        studio_json_get_string(json, "path", path, sizeof(path));
        studio_json_get_string(json, "newName", name, sizeof(name));
        if (path[0] == '\0' || name[0] == '\0') {
            studio_app_post_error(app, "Rename request is missing the path or the new name");
            return;
        }
        if (_stricmp(path, app->project.root_path) == 0) {
            studio_app_post_error(app, "The project root cannot be renamed from Explorer");
            return;
        }
        if (_stricmp(path, app->project.app_json_path) == 0) {
            studio_app_post_error(app, "app.json cannot be renamed while the project is open");
            return;
        }

        attributes = GetFileAttributesA(path);
        er_error_clear(&error);
        if (!studio_project_rename_entry(path, name, new_path, sizeof(new_path), &error)) {
            studio_app_post_error(app, error.message);
            return;
        }

        if (studio_bridge_path_matches_or_is_child(app->active_file, path)) {
            if (_stricmp(app->active_file, path) == 0) {
                studio_app_set_active_file(app, new_path);
            } else {
                char updated_active[STUDIO_MAX_PATH];
                snprintf(updated_active, sizeof(updated_active), "%s%s", new_path, app->active_file + strlen(path));
                studio_app_set_active_file(app, updated_active);
            }
        }
        if (studio_bridge_path_matches_or_is_child(app->project.entry_file, path)) {
            if (_stricmp(app->project.entry_file, path) == 0) {
                strncpy(app->project.entry_file, new_path, sizeof(app->project.entry_file) - 1);
                app->project.entry_file[sizeof(app->project.entry_file) - 1] = '\0';
            } else {
                char updated_entry[STUDIO_MAX_PATH];
                snprintf(updated_entry, sizeof(updated_entry), "%s%s", new_path, app->project.entry_file + strlen(path));
                strncpy(app->project.entry_file, updated_entry, sizeof(app->project.entry_file) - 1);
                app->project.entry_file[sizeof(app->project.entry_file) - 1] = '\0';
            }
        }

        studio_bridge_send_entry_event(
            app,
            "project:entryRenamed",
            path,
            new_path,
            (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ? "folder" : "file"
        );
        studio_app_refresh_project(app);
        return;
    }

    if (strcmp(type, "project:deleteEntry") == 0) {
        ErError error;
        DWORD attributes;

        path[0] = '\0';
        studio_json_get_string(json, "path", path, sizeof(path));
        if (path[0] == '\0') {
            studio_app_post_error(app, "Delete request is missing the path");
            return;
        }
        if (_stricmp(path, app->project.root_path) == 0) {
            studio_app_post_error(app, "The project root cannot be deleted from Explorer");
            return;
        }
        if (_stricmp(path, app->project.app_json_path) == 0) {
            studio_app_post_error(app, "app.json cannot be deleted while the project is open");
            return;
        }

        attributes = GetFileAttributesA(path);
        er_error_clear(&error);
        if (!studio_project_delete_entry(path, &error)) {
            studio_app_post_error(app, error.message);
            return;
        }

        if (studio_bridge_path_matches_or_is_child(app->active_file, path)) {
            studio_app_set_active_file(app, "");
        }
        if (studio_bridge_path_matches_or_is_child(app->project.entry_file, path)) {
            app->project.entry_file[0] = '\0';
        }

        studio_bridge_send_entry_event(
            app,
            "project:entryDeleted",
            path,
            NULL,
            (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ? "folder" : "file"
        );
        studio_app_refresh_project(app);
        return;
    }

    if (strcmp(type, "project:close") == 0) {
        studio_project_close(&app->project);
        app->settings.last_project[0] = '\0';
        studio_settings_save(&app->settings);
        studio_app_set_active_file(app, "");
        studio_app_refresh_project(app);
        return;
    }

    if (strcmp(type, "run:project") == 0) {
        studio_bridge_run_project(app);
        return;
    }

    if (strcmp(type, "build:check") == 0) {
        studio_bridge_check_project(app);
        return;
    }

    if (strcmp(type, "build:project") == 0) {
        studio_bridge_build_project(app);
        return;
    }

    if (strcmp(type, "run:python") == 0) {
        path[0] = '\0';
        studio_json_get_string(json, "path", path, sizeof(path));
        studio_bridge_run_python(app, path);
        return;
    }

    if (strcmp(type, "terminal:execute") == 0) {
        path[0] = '\0';
        content[0] = '\0';
        studio_json_get_string(json, "cwd", path, sizeof(path));
        studio_json_get_string(json, "command", content, sizeof(content));
        studio_bridge_execute_terminal_command(app, path, content);
        return;
    }

    if (strcmp(type, "terminal:changeDirectory") == 0) {
        path[0] = '\0';
        content[0] = '\0';
        studio_json_get_string(json, "cwd", path, sizeof(path));
        studio_json_get_string(json, "path", content, sizeof(content));
        studio_bridge_change_terminal_directory(app, path, content);
        return;
    }

    if (strcmp(type, "tools:openProjectFolder") == 0) {
        if (app->project.is_open) {
            ShellExecuteA(app->hwnd, "open", app->project.root_path, NULL, NULL, SW_SHOWNORMAL);
        }
        return;
    }

    if (strcmp(type, "process:stop") == 0) {
        studio_runner_stop(&app->runner);
        return;
    }

    if (strcmp(type, "settings:load") == 0) {
        studio_app_send_settings(app);
        return;
    }

    if (strcmp(type, "settings:save") == 0) {
        if (studio_json_get_string(json, "erireExe", path, sizeof(path))) {
            strncpy(app->settings.erire_exe, path, sizeof(app->settings.erire_exe) - 1);
            app->settings.erire_exe[sizeof(app->settings.erire_exe) - 1] = '\0';
        }
        if (studio_json_get_string(json, "pythonExe", path, sizeof(path))) {
            strncpy(app->settings.python_exe, path, sizeof(app->settings.python_exe) - 1);
            app->settings.python_exe[sizeof(app->settings.python_exe) - 1] = '\0';
        }
        if (studio_json_get_string(json, "fontName", path, sizeof(path))) {
            strncpy(app->settings.font_name, path, sizeof(app->settings.font_name) - 1);
            app->settings.font_name[sizeof(app->settings.font_name) - 1] = '\0';
        }
        if (studio_json_get_string(json, "themeName", path, sizeof(path))) {
            strncpy(app->settings.theme_name, path, sizeof(app->settings.theme_name) - 1);
            app->settings.theme_name[sizeof(app->settings.theme_name) - 1] = '\0';
        }
        if (studio_json_get_int(json, "fontSize", &int_value)) {
            app->settings.font_size = int_value;
        }
        if (studio_json_get_bool(json, "autosave", &bool_value)) {
            app->settings.autosave = bool_value;
        }
        if (studio_json_get_bool(json, "softwareRendering", &bool_value)) {
            app->settings.software_rendering = bool_value;
        }
        if (studio_json_get_bool(json, "showExplorer", &bool_value)) {
            app->settings.show_explorer = bool_value;
        }
        if (studio_json_get_bool(json, "showOutline", &bool_value)) {
            app->settings.show_outline = bool_value;
        }
        if (studio_json_get_bool(json, "showBottom", &bool_value)) {
            app->settings.show_bottom = bool_value;
        }
        studio_settings_save(&app->settings);
        studio_app_send_settings(app);
        return;
    }

    if (strcmp(type, "outline:request") == 0) {
        char *outline_json;

        path[0] = '\0';
        content[0] = '\0';
        studio_json_get_string(json, "path", path, sizeof(path));
        studio_json_get_string(json, "content", content, sizeof(content));
        outline_json = studio_outline_extract_json(path, content);
        if (outline_json) {
            studio_app_post_json(app, "outline:data", outline_json);
            free(outline_json);
        }
        return;
    }

    if (strcmp(type, "file:setActive") == 0) {
        path[0] = '\0';
        if (studio_json_get_string(json, "path", path, sizeof(path))) {
            studio_app_set_active_file(app, path);
        }
        return;
    }

    if (strcmp(type, "file:closeTab") == 0) {
        path[0] = '\0';
        if (studio_json_get_string(json, "path", path, sizeof(path)) && _stricmp(app->active_file, path) == 0) {
            studio_app_set_active_file(app, "");
        }
        return;
    }

    if (strcmp(type, "dialog:pickFolder") == 0) {
        target[0] = '\0';
        path[0] = '\0';
        studio_json_get_string(json, "target", target, sizeof(target));
        if (studio_pick_folder(app->hwnd, path, sizeof(path))) {
            studio_bridge_send_dialog_result(app, target, path);
        }
        return;
    }

    if (strcmp(type, "dialog:pickFile") == 0) {
        target[0] = '\0';
        path[0] = '\0';
        studio_json_get_string(json, "target", target, sizeof(target));
        if (studio_open_file_dialog(app->hwnd, path, sizeof(path))) {
            studio_bridge_send_dialog_result(app, target, path);
        }
        return;
    }

    studio_app_post_error(app, "Unhandled bridge message");
}
