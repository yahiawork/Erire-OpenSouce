#include "packager.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

#pragma pack(push, 1)
typedef struct ErIcoDirHeader {
    WORD reserved;
    WORD type;
    WORD count;
} ErIcoDirHeader;

typedef struct ErIcoDirEntry {
    BYTE width;
    BYTE height;
    BYTE color_count;
    BYTE reserved;
    WORD planes;
    WORD bit_count;
    DWORD bytes_in_res;
    DWORD image_offset;
} ErIcoDirEntry;

typedef struct ErGrpIconDirEntry {
    BYTE width;
    BYTE height;
    BYTE color_count;
    BYTE reserved;
    WORD planes;
    WORD bit_count;
    DWORD bytes_in_res;
    WORD resource_id;
} ErGrpIconDirEntry;
#pragma pack(pop)
#endif

#include "ast.h"
#include "fileio.h"
#include "frontend.h"

typedef enum ErPackagedFileKind {
    ER_PACKAGE_FILE_ENTRY = 1,
    ER_PACKAGE_FILE_ASSET = 2,
    ER_PACKAGE_FILE_PY_HELPER = 3,
    ER_PACKAGE_FILE_CPP_HELPER = 4,
    ER_PACKAGE_FILE_METADATA = 5
} ErPackagedFileKind;

typedef enum ErCollectedFileKind {
    ER_COLLECTED_FILE_ENTRY,
    ER_COLLECTED_FILE_ASSET,
    ER_COLLECTED_FILE_PY_SOURCE,
    ER_COLLECTED_FILE_PY_HELPER,
    ER_COLLECTED_FILE_METADATA
} ErCollectedFileKind;

typedef struct ErPackageFooterV1 {
    char magic[8];
    uint64_t source_size;
} ErPackageFooterV1;

typedef struct ErPackageFooterV2 {
    char magic[8];
    uint64_t payload_size;
} ErPackageFooterV2;

typedef struct ErPackageHeaderV2 {
    uint32_t file_count;
    uint32_t entry_path_length;
} ErPackageHeaderV2;

typedef struct ErPackageFileHeaderV2 {
    uint32_t kind;
    uint32_t path_length;
    uint64_t size;
} ErPackageFileHeaderV2;

typedef struct ErCollectedFile {
    char *absolute_path;
    ErCollectedFileKind kind;
} ErCollectedFile;

typedef struct ErCollectedFileArray {
    ErCollectedFile *items;
    size_t count;
    size_t capacity;
} ErCollectedFileArray;

typedef struct ErPackagedResource {
    char *absolute_path;
    char *relative_path;
    uint32_t kind;
} ErPackagedResource;

typedef struct ErPackagedResourceArray {
    ErPackagedResource *items;
    size_t count;
    size_t capacity;
} ErPackagedResourceArray;

typedef struct ErStringArray {
    char **items;
    size_t count;
    size_t capacity;
} ErStringArray;

static const char ER_PACKAGE_MAGIC_V1[8] = { 'E', 'R', 'I', 'R', 'E', 'P', 'K', '1' };
static const char ER_PACKAGE_MAGIC_V2[8] = { 'E', 'R', 'I', 'R', 'E', 'P', '2', '1' };

static bool er_packager_command_succeeds(const char *command);
static bool er_packager_add_packaged_resource(
    ErPackagedResourceArray *array,
    const char *absolute_path,
    const char *relative_path,
    uint32_t kind,
    ErError *error
);

static char *er_packager_dup(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *) malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static bool er_packager_char_equal(char left, char right) {
#ifdef _WIN32
    if (left >= 'A' && left <= 'Z') {
        left = (char) (left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = (char) (right - 'A' + 'a');
    }
#endif
    return left == right;
}

static bool er_packager_is_separator(char ch) {
    return ch == '\\' || ch == '/';
}

static bool er_packager_paths_equal(const char *left, const char *right) {
    size_t i = 0;

    if (!left || !right) {
        return false;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (er_packager_is_separator(left[i]) && er_packager_is_separator(right[i])) {
            ++i;
            continue;
        }
        if (!er_packager_char_equal(left[i], right[i])) {
            return false;
        }
        ++i;
    }

    return left[i] == '\0' && right[i] == '\0';
}

static bool er_packager_is_absolute_path(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

#ifdef _WIN32
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        return true;
    }
    if (path[0] == '\\' && path[1] == '\\') {
        return true;
    }
#endif

    return path[0] == '/' || path[0] == '\\';
}

static void er_packager_replace_extension(
    const char *path,
    const char *new_extension,
    char *out_path,
    size_t out_capacity
) {
    const char *dot = strrchr(path ? path : "", '.');
    const char *slash = strrchr(path ? path : "", '\\');
    const char *slash2 = strrchr(path ? path : "", '/');
    const char *last_sep = slash;
    size_t prefix_length;

    if (slash2 && (!last_sep || slash2 > last_sep)) {
        last_sep = slash2;
    }

    if (!dot || (last_sep && dot < last_sep)) {
        snprintf(out_path, out_capacity, "%s%s", path ? path : "", new_extension ? new_extension : "");
        return;
    }

    prefix_length = (size_t) (dot - path);
    if (prefix_length >= out_capacity) {
        prefix_length = out_capacity > 0 ? out_capacity - 1 : 0;
    }
    memcpy(out_path, path, prefix_length);
    out_path[prefix_length] = '\0';
    snprintf(out_path + prefix_length, out_capacity > prefix_length ? out_capacity - prefix_length : 0, "%s", new_extension ? new_extension : "");
}

static char *er_packager_dirname_dup(const char *path) {
    char buffer[1024];

    er_path_dirname(path, buffer, sizeof(buffer));
    return er_packager_dup(buffer);
}

static const char *er_packager_basename(const char *path) {
    const char *base = path ? path : "";
    const char *slash = strrchr(base, '\\');
    const char *slash2 = strrchr(base, '/');

    if (slash && slash + 1 > base) {
        base = slash + 1;
    }
    if (slash2 && slash2 + 1 > base) {
        base = slash2 + 1;
    }
    return base;
}

static void er_packager_join_relative_path(
    const char *directory,
    const char *child,
    char *out_path,
    size_t out_capacity
) {
    if (!directory || directory[0] == '\0' || strcmp(directory, ".") == 0) {
        snprintf(out_path, out_capacity, "%s", child ? child : "");
        return;
    }
    er_path_join(directory, child ? child : "", out_path, out_capacity);
}

static bool er_packager_json_escape_string(const char *text, char *out_text, size_t out_capacity) {
    size_t out_index = 0;
    const char *cursor = text ? text : "";

    if (!out_text || out_capacity == 0) {
        return false;
    }

    while (*cursor != '\0') {
        char ch = *cursor++;
        const char *replacement = NULL;
        char single[3];

        if (ch == '\\') {
            replacement = "\\\\";
        } else if (ch == '"') {
            replacement = "\\\"";
        } else if (ch == '\n') {
            replacement = "\\n";
        } else if (ch == '\r') {
            replacement = "\\r";
        } else if (ch == '\t') {
            replacement = "\\t";
        } else {
            single[0] = ch;
            single[1] = '\0';
            replacement = single;
        }

        while (*replacement != '\0') {
            if (out_index + 1 >= out_capacity) {
                return false;
            }
            out_text[out_index++] = *replacement++;
        }
    }

    out_text[out_index] = '\0';
    return true;
}

static bool er_packager_json_extract_string(
    const char *json_text,
    const char *key,
    char *out_value,
    size_t out_capacity
) {
    char pattern[128];
    const char *match;
    const char *cursor;
    size_t length;

    if (!json_text || !key || !out_value || out_capacity == 0) {
        return false;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    match = strstr(json_text, pattern);
    if (!match) {
        return false;
    }

    cursor = match + strlen(pattern);
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        ++cursor;
    }
    if (*cursor != ':') {
        return false;
    }
    ++cursor;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        ++cursor;
    }
    if (*cursor != '"') {
        return false;
    }
    ++cursor;

    length = 0;
    while (*cursor != '\0' && *cursor != '"') {
        if (*cursor == '\\' && cursor[1] != '\0') {
            ++cursor;
        }
        if (length + 1 >= out_capacity) {
            return false;
        }
        out_value[length++] = *cursor++;
    }

    if (*cursor != '"') {
        return false;
    }

    out_value[length] = '\0';
    return true;
}

static bool er_packager_resolve_build_icon(
    const char *source_path,
    const ErPackagerBuildOptions *options,
    const char *build_root,
    char *out_exe_icon_path,
    size_t out_exe_icon_path_capacity,
    char *out_runtime_icon_path,
    size_t out_runtime_icon_path_capacity,
    char *out_runtime_icon_relative,
    size_t out_runtime_icon_relative_capacity,
    bool *out_found,
    ErError *error
) {
    char source_dir[1024];
    char app_json_path[1024];
    char icon_value[1024];
    char win_icon_value[1024];
    char resolved_icon_path[1024];
    char staged_runtime_icon_path[1024];
    char staged_runtime_icon_relative[1024];
    char staged_exe_icon_path[1024];
    char module_path[1024];
    char module_dir[1024];
    char converter_script[1024];
    char command[4096];
    const char *raw_icon = NULL;
    const char *extension = NULL;
    char *json_text = NULL;
    size_t json_size = 0;

    *out_found = false;
    if (out_exe_icon_path && out_exe_icon_path_capacity > 0) {
        out_exe_icon_path[0] = '\0';
    }
    if (out_runtime_icon_path && out_runtime_icon_path_capacity > 0) {
        out_runtime_icon_path[0] = '\0';
    }
    if (out_runtime_icon_relative && out_runtime_icon_relative_capacity > 0) {
        out_runtime_icon_relative[0] = '\0';
    }

    if (!source_path || !build_root || !out_exe_icon_path || out_exe_icon_path_capacity == 0) {
        return true;
    }

    er_path_dirname(source_path, source_dir, sizeof(source_dir));
    if (options && options->icon_path_override && options->icon_path_override[0] != '\0') {
        raw_icon = options->icon_path_override;
    } else {
        snprintf(app_json_path, sizeof(app_json_path), "%s\\app.json", source_dir);
        if (!er_file_exists(app_json_path)) {
            return true;
        }

        if (!er_file_read_all(app_json_path, &json_text, &json_size, error)) {
            return false;
        }
        (void) json_size;

        if (er_packager_json_extract_string(json_text, "win_icon", win_icon_value, sizeof(win_icon_value)) &&
            win_icon_value[0] != '\0') {
            raw_icon = win_icon_value;
        } else if (er_packager_json_extract_string(json_text, "icon", icon_value, sizeof(icon_value)) &&
                   icon_value[0] != '\0') {
            raw_icon = icon_value;
        }
    }
    free(json_text);

    if (!raw_icon || raw_icon[0] == '\0') {
        return true;
    }

    if (er_packager_is_absolute_path(raw_icon)) {
        snprintf(resolved_icon_path, sizeof(resolved_icon_path), "%s", raw_icon);
    } else {
        er_path_join(source_dir, raw_icon, resolved_icon_path, sizeof(resolved_icon_path));
        if (!er_file_exists(resolved_icon_path) && er_file_exists(raw_icon)) {
            snprintf(resolved_icon_path, sizeof(resolved_icon_path), "%s", raw_icon);
        }
    }

    if (!er_file_exists(resolved_icon_path)) {
        er_error_set(error, 0, 0, "Build icon file was not found: %s", resolved_icon_path);
        return false;
    }

    extension = strrchr(resolved_icon_path, '.');
    if (!extension || (!er_path_has_extension(resolved_icon_path, ".ico") && !er_path_has_extension(resolved_icon_path, ".png"))) {
        er_error_set(error, 0, 0, "Build icon must be a .ico or .png file: %s", resolved_icon_path);
        return false;
    }

    if (out_runtime_icon_path && out_runtime_icon_path_capacity > 0 &&
        out_runtime_icon_relative && out_runtime_icon_relative_capacity > 0 &&
        options && options->icon_path_override && options->icon_path_override[0] != '\0') {
        snprintf(staged_runtime_icon_path, sizeof(staged_runtime_icon_path), "%s\\__erire_cli_icon%s", build_root, extension);
        if (!er_file_copy(resolved_icon_path, staged_runtime_icon_path, error)) {
            return false;
        }
        snprintf(staged_runtime_icon_relative, sizeof(staged_runtime_icon_relative), "assets\\__erire_cli_icon%s", extension);
        snprintf(out_runtime_icon_path, out_runtime_icon_path_capacity, "%s", staged_runtime_icon_path);
        snprintf(out_runtime_icon_relative, out_runtime_icon_relative_capacity, "%s", staged_runtime_icon_relative);
    }

    if (er_path_has_extension(resolved_icon_path, ".ico")) {
        if (options && options->icon_path_override && options->icon_path_override[0] != '\0') {
            snprintf(out_exe_icon_path, out_exe_icon_path_capacity, "%s", staged_runtime_icon_path);
        } else {
            snprintf(out_exe_icon_path, out_exe_icon_path_capacity, "%s", resolved_icon_path);
        }
        *out_found = true;
        return true;
    }

    if (!er_get_current_module_path(module_path, sizeof(module_path), error)) {
        return false;
    }
    er_path_dirname(module_path, module_dir, sizeof(module_dir));
    er_path_join(module_dir, "tools\\png_to_ico.py", converter_script, sizeof(converter_script));
    if (!er_file_exists(converter_script)) {
        er_error_set(error, 0, 0, "PNG to ICO converter script was not found: %s", converter_script);
        return false;
    }

    snprintf(staged_exe_icon_path, sizeof(staged_exe_icon_path), "%s\\__erire_cli_icon.ico", build_root);
    snprintf(
        command,
        sizeof(command),
        "python \"%s\" \"%s\" \"%s\" >NUL 2>&1",
        converter_script,
        resolved_icon_path,
        staged_exe_icon_path
    );
    if (!er_packager_command_succeeds(command)) {
        er_error_set(error, 0, 0, "Could not convert PNG build icon to ICO: %s", resolved_icon_path);
        return false;
    }
    if (!er_file_exists(staged_exe_icon_path)) {
        er_error_set(error, 0, 0, "PNG build icon conversion did not produce an ICO file");
        return false;
    }

    snprintf(out_exe_icon_path, out_exe_icon_path_capacity, "%s", staged_exe_icon_path);
    *out_found = true;
    return true;
}

static bool er_packager_upsert_packaged_resource(
    ErPackagedResourceArray *array,
    const char *absolute_path,
    const char *relative_path,
    uint32_t kind,
    ErError *error
) {
    size_t i;
    char *absolute_copy;
    char *relative_copy;

    if (!array || !absolute_path || !relative_path) {
        return true;
    }

    for (i = 0; i < array->count; ++i) {
        if (er_packager_paths_equal(array->items[i].relative_path, relative_path)) {
            absolute_copy = er_packager_dup(absolute_path);
            relative_copy = er_packager_dup(relative_path);
            if (!absolute_copy || !relative_copy) {
                free(absolute_copy);
                free(relative_copy);
                er_error_set(error, 0, 0, "Out of memory while replacing packaged resource");
                return false;
            }
            free(array->items[i].absolute_path);
            free(array->items[i].relative_path);
            array->items[i].absolute_path = absolute_copy;
            array->items[i].relative_path = relative_copy;
            array->items[i].kind = kind;
            return true;
        }
    }

    return er_packager_add_packaged_resource(array, absolute_path, relative_path, kind, error);
}

static bool er_packager_prepare_override_metadata(
    const char *entry_path,
    const char *build_root,
    const ErPackagerBuildOptions *options,
    const char *runtime_icon_relative,
    char *out_metadata_path,
    size_t out_metadata_path_capacity,
    bool *out_generated,
    ErError *error
) {
    char source_dir[1024];
    char app_json_path[1024];
    char entry_name[256];
    char app_name[256];
    char name_value[1024];
    char title_value[1024];
    char icon_value[1024];
    char win_icon_value[1024];
    char escaped_name[2048];
    char escaped_entry[1024];
    char escaped_title[2048];
    char escaped_icon[2048];
    char json_buffer[8192];
    char *json_text = NULL;
    size_t json_size = 0;
    const char *final_name;
    const char *final_title;
    const char *final_icon;
    bool has_locked_title = false;
    bool needs_override = false;

    if (out_generated) {
        *out_generated = false;
    }
    if (out_metadata_path && out_metadata_path_capacity > 0) {
        out_metadata_path[0] = '\0';
    }

    if (!entry_path || !build_root || !out_metadata_path || out_metadata_path_capacity == 0) {
        return true;
    }

    if (options && options->win_title_override && options->win_title_override[0] != '\0') {
        needs_override = true;
    }
    if (options && options->icon_path_override && options->icon_path_override[0] != '\0') {
        needs_override = true;
    }
    if (!needs_override) {
        return true;
    }

    name_value[0] = '\0';
    title_value[0] = '\0';
    icon_value[0] = '\0';
    win_icon_value[0] = '\0';

    er_path_dirname(entry_path, source_dir, sizeof(source_dir));
    snprintf(app_json_path, sizeof(app_json_path), "%s\\app.json", source_dir);
    if (er_file_exists(app_json_path)) {
        if (!er_file_read_all(app_json_path, &json_text, &json_size, error)) {
            return false;
        }
        (void) json_size;
        er_packager_json_extract_string(json_text, "name", name_value, sizeof(name_value));
        er_packager_json_extract_string(json_text, "win_title", title_value, sizeof(title_value));
        er_packager_json_extract_string(json_text, "icon", icon_value, sizeof(icon_value));
        er_packager_json_extract_string(json_text, "win_icon", win_icon_value, sizeof(win_icon_value));
        free(json_text);
        json_text = NULL;
    }

    snprintf(entry_name, sizeof(entry_name), "%s", er_packager_basename(entry_path));
    er_path_basename_without_extension(entry_path, app_name, sizeof(app_name));

    final_name = name_value[0] != '\0' ? name_value : app_name;
    has_locked_title = (options && options->win_title_override && options->win_title_override[0] != '\0') ||
        title_value[0] != '\0';
    final_title = (options && options->win_title_override && options->win_title_override[0] != '\0')
        ? options->win_title_override
        : (title_value[0] != '\0' ? title_value : NULL);
    if (runtime_icon_relative && runtime_icon_relative[0] != '\0') {
        final_icon = runtime_icon_relative;
    } else if (win_icon_value[0] != '\0') {
        final_icon = win_icon_value;
    } else {
        final_icon = icon_value[0] != '\0' ? icon_value : NULL;
    }

    if (!er_packager_json_escape_string(final_name, escaped_name, sizeof(escaped_name)) ||
        !er_packager_json_escape_string(entry_name, escaped_entry, sizeof(escaped_entry))) {
        er_error_set(error, 0, 0, "Out of memory while preparing app metadata");
        return false;
    }
    if (has_locked_title) {
        if (!er_packager_json_escape_string(final_title, escaped_title, sizeof(escaped_title))) {
            er_error_set(error, 0, 0, "Out of memory while preparing app title metadata");
            return false;
        }
    } else {
        escaped_title[0] = '\0';
    }
    if (final_icon && final_icon[0] != '\0') {
        if (!er_packager_json_escape_string(final_icon, escaped_icon, sizeof(escaped_icon))) {
            er_error_set(error, 0, 0, "Out of memory while preparing app icon metadata");
            return false;
        }
    } else {
        escaped_icon[0] = '\0';
    }

    if (has_locked_title && escaped_icon[0] != '\0') {
        snprintf(
            json_buffer,
            sizeof(json_buffer),
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"entry\": \"%s\",\n"
            "  \"win_title\": \"%s\",\n"
            "  \"win_icon\": \"%s\",\n"
            "  \"icon\": \"%s\"\n"
            "}\n",
            escaped_name,
            escaped_entry,
            escaped_title,
            escaped_icon,
            escaped_icon
        );
    } else if (has_locked_title) {
        snprintf(
            json_buffer,
            sizeof(json_buffer),
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"entry\": \"%s\",\n"
            "  \"win_title\": \"%s\"\n"
            "}\n",
            escaped_name,
            escaped_entry,
            escaped_title
        );
    } else if (escaped_icon[0] != '\0') {
        snprintf(
            json_buffer,
            sizeof(json_buffer),
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"entry\": \"%s\",\n"
            "  \"win_icon\": \"%s\",\n"
            "  \"icon\": \"%s\"\n"
            "}\n",
            escaped_name,
            escaped_entry,
            escaped_icon,
            escaped_icon
        );
    } else {
        snprintf(
            json_buffer,
            sizeof(json_buffer),
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"entry\": \"%s\"\n"
            "}\n",
            escaped_name,
            escaped_entry
        );
    }

    snprintf(out_metadata_path, out_metadata_path_capacity, "%s\\__erire_app.json", build_root);
    if (!er_file_write_all(out_metadata_path, json_buffer, strlen(json_buffer), error)) {
        return false;
    }

    if (out_generated) {
        *out_generated = true;
    }
    return true;
}

static bool er_packager_normalize_path(const char *path, char **out_path, ErError *error) {
    char buffer[1024];

    *out_path = NULL;

#ifdef _WIN32
    if (!_fullpath(buffer, path, sizeof(buffer))) {
        er_error_set(error, 0, 0, "Could not normalize path: %s", path);
        return false;
    }
#else
    {
        char *resolved = realpath(path, NULL);
        if (!resolved) {
            er_error_set(error, 0, 0, "Could not normalize path: %s", path);
            return false;
        }
        strncpy(buffer, resolved, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        free(resolved);
    }
#endif

    *out_path = er_packager_dup(buffer);
    if (!*out_path) {
        er_error_set(error, 0, 0, "Out of memory while normalizing path");
        return false;
    }

    return true;
}

static bool er_packager_directory_exists(const char *path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    (void) path;
    return false;
#endif
}

static bool er_packager_ensure_collected_capacity(ErCollectedFileArray *array, ErError *error) {
    ErCollectedFile *new_items;
    size_t new_capacity;

    if (array->count < array->capacity) {
        return true;
    }

    new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
    new_items = (ErCollectedFile *) realloc(array->items, new_capacity * sizeof(ErCollectedFile));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing collected file list");
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    return true;
}

static bool er_packager_ensure_packaged_capacity(ErPackagedResourceArray *array, ErError *error) {
    ErPackagedResource *new_items;
    size_t new_capacity;

    if (array->count < array->capacity) {
        return true;
    }

    new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
    new_items = (ErPackagedResource *) realloc(array->items, new_capacity * sizeof(ErPackagedResource));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing packaged resource list");
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    return true;
}

static bool er_packager_ensure_string_capacity(ErStringArray *array, ErError *error) {
    char **new_items;
    size_t new_capacity;

    if (array->count < array->capacity) {
        return true;
    }

    new_capacity = array->capacity == 0 ? 4 : array->capacity * 2;
    new_items = (char **) realloc(array->items, new_capacity * sizeof(char *));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing string list");
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    return true;
}

static bool er_packager_add_string(ErStringArray *array, const char *text, ErError *error) {
    char *copy;

    if (!array || !text) {
        return true;
    }

    if (!er_packager_ensure_string_capacity(array, error)) {
        return false;
    }

    copy = er_packager_dup(text);
    if (!copy) {
        er_error_set(error, 0, 0, "Out of memory while copying string");
        return false;
    }

    array->items[array->count++] = copy;
    return true;
}

static void er_packager_free_strings(ErStringArray *array) {
    size_t i;

    for (i = 0; i < array->count; ++i) {
        free(array->items[i]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static bool er_packager_add_collected_file(
    ErCollectedFileArray *array,
    const char *absolute_path,
    ErCollectedFileKind kind,
    ErError *error
) {
    size_t i;
    char *normalized = NULL;

    if (!absolute_path) {
        return true;
    }

    if (!er_packager_normalize_path(absolute_path, &normalized, error)) {
        return false;
    }

    for (i = 0; i < array->count; ++i) {
        if (er_packager_paths_equal(array->items[i].absolute_path, normalized)) {
            free(normalized);
            return true;
        }
    }

    if (!er_packager_ensure_collected_capacity(array, error)) {
        free(normalized);
        return false;
    }

    array->items[array->count].absolute_path = normalized;
    array->items[array->count].kind = kind;
    ++array->count;
    return true;
}

static bool er_packager_add_packaged_resource(
    ErPackagedResourceArray *array,
    const char *absolute_path,
    const char *relative_path,
    uint32_t kind,
    ErError *error
) {
    size_t i;
    char *absolute_copy;
    char *relative_copy;

    for (i = 0; i < array->count; ++i) {
        if (er_packager_paths_equal(array->items[i].relative_path, relative_path)) {
            return true;
        }
    }

    if (!er_packager_ensure_packaged_capacity(array, error)) {
        return false;
    }

    absolute_copy = er_packager_dup(absolute_path);
    relative_copy = er_packager_dup(relative_path);
    if (!absolute_copy || !relative_copy) {
        free(absolute_copy);
        free(relative_copy);
        er_error_set(error, 0, 0, "Out of memory while recording packaged resource");
        return false;
    }

    array->items[array->count].absolute_path = absolute_copy;
    array->items[array->count].relative_path = relative_copy;
    array->items[array->count].kind = kind;
    ++array->count;
    return true;
}

#ifdef _WIN32
static bool er_packager_add_packaged_resources_from_directory(
    const char *source_directory,
    const char *relative_directory,
    uint32_t kind,
    ErPackagedResourceArray *packaged,
    ErError *error
) {
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    char pattern[1024];

    snprintf(pattern, sizeof(pattern), "%s\\*", source_directory);
    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return true;
    }

    do {
        char child_source[1024];
        char child_relative[1024];

        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        er_path_join(source_directory, find_data.cFileName, child_source, sizeof(child_source));
        er_path_join(relative_directory, find_data.cFileName, child_relative, sizeof(child_relative));

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!er_packager_add_packaged_resources_from_directory(child_source, child_relative, kind, packaged, error)) {
                FindClose(handle);
                return false;
            }
            continue;
        }

        if (!er_packager_add_packaged_resource(packaged, child_source, child_relative, kind, error)) {
            FindClose(handle);
            return false;
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return true;
}
#else
static bool er_packager_add_packaged_resources_from_directory(
    const char *source_directory,
    const char *relative_directory,
    uint32_t kind,
    ErPackagedResourceArray *packaged,
    ErError *error
) {
    (void) source_directory;
    (void) relative_directory;
    (void) kind;
    (void) packaged;
    er_error_set(error, 0, 0, "Directory-backed Python helper packaging is currently implemented on Windows only");
    return false;
}
#endif

static void er_packager_free_collected_files(ErCollectedFileArray *array) {
    size_t i;

    for (i = 0; i < array->count; ++i) {
        free(array->items[i].absolute_path);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static void er_packager_free_packaged_resources(ErPackagedResourceArray *array) {
    size_t i;

    for (i = 0; i < array->count; ++i) {
        free(array->items[i].absolute_path);
        free(array->items[i].relative_path);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static bool er_packager_find_runner(char *out_path, size_t out_capacity, ErError *error) {
    char exe_path[1024];
    char dir[1024];

    if (!er_get_current_module_path(exe_path, sizeof(exe_path), error)) {
        return false;
    }

    er_path_dirname(exe_path, dir, sizeof(dir));
    er_path_join(dir, "ErireRunner.exe", out_path, out_capacity);

    if (!er_file_exists(out_path)) {
        er_error_set(error, 0, 0, "Could not find ErireRunner.exe next to erire.exe");
        return false;
    }

    return true;
}

static const char *er_packager_literal_string(const ErValue *value) {
    if (!value) {
        return NULL;
    }

    if (value->type == ER_VALUE_STRING) {
        return value->as.string;
    }
    if (value->type == ER_VALUE_SYMBOL) {
        return value->as.symbol;
    }
    return NULL;
}

static bool er_packager_collect_relative_file(
    const char *literal_path,
    const char *source_dir,
    ErCollectedFileKind kind,
    ErCollectedFileArray *files,
    ErError *error,
    const char *label
) {
    char candidate[1024];

    if (!literal_path || literal_path[0] == '\0') {
        return true;
    }

    if (er_packager_is_absolute_path(literal_path)) {
        er_error_set(error, 0, 0, "Packaged apps require relative %s paths, got: %s", label, literal_path);
        return false;
    }

    er_path_join(source_dir, literal_path, candidate, sizeof(candidate));
    if (!er_file_exists(candidate)) {
        er_error_set(error, 0, 0, "Referenced %s file was not found: %s", label, candidate);
        return false;
    }

    return er_packager_add_collected_file(files, candidate, kind, error);
}

static bool er_packager_collect_statement_resources(
    const ErStatement *statement,
    const char *source_dir,
    ErCollectedFileArray *files,
    ErError *error
);

static bool er_packager_collect_block_resources(
    const ErStatementArray *body,
    const char *source_dir,
    ErCollectedFileArray *files,
    ErError *error
) {
    size_t i;

    for (i = 0; i < body->count; ++i) {
        if (!er_packager_collect_statement_resources(body->items[i], source_dir, files, error)) {
            return false;
        }
    }
    return true;
}

static bool er_packager_collect_element_resources(
    const ErElement *element,
    const char *source_dir,
    ErCollectedFileArray *files,
    ErError *error
) {
    size_t i;

    if (!element) {
        return true;
    }

    if (element->base_property && strcmp(element->base_property, "src") == 0 && element->base_args.count > 0) {
        if (!er_packager_collect_relative_file(
                er_packager_literal_string(&element->base_args.items[0]),
                source_dir,
                ER_COLLECTED_FILE_ASSET,
                files,
                error,
                "asset")) {
            return false;
        }
    }

    for (i = 0; i < element->properties.count; ++i) {
        const ErProperty *property = &element->properties.items[i];

        if (property->is_event) {
            if (!er_packager_collect_block_resources(&property->block, source_dir, files, error)) {
                return false;
            }
            continue;
        }

        if (strcmp(property->name, "src") == 0 && property->values.count > 0) {
            if (!er_packager_collect_relative_file(
                    er_packager_literal_string(&property->values.items[0]),
                    source_dir,
                    ER_COLLECTED_FILE_ASSET,
                    files,
                    error,
                    "asset")) {
                return false;
            }
        }
    }

    return true;
}

#ifdef _WIN32
static bool er_packager_apply_exe_icon(const char *exe_path, const char *icon_path, ErError *error) {
    char *icon_bytes = NULL;
    size_t icon_size = 0;
    HANDLE update_handle = NULL;
    const ErIcoDirHeader *header;
    const ErIcoDirEntry *entries;
    BYTE *group_bytes = NULL;
    size_t group_size;
    WORD i;
    WORD language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

    if (!er_file_read_all(icon_path, &icon_bytes, &icon_size, error)) {
        return false;
    }

    if (icon_size < sizeof(ErIcoDirHeader)) {
        free(icon_bytes);
        er_error_set(error, 0, 0, "ICO file is too small: %s", icon_path);
        return false;
    }

    header = (const ErIcoDirHeader *) icon_bytes;
    if (header->reserved != 0 || header->type != 1 || header->count == 0) {
        free(icon_bytes);
        er_error_set(error, 0, 0, "Invalid ICO file header: %s", icon_path);
        return false;
    }

    if (icon_size < sizeof(ErIcoDirHeader) + ((size_t) header->count * sizeof(ErIcoDirEntry))) {
        free(icon_bytes);
        er_error_set(error, 0, 0, "ICO directory is truncated: %s", icon_path);
        return false;
    }

    entries = (const ErIcoDirEntry *) (icon_bytes + sizeof(ErIcoDirHeader));
    group_size = sizeof(ErIcoDirHeader) + ((size_t) header->count * sizeof(ErGrpIconDirEntry));
    group_bytes = (BYTE *) calloc(1, group_size);
    if (!group_bytes) {
        free(icon_bytes);
        er_error_set(error, 0, 0, "Out of memory while building icon group resource");
        return false;
    }

    memcpy(group_bytes, header, sizeof(ErIcoDirHeader));
    for (i = 0; i < header->count; ++i) {
        const ErIcoDirEntry *entry = &entries[i];
        ErGrpIconDirEntry *group_entry = (ErGrpIconDirEntry *) (group_bytes + sizeof(ErIcoDirHeader) + ((size_t) i * sizeof(ErGrpIconDirEntry)));
        WORD resource_id = (WORD) (10 + i);

        if ((size_t) entry->image_offset + (size_t) entry->bytes_in_res > icon_size) {
            free(group_bytes);
            free(icon_bytes);
            er_error_set(error, 0, 0, "ICO image data is truncated: %s", icon_path);
            return false;
        }

        group_entry->width = entry->width;
        group_entry->height = entry->height;
        group_entry->color_count = entry->color_count;
        group_entry->reserved = entry->reserved;
        group_entry->planes = entry->planes;
        group_entry->bit_count = entry->bit_count;
        group_entry->bytes_in_res = entry->bytes_in_res;
        group_entry->resource_id = resource_id;
    }

    update_handle = BeginUpdateResourceA(exe_path, FALSE);
    if (!update_handle) {
        free(group_bytes);
        free(icon_bytes);
        er_error_set(error, 0, 0, "Could not open executable resources for icon update");
        return false;
    }

    for (i = 0; i < header->count; ++i) {
        const ErIcoDirEntry *entry = &entries[i];
        if (!UpdateResourceA(
                update_handle,
                RT_ICON,
                MAKEINTRESOURCEA(10 + i),
                language,
                icon_bytes + entry->image_offset,
                entry->bytes_in_res
            )) {
            EndUpdateResourceA(update_handle, TRUE);
            free(group_bytes);
            free(icon_bytes);
            er_error_set(error, 0, 0, "Could not update RT_ICON resources in packaged executable");
            return false;
        }
    }

    if (!UpdateResourceA(
            update_handle,
            RT_GROUP_ICON,
            MAKEINTRESOURCEA(1),
            language,
            group_bytes,
            (DWORD) group_size
        )) {
        EndUpdateResourceA(update_handle, TRUE);
        free(group_bytes);
        free(icon_bytes);
        er_error_set(error, 0, 0, "Could not update RT_GROUP_ICON resource in packaged executable");
        return false;
    }

    if (!EndUpdateResourceA(update_handle, FALSE)) {
        free(group_bytes);
        free(icon_bytes);
        er_error_set(error, 0, 0, "Could not finalize packaged executable icon update");
        return false;
    }

    free(group_bytes);
    free(icon_bytes);
    return true;
}
#else
static bool er_packager_apply_exe_icon(const char *exe_path, const char *icon_path, ErError *error) {
    (void) exe_path;
    (void) icon_path;
    er_error_set(error, 0, 0, "Packaged executable icon updates are currently supported on Windows only");
    return false;
}
#endif

static bool er_packager_collect_statement_resources(
    const ErStatement *statement,
    const char *source_dir,
    ErCollectedFileArray *files,
    ErError *error
) {
    if (!statement) {
        return true;
    }

    switch (statement->type) {
        case ER_STMT_IMPORT:
            if (statement->as.import_directive.type == ER_IMPORT_DIRECTIVE_PYTHON) {
                return er_packager_collect_relative_file(
                    statement->as.import_directive.path,
                    source_dir,
                    er_path_has_extension(statement->as.import_directive.path, ".exe")
                        ? ER_COLLECTED_FILE_PY_HELPER
                        : ER_COLLECTED_FILE_PY_SOURCE,
                    files,
                    error,
                    "Python bridge"
                );
            }
            return true;
        case ER_STMT_SCREEN_ADD:
            return er_packager_collect_element_resources(statement->as.add.element, source_dir, files, error);
        case ER_STMT_IF:
            if (!er_packager_collect_block_resources(&statement->as.if_stmt.body, source_dir, files, error)) {
                return false;
            }
            {
                size_t else_if_index;
                for (else_if_index = 0; else_if_index < statement->as.if_stmt.else_ifs.count; ++else_if_index) {
                    if (!er_packager_collect_block_resources(
                            &statement->as.if_stmt.else_ifs.items[else_if_index].body,
                            source_dir,
                            files,
                            error
                        )) {
                        return false;
                    }
                }
            }
            if (statement->as.if_stmt.has_else) {
                return er_packager_collect_block_resources(&statement->as.if_stmt.else_body, source_dir, files, error);
            }
            return true;
        case ER_STMT_WHILE:
            return er_packager_collect_block_resources(&statement->as.while_stmt.body, source_dir, files, error);
        case ER_STMT_FOR:
            return er_packager_collect_block_resources(&statement->as.for_stmt.body, source_dir, files, error);
        default:
            return true;
    }
}

#ifdef _WIN32
static bool er_packager_collect_directory_recursive(
    const char *directory_path,
    ErCollectedFileKind kind,
    const char *extension_filter,
    ErCollectedFileArray *files,
    ErError *error
) {
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    char pattern[1024];

    snprintf(pattern, sizeof(pattern), "%s\\*", directory_path);
    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return true;
    }

    do {
        char child_path[1024];

        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        er_path_join(directory_path, find_data.cFileName, child_path, sizeof(child_path));
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!er_packager_collect_directory_recursive(child_path, kind, extension_filter, files, error)) {
                FindClose(handle);
                return false;
            }
            continue;
        }

        if (extension_filter && !er_path_has_extension(child_path, extension_filter)) {
            continue;
        }

        if (!er_packager_add_collected_file(files, child_path, kind, error)) {
            FindClose(handle);
            return false;
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return true;
}
#endif

#ifdef _WIN32
static bool er_packager_collect_python_directory_recursive(
    const char *directory_path,
    ErCollectedFileArray *files,
    ErError *error
) {
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    char pattern[1024];

    snprintf(pattern, sizeof(pattern), "%s\\*", directory_path);
    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return true;
    }

    do {
        char child_path[1024];
        ErCollectedFileKind kind;

        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        er_path_join(directory_path, find_data.cFileName, child_path, sizeof(child_path));
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!er_packager_collect_python_directory_recursive(child_path, files, error)) {
                FindClose(handle);
                return false;
            }
            continue;
        }

        if (er_path_has_extension(child_path, ".py")) {
            kind = ER_COLLECTED_FILE_PY_SOURCE;
        } else if (er_path_has_extension(child_path, ".exe")) {
            kind = ER_COLLECTED_FILE_PY_HELPER;
        } else {
            kind = ER_COLLECTED_FILE_ASSET;
        }

        if (!er_packager_add_collected_file(files, child_path, kind, error)) {
            FindClose(handle);
            return false;
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return true;
}
#endif

static char *er_packager_common_directory_pair(const char *left, const char *right) {
    size_t i = 0;
    size_t last_sep = 0;
    size_t left_length;
    size_t right_length;
    size_t prefix_length;
    char *result;

    if (!left) {
        return er_packager_dup(right);
    }
    if (!right) {
        return er_packager_dup(left);
    }

    left_length = strlen(left);
    right_length = strlen(right);

    while (i < left_length && i < right_length) {
        if (er_packager_is_separator(left[i]) && er_packager_is_separator(right[i])) {
            last_sep = i;
            ++i;
            continue;
        }
        if (!er_packager_char_equal(left[i], right[i])) {
            break;
        }
        if (er_packager_is_separator(left[i])) {
            last_sep = i;
        }
        ++i;
    }

#ifdef _WIN32
    if (last_sep == 0 &&
        left_length >= 3 &&
        right_length >= 3 &&
        er_packager_char_equal(left[0], right[0]) &&
        left[1] == ':' &&
        right[1] == ':') {
        last_sep = 2;
    }
#endif

    prefix_length = last_sep;
    if (left[last_sep] != '\0' && er_packager_is_separator(left[last_sep])) {
        prefix_length = last_sep + 1;
    }

    if (prefix_length == 0) {
        return er_packager_dup(".");
    }

    result = (char *) malloc(prefix_length + 1);
    if (!result) {
        return NULL;
    }

    memcpy(result, left, prefix_length);
    result[prefix_length] = '\0';

    while (prefix_length > 1 && er_packager_is_separator(result[prefix_length - 1])) {
#ifdef _WIN32
        if (prefix_length == 3 && result[1] == ':') {
            break;
        }
#endif
        result[prefix_length - 1] = '\0';
        --prefix_length;
    }

    return result;
}

static bool er_packager_compute_project_root(
    const char *entry_path,
    const ErCollectedFileArray *files,
    char **out_root,
    ErError *error
) {
    char *root = NULL;
    char *entry_dir = er_packager_dirname_dup(entry_path);
    size_t i;

    if (!entry_dir) {
        er_error_set(error, 0, 0, "Out of memory while determining project root");
        return false;
    }

    root = entry_dir;

    for (i = 0; i < files->count; ++i) {
        char *resource_dir = er_packager_dirname_dup(files->items[i].absolute_path);
        char *merged;

        if (!resource_dir) {
            free(root);
            er_error_set(error, 0, 0, "Out of memory while determining project root");
            return false;
        }

        merged = er_packager_common_directory_pair(root, resource_dir);
        free(root);
        free(resource_dir);
        if (!merged) {
            er_error_set(error, 0, 0, "Out of memory while determining project root");
            return false;
        }
        root = merged;
    }

    *out_root = root;
    return true;
}

static bool er_packager_make_relative_path(
    const char *root,
    const char *path,
    char *out_path,
    size_t out_capacity,
    ErError *error
) {
    size_t i = 0;
    size_t root_length = strlen(root);

    while (i < root_length) {
        if (er_packager_is_separator(root[i]) && er_packager_is_separator(path[i])) {
            ++i;
            continue;
        }
        if (!er_packager_char_equal(root[i], path[i])) {
            er_error_set(error, 0, 0, "Path is outside the packaged project root: %s", path);
            return false;
        }
        ++i;
    }

    if (path[i] == '\0') {
        er_error_set(error, 0, 0, "Expected a file path inside the project root");
        return false;
    }

    if (er_packager_is_separator(path[i])) {
        ++i;
    }

    snprintf(out_path, out_capacity, "%s", path + i);
    return true;
}

static bool er_packager_get_file_size(const char *path, uint64_t *out_size, ErError *error) {
    FILE *file;
    long length;

    *out_size = 0;

    file = fopen(path, "rb");
    if (!file) {
        er_error_set(error, 0, 0, "Could not open file: %s", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not seek file: %s", path);
        return false;
    }

    length = ftell(file);
    fclose(file);
    if (length < 0) {
        er_error_set(error, 0, 0, "Could not determine file size: %s", path);
        return false;
    }

    *out_size = (uint64_t) length;
    return true;
}

static bool er_packager_command_succeeds(const char *command) {
    return system(command) == 0;
}

static bool er_packager_python_compiler_available(void) {
    return er_packager_command_succeeds("python -m PyInstaller --version >NUL 2>&1");
}

static const char *er_packager_choose_cpp_compiler(void) {
    if (er_packager_command_succeeds("g++ --version >NUL 2>&1")) {
        return "g++";
    }
    if (er_packager_command_succeeds("clang++ --version >NUL 2>&1")) {
        return "clang++";
    }
    if (er_packager_command_succeeds("cl >NUL 2>&1")) {
        return "cl";
    }
    return NULL;
}

static bool er_packager_compile_python_helper(
    const char *script_path,
    const char *build_root,
    size_t index,
    char *out_bundle_dir,
    size_t out_bundle_dir_capacity,
    char *out_exe_path,
    size_t out_exe_path_capacity,
    ErError *error
) {
    char base_name[256];
    char dist_dir[1024];
    char work_dir[1024];
    char spec_dir[1024];
    char command[4096];

    er_path_basename_without_extension(script_path, base_name, sizeof(base_name));
    snprintf(dist_dir, sizeof(dist_dir), "%s\\py_dist_%zu", build_root, index);
    snprintf(work_dir, sizeof(work_dir), "%s\\py_work_%zu", build_root, index);
    snprintf(spec_dir, sizeof(spec_dir), "%s\\py_spec_%zu", build_root, index);

    if (!er_directory_create_recursive(dist_dir, error) ||
        !er_directory_create_recursive(work_dir, error) ||
        !er_directory_create_recursive(spec_dir, error)) {
        return false;
    }

    snprintf(
        command,
        sizeof(command),
        "python -m PyInstaller --onedir --noconfirm --clean --distpath \"%s\" --workpath \"%s\" --specpath \"%s\" \"%s\" >NUL 2>&1",
        dist_dir,
        work_dir,
        spec_dir,
        script_path
    );

    if (!er_packager_command_succeeds(command)) {
        er_error_set(error, 0, 0, "PyInstaller failed while building: %s", script_path);
        return false;
    }

    snprintf(out_bundle_dir, out_bundle_dir_capacity, "%s\\%s", dist_dir, base_name);
    snprintf(out_exe_path, out_exe_path_capacity, "%s\\%s.exe", out_bundle_dir, base_name);
    if (!er_file_exists(out_exe_path)) {
        er_error_set(error, 0, 0, "PyInstaller did not produce an executable for: %s", script_path);
        return false;
    }

    return true;
}

static bool er_packager_is_cpp_source(const char *path) {
    return er_path_has_extension(path, ".cpp") ||
           er_path_has_extension(path, ".cc") ||
           er_path_has_extension(path, ".cxx");
}

static bool er_packager_collect_cpp_sources(
    const char *cpp_dir,
    ErStringArray *cpp_sources,
    ErError *error
) {
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE handle;
    char pattern[1024];

    snprintf(pattern, sizeof(pattern), "%s\\*", cpp_dir);
    handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return true;
    }

    do {
        char child_path[1024];

        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        er_path_join(cpp_dir, find_data.cFileName, child_path, sizeof(child_path));
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!er_packager_collect_cpp_sources(child_path, cpp_sources, error)) {
                FindClose(handle);
                return false;
            }
            continue;
        }

        if (er_packager_is_cpp_source(child_path) &&
            !er_packager_add_string(cpp_sources, child_path, error)) {
            FindClose(handle);
            return false;
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return true;
#else
    (void) cpp_dir;
    (void) cpp_sources;
    er_error_set(error, 0, 0, "C++ helper collection is currently implemented on Windows only");
    return false;
#endif
}

static bool er_packager_compile_cpp_helper(
    const ErStringArray *cpp_sources,
    const char *cpp_dir,
    const char *build_root,
    char *out_exe_path,
    size_t out_exe_path_capacity,
    ErError *error
) {
    const char *compiler;
    char command[8192];
    size_t i;

    compiler = er_packager_choose_cpp_compiler();
    if (!compiler) {
        er_error_set(error, 0, 0, "C++ sources were found, but no supported C++ compiler is available (tried g++, clang++, cl)");
        return false;
    }

    snprintf(out_exe_path, out_exe_path_capacity, "%s\\cpp\\native_cpp.exe", build_root);
    {
        char out_dir[1024];
        er_path_dirname(out_exe_path, out_dir, sizeof(out_dir));
        if (!er_directory_create_recursive(out_dir, error)) {
            return false;
        }
    }

    if (strcmp(compiler, "cl") == 0) {
        snprintf(command, sizeof(command), "cmd /c \"cl /nologo /EHsc /std:c++17 /Fe:\\\"%s\\\" /I\\\"%s\\\"", out_exe_path, cpp_dir);
        for (i = 0; i < cpp_sources->count; ++i) {
            strncat(command, " \"", sizeof(command) - strlen(command) - 1);
            strncat(command, cpp_sources->items[i], sizeof(command) - strlen(command) - 1);
            strncat(command, "\"", sizeof(command) - strlen(command) - 1);
        }
        strncat(command, " >NUL 2>&1\"", sizeof(command) - strlen(command) - 1);
    } else {
        snprintf(command, sizeof(command), "%s -std=c++17 -O2 -I\"%s\"", compiler, cpp_dir);
        for (i = 0; i < cpp_sources->count; ++i) {
            strncat(command, " \"", sizeof(command) - strlen(command) - 1);
            strncat(command, cpp_sources->items[i], sizeof(command) - strlen(command) - 1);
            strncat(command, "\"", sizeof(command) - strlen(command) - 1);
        }
        strncat(command, " -o \"", sizeof(command) - strlen(command) - 1);
        strncat(command, out_exe_path, sizeof(command) - strlen(command) - 1);
        strncat(command, "\" >NUL 2>&1", sizeof(command) - strlen(command) - 1);
    }

    if (!er_packager_command_succeeds(command)) {
        er_error_set(error, 0, 0, "C++ helper build failed");
        return false;
    }

    if (!er_file_exists(out_exe_path)) {
        er_error_set(error, 0, 0, "C++ compiler did not produce native_cpp.exe");
        return false;
    }

    return true;
}

static bool er_packager_finalize_resources(
    const char *entry_path,
    const char *project_root,
    const ErCollectedFileArray *collected_files,
    const char *build_root,
    ErPackagedResourceArray *packaged,
    char **out_entry_relative_path,
    ErError *error
) {
    size_t i;
    char entry_relative[1024];
    char cpp_dir[1024];
    ErStringArray cpp_sources;

    memset(&cpp_sources, 0, sizeof(cpp_sources));
    *out_entry_relative_path = NULL;

    if (!er_packager_make_relative_path(project_root, entry_path, entry_relative, sizeof(entry_relative), error)) {
        return false;
    }
    if (!er_packager_add_packaged_resource(packaged, entry_path, entry_relative, ER_PACKAGE_FILE_ENTRY, error)) {
        return false;
    }
    *out_entry_relative_path = er_packager_dup(entry_relative);
    if (!*out_entry_relative_path) {
        er_error_set(error, 0, 0, "Out of memory while recording packaged entry path");
        return false;
    }

    for (i = 0; i < collected_files->count; ++i) {
        const ErCollectedFile *file = &collected_files->items[i];
        char relative_path[1024];

        if (file->kind == ER_COLLECTED_FILE_ENTRY) {
            continue;
        }

        if (!er_packager_make_relative_path(project_root, file->absolute_path, relative_path, sizeof(relative_path), error)) {
            free(*out_entry_relative_path);
            *out_entry_relative_path = NULL;
            return false;
        }

        if (file->kind == ER_COLLECTED_FILE_PY_SOURCE) {
            char compiled_dir[1024];
            char compiled_path[1024];
            char relative_helper_root[1024];

            if (!er_packager_python_compiler_available()) {
                er_error_set(
                    error,
                    0,
                    0,
                    "Python imports were found, but PyInstaller is not installed. Install it with: python -m pip install pyinstaller"
                );
                free(*out_entry_relative_path);
                *out_entry_relative_path = NULL;
                return false;
            }

            if (!er_packager_compile_python_helper(
                    file->absolute_path,
                    build_root,
                    i,
                    compiled_dir,
                    sizeof(compiled_dir),
                    compiled_path,
                    sizeof(compiled_path),
                    error)) {
                free(*out_entry_relative_path);
                *out_entry_relative_path = NULL;
                return false;
            }

            er_packager_replace_extension(relative_path, "", relative_helper_root, sizeof(relative_helper_root));
            if (!er_packager_add_packaged_resources_from_directory(
                    compiled_dir,
                    relative_helper_root,
                    ER_PACKAGE_FILE_PY_HELPER,
                    packaged,
                    error)) {
                free(*out_entry_relative_path);
                *out_entry_relative_path = NULL;
                return false;
            }
            continue;
        }

        if (file->kind == ER_COLLECTED_FILE_PY_HELPER) {
            char relative_exe[1024];
            er_packager_replace_extension(relative_path, ".exe", relative_exe, sizeof(relative_exe));
            if (!er_packager_add_packaged_resource(packaged, file->absolute_path, relative_exe, ER_PACKAGE_FILE_PY_HELPER, error)) {
                free(*out_entry_relative_path);
                *out_entry_relative_path = NULL;
                return false;
            }
            continue;
        }

        if (!er_packager_add_packaged_resource(
                packaged,
                file->absolute_path,
                relative_path,
                file->kind == ER_COLLECTED_FILE_METADATA ? ER_PACKAGE_FILE_METADATA : ER_PACKAGE_FILE_ASSET,
                error)) {
            free(*out_entry_relative_path);
            *out_entry_relative_path = NULL;
            return false;
        }
    }

    er_path_join(project_root, "cpp", cpp_dir, sizeof(cpp_dir));
    if (er_packager_directory_exists(cpp_dir)) {
        char compiled_cpp_path[1024];

        if (!er_packager_collect_cpp_sources(cpp_dir, &cpp_sources, error)) {
            free(*out_entry_relative_path);
            *out_entry_relative_path = NULL;
            er_packager_free_strings(&cpp_sources);
            return false;
        }

        if (cpp_sources.count > 0) {
            if (!er_packager_compile_cpp_helper(&cpp_sources, cpp_dir, build_root, compiled_cpp_path, sizeof(compiled_cpp_path), error)) {
                free(*out_entry_relative_path);
                *out_entry_relative_path = NULL;
                er_packager_free_strings(&cpp_sources);
                return false;
            }

            if (!er_packager_add_packaged_resource(packaged, compiled_cpp_path, "cpp\\native_cpp.exe", ER_PACKAGE_FILE_CPP_HELPER, error)) {
                free(*out_entry_relative_path);
                *out_entry_relative_path = NULL;
                er_packager_free_strings(&cpp_sources);
                return false;
            }
        }
    }

    er_packager_free_strings(&cpp_sources);
    return true;
}

static bool er_packager_build_payload_size(
    const char *entry_relative_path,
    const ErPackagedResourceArray *resources,
    uint64_t *out_size,
    ErError *error
) {
    uint64_t size = sizeof(ErPackageHeaderV2) + (uint64_t) strlen(entry_relative_path);
    size_t i;

    for (i = 0; i < resources->count; ++i) {
        uint64_t file_size = 0;

        if (!er_packager_get_file_size(resources->items[i].absolute_path, &file_size, error)) {
            return false;
        }

        size += sizeof(ErPackageFileHeaderV2);
        size += (uint64_t) strlen(resources->items[i].relative_path);
        size += file_size;
    }

    *out_size = size;
    return true;
}

static bool er_packager_write_file_payload(FILE *out, const char *path, ErError *error) {
    FILE *input;
    char buffer[4096];
    size_t read_size;

    input = fopen(path, "rb");
    if (!input) {
        er_error_set(error, 0, 0, "Could not open packaged file: %s", path);
        return false;
    }

    while ((read_size = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, read_size, out) != read_size) {
            fclose(input);
            er_error_set(error, 0, 0, "Could not append packaged file payload: %s", path);
            return false;
        }
    }

    fclose(input);
    return true;
}

static bool er_packager_collect_project_inputs(
    const char *source_path,
    ErCollectedFileArray *files,
    char **out_project_root,
    char **out_entry_path,
    ErError *error
) {
    ErFrontendUnit unit;
    char *project_root = NULL;
    char *entry_path = NULL;
    char source_dir[1024];
    char app_json_path[1024];
    char assets_dir[1024];
    char python_dir[1024];
    size_t i;

    *out_project_root = NULL;
    *out_entry_path = NULL;
    memset(&unit, 0, sizeof(unit));

    if (!er_frontend_load_file(source_path, &unit, error)) {
        return false;
    }

    entry_path = er_packager_dup(unit.graph.modules[unit.graph.entry_index].normalized_path);
    if (!entry_path) {
        er_frontend_unit_free(&unit);
        er_error_set(error, 0, 0, "Out of memory while recording entry path");
        return false;
    }

    if (!er_packager_add_collected_file(files, entry_path, ER_COLLECTED_FILE_ENTRY, error)) {
        er_frontend_unit_free(&unit);
        free(entry_path);
        return false;
    }

    for (i = 0; i < unit.graph.count; ++i) {
        if (i == unit.graph.entry_index) {
            continue;
        }
        if (!er_packager_add_collected_file(files, unit.graph.modules[i].normalized_path, ER_COLLECTED_FILE_ASSET, error)) {
            er_frontend_unit_free(&unit);
            free(entry_path);
            return false;
        }
    }

    er_path_dirname(entry_path, source_dir, sizeof(source_dir));

    for (i = 0; i < unit.program->statements.count; ++i) {
        if (!er_packager_collect_statement_resources(unit.program->statements.items[i], source_dir, files, error)) {
            er_frontend_unit_free(&unit);
            free(entry_path);
            return false;
        }
    }

    snprintf(app_json_path, sizeof(app_json_path), "%s\\app.json", source_dir);
    if (er_file_exists(app_json_path) &&
        !er_packager_add_collected_file(files, app_json_path, ER_COLLECTED_FILE_METADATA, error)) {
        er_frontend_unit_free(&unit);
        free(entry_path);
        return false;
    }

    snprintf(assets_dir, sizeof(assets_dir), "%s\\assets", source_dir);
#ifdef _WIN32
    if (er_packager_directory_exists(assets_dir) &&
        !er_packager_collect_directory_recursive(assets_dir, ER_COLLECTED_FILE_ASSET, NULL, files, error)) {
        er_frontend_unit_free(&unit);
        free(entry_path);
        return false;
    }
#endif

    snprintf(python_dir, sizeof(python_dir), "%s\\python", source_dir);
#ifdef _WIN32
    if (er_packager_directory_exists(python_dir) &&
        !er_packager_collect_python_directory_recursive(python_dir, files, error)) {
        er_frontend_unit_free(&unit);
        free(entry_path);
        return false;
    }
#endif

    if (!er_packager_compute_project_root(entry_path, files, &project_root, error)) {
        er_frontend_unit_free(&unit);
        free(entry_path);
        return false;
    }

    er_frontend_unit_free(&unit);
    *out_project_root = project_root;
    *out_entry_path = entry_path;
    return true;
}

bool er_packager_build(
    const char *source_path,
    const char *output_path,
    const ErPackagerBuildOptions *options,
    ErError *error
) {
    char runner_path[1024];
    char output_dir[1024];
    char app_name[256];
    char build_root[1024];
    char build_icon_path[1024];
    char staged_runtime_icon_path[1024];
    char runtime_icon_relative[1024];
    char generated_metadata_path[1024];
    char entry_dir_relative[1024];
    char packaged_metadata_relative[1024];
    char packaged_runtime_icon_relative[1024];
    char *project_root = NULL;
    char *entry_path = NULL;
    char *entry_relative = NULL;
    FILE *out = NULL;
    ErCollectedFileArray collected_files;
    ErPackagedResourceArray packaged_resources;
    ErPackageHeaderV2 header;
    ErPackageFooterV2 footer;
    uint64_t payload_size = 0;
    size_t i;
    bool ok = false;
    bool has_build_icon = false;
    bool has_generated_metadata = false;

    memset(&collected_files, 0, sizeof(collected_files));
    memset(&packaged_resources, 0, sizeof(packaged_resources));
    build_icon_path[0] = '\0';
    staged_runtime_icon_path[0] = '\0';
    runtime_icon_relative[0] = '\0';
    generated_metadata_path[0] = '\0';
    entry_dir_relative[0] = '\0';
    packaged_metadata_relative[0] = '\0';
    packaged_runtime_icon_relative[0] = '\0';

    if (!er_packager_find_runner(runner_path, sizeof(runner_path), error)) {
        goto cleanup;
    }

    if (!er_packager_collect_project_inputs(source_path, &collected_files, &project_root, &entry_path, error)) {
        goto cleanup;
    }

    er_path_basename_without_extension(source_path, app_name, sizeof(app_name));
    er_path_dirname(output_path, output_dir, sizeof(output_dir));
    snprintf(build_root, sizeof(build_root), "%s\\_erire_build\\%s", output_dir, app_name);
    if (!er_directory_create_recursive(build_root, error)) {
        goto cleanup;
    }

    if (!er_packager_resolve_build_icon(
            entry_path,
            options,
            build_root,
            build_icon_path,
            sizeof(build_icon_path),
            staged_runtime_icon_path,
            sizeof(staged_runtime_icon_path),
            runtime_icon_relative,
            sizeof(runtime_icon_relative),
            &has_build_icon,
            error)) {
        goto cleanup;
    }

    if (!er_packager_finalize_resources(entry_path, project_root, &collected_files, build_root, &packaged_resources, &entry_relative, error)) {
        goto cleanup;
    }

    er_path_dirname(entry_relative, entry_dir_relative, sizeof(entry_dir_relative));

    if (runtime_icon_relative[0] != '\0') {
        er_packager_join_relative_path(
            entry_dir_relative,
            runtime_icon_relative,
            packaged_runtime_icon_relative,
            sizeof(packaged_runtime_icon_relative)
        );
        if (!er_packager_upsert_packaged_resource(
                &packaged_resources,
                staged_runtime_icon_path,
                packaged_runtime_icon_relative,
                ER_PACKAGE_FILE_ASSET,
                error)) {
            goto cleanup;
        }
    }

    if (!er_packager_prepare_override_metadata(
            entry_path,
            build_root,
            options,
            runtime_icon_relative[0] != '\0' ? runtime_icon_relative : NULL,
            generated_metadata_path,
            sizeof(generated_metadata_path),
            &has_generated_metadata,
            error)) {
        goto cleanup;
    }

    if (has_generated_metadata) {
        er_packager_join_relative_path(entry_dir_relative, "app.json", packaged_metadata_relative, sizeof(packaged_metadata_relative));
        if (!er_packager_upsert_packaged_resource(
                &packaged_resources,
                generated_metadata_path,
                packaged_metadata_relative,
                ER_PACKAGE_FILE_METADATA,
                error)) {
            goto cleanup;
        }
    }

    if (!er_packager_build_payload_size(entry_relative, &packaged_resources, &payload_size, error)) {
        goto cleanup;
    }

    if (!er_file_copy(runner_path, output_path, error)) {
        goto cleanup;
    }

    if (has_build_icon && !er_packager_apply_exe_icon(output_path, build_icon_path, error)) {
        goto cleanup;
    }

    out = fopen(output_path, "ab");
    if (!out) {
        er_error_set(error, 0, 0, "Could not open package output: %s", output_path);
        goto cleanup;
    }

    header.file_count = (uint32_t) packaged_resources.count;
    header.entry_path_length = (uint32_t) strlen(entry_relative);

    if (fwrite(&header, 1, sizeof(header), out) != sizeof(header) ||
        fwrite(entry_relative, 1, header.entry_path_length, out) != header.entry_path_length) {
        er_error_set(error, 0, 0, "Could not write package header");
        goto cleanup;
    }

    for (i = 0; i < packaged_resources.count; ++i) {
        ErPackageFileHeaderV2 file_header;

        if (!er_packager_get_file_size(packaged_resources.items[i].absolute_path, &file_header.size, error)) {
            goto cleanup;
        }
        file_header.kind = packaged_resources.items[i].kind;
        file_header.path_length = (uint32_t) strlen(packaged_resources.items[i].relative_path);

        if (fwrite(&file_header, 1, sizeof(file_header), out) != sizeof(file_header) ||
            fwrite(packaged_resources.items[i].relative_path, 1, file_header.path_length, out) != file_header.path_length) {
            er_error_set(error, 0, 0, "Could not write package resource header");
            goto cleanup;
        }

        if (!er_packager_write_file_payload(out, packaged_resources.items[i].absolute_path, error)) {
            goto cleanup;
        }
    }

    memcpy(footer.magic, ER_PACKAGE_MAGIC_V2, sizeof(footer.magic));
    footer.payload_size = payload_size;
    if (fwrite(&footer, 1, sizeof(footer), out) != sizeof(footer)) {
        er_error_set(error, 0, 0, "Could not append package footer");
        goto cleanup;
    }

    ok = true;

cleanup:
    if (out) {
        fclose(out);
    }
    free(project_root);
    free(entry_path);
    free(entry_relative);
    er_packager_free_collected_files(&collected_files);
    er_packager_free_packaged_resources(&packaged_resources);
    return ok;
}

static bool er_packager_read_footer_v2(FILE *file, long file_size, ErPackageFooterV2 *footer, ErError *error) {
    if (file_size < (long) sizeof(ErPackageFooterV2)) {
        return false;
    }

    if (fseek(file, file_size - (long) sizeof(ErPackageFooterV2), SEEK_SET) != 0) {
        er_error_set(error, 0, 0, "Could not read package footer");
        return false;
    }
    if (fread(footer, 1, sizeof(*footer), file) != sizeof(*footer)) {
        er_error_set(error, 0, 0, "Could not load package footer");
        return false;
    }
    if (memcmp(footer->magic, ER_PACKAGE_MAGIC_V2, sizeof(footer->magic)) != 0) {
        return false;
    }
    return true;
}

static bool er_packager_is_safe_relative_path(const char *path) {
    const char *segment = path;

    if (!path || path[0] == '\0' || er_packager_is_absolute_path(path) || strchr(path, ':')) {
        return false;
    }

    while (*path) {
        if (er_packager_is_separator(*path)) {
            size_t segment_length = (size_t) (path - segment);
            if (segment_length == 2 && segment[0] == '.' && segment[1] == '.') {
                return false;
            }
            segment = path + 1;
        }
        ++path;
    }

    return !((path - segment) == 2 && segment[0] == '.' && segment[1] == '.');
}

static bool er_packager_extract_file_bytes(
    FILE *file,
    const char *dest_path,
    uint64_t size,
    ErError *error
) {
    FILE *out = fopen(dest_path, "wb");
    char buffer[4096];
    uint64_t remaining = size;

    if (!out) {
        er_error_set(error, 0, 0, "Could not create extracted file: %s", dest_path);
        return false;
    }

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : (size_t) remaining;
        if (fread(buffer, 1, chunk, file) != chunk) {
            fclose(out);
            er_error_set(error, 0, 0, "Could not read embedded resource payload");
            return false;
        }
        if (fwrite(buffer, 1, chunk, out) != chunk) {
            fclose(out);
            er_error_set(error, 0, 0, "Could not write extracted resource file");
            return false;
        }
        remaining -= chunk;
    }

    fclose(out);
    return true;
}

static bool er_packager_make_extract_root(
    const char *exe_path,
    char *out_path,
    size_t out_capacity,
    ErError *error
) {
#ifdef _WIN32
    char temp_base[MAX_PATH];
    char app_name[256];
    DWORD temp_length;

    temp_length = GetTempPathA((DWORD) sizeof(temp_base), temp_base);
    if (temp_length == 0 || temp_length >= sizeof(temp_base)) {
        er_error_set(error, 0, 0, "Could not determine temporary directory");
        return false;
    }

    er_path_basename_without_extension(exe_path, app_name, sizeof(app_name));
    snprintf(
        out_path,
        out_capacity,
        "%sErireExtract\\%s_%lu",
        temp_base,
        app_name,
        (unsigned long) GetCurrentProcessId()
    );
    return er_directory_create_recursive(out_path, error);
#else
    (void) exe_path;
    (void) out_path;
    (void) out_capacity;
    er_error_set(error, 0, 0, "Embedded app extraction is currently implemented on Windows only");
    return false;
#endif
}

bool er_packager_extract_embedded_app(const char *exe_path, ErPackagedApp *out_app, ErError *error) {
    FILE *file = NULL;
    long file_size;
    long payload_start;
    ErPackageFooterV2 footer;
    ErPackageHeaderV2 header;
    char extract_root[1024];
    char *entry_relative = NULL;
    size_t i;

    memset(out_app, 0, sizeof(*out_app));

    file = fopen(exe_path, "rb");
    if (!file) {
        er_error_set(error, 0, 0, "Could not open executable: %s", exe_path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not seek executable");
        return false;
    }

    file_size = ftell(file);
    if (file_size < 0 || !er_packager_read_footer_v2(file, file_size, &footer, error)) {
        fclose(file);
        return false;
    }

    if ((uint64_t) file_size < footer.payload_size + sizeof(footer)) {
        fclose(file);
        er_error_set(error, 0, 0, "Invalid packaged application payload size");
        return false;
    }

    payload_start = file_size - (long) sizeof(footer) - (long) footer.payload_size;
    if (fseek(file, payload_start, SEEK_SET) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not seek packaged application payload");
        return false;
    }

    if (fread(&header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not read packaged application header");
        return false;
    }

    entry_relative = (char *) malloc((size_t) header.entry_path_length + 1);
    if (!entry_relative) {
        fclose(file);
        er_error_set(error, 0, 0, "Out of memory while reading packaged entry path");
        return false;
    }

    if (header.entry_path_length > 0 &&
        fread(entry_relative, 1, header.entry_path_length, file) != header.entry_path_length) {
        fclose(file);
        free(entry_relative);
        er_error_set(error, 0, 0, "Could not read packaged entry path");
        return false;
    }
    entry_relative[header.entry_path_length] = '\0';

    if (!er_packager_is_safe_relative_path(entry_relative)) {
        fclose(file);
        free(entry_relative);
        er_error_set(error, 0, 0, "Packaged entry path is invalid");
        return false;
    }

    if (!er_packager_make_extract_root(exe_path, extract_root, sizeof(extract_root), error)) {
        fclose(file);
        free(entry_relative);
        return false;
    }

    for (i = 0; i < header.file_count; ++i) {
        ErPackageFileHeaderV2 file_header;
        char *relative_path;
        char full_path[1024];
        char full_dir[1024];

        if (fread(&file_header, 1, sizeof(file_header), file) != sizeof(file_header)) {
            fclose(file);
            free(entry_relative);
            er_error_set(error, 0, 0, "Could not read packaged resource header");
            return false;
        }

        relative_path = (char *) malloc((size_t) file_header.path_length + 1);
        if (!relative_path) {
            fclose(file);
            free(entry_relative);
            er_error_set(error, 0, 0, "Out of memory while reading packaged resource path");
            return false;
        }

        if (file_header.path_length > 0 &&
            fread(relative_path, 1, file_header.path_length, file) != file_header.path_length) {
            fclose(file);
            free(entry_relative);
            free(relative_path);
            er_error_set(error, 0, 0, "Could not read packaged resource path");
            return false;
        }
        relative_path[file_header.path_length] = '\0';

        if (!er_packager_is_safe_relative_path(relative_path)) {
            fclose(file);
            free(entry_relative);
            free(relative_path);
            er_error_set(error, 0, 0, "Packaged resource path is invalid");
            return false;
        }

        er_path_join(extract_root, relative_path, full_path, sizeof(full_path));
        er_path_dirname(full_path, full_dir, sizeof(full_dir));
        if (!er_directory_create_recursive(full_dir, error) ||
            !er_packager_extract_file_bytes(file, full_path, file_header.size, error)) {
            fclose(file);
            free(entry_relative);
            free(relative_path);
            return false;
        }

        free(relative_path);
    }

    fclose(file);

    out_app->extract_root = er_packager_dup(extract_root);
    if (!out_app->extract_root) {
        free(entry_relative);
        er_error_set(error, 0, 0, "Out of memory while recording extraction root");
        return false;
    }

    {
        char entry_path[1024];
        er_path_join(extract_root, entry_relative, entry_path, sizeof(entry_path));
        out_app->entry_path = er_packager_dup(entry_path);
    }
    free(entry_relative);

    if (!out_app->entry_path) {
        er_packaged_app_free(out_app);
        er_error_set(error, 0, 0, "Out of memory while recording extracted entry path");
        return false;
    }

    return true;
}

void er_packaged_app_free(ErPackagedApp *app) {
    if (!app) {
        return;
    }

    free(app->extract_root);
    free(app->entry_path);
    memset(app, 0, sizeof(*app));
}

bool er_packager_extract_embedded_source(
    const char *exe_path,
    char **out_source,
    size_t *out_size,
    ErError *error
) {
    FILE *file;
    long file_size;
    ErPackageFooterV1 footer;
    char *buffer;

    *out_source = NULL;
    if (out_size) {
        *out_size = 0;
    }

    file = fopen(exe_path, "rb");
    if (!file) {
        er_error_set(error, 0, 0, "Could not open executable: %s", exe_path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not seek executable");
        return false;
    }

    file_size = ftell(file);
    if (file_size < (long) sizeof(ErPackageFooterV1)) {
        fclose(file);
        return false;
    }

    if (fseek(file, file_size - (long) sizeof(ErPackageFooterV1), SEEK_SET) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not read package footer");
        return false;
    }

    if (fread(&footer, 1, sizeof(footer), file) != sizeof(footer)) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not load package footer");
        return false;
    }

    if (memcmp(footer.magic, ER_PACKAGE_MAGIC_V1, sizeof(footer.magic)) != 0) {
        fclose(file);
        return false;
    }

    if ((uint64_t) file_size < footer.source_size + sizeof(ErPackageFooterV1)) {
        fclose(file);
        er_error_set(error, 0, 0, "Invalid package payload size");
        return false;
    }

    if (fseek(file, file_size - (long) sizeof(ErPackageFooterV1) - (long) footer.source_size, SEEK_SET) != 0) {
        fclose(file);
        er_error_set(error, 0, 0, "Could not read package payload");
        return false;
    }

    buffer = (char *) malloc((size_t) footer.source_size + 1);
    if (!buffer) {
        fclose(file);
        er_error_set(error, 0, 0, "Out of memory while loading embedded package");
        return false;
    }

    if (footer.source_size > 0 && fread(buffer, 1, (size_t) footer.source_size, file) != (size_t) footer.source_size) {
        fclose(file);
        free(buffer);
        er_error_set(error, 0, 0, "Could not load embedded package contents");
        return false;
    }

    fclose(file);
    buffer[footer.source_size] = '\0';
    *out_source = buffer;
    if (out_size) {
        *out_size = (size_t) footer.source_size;
    }
    return true;
}
