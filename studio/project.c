#include "project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "fileio.h"
#include "json.h"

static bool studio_project_has_manifest(const char *root_path) {
    char manifest[STUDIO_MAX_PATH];

    studio_join_path(root_path, "app.json", manifest, sizeof(manifest));
    return er_file_exists(manifest);
}

bool studio_project_detect_root(const char *path, char *out_root, size_t out_capacity) {
    char current[STUDIO_MAX_PATH];
    char parent[STUDIO_MAX_PATH];
    DWORD attributes;

    if (!path || !out_root || out_capacity == 0) {
        return false;
    }
    strncpy(current, path, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';
    studio_normalize_slashes(current);

    attributes = GetFileAttributesA(current);
    if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        er_path_dirname(current, current, sizeof(current));
    }

    while (current[0] != '\0') {
        if (studio_project_has_manifest(current)) {
            strncpy(out_root, current, out_capacity - 1);
            out_root[out_capacity - 1] = '\0';
            return true;
        }
        er_path_dirname(current, parent, sizeof(parent));
        if (_stricmp(parent, current) == 0) {
            break;
        }
        strncpy(current, parent, sizeof(current) - 1);
        current[sizeof(current) - 1] = '\0';
    }
    return false;
}

static void studio_project_parse_manifest(StudioProject *project) {
    char *json = NULL;
    ErError error;

    er_error_clear(&error);
    if (!er_file_read_all(project->app_json_path, &json, NULL, &error)) {
        return;
    }
    studio_json_get_string(json, "name", project->name, sizeof(project->name));
    if (!studio_json_get_string(json, "entry", project->entry_file, sizeof(project->entry_file))) {
        studio_join_path(project->root_path, "src\\main.er", project->entry_file, sizeof(project->entry_file));
    } else {
        char absolute_entry[STUDIO_MAX_PATH];
        studio_join_path(project->root_path, project->entry_file, absolute_entry, sizeof(absolute_entry));
        strncpy(project->entry_file, absolute_entry, sizeof(project->entry_file) - 1);
        project->entry_file[sizeof(project->entry_file) - 1] = '\0';
    }
    free(json);
}

bool studio_project_open(StudioProject *project, const char *path, ErError *error) {
    char root[STUDIO_MAX_PATH];

    memset(project, 0, sizeof(*project));
    if (!studio_project_detect_root(path, root, sizeof(root))) {
        er_error_set(error, 0, 0, "Could not find app.json for: %s", path);
        return false;
    }
    strncpy(project->root_path, root, sizeof(project->root_path) - 1);
    project->root_path[sizeof(project->root_path) - 1] = '\0';
    studio_join_path(project->root_path, "app.json", project->app_json_path, sizeof(project->app_json_path));
    project->is_open = true;
    studio_project_parse_manifest(project);
    if (project->name[0] == '\0') {
        er_path_basename_without_extension(project->root_path, project->name, sizeof(project->name));
    }
    if (project->entry_file[0] == '\0') {
        studio_join_path(project->root_path, "src\\main.er", project->entry_file, sizeof(project->entry_file));
    }
    return true;
}

void studio_project_close(StudioProject *project) {
    memset(project, 0, sizeof(*project));
}

typedef struct StudioTreeScanContext {
    StudioJsonBuilder *json;
    size_t count;
} StudioTreeScanContext;

static bool studio_tree_should_skip(const char *name) {
    return strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0 ||
        strcmp(name, ".git") == 0 ||
        strcmp(name, ".vs") == 0 ||
        strcmp(name, ".idea") == 0;
}

static void studio_tree_emit_entry(
    StudioTreeScanContext *context,
    const char *full_path,
    const char *relative_path,
    const char *name,
    bool is_directory,
    int depth
) {
    if (context->count > 0) {
        studio_json_builder_append(context->json, ",");
    }
    studio_json_builder_append(context->json, "{");
    studio_json_builder_append(context->json, "\"path\":");
    studio_json_builder_append_escaped(context->json, full_path);
    studio_json_builder_append(context->json, ",\"relPath\":");
    studio_json_builder_append_escaped(context->json, relative_path);
    studio_json_builder_append(context->json, ",\"name\":");
    studio_json_builder_append_escaped(context->json, name);
    studio_json_builder_appendf(context->json, ",\"depth\":%d", depth);
    studio_json_builder_append(context->json, ",\"kind\":");
    studio_json_builder_append_escaped(context->json, is_directory ? "folder" : "file");
    studio_json_builder_append(context->json, "}");
    context->count += 1;
}

static void studio_project_scan_dir(StudioTreeScanContext *context, const char *root, const char *relative, int depth) {
    char search_path[STUDIO_MAX_PATH];
    char full_path[STUDIO_MAX_PATH];
    char child_relative[STUDIO_MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;

    if (relative[0] != '\0') {
        studio_join_path(root, relative, full_path, sizeof(full_path));
        studio_join_path(full_path, "*", search_path, sizeof(search_path));
    } else {
        studio_join_path(root, "*", search_path, sizeof(search_path));
    }

    handle = FindFirstFileA(search_path, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (studio_tree_should_skip(find_data.cFileName)) {
            continue;
        }
        if (relative[0] != '\0') {
            snprintf(child_relative, sizeof(child_relative), "%s\\%s", relative, find_data.cFileName);
            studio_join_path(root, child_relative, full_path, sizeof(full_path));
        } else {
            snprintf(child_relative, sizeof(child_relative), "%s", find_data.cFileName);
            studio_join_path(root, find_data.cFileName, full_path, sizeof(full_path));
        }
        studio_tree_emit_entry(
            context,
            full_path,
            child_relative,
            find_data.cFileName,
            (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
            depth
        );
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            studio_project_scan_dir(context, root, child_relative, depth + 1);
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
}

char *studio_project_tree_json(const StudioProject *project) {
    StudioJsonBuilder builder;
    StudioTreeScanContext context;

    if (!project || !project->is_open) {
        return NULL;
    }

    studio_json_builder_init(&builder);
    context.json = &builder;
    context.count = 0;

    studio_json_builder_append(&builder, "{\"name\":");
    studio_json_builder_append_escaped(&builder, project->name);
    studio_json_builder_append(&builder, ",\"root\":");
    studio_json_builder_append_escaped(&builder, project->root_path);
    studio_json_builder_append(&builder, ",\"entries\":[");
    studio_project_scan_dir(&context, project->root_path, "", 0);
    studio_json_builder_append(&builder, "]}");
    return studio_json_builder_take(&builder);
}

bool studio_project_read_file(const char *path, char **out_data, size_t *out_size, ErError *error) {
    return er_file_read_all(path, out_data, out_size, error);
}

bool studio_project_write_file(const char *path, const char *content, ErError *error) {
    return er_file_write_all(path, content ? content : "", content ? strlen(content) : 0, error);
}

bool studio_project_rename_entry(const char *path, const char *new_name, char *out_new_path, size_t out_capacity, ErError *error) {
    char directory[STUDIO_MAX_PATH];
    char destination[STUDIO_MAX_PATH];
    DWORD attributes;
    const char *invalid_chars = "\\/:*?\"<>|";

    if (!path || !new_name || new_name[0] == '\0') {
        er_error_set(error, 0, 0, "Rename requires a valid source path and new name");
        return false;
    }
    if (strpbrk(new_name, invalid_chars) != NULL) {
        er_error_set(error, 0, 0, "The new name contains invalid path characters");
        return false;
    }

    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        er_error_set(error, 0, 0, "The selected file or folder no longer exists");
        return false;
    }

    er_path_dirname(path, directory, sizeof(directory));
    studio_join_path(directory, new_name, destination, sizeof(destination));

    if (_stricmp(path, destination) == 0) {
        strncpy(out_new_path, path, out_capacity - 1);
        out_new_path[out_capacity - 1] = '\0';
        return true;
    }
    if (GetFileAttributesA(destination) != INVALID_FILE_ATTRIBUTES) {
        er_error_set(error, 0, 0, "A file or folder with that name already exists");
        return false;
    }
    if (!MoveFileA(path, destination)) {
        er_error_set(error, 0, 0, "Could not rename '%s' to '%s' (Win32 %lu)", path, destination, (unsigned long) GetLastError());
        return false;
    }

    strncpy(out_new_path, destination, out_capacity - 1);
    out_new_path[out_capacity - 1] = '\0';
    return true;
}

static bool studio_project_delete_entry_recursive(const char *path, ErError *error) {
    DWORD attributes;

    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        if (!DeleteFileA(path)) {
            er_error_set(error, 0, 0, "Could not delete file '%s' (Win32 %lu)", path, (unsigned long) GetLastError());
            return false;
        }
        return true;
    }

    {
        char search[STUDIO_MAX_PATH];
        char child_path[STUDIO_MAX_PATH];
        WIN32_FIND_DATAA find_data;
        HANDLE handle;

        studio_join_path(path, "*", search, sizeof(search));
        handle = FindFirstFileA(search, &find_data);
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                    continue;
                }
                studio_join_path(path, find_data.cFileName, child_path, sizeof(child_path));
                if (!studio_project_delete_entry_recursive(child_path, error)) {
                    FindClose(handle);
                    return false;
                }
            } while (FindNextFileA(handle, &find_data));
            FindClose(handle);
        }
    }

    if (!RemoveDirectoryA(path)) {
        er_error_set(error, 0, 0, "Could not delete folder '%s' (Win32 %lu)", path, (unsigned long) GetLastError());
        return false;
    }
    return true;
}

bool studio_project_delete_entry(const char *path, ErError *error) {
    if (!path || path[0] == '\0') {
        er_error_set(error, 0, 0, "Delete requires a valid path");
        return false;
    }
    return studio_project_delete_entry_recursive(path, error);
}
