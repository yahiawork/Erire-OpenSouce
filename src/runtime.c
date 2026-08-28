#include "runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "ast.h"
#include "compiler.h"
#include "console.h"
#include "fileio.h"
#include "frontend.h"
#include "media.h"
#include "ui.h"

#ifdef _WIN32
#include <commdlg.h>
#include <direct.h>
#include <windows.h>
#define ER_RT_POPEN _popen
#define ER_RT_PCLOSE _pclose
#define ER_RT_GETCWD _getcwd
#else
#include <unistd.h>
#define ER_RT_POPEN popen
#define ER_RT_PCLOSE pclose
#define ER_RT_GETCWD getcwd
#endif

typedef struct ErRuntimeVar {
    char *name;
    ErValue value;
} ErRuntimeVar;

typedef struct ErRuntimePythonModule {
    char *alias;
    char *script_path;
    bool is_executable;
} ErRuntimePythonModule;

typedef struct ErRuntimePyResult {
    char *json;
    ErValue value;
    bool structured;
} ErRuntimePyResult;

typedef struct ErRuntimeLiveNode {
    char *id;
    bool seen_in_patch;
} ErRuntimeLiveNode;

typedef struct ErRuntimeLiveTimer {
    char *name;
    bool seen_in_patch;
} ErRuntimeLiveTimer;

typedef struct ErRuntime ErRuntime;

typedef struct ErEventBinding {
    ErRuntime *runtime;
    ErStatementArray *body;
    char *bound_variable;
} ErEventBinding;

typedef struct ErRuntimeTimer {
    char *name;
    unsigned int timer_id;
    ErEventBinding *binding;
} ErRuntimeTimer;

typedef struct ErRuntimeLivePatchState {
    bool enabled;
    unsigned int poll_ms;
    char *entry_path;
    char *backend_path;
    char *app_json_path;
    unsigned long long entry_stamp;
    unsigned long long backend_stamp;
    unsigned long long app_json_stamp;
    unsigned int patch_version;
    char *status_text;
    char *last_error_text;
    ErRuntimeLiveNode *nodes;
    size_t node_count;
    size_t node_capacity;
    ErRuntimeLiveTimer *timers;
    size_t timer_count;
    size_t timer_capacity;
    ErFrontendUnit *units;
    size_t unit_count;
    size_t unit_capacity;
} ErRuntimeLivePatchState;

struct ErRuntime {
    ErProgram *program;
    ErUiApp app;
    bool app_initialized;
    char *title;
    char *icon_path;
    bool title_locked;
    bool icon_locked;
    int x;
    int y;
    int w;
    int h;
    bool window_resizable;
    unsigned int background_rgb;
    char *current_page;
    char *source_name;
    char *source_dir;
    ErRuntimeVar *variables;
    size_t variable_count;
    size_t variable_capacity;
    ErRuntimePythonModule *python_modules;
    size_t python_module_count;
    size_t python_module_capacity;
    ErEventBinding **bindings;
    size_t binding_count;
    size_t binding_capacity;
    ErRuntimeTimer *timers;
    size_t timer_count;
    size_t timer_capacity;
    ErStatementArray **pending_on_loads;
    size_t pending_on_load_count;
    size_t pending_on_load_capacity;
    bool defer_on_load;
    unsigned int next_runtime_timer_id;
    ErMediaPlayer media;
    ErRuntimeLivePatchState live;
};

#define ER_RUNTIME_MEDIA_TIMER_ID 1u
#define ER_RUNTIME_LIVE_PATCH_TIMER_ID 2u

static bool er_runtime_text_means_true(const char *text);
static void er_runtime_on_media_sync_timer(ErUiApp *app, unsigned int timer_id, void *user_data);
static void er_runtime_on_live_patch_timer(ErUiApp *app, unsigned int timer_id, void *user_data);
static bool er_runtime_buffer_append_text(char **buffer, size_t *length, size_t *capacity, const char *text, ErError *error);
static bool er_runtime_set_variable(ErRuntime *runtime, const char *name, const ErValue *value, ErError *error);

static char *er_rt_dup(const char *text) {
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

static char *er_rt_dup_range(const char *start, size_t length) {
    char *copy = (char *) malloc(length + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static void er_runtime_live_set_status_text(ErRuntime *runtime, const char *text) {
    char *copy;

    if (!runtime) {
        return;
    }

    copy = er_rt_dup(text ? text : "");
    free(runtime->live.status_text);
    runtime->live.status_text = copy;
}

static void er_runtime_live_set_last_error_text(ErRuntime *runtime, const char *text) {
    char *copy;

    if (!runtime) {
        return;
    }

    copy = er_rt_dup(text ? text : "");
    free(runtime->live.last_error_text);
    runtime->live.last_error_text = copy;
}

static void er_runtime_live_set_statusf(ErRuntime *runtime, const char *format, ...) {
    va_list args;
    va_list args_copy;
    int needed;
    char stack_buffer[512];
    char *heap_buffer = NULL;
    char *final_text = NULL;

    if (!runtime || !format) {
        return;
    }

    va_start(args, format);
    va_copy(args_copy, args);
    needed = vsnprintf(stack_buffer, sizeof(stack_buffer), format, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        er_runtime_live_set_status_text(runtime, "");
        return;
    }

    if ((size_t) needed < sizeof(stack_buffer)) {
        er_runtime_live_set_status_text(runtime, stack_buffer);
        va_end(args_copy);
        return;
    }

    heap_buffer = (char *) malloc((size_t) needed + 1);
    if (!heap_buffer) {
        va_end(args_copy);
        er_runtime_live_set_status_text(runtime, "live patch status: out of memory");
        return;
    }

    vsnprintf(heap_buffer, (size_t) needed + 1, format, args_copy);
    va_end(args_copy);
    final_text = heap_buffer;
    er_runtime_live_set_status_text(runtime, final_text);
    free(heap_buffer);
}

static void er_runtime_live_publish_state(ErRuntime *runtime) {
    ErError ignored;
    ErValue value;

    if (!runtime) {
        return;
    }

    er_error_clear(&ignored);

    value = er_value_make_bool(runtime->live.enabled);
    if (value.type == ER_VALUE_BOOL) {
        er_runtime_set_variable(runtime, "live.enabled", &value, &ignored);
    }
    er_value_free(&value);

    value = er_value_make_number((double) runtime->live.patch_version);
    er_runtime_set_variable(runtime, "live.version", &value, &ignored);
    er_value_free(&value);

    value = er_value_make_string(runtime->live.status_text ? runtime->live.status_text : "");
    er_runtime_set_variable(runtime, "live.status", &value, &ignored);
    er_value_free(&value);

    value = er_value_make_string(runtime->live.last_error_text ? runtime->live.last_error_text : "");
    er_runtime_set_variable(runtime, "live.error", &value, &ignored);
    er_value_free(&value);
}

static char *er_runtime_derive_source_dir(const char *source_name) {
    char buffer[1024];

    if (!source_name || source_name[0] == '\0') {
        return er_rt_dup(".");
    }

    er_path_dirname(source_name, buffer, sizeof(buffer));
    return er_rt_dup(buffer);
}

static bool er_runtime_json_extract_string(
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

static bool er_runtime_json_extract_bool(
    const char *json_text,
    const char *key,
    bool *out_value
) {
    char pattern[128];
    const char *match;
    const char *cursor;

    if (!json_text || !key || !out_value) {
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

    if (strncmp(cursor, "true", 4) == 0) {
        *out_value = true;
        return true;
    }
    if (strncmp(cursor, "false", 5) == 0) {
        *out_value = false;
        return true;
    }

    return false;
}

static bool er_runtime_apply_app_json_config(ErRuntime *runtime, ErError *error) {
    char app_json_path[1024];
    char icon_value[1024];
    char win_icon_value[1024];
    char name_value[1024];
    char win_title_value[1024];
    char resolved_path[1024];
    char *json_text = NULL;
    size_t json_size = 0;
    bool resizable_value = true;

    if (!runtime || !runtime->source_dir || runtime->source_dir[0] == '\0') {
        return true;
    }

    snprintf(app_json_path, sizeof(app_json_path), "%s\\app.json", runtime->source_dir);
    if (!er_file_exists(app_json_path)) {
        return true;
    }

    if (!er_file_read_all(app_json_path, &json_text, &json_size, error)) {
        return false;
    }
    (void) json_size;

    if (er_runtime_json_extract_string(json_text, "win_title", win_title_value, sizeof(win_title_value)) &&
        win_title_value[0] != '\0') {
        free(runtime->title);
        runtime->title = er_rt_dup(win_title_value);
        runtime->title_locked = runtime->title != NULL;
        if (!runtime->title) {
            free(json_text);
            er_error_set(error, 0, 0, "Out of memory while applying app.json win_title");
            return false;
        }
        if (runtime->app_initialized) {
            er_ui_app_set_title(&runtime->app, runtime->title);
        }
    } else if (er_runtime_json_extract_string(json_text, "name", name_value, sizeof(name_value)) &&
               name_value[0] != '\0' &&
               runtime->title &&
               strcmp(runtime->title, "Erire App") == 0) {
        free(runtime->title);
        runtime->title = er_rt_dup(name_value);
        if (!runtime->title) {
            free(json_text);
            er_error_set(error, 0, 0, "Out of memory while applying app.json name");
            return false;
        }
        if (runtime->app_initialized) {
            er_ui_app_set_title(&runtime->app, runtime->title);
        }
    }

    if (er_runtime_json_extract_bool(json_text, "resizable", &resizable_value)) {
        runtime->window_resizable = resizable_value;
    }

    if (er_runtime_json_extract_string(json_text, "win_icon", win_icon_value, sizeof(win_icon_value)) &&
        win_icon_value[0] != '\0') {
        if (er_path_has_extension(win_icon_value, ".ico") || er_path_has_extension(win_icon_value, ".png")) {
            if (win_icon_value[0] != '\0' && !strchr(win_icon_value, ':') &&
                !(win_icon_value[0] == '\\' && win_icon_value[1] == '\\')) {
                er_path_join(runtime->source_dir, win_icon_value, resolved_path, sizeof(resolved_path));
            } else {
                snprintf(resolved_path, sizeof(resolved_path), "%s", win_icon_value);
            }
            free(runtime->icon_path);
            runtime->icon_path = er_rt_dup(resolved_path);
            runtime->icon_locked = runtime->icon_path != NULL;
            if (!runtime->icon_path) {
                free(json_text);
                er_error_set(error, 0, 0, "Out of memory while applying app.json win_icon");
                return false;
            }
            if (runtime->app_initialized &&
                !er_ui_app_set_icon(&runtime->app, runtime->icon_path, error)) {
                free(json_text);
                return false;
            }
        }
    } else if (er_runtime_json_extract_string(json_text, "icon", icon_value, sizeof(icon_value)) &&
               icon_value[0] != '\0') {
        if (icon_value[0] != '\0' && !strchr(icon_value, ':') &&
            !(icon_value[0] == '\\' && icon_value[1] == '\\')) {
            er_path_join(runtime->source_dir, icon_value, resolved_path, sizeof(resolved_path));
        } else {
            snprintf(resolved_path, sizeof(resolved_path), "%s", icon_value);
        }
        free(runtime->icon_path);
        runtime->icon_path = er_rt_dup(resolved_path);
        if (!runtime->icon_path) {
            free(json_text);
            er_error_set(error, 0, 0, "Out of memory while applying app.json icon");
            return false;
        }
        if (runtime->app_initialized &&
            !er_ui_app_set_icon(&runtime->app, runtime->icon_path, error)) {
            free(json_text);
            return false;
        }
    }

    free(json_text);
    free(runtime->media.default_art_path);
    runtime->media.default_art_path = er_rt_dup(runtime->icon_path ? runtime->icon_path : "");
    return true;
}

static void er_runtime_init(ErRuntime *runtime, ErProgram *program, const char *source_name, const char *source_dir) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->program = program;
    runtime->title = er_rt_dup("Erire App");
    runtime->icon_path = er_rt_dup("assets\\brand\\erire-logo.png");
    runtime->x = 100;
    runtime->y = 100;
    runtime->w = 900;
    runtime->h = 600;
    runtime->window_resizable = true;
    runtime->background_rgb = 0x0f172a;
    runtime->current_page = er_rt_dup("main");
    runtime->source_name = er_rt_dup(source_name ? source_name : "<memory>");
    runtime->source_dir = er_rt_dup(source_dir ? source_dir : ".");
    runtime->defer_on_load = true;
    runtime->next_runtime_timer_id = 100u;
    er_media_player_init(&runtime->media, runtime->source_dir, runtime->icon_path);
}

static void er_runtime_destroy_binding(ErRuntime *runtime, ErEventBinding *binding) {
    if (!binding) {
        return;
    }

    (void) runtime;

    if (binding->runtime == NULL &&
        binding->body == NULL &&
        binding->bound_variable == NULL) {
        return;
    }

    free(binding->bound_variable);
    binding->bound_variable = NULL;
    binding->runtime = NULL;
    binding->body = NULL;
}

static bool er_runtime_live_grow_nodes(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErRuntimeLiveNode *new_items;

    if (runtime->live.node_count < runtime->live.node_capacity) {
        return true;
    }

    new_capacity = runtime->live.node_capacity == 0 ? 8 : runtime->live.node_capacity * 2;
    new_items = (ErRuntimeLiveNode *) realloc(runtime->live.nodes, new_capacity * sizeof(ErRuntimeLiveNode));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing live node table");
        return false;
    }

    runtime->live.nodes = new_items;
    runtime->live.node_capacity = new_capacity;
    return true;
}

static bool er_runtime_live_grow_timers(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErRuntimeLiveTimer *new_items;

    if (runtime->live.timer_count < runtime->live.timer_capacity) {
        return true;
    }

    new_capacity = runtime->live.timer_capacity == 0 ? 8 : runtime->live.timer_capacity * 2;
    new_items = (ErRuntimeLiveTimer *) realloc(runtime->live.timers, new_capacity * sizeof(ErRuntimeLiveTimer));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing live timer table");
        return false;
    }

    runtime->live.timers = new_items;
    runtime->live.timer_capacity = new_capacity;
    return true;
}

static bool er_runtime_live_grow_units(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErFrontendUnit *new_items;

    if (runtime->live.unit_count < runtime->live.unit_capacity) {
        return true;
    }

    new_capacity = runtime->live.unit_capacity == 0 ? 4 : runtime->live.unit_capacity * 2;
    new_items = (ErFrontendUnit *) realloc(runtime->live.units, new_capacity * sizeof(ErFrontendUnit));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing live unit table");
        return false;
    }

    runtime->live.units = new_items;
    runtime->live.unit_capacity = new_capacity;
    return true;
}

static int er_runtime_live_find_node_index(ErRuntime *runtime, const char *id) {
    size_t i;

    if (!runtime || !id) {
        return -1;
    }

    for (i = 0; i < runtime->live.node_count; ++i) {
        if (runtime->live.nodes[i].id && strcmp(runtime->live.nodes[i].id, id) == 0) {
            return (int) i;
        }
    }

    return -1;
}

static int er_runtime_live_find_timer_index(ErRuntime *runtime, const char *name) {
    size_t i;

    if (!runtime || !name) {
        return -1;
    }

    for (i = 0; i < runtime->live.timer_count; ++i) {
        if (runtime->live.timers[i].name && strcmp(runtime->live.timers[i].name, name) == 0) {
            return (int) i;
        }
    }

    return -1;
}

static bool er_runtime_live_touch_node(ErRuntime *runtime, const char *id, ErError *error) {
    int index;

    if (!runtime || !id || id[0] == '\0') {
        return false;
    }

    index = er_runtime_live_find_node_index(runtime, id);
    if (index >= 0) {
        runtime->live.nodes[index].seen_in_patch = true;
        return true;
    }

    if (!er_runtime_live_grow_nodes(runtime, error)) {
        return false;
    }

    runtime->live.nodes[runtime->live.node_count].id = er_rt_dup(id);
    if (!runtime->live.nodes[runtime->live.node_count].id) {
        er_error_set(error, 0, 0, "Out of memory while storing live node id");
        return false;
    }
    runtime->live.nodes[runtime->live.node_count].seen_in_patch = true;
    runtime->live.node_count++;
    return true;
}

static bool er_runtime_live_touch_timer(ErRuntime *runtime, const char *name, ErError *error) {
    int index;

    if (!runtime || !name || name[0] == '\0') {
        return false;
    }

    index = er_runtime_live_find_timer_index(runtime, name);
    if (index >= 0) {
        runtime->live.timers[index].seen_in_patch = true;
        return true;
    }

    if (!er_runtime_live_grow_timers(runtime, error)) {
        return false;
    }

    runtime->live.timers[runtime->live.timer_count].name = er_rt_dup(name);
    if (!runtime->live.timers[runtime->live.timer_count].name) {
        er_error_set(error, 0, 0, "Out of memory while storing live timer name");
        return false;
    }
    runtime->live.timers[runtime->live.timer_count].seen_in_patch = true;
    runtime->live.timer_count++;
    return true;
}

static void er_runtime_live_begin_patch(ErRuntime *runtime) {
    size_t i;

    if (!runtime) {
        return;
    }

    for (i = 0; i < runtime->live.node_count; ++i) {
        runtime->live.nodes[i].seen_in_patch = false;
    }
    for (i = 0; i < runtime->live.timer_count; ++i) {
        runtime->live.timers[i].seen_in_patch = false;
    }
}

static bool er_runtime_live_keep_unit(ErRuntime *runtime, ErFrontendUnit *unit, ErError *error) {
    if (!runtime || !unit) {
        er_error_set(error, 0, 0, "Live runtime cannot retain an empty frontend unit");
        return false;
    }

    if (!er_runtime_live_grow_units(runtime, error)) {
        return false;
    }

    runtime->live.units[runtime->live.unit_count++] = *unit;
    memset(unit, 0, sizeof(*unit));
    return true;
}

static void er_runtime_live_refresh_file_stamps(ErRuntime *runtime) {
    if (!runtime) {
        return;
    }

    runtime->live.entry_stamp = er_file_last_write_time(runtime->live.entry_path);
    runtime->live.backend_stamp = er_file_last_write_time(runtime->live.backend_path);
    runtime->live.app_json_stamp = er_file_last_write_time(runtime->live.app_json_path);
}

static void er_runtime_live_free(ErRuntime *runtime) {
    size_t i;

    if (!runtime) {
        return;
    }

    free(runtime->live.entry_path);
    free(runtime->live.backend_path);
    free(runtime->live.app_json_path);
    free(runtime->live.status_text);
    free(runtime->live.last_error_text);

    for (i = 0; i < runtime->live.node_count; ++i) {
        free(runtime->live.nodes[i].id);
    }
    free(runtime->live.nodes);

    for (i = 0; i < runtime->live.timer_count; ++i) {
        free(runtime->live.timers[i].name);
    }
    free(runtime->live.timers);

    for (i = 0; i < runtime->live.unit_count; ++i) {
        er_frontend_unit_free(&runtime->live.units[i]);
    }
    free(runtime->live.units);

    memset(&runtime->live, 0, sizeof(runtime->live));
}

static void er_runtime_free(ErRuntime *runtime) {
    size_t i;

    if (runtime->app_initialized) {
        er_ui_app_destroy(&runtime->app);
    }

    free(runtime->title);
    free(runtime->icon_path);
    free(runtime->current_page);
    free(runtime->source_name);
    free(runtime->source_dir);

    for (i = 0; i < runtime->variable_count; ++i) {
        free(runtime->variables[i].name);
        er_value_free(&runtime->variables[i].value);
    }
    free(runtime->variables);

    for (i = 0; i < runtime->python_module_count; ++i) {
        free(runtime->python_modules[i].alias);
        free(runtime->python_modules[i].script_path);
    }
    free(runtime->python_modules);

    for (i = 0; i < runtime->binding_count; ++i) {
        free(runtime->bindings[i]->bound_variable);
        free(runtime->bindings[i]);
    }
    free(runtime->bindings);
    for (i = 0; i < runtime->timer_count; ++i) {
        free(runtime->timers[i].name);
    }
    free(runtime->timers);
    free(runtime->pending_on_loads);
    er_runtime_live_free(runtime);
    er_media_player_destroy(&runtime->media);
}

static bool er_runtime_grow_pending_on_loads(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErStatementArray **new_items;

    if (runtime->pending_on_load_count < runtime->pending_on_load_capacity) {
        return true;
    }

    new_capacity = runtime->pending_on_load_capacity == 0 ? 8 : runtime->pending_on_load_capacity * 2;
    new_items = (ErStatementArray **) realloc(runtime->pending_on_loads, new_capacity * sizeof(ErStatementArray *));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing pending onLoad queue");
        return false;
    }

    runtime->pending_on_loads = new_items;
    runtime->pending_on_load_capacity = new_capacity;
    return true;
}

static bool er_runtime_queue_on_load(ErRuntime *runtime, ErStatementArray *body, ErError *error) {
    if (!body) {
        return true;
    }

    if (!er_runtime_grow_pending_on_loads(runtime, error)) {
        return false;
    }

    runtime->pending_on_loads[runtime->pending_on_load_count++] = body;
    return true;
}

static bool er_runtime_ensure_app(ErRuntime *runtime, ErError *error) {
    if (runtime->app_initialized) {
        return true;
    }

    if (!er_ui_app_init(
            &runtime->app,
            runtime->title,
            runtime->x,
            runtime->y,
            runtime->w,
            runtime->h,
            runtime->window_resizable,
            error
        )) {
        return false;
    }

    er_ui_app_set_background(&runtime->app, runtime->background_rgb);
    if (runtime->icon_path && runtime->icon_path[0] != '\0') {
        ErError icon_error;
        er_error_clear(&icon_error);
        er_ui_app_set_icon(&runtime->app, runtime->icon_path, &icon_error);
        er_media_player_destroy(&runtime->media);
        er_media_player_init(&runtime->media, runtime->source_dir, runtime->icon_path);
    }
    er_ui_app_show_page(&runtime->app, runtime->current_page);
    runtime->app_initialized = true;
    if (!er_ui_app_set_timer(
            &runtime->app,
            ER_RUNTIME_MEDIA_TIMER_ID,
            250u,
            er_runtime_on_media_sync_timer,
            runtime,
            error
        )) {
        return false;
    }

    if (runtime->live.enabled &&
        !er_ui_app_set_timer(
            &runtime->app,
            ER_RUNTIME_LIVE_PATCH_TIMER_ID,
            runtime->live.poll_ms > 0 ? runtime->live.poll_ms : 300u,
            er_runtime_on_live_patch_timer,
            runtime,
            error
        )) {
        return false;
    }
// Hello world
    return true;
}

static ErRuntimeVar *er_runtime_find_variable(ErRuntime *runtime, const char *name) {
    size_t i;
    for (i = 0; i < runtime->variable_count; ++i) {
        if (strcmp(runtime->variables[i].name, name) == 0) {
            return &runtime->variables[i];
        }
    }
    return NULL;
}

static bool er_runtime_grow_variables(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErRuntimeVar *new_vars;

    if (runtime->variable_count < runtime->variable_capacity) {
        return true;
    }

    new_capacity = runtime->variable_capacity == 0 ? 8 : runtime->variable_capacity * 2;
    new_vars = (ErRuntimeVar *) realloc(runtime->variables, new_capacity * sizeof(ErRuntimeVar));
    if (!new_vars) {
        er_error_set(error, 0, 0, "Out of memory while growing variable table");
        return false;
    }

    runtime->variables = new_vars;
    runtime->variable_capacity = new_capacity;
    return true;
}

static bool er_runtime_set_variable(ErRuntime *runtime, const char *name, const ErValue *value, ErError *error) {
    ErRuntimeVar *var = er_runtime_find_variable(runtime, name);

    if (var) {
        er_value_free(&var->value);
        var->value = er_value_clone(value);
        return true;
    }

    if (!er_runtime_grow_variables(runtime, error)) {
        return false;
    }

    var = &runtime->variables[runtime->variable_count++];
    memset(var, 0, sizeof(*var));
    var->name = er_rt_dup(name);
    var->value = er_value_clone(value);
    return true;
}

static bool er_runtime_grow_python_modules(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErRuntimePythonModule *new_modules;

    if (runtime->python_module_count < runtime->python_module_capacity) {
        return true;
    }

    new_capacity = runtime->python_module_capacity == 0 ? 4 : runtime->python_module_capacity * 2;
    new_modules = (ErRuntimePythonModule *) realloc(
        runtime->python_modules,
        new_capacity * sizeof(ErRuntimePythonModule)
    );
    if (!new_modules) {
        er_error_set(error, 0, 0, "Out of memory while growing Python import table");
        return false;
    }

    runtime->python_modules = new_modules;
    runtime->python_module_capacity = new_capacity;
    return true;
}

static ErRuntimePythonModule *er_runtime_find_python_module(ErRuntime *runtime, const char *alias) {
    size_t i;

    for (i = 0; i < runtime->python_module_count; ++i) {
        if (strcmp(runtime->python_modules[i].alias, alias) == 0) {
            return &runtime->python_modules[i];
        }
    }

    return NULL;
}

static bool er_runtime_is_absolute_path(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

#ifdef _WIN32
    if ((isalpha((unsigned char) path[0]) && path[1] == ':') ||
        (path[0] == '\\' && path[1] == '\\')) {
        return true;
    }
#endif

    return path[0] == '/' || path[0] == '\\';
}

static bool er_runtime_normalize_path(const char *path, char **out_path, ErError *error);

static void er_runtime_replace_extension(
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

static bool er_runtime_resolve_python_module_path(
    const char *candidate_path,
    char **out_normalized_path,
    bool *out_is_executable,
    ErError *error
) {
    char alt_candidate[1024];
    char helper_directory[1024];
    char helper_executable[1024];
    char base_name[256];
    char helper_file_name[320];

    *out_normalized_path = NULL;
    *out_is_executable = false;

    if (er_file_exists(candidate_path)) {
        return er_runtime_normalize_path(candidate_path, out_normalized_path, error);
    }

    er_runtime_replace_extension(candidate_path, ".exe", alt_candidate, sizeof(alt_candidate));
    if (er_file_exists(alt_candidate)) {
        if (!er_runtime_normalize_path(alt_candidate, out_normalized_path, error)) {
            return false;
        }
        *out_is_executable = true;
        return true;
    }

    snprintf(alt_candidate, sizeof(alt_candidate), "%s.exe", candidate_path);
    if (er_file_exists(alt_candidate)) {
        if (!er_runtime_normalize_path(alt_candidate, out_normalized_path, error)) {
            return false;
        }
        *out_is_executable = true;
        return true;
    }

    er_runtime_replace_extension(candidate_path, "", helper_directory, sizeof(helper_directory));
    er_path_basename_without_extension(candidate_path, base_name, sizeof(base_name));
    snprintf(helper_file_name, sizeof(helper_file_name), "%s.exe", base_name);
    er_path_join(helper_directory, helper_file_name, helper_executable, sizeof(helper_executable));
    if (er_file_exists(helper_executable)) {
        if (!er_runtime_normalize_path(helper_executable, out_normalized_path, error)) {
            return false;
        }
        *out_is_executable = true;
        return true;
    }

    er_error_set(error, 0, 0, "Python bridge script was not found: %s", candidate_path);
    return false;
}

static bool er_runtime_normalize_path(const char *path, char **out_path, ErError *error) {
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

    *out_path = er_rt_dup(buffer);
    if (!*out_path) {
        er_error_set(error, 0, 0, "Out of memory while normalizing path");
        return false;
    }

    return true;
}

static bool er_runtime_register_python_import(
    ErRuntime *runtime,
    const char *alias,
    const char *path,
    ErError *error
) {
    ErRuntimePythonModule *module;
    char candidate[1024];
    char *normalized = NULL;
    bool is_executable = false;

    if (!alias || !path) {
        er_error_set(error, 0, 0, "Python imports require both an alias and a path");
        return false;
    }

    if (er_runtime_is_absolute_path(path)) {
        strncpy(candidate, path, sizeof(candidate) - 1);
        candidate[sizeof(candidate) - 1] = '\0';
    } else {
        er_path_join(runtime->source_dir ? runtime->source_dir : ".", path, candidate, sizeof(candidate));
    }

    if (!er_runtime_resolve_python_module_path(candidate, &normalized, &is_executable, error)) {
        return false;
    }

    module = er_runtime_find_python_module(runtime, alias);
    if (module) {
        free(module->script_path);
        module->script_path = normalized;
        module->is_executable = is_executable;
        return true;
    }

    if (!er_runtime_grow_python_modules(runtime, error)) {
        free(normalized);
        return false;
    }

    module = &runtime->python_modules[runtime->python_module_count++];
    memset(module, 0, sizeof(*module));
    module->alias = er_rt_dup(alias);
    module->script_path = normalized;
    module->is_executable = is_executable;
    if (!module->alias) {
        free(module->script_path);
        memset(module, 0, sizeof(*module));
        runtime->python_module_count--;
        er_error_set(error, 0, 0, "Out of memory while recording Python import alias");
        return false;
    }

    return true;
}

static void er_runtime_sanitize_storage_segment(const char *input, char *out, size_t out_capacity) {
    size_t i;
    size_t j = 0;
    bool last_was_separator = false;

    if (!out || out_capacity == 0) {
        return;
    }

    out[0] = '\0';
    if (!input || input[0] == '\0') {
        input = "app";
    }

    for (i = 0; input[i] != '\0' && j + 1 < out_capacity; ++i) {
        unsigned char ch = (unsigned char) input[i];
        if (isalnum(ch)) {
            out[j++] = (char) ch;
            last_was_separator = false;
        } else if (ch == '-' || ch == '_') {
            out[j++] = (char) ch;
            last_was_separator = false;
        } else if (!last_was_separator && j > 0) {
            out[j++] = '_';
            last_was_separator = true;
        }
    }

    while (j > 0 && out[j - 1] == '_') {
        --j;
    }
    if (j == 0 && out_capacity > 1) {
        out[j++] = 'a';
        out[j++] = 'p';
        if (j + 1 < out_capacity) {
            out[j++] = 'p';
        }
    }
    out[j] = '\0';
}

static bool er_runtime_storage_base_dir(ErRuntime *runtime, char *out_dir, size_t out_capacity, ErError *error) {
    char base[1024];
    char erire_dir[1024];
    char app_segment[256];
    const char *name_source;

    if (!runtime || !out_dir || out_capacity == 0) {
        er_error_set(error, 0, 0, "Storage path resolution requires an app runtime");
        return false;
    }

    base[0] = '\0';

#ifdef _WIN32
    if (GetEnvironmentVariableA("APPDATA", base, (DWORD) sizeof(base)) == 0 ||
        base[0] == '\0') {
        if (GetEnvironmentVariableA("LOCALAPPDATA", base, (DWORD) sizeof(base)) == 0 ||
            base[0] == '\0') {
            snprintf(base, sizeof(base), "%s", runtime->source_dir ? runtime->source_dir : ".");
        }
    }
#else
    {
        const char *home = getenv("HOME");
        snprintf(base, sizeof(base), "%s", (home && home[0] != '\0') ? home : (runtime->source_dir ? runtime->source_dir : "."));
    }
#endif

    name_source = (runtime->title && runtime->title[0] != '\0') ? runtime->title : runtime->source_name;
    er_runtime_sanitize_storage_segment(name_source, app_segment, sizeof(app_segment));

#ifdef _WIN32
    er_path_join(base, "Erire", erire_dir, sizeof(erire_dir));
#else
    er_path_join(base, ".erire", erire_dir, sizeof(erire_dir));
#endif
    er_path_join(erire_dir, app_segment, out_dir, out_capacity);
    return er_directory_create_recursive(out_dir, error);
}

static bool er_runtime_storage_key_path(
    ErRuntime *runtime,
    const char *key,
    char *out_path,
    size_t out_capacity,
    ErError *error
) {
    char dir[1024];
    char key_segment[256];
    char file_name[320];

    if (!key || key[0] == '\0') {
        er_error_set(error, 0, 0, "storage key cannot be empty");
        return false;
    }

    er_runtime_sanitize_storage_segment(key, key_segment, sizeof(key_segment));
    if (key_segment[0] == '\0') {
        er_error_set(error, 0, 0, "storage key must contain at least one safe character");
        return false;
    }

    if (!er_runtime_storage_base_dir(runtime, dir, sizeof(dir), error)) {
        return false;
    }

    snprintf(file_name, sizeof(file_name), "%s.txt", key_segment);
    er_path_join(dir, file_name, out_path, out_capacity);
    return true;
}

static bool er_runtime_storage_read_text(ErRuntime *runtime, const char *key, char **out_text, ErError *error) {
    char path[1024];
    size_t size = 0;

    *out_text = NULL;
    if (!er_runtime_storage_key_path(runtime, key, path, sizeof(path), error)) {
        return false;
    }

    if (!er_file_exists(path)) {
        *out_text = er_rt_dup("");
        if (!*out_text) {
            er_error_set(error, 0, 0, "Out of memory while reading storage key");
            return false;
        }
        return true;
    }

    return er_file_read_all(path, out_text, &size, error);
}

static bool er_runtime_storage_write_text(ErRuntime *runtime, const char *key, const char *text, ErError *error) {
    char path[1024];
    const char *safe_text = text ? text : "";

    if (!er_runtime_storage_key_path(runtime, key, path, sizeof(path), error)) {
        return false;
    }

    return er_file_write_all(path, safe_text, strlen(safe_text), error);
}

static bool er_runtime_buffer_reserve(char **buffer, size_t *capacity, size_t needed, ErError *error) {
    char *new_buffer;
    size_t new_capacity;

    if (needed <= *capacity) {
        return true;
    }

    new_capacity = *capacity == 0 ? 64 : *capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    new_buffer = (char *) realloc(*buffer, new_capacity);
    if (!new_buffer) {
        er_error_set(error, 0, 0, "Out of memory while growing runtime buffer");
        return false;
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
    return true;
}

static bool er_runtime_buffer_append_range(
    char **buffer,
    size_t *length,
    size_t *capacity,
    const char *text,
    size_t text_length,
    ErError *error
) {
    if (!er_runtime_buffer_reserve(buffer, capacity, *length + text_length + 1, error)) {
        return false;
    }

    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';
    return true;
}

static bool er_runtime_buffer_append_text(
    char **buffer,
    size_t *length,
    size_t *capacity,
    const char *text,
    ErError *error
) {
    return er_runtime_buffer_append_range(buffer, length, capacity, text, strlen(text), error);
}

static bool er_runtime_buffer_append_char(
    char **buffer,
    size_t *length,
    size_t *capacity,
    char ch,
    ErError *error
) {
    return er_runtime_buffer_append_range(buffer, length, capacity, &ch, 1, error);
}

static bool er_runtime_json_append_escaped(
    char **buffer,
    size_t *length,
    size_t *capacity,
    const char *text,
    ErError *error
) {
    const unsigned char *cursor = (const unsigned char *) (text ? text : "");

    if (!er_runtime_buffer_append_char(buffer, length, capacity, '"', error)) {
        return false;
    }

    while (*cursor) {
        switch (*cursor) {
            case '\\':
                if (!er_runtime_buffer_append_text(buffer, length, capacity, "\\\\", error)) {
                    return false;
                }
                break;
            case '"':
                if (!er_runtime_buffer_append_text(buffer, length, capacity, "\\\"", error)) {
                    return false;
                }
                break;
            case '\n':
                if (!er_runtime_buffer_append_text(buffer, length, capacity, "\\n", error)) {
                    return false;
                }
                break;
            case '\r':
                if (!er_runtime_buffer_append_text(buffer, length, capacity, "\\r", error)) {
                    return false;
                }
                break;
            case '\t':
                if (!er_runtime_buffer_append_text(buffer, length, capacity, "\\t", error)) {
                    return false;
                }
                break;
            default:
                if (*cursor < 0x20) {
                    char escaped[7];
                    snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int) *cursor);
                    if (!er_runtime_buffer_append_text(buffer, length, capacity, escaped, error)) {
                        return false;
                    }
                } else if (!er_runtime_buffer_append_char(buffer, length, capacity, (char) *cursor, error)) {
                    return false;
                }
                break;
        }
        ++cursor;
    }

    return er_runtime_buffer_append_char(buffer, length, capacity, '"', error);
}

static char *er_runtime_base64_encode(const unsigned char *data, size_t length) {
    static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_length;
    size_t i;
    size_t j = 0;
    char *output;

    out_length = ((length + 2) / 3) * 4;
    output = (char *) malloc(out_length + 1);
    if (!output) {
        return NULL;
    }

    for (i = 0; i < length; i += 3) {
        unsigned int octet_a = data[i];
        unsigned int octet_b = i + 1 < length ? data[i + 1] : 0;
        unsigned int octet_c = i + 2 < length ? data[i + 2] : 0;
        unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = alphabet[(triple >> 18) & 0x3f];
        output[j++] = alphabet[(triple >> 12) & 0x3f];
        output[j++] = i + 1 < length ? alphabet[(triple >> 6) & 0x3f] : '=';
        output[j++] = i + 2 < length ? alphabet[triple & 0x3f] : '=';
    }

    output[j] = '\0';
    return output;
}

static char *er_runtime_read_pipe_all(FILE *pipe, ErError *error) {
    char chunk[256];
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    while (fgets(chunk, sizeof(chunk), pipe)) {
        if (!er_runtime_buffer_append_text(&buffer, &length, &capacity, chunk, error)) {
            free(buffer);
            return NULL;
        }
    }

    if (!buffer) {
        buffer = er_rt_dup("");
    }

    return buffer;
}

static void er_runtime_json_skip_ws(const char **cursor) {
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n') {
        ++(*cursor);
    }
}

static bool er_runtime_json_parse_string(const char **cursor, char **out_text, ErError *error) {
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    *out_text = NULL;

    if (**cursor != '"') {
        er_error_set(error, 0, 0, "Expected JSON string");
        return false;
    }

    ++(*cursor);
    while (**cursor && **cursor != '"') {
        char ch = **cursor;

        if (ch == '\\') {
            ++(*cursor);
            switch (**cursor) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u':
                    if (!er_runtime_buffer_append_char(&buffer, &length, &capacity, '?', error)) {
                        free(buffer);
                        return false;
                    }
                    ++(*cursor);
                    for (int i = 0; i < 4; ++i) {
                        if (!isxdigit((unsigned char) **cursor)) {
                            free(buffer);
                            er_error_set(error, 0, 0, "Invalid JSON unicode escape");
                            return false;
                        }
                        ++(*cursor);
                    }
                    continue;
                default:
                    free(buffer);
                    er_error_set(error, 0, 0, "Invalid JSON escape sequence");
                    return false;
            }
        }

        if (!er_runtime_buffer_append_char(&buffer, &length, &capacity, ch, error)) {
            free(buffer);
            return false;
        }

        ++(*cursor);
    }

    if (**cursor != '"') {
        free(buffer);
        er_error_set(error, 0, 0, "Unterminated JSON string");
        return false;
    }

    ++(*cursor);
    if (!buffer) {
        buffer = er_rt_dup("");
    }
    *out_text = buffer;
    return true;
}

static bool er_runtime_json_parse_scalar_value(const char **cursor, ErValue *out_value, ErError *error) {
    char *text = NULL;
    char *end = NULL;

    if (**cursor == '"') {
        if (!er_runtime_json_parse_string(cursor, &text, error)) {
            return false;
        }
        *out_value = er_value_make_string(text);
        free(text);
        return true;
    }
    if (strncmp(*cursor, "true", 4) == 0) {
        *cursor += 4;
        *out_value = er_value_make_bool(true);
        return true;
    }
    if (strncmp(*cursor, "false", 5) == 0) {
        *cursor += 5;
        *out_value = er_value_make_bool(false);
        return true;
    }
    if (strncmp(*cursor, "null", 4) == 0) {
        *cursor += 4;
        *out_value = er_value_make_string("");
        return true;
    }

    if (!(**cursor == '-' || isdigit((unsigned char) **cursor))) {
        er_error_set(error, 0, 0, "Expected JSON value");
        return false;
    }

    out_value->type = ER_VALUE_NUMBER;
    out_value->as.number = strtod(*cursor, &end);
    if (end == *cursor) {
        er_error_set(error, 0, 0, "Invalid JSON number");
        return false;
    }
    *cursor = end;
    return true;
}

static char *er_runtime_sanitize_json_key(const char *key) {
    size_t length = strlen(key);
    size_t offset = (!isalpha((unsigned char) key[0]) && key[0] != '_') ? 1 : 0;
    size_t i;
    char *sanitized = (char *) malloc(length + offset + 1);

    if (!sanitized) {
        return NULL;
    }

    if (offset == 1) {
        sanitized[0] = '_';
    }

    for (i = 0; i < length; ++i) {
        char ch = key[i];
        sanitized[i + offset] = (isalnum((unsigned char) ch) || ch == '_' || ch == '-') ? ch : '_';
    }
    sanitized[length + offset] = '\0';
    return sanitized;
}

static char *er_runtime_make_child_path(const char *base, const char *segment) {
    size_t base_length = base ? strlen(base) : 0;
    size_t segment_length = segment ? strlen(segment) : 0;
    char *path = (char *) malloc(base_length + segment_length + (base_length > 0 ? 2 : 1));

    if (!path) {
        return NULL;
    }

    if (base_length > 0) {
        memcpy(path, base, base_length);
        path[base_length] = '.';
        memcpy(path + base_length + 1, segment, segment_length);
        path[base_length + segment_length + 1] = '\0';
    } else {
        memcpy(path, segment, segment_length);
        path[segment_length] = '\0';
    }

    return path;
}

static bool er_runtime_json_parse_and_store_value(
    ErRuntime *runtime,
    const char **cursor,
    const char *path,
    ErError *error
);

static bool er_runtime_json_store_subtree(
    ErRuntime *runtime,
    const char *path,
    const char *start,
    const char *end,
    ErError *error
) {
    ErValue value;
    char *json;
    bool ok;

    if (!runtime || !path || path[0] == '\0') {
        return true;
    }

    json = er_rt_dup_range(start, (size_t) (end - start));
    if (!json) {
        er_error_set(error, 0, 0, "Out of memory while recording JSON subtree");
        return false;
    }

    value = er_value_make_string(json);
    ok = er_runtime_set_variable(runtime, path, &value, error);
    er_value_free(&value);
    free(json);
    return ok;
}

static bool er_runtime_json_parse_object(
    ErRuntime *runtime,
    const char **cursor,
    const char *path,
    ErError *error
) {
    const char *start = *cursor;

    ++(*cursor);
    er_runtime_json_skip_ws(cursor);
    if (**cursor == '}') {
        ++(*cursor);
        return er_runtime_json_store_subtree(runtime, path, start, *cursor, error);
    }

    for (;;) {
        char *key = NULL;
        char *sanitized = NULL;
        char *child_path = NULL;

        if (!er_runtime_json_parse_string(cursor, &key, error)) {
            return false;
        }
        sanitized = er_runtime_sanitize_json_key(key);
        free(key);
        if (!sanitized) {
            er_error_set(error, 0, 0, "Out of memory while sanitizing JSON key");
            return false;
        }
        child_path = er_runtime_make_child_path(path, sanitized);
        free(sanitized);
        if (!child_path) {
            er_error_set(error, 0, 0, "Out of memory while building JSON variable path");
            return false;
        }

        er_runtime_json_skip_ws(cursor);
        if (**cursor != ':') {
            free(child_path);
            er_error_set(error, 0, 0, "Expected ':' after JSON object key");
            return false;
        }
        ++(*cursor);
        er_runtime_json_skip_ws(cursor);

        if (!er_runtime_json_parse_and_store_value(runtime, cursor, child_path, error)) {
            free(child_path);
            return false;
        }
        free(child_path);

        er_runtime_json_skip_ws(cursor);
        if (**cursor == ',') {
            ++(*cursor);
            er_runtime_json_skip_ws(cursor);
            continue;
        }
        if (**cursor == '}') {
            ++(*cursor);
            break;
        }

        er_error_set(error, 0, 0, "Expected ',' or '}' in JSON object");
        return false;
    }

    return er_runtime_json_store_subtree(runtime, path, start, *cursor, error);
}

static bool er_runtime_json_parse_array(
    ErRuntime *runtime,
    const char **cursor,
    const char *path,
    ErError *error
) {
    const char *start = *cursor;
    size_t index = 0;

    ++(*cursor);
    er_runtime_json_skip_ws(cursor);
    if (**cursor == ']') {
        ++(*cursor);
        return er_runtime_json_store_subtree(runtime, path, start, *cursor, error);
    }

    for (;;) {
        char label[32];
        char *child_path = NULL;

        snprintf(label, sizeof(label), "item%zu", index++);
        child_path = er_runtime_make_child_path(path, label);
        if (!child_path) {
            er_error_set(error, 0, 0, "Out of memory while building JSON array path");
            return false;
        }

        if (!er_runtime_json_parse_and_store_value(runtime, cursor, child_path, error)) {
            free(child_path);
            return false;
        }
        free(child_path);

        er_runtime_json_skip_ws(cursor);
        if (**cursor == ',') {
            ++(*cursor);
            er_runtime_json_skip_ws(cursor);
            continue;
        }
        if (**cursor == ']') {
            ++(*cursor);
            break;
        }

        er_error_set(error, 0, 0, "Expected ',' or ']' in JSON array");
        return false;
    }

    return er_runtime_json_store_subtree(runtime, path, start, *cursor, error);
}

static bool er_runtime_json_parse_and_store_value(
    ErRuntime *runtime,
    const char **cursor,
    const char *path,
    ErError *error
) {
    ErValue value;
    bool ok;

    er_runtime_json_skip_ws(cursor);

    if (**cursor == '{') {
        return er_runtime_json_parse_object(runtime, cursor, path, error);
    }
    if (**cursor == '[') {
        return er_runtime_json_parse_array(runtime, cursor, path, error);
    }

    memset(&value, 0, sizeof(value));
    if (!er_runtime_json_parse_scalar_value(cursor, &value, error)) {
        return false;
    }

    ok = true;
    if (runtime && path && path[0] != '\0') {
        ok = er_runtime_set_variable(runtime, path, &value, error);
    }

    er_value_free(&value);
    return ok;
}

static bool er_runtime_store_structured_json(
    ErRuntime *runtime,
    const char *base_name,
    const char *json,
    ErError *error
) {
    const char *cursor = json;

    if (!runtime || !base_name || !json) {
        return false;
    }

    if (!er_runtime_json_parse_and_store_value(runtime, &cursor, base_name, error)) {
        return false;
    }
    er_runtime_json_skip_ws(&cursor);
    if (*cursor != '\0') {
        er_error_set(error, 0, 0, "Unexpected trailing characters in Python bridge JSON output");
        return false;
    }
    return true;
}

static bool er_runtime_json_root_to_value(
    const char *json,
    ErValue *out_value,
    bool *out_structured,
    ErError *error
) {
    const char *cursor = json;

    memset(out_value, 0, sizeof(*out_value));
    *out_structured = false;

    er_runtime_json_skip_ws(&cursor);
    if (*cursor == '{' || *cursor == '[') {
        if (!er_runtime_json_parse_and_store_value(NULL, &cursor, NULL, error)) {
            return false;
        }
        er_runtime_json_skip_ws(&cursor);
        if (*cursor != '\0') {
            er_error_set(error, 0, 0, "Unexpected trailing characters in Python bridge JSON output");
            return false;
        }
        *out_structured = true;
        *out_value = er_value_make_string(json);
        return true;
    }

    if (!er_runtime_json_parse_scalar_value(&cursor, out_value, error)) {
        return false;
    }
    er_runtime_json_skip_ws(&cursor);
    if (*cursor != '\0') {
        er_value_free(out_value);
        er_error_set(error, 0, 0, "Unexpected trailing characters in Python bridge JSON output");
        return false;
    }
    return true;
}

static bool er_runtime_evaluate_value(ErRuntime *runtime, const ErValue *value, ErValue *out_value, ErError *error);
static bool er_runtime_value_to_string(ErRuntime *runtime, const ErValue *value, char **out_text, ErError *error);
static bool er_runtime_value_to_number(ErRuntime *runtime, const ErValue *value, double *out_number, ErError *error);

static bool er_runtime_value_to_json_text(
    ErRuntime *runtime,
    const ErValue *value,
    char **out_json,
    ErError *error
) {
    ErValue resolved;
    char number_text[64];
    char *json = NULL;
    size_t length = 0;
    size_t capacity = 0;

    *out_json = NULL;
    memset(&resolved, 0, sizeof(resolved));

    if (!er_runtime_evaluate_value(runtime, value, &resolved, error)) {
        return false;
    }

    switch (resolved.type) {
        case ER_VALUE_STRING:
            if (!er_runtime_json_append_escaped(&json, &length, &capacity, resolved.as.string, error)) {
                er_value_free(&resolved);
                free(json);
                return false;
            }
            break;
        case ER_VALUE_SYMBOL:
            if (!er_runtime_json_append_escaped(&json, &length, &capacity, resolved.as.symbol, error)) {
                er_value_free(&resolved);
                free(json);
                return false;
            }
            break;
        case ER_VALUE_BOOL:
            if (!er_runtime_buffer_append_text(&json, &length, &capacity, resolved.as.boolean ? "true" : "false", error)) {
                er_value_free(&resolved);
                free(json);
                return false;
            }
            break;
        case ER_VALUE_NUMBER:
            snprintf(number_text, sizeof(number_text), "%g", resolved.as.number);
            if (!er_runtime_buffer_append_text(&json, &length, &capacity, number_text, error)) {
                er_value_free(&resolved);
                free(json);
                return false;
            }
            break;
        default:
            if (!er_runtime_buffer_append_text(&json, &length, &capacity, "null", error)) {
                er_value_free(&resolved);
                free(json);
                return false;
            }
            break;
    }

    er_value_free(&resolved);
    *out_json = json ? json : er_rt_dup("null");
    return *out_json != NULL;
}

static bool er_runtime_is_safe_identifier_text(const char *text) {
    size_t i;

    if (!text || text[0] == '\0' || (!isalpha((unsigned char) text[0]) && text[0] != '_')) {
        return false;
    }

    for (i = 1; text[i] != '\0'; ++i) {
        if (!isalnum((unsigned char) text[i]) && text[i] != '_') {
            return false;
        }
    }

    return true;
}

static bool er_runtime_split_python_target(
    const char *target,
    char **out_alias,
    char **out_function,
    ErError *error
) {
    const char *dot;

    *out_alias = NULL;
    *out_function = NULL;

    if (!target) {
        er_error_set(error, 0, 0, "py.call requires a target in the form alias.function");
        return false;
    }

    dot = strchr(target, '.');
    if (!dot || dot == target || dot[1] == '\0' || strchr(dot + 1, '.')) {
        er_error_set(error, 0, 0, "py.call target must use the form alias.function");
        return false;
    }

    *out_alias = er_rt_dup_range(target, (size_t) (dot - target));
    *out_function = er_rt_dup(dot + 1);
    if (!*out_alias || !*out_function) {
        free(*out_alias);
        free(*out_function);
        *out_alias = NULL;
        *out_function = NULL;
        er_error_set(error, 0, 0, "Out of memory while parsing py.call target");
        return false;
    }

    if (!er_runtime_is_safe_identifier_text(*out_alias) || !er_runtime_is_safe_identifier_text(*out_function)) {
        free(*out_alias);
        free(*out_function);
        *out_alias = NULL;
        *out_function = NULL;
        er_error_set(error, 0, 0, "py.call target contains unsupported characters");
        return false;
    }

    return true;
}

static bool er_runtime_execute_python_call(
    ErRuntime *runtime,
    const ErCallExpression *call,
    ErRuntimePyResult *result,
    ErError *error
) {
    const char *python_cmd = "python";
    char *target = NULL;
    char *alias = NULL;
    char *function = NULL;
    char *args_json = NULL;
    char *args_b64 = NULL;
    char *command = NULL;
    char *output = NULL;
    FILE *pipe = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t i;
    int exit_code;
    ErRuntimePythonModule *module;

    memset(result, 0, sizeof(*result));

    if (!call || !call->name || strcmp(call->name, "py.call") != 0) {
        er_error_set(error, 0, 0, "Unsupported call expression");
        return false;
    }
    if (call->args.count == 0) {
        er_error_set(error, 0, 0, "py.call expects a target argument");
        return false;
    }

    if (!er_runtime_value_to_string(runtime, &call->args.items[0], &target, error)) {
        return false;
    }
    if (!er_runtime_split_python_target(target, &alias, &function, error)) {
        free(target);
        return false;
    }

    module = er_runtime_find_python_module(runtime, alias);
    if (!module) {
        er_error_set(error, 0, 0, "Unknown Python import alias '%s'", alias);
        free(target);
        free(alias);
        free(function);
        return false;
    }

    if (!er_runtime_buffer_append_char(&args_json, &length, &capacity, '[', error)) {
        free(target);
        free(alias);
        free(function);
        return false;
    }
    for (i = 1; i < call->args.count; ++i) {
        char *json_arg = NULL;
        if (i > 1 && !er_runtime_buffer_append_text(&args_json, &length, &capacity, ",", error)) {
            free(target);
            free(alias);
            free(function);
            free(args_json);
            return false;
        }
        if (!er_runtime_value_to_json_text(runtime, &call->args.items[i], &json_arg, error)) {
            free(target);
            free(alias);
            free(function);
            free(args_json);
            return false;
        }
        if (!er_runtime_buffer_append_text(&args_json, &length, &capacity, json_arg, error)) {
            free(target);
            free(alias);
            free(function);
            free(args_json);
            free(json_arg);
            return false;
        }
        free(json_arg);
    }
    if (!er_runtime_buffer_append_char(&args_json, &length, &capacity, ']', error)) {
        free(target);
        free(alias);
        free(function);
        free(args_json);
        return false;
    }

    args_b64 = er_runtime_base64_encode((const unsigned char *) args_json, strlen(args_json));
    if (!args_b64) {
        er_error_set(error, 0, 0, "Out of memory while encoding Python bridge arguments");
        free(target);
        free(alias);
        free(function);
        free(args_json);
        return false;
    }

    command = (char *) malloc(
        strlen(python_cmd) + strlen(module->script_path) + strlen(function) + strlen(args_b64) + 128
    );
    if (!command) {
        er_error_set(error, 0, 0, "Out of memory while preparing Python bridge command");
        free(target);
        free(alias);
        free(function);
        free(args_json);
        free(args_b64);
        return false;
    }

#ifdef _WIN32
    if (module->is_executable) {
        snprintf(
            command,
            strlen(python_cmd) + strlen(module->script_path) + strlen(function) + strlen(args_b64) + 128,
            "\"\"%s\" --erire-call %s --erire-args-b64 %s\"",
            module->script_path,
            function,
            args_b64
        );
    } else {
        snprintf(
            command,
            strlen(python_cmd) + strlen(module->script_path) + strlen(function) + strlen(args_b64) + 128,
            "\"\"%s\" \"%s\" --erire-call %s --erire-args-b64 %s\"",
            python_cmd,
            module->script_path,
            function,
            args_b64
        );
    }
#else
    if (module->is_executable) {
        snprintf(
            command,
            strlen(python_cmd) + strlen(module->script_path) + strlen(function) + strlen(args_b64) + 128,
            "\"%s\" --erire-call \"%s\" --erire-args-b64 \"%s\" 2>&1",
            module->script_path,
            function,
            args_b64
        );
    } else {
        snprintf(
            command,
            strlen(python_cmd) + strlen(module->script_path) + strlen(function) + strlen(args_b64) + 128,
            "%s \"%s\" --erire-call \"%s\" --erire-args-b64 \"%s\" 2>&1",
            python_cmd,
            module->script_path,
            function,
            args_b64
        );
    }
#endif

    pipe = ER_RT_POPEN(command, "r");
    if (!pipe) {
        er_error_set(error, 0, 0, "Could not start Python bridge process");
        free(target);
        free(alias);
        free(function);
        free(args_json);
        free(args_b64);
        free(command);
        return false;
    }

    output = er_runtime_read_pipe_all(pipe, error);
    exit_code = ER_RT_PCLOSE(pipe);
    pipe = NULL;

    if (!output) {
        free(target);
        free(alias);
        free(function);
        free(args_json);
        free(args_b64);
        free(command);
        return false;
    }

    if (exit_code != 0) {
        er_error_set(
            error,
            0,
            0,
            "Python bridge call failed for %s: %s",
            target,
            output[0] != '\0' ? output : "process exited without stdout"
        );
        free(target);
        free(alias);
        free(function);
        free(args_json);
        free(args_b64);
        free(command);
        free(output);
        return false;
    }

    result->json = er_rt_dup(output);
    free(output);
    if (!result->json) {
        er_error_set(error, 0, 0, "Out of memory while copying Python bridge output");
        free(target);
        free(alias);
        free(function);
        free(args_json);
        free(args_b64);
        free(command);
        return false;
    }

    if (!er_runtime_json_root_to_value(result->json, &result->value, &result->structured, error)) {
        free(target);
        free(alias);
        free(function);
        free(args_json);
        free(args_b64);
        free(command);
        free(result->json);
        memset(result, 0, sizeof(*result));
        return false;
    }

    free(target);
    free(alias);
    free(function);
    free(args_json);
    free(args_b64);
    free(command);
    return true;
}

static bool er_runtime_native_expect_arg_range(
    const ErCallExpression *call,
    size_t min_args,
    size_t max_args,
    ErError *error
) {
    if (!call) {
        er_error_set(error, 0, 0, "Native call is missing its target");
        return false;
    }

    if (call->args.count < min_args || call->args.count > max_args) {
        if (min_args == max_args) {
            er_error_set(
                error,
                0,
                0,
                "Native call '%s' expects %zu argument%s, got %zu",
                call->name ? call->name : "<native>",
                min_args,
                min_args == 1 ? "" : "s",
                call->args.count
            );
        } else {
            er_error_set(
                error,
                0,
                0,
                "Native call '%s' expects between %zu and %zu arguments, got %zu",
                call->name ? call->name : "<native>",
                min_args,
                max_args,
                call->args.count
            );
        }
        return false;
    }

    return true;
}

static bool er_runtime_native_expect_min_args(
    const ErCallExpression *call,
    size_t min_args,
    ErError *error
) {
    if (!call) {
        er_error_set(error, 0, 0, "Native call is missing its target");
        return false;
    }

    if (call->args.count < min_args) {
        er_error_set(
            error,
            0,
            0,
            "Native call '%s' expects at least %zu argument%s, got %zu",
            call->name ? call->name : "<native>",
            min_args,
            min_args == 1 ? "" : "s",
            call->args.count
        );
        return false;
    }

    return true;
}

static char *er_runtime_make_time_text(void) {
    char buffer[64];
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);

    if (!time_info) {
        return er_rt_dup("");
    }

    if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info) == 0) {
        return er_rt_dup("");
    }
    return er_rt_dup(buffer);
}

static char *er_runtime_get_cwd_text(void) {
    char buffer[1024];

    if (!ER_RT_GETCWD(buffer, sizeof(buffer))) {
        return er_rt_dup("");
    }
    return er_rt_dup(buffer);
}

static char *er_runtime_get_hostname_text(void) {
    char buffer[256];

#ifdef _WIN32
    DWORD size = (DWORD) sizeof(buffer);
    if (!GetComputerNameA(buffer, &size)) {
        return er_rt_dup("");
    }
#else
    if (gethostname(buffer, sizeof(buffer) - 1) != 0) {
        return er_rt_dup("");
    }
    buffer[sizeof(buffer) - 1] = '\0';
#endif
    return er_rt_dup(buffer);
}

static char *er_runtime_get_platform_text(void) {
#ifdef _WIN32
    return er_rt_dup("windows");
#elif __APPLE__
    return er_rt_dup("macos");
#else
    return er_rt_dup("linux");
#endif
}

#ifdef _WIN32
static char *er_runtime_dialog_make_filter(const char *filter_text, const char *fallback_text, ErError *error) {
    const char *source = (filter_text && filter_text[0] != '\0') ? filter_text : fallback_text;
    size_t length;
    size_t i;
    char *filter;

    if (!source) {
        source = "All Files|*.*";
    }

    length = strlen(source);
    filter = (char *) malloc(length + 2);
    if (!filter) {
        er_error_set(error, 0, 0, "Out of memory while preparing file dialog filter");
        return NULL;
    }

    for (i = 0; i < length; ++i) {
        filter[i] = source[i] == '|' ? '\0' : source[i];
    }
    filter[length] = '\0';
    filter[length + 1] = '\0';
    return filter;
}

static char *er_runtime_dialog_open_file(
    ErRuntime *runtime,
    bool allow_multiple,
    const char *title,
    const char *filter_text,
    ErError *error
) {
    OPENFILENAMEA dialog;
    char *filter = NULL;
    char *result = NULL;
    char buffer[32768];
    const char *fallback_filter = "Audio Files|*.mp3;*.wav;*.aac;*.wma;*.m4a;*.flac;*.ogg|All Files|*.*";

    memset(&dialog, 0, sizeof(dialog));
    memset(buffer, 0, sizeof(buffer));

    filter = er_runtime_dialog_make_filter(filter_text, fallback_filter, error);
    if (!filter) {
        return NULL;
    }

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = (runtime && runtime->app_initialized) ? runtime->app.hwnd : NULL;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = (DWORD) sizeof(buffer);
    dialog.lpstrTitle = title && title[0] != '\0' ? title : NULL;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (allow_multiple) {
        dialog.Flags |= OFN_ALLOWMULTISELECT;
    }

    if (!GetOpenFileNameA(&dialog)) {
        free(filter);
        return er_rt_dup("");
    }

    if (!allow_multiple) {
        result = er_rt_dup(buffer);
        free(filter);
        return result;
    }

    if (buffer[dialog.nFileOffset] == '\0') {
        result = er_rt_dup(buffer);
        free(filter);
        return result;
    }

    {
        char *cursor = buffer + dialog.nFileOffset;
        char *combined = NULL;
        size_t length = 0;
        size_t capacity = 0;

        while (*cursor != '\0') {
            char full_path[1024];
            er_path_join(buffer, cursor, full_path, sizeof(full_path));
            if (!er_runtime_buffer_append_text(&combined, &length, &capacity, full_path, error) ||
                !er_runtime_buffer_append_text(&combined, &length, &capacity, "\n", error)) {
                free(combined);
                free(filter);
                return NULL;
            }
            cursor += strlen(cursor) + 1;
        }

        if (length > 0 && combined) {
            combined[length - 1] = '\0';
        }
        result = combined ? combined : er_rt_dup("");
    }

    free(filter);
    return result;
}

static char *er_runtime_dialog_save_file(
    ErRuntime *runtime,
    const char *title,
    const char *default_name,
    const char *filter_text,
    ErError *error
) {
    OPENFILENAMEA dialog;
    char *filter = NULL;
    char buffer[4096];
    const char *fallback_filter = "Playlist Files|*.pyplaylist|All Files|*.*";

    memset(&dialog, 0, sizeof(dialog));
    memset(buffer, 0, sizeof(buffer));

    if (default_name && default_name[0] != '\0') {
        strncpy(buffer, default_name, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
    }

    filter = er_runtime_dialog_make_filter(filter_text, fallback_filter, error);
    if (!filter) {
        return NULL;
    }

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = (runtime && runtime->app_initialized) ? runtime->app.hwnd : NULL;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = (DWORD) sizeof(buffer);
    dialog.lpstrTitle = title && title[0] != '\0' ? title : NULL;
    dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameA(&dialog)) {
        free(filter);
        return er_rt_dup("");
    }

    free(filter);
    return er_rt_dup(buffer);
}
#else
static char *er_runtime_dialog_open_file(
    ErRuntime *runtime,
    bool allow_multiple,
    const char *title,
    const char *filter_text,
    ErError *error
) {
    (void) runtime;
    (void) allow_multiple;
    (void) title;
    (void) filter_text;
    (void) error;
    return er_rt_dup("");
}

static char *er_runtime_dialog_save_file(
    ErRuntime *runtime,
    const char *title,
    const char *default_name,
    const char *filter_text,
    ErError *error
) {
    (void) runtime;
    (void) title;
    (void) default_name;
    (void) filter_text;
    (void) error;
    return er_rt_dup("");
}
#endif

static void er_runtime_ascii_upper(char *text) {
    unsigned char *cursor = (unsigned char *) text;
    while (cursor && *cursor != '\0') {
        *cursor = (unsigned char) toupper(*cursor);
        ++cursor;
    }
}

static void er_runtime_ascii_lower(char *text) {
    unsigned char *cursor = (unsigned char *) text;
    while (cursor && *cursor != '\0') {
        *cursor = (unsigned char) tolower(*cursor);
        ++cursor;
    }
}

static void er_runtime_ascii_title(char *text) {
    bool capitalize_next = true;
    unsigned char *cursor = (unsigned char *) text;

    while (cursor && *cursor != '\0') {
        if (isalnum(*cursor)) {
            *cursor = (unsigned char) (capitalize_next ? toupper(*cursor) : tolower(*cursor));
            capitalize_next = false;
        } else {
            capitalize_next = true;
        }
        ++cursor;
    }
}

static bool er_runtime_value_to_flag(ErRuntime *runtime, const ErValue *value, bool *out_flag, ErError *error) {
    char *text = NULL;

    if (!out_flag) {
        er_error_set(error, 0, 0, "Boolean output pointer is required");
        return false;
    }

    if (!er_runtime_value_to_string(runtime, value, &text, error)) {
        return false;
    }

    *out_flag = er_runtime_text_means_true(text);
    free(text);
    return true;
}

static bool er_runtime_execute_native_call(
    ErRuntime *runtime,
    const ErCallExpression *call,
    ErValue *out_value,
    ErError *error
) {
    size_t i;

    if (!call || !call->name || call->name[0] == '\0') {
        er_error_set(error, 0, 0, "Native call target cannot be empty");
        return false;
    }

    memset(out_value, 0, sizeof(*out_value));

    if (strcmp(call->name, "time.now") == 0) {
        char *text;
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }
        text = er_runtime_make_time_text();
        *out_value = er_value_make_string(text ? text : "");
        free(text);
        return true;
    }

    if (strcmp(call->name, "sys.cwd") == 0) {
        char *text;
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }
        text = er_runtime_get_cwd_text();
        *out_value = er_value_make_string(text ? text : "");
        free(text);
        return true;
    }

    if (strcmp(call->name, "sys.hostname") == 0) {
        char *text;
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }
        text = er_runtime_get_hostname_text();
        *out_value = er_value_make_string(text ? text : "");
        free(text);
        return true;
    }

    if (strcmp(call->name, "sys.platform") == 0) {
        char *text;
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }
        text = er_runtime_get_platform_text();
        *out_value = er_value_make_string(text ? text : "");
        free(text);
        return true;
    }

    if (strcmp(call->name, "runtime.liveEnabled") == 0 ||
        strcmp(call->name, "runtime.liveVersion") == 0 ||
        strcmp(call->name, "runtime.liveStatus") == 0 ||
        strcmp(call->name, "runtime.liveError") == 0 ||
        strcmp(call->name, "runtime.widgetCount") == 0 ||
        strcmp(call->name, "runtime.timerCount") == 0) {
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }

        if (strcmp(call->name, "runtime.liveEnabled") == 0) {
            *out_value = er_value_make_bool(runtime->live.enabled);
        } else if (strcmp(call->name, "runtime.liveVersion") == 0) {
            *out_value = er_value_make_number((double) runtime->live.patch_version);
        } else if (strcmp(call->name, "runtime.liveStatus") == 0) {
            *out_value = er_value_make_string(runtime->live.status_text ? runtime->live.status_text : "");
        } else if (strcmp(call->name, "runtime.liveError") == 0) {
            *out_value = er_value_make_string(runtime->live.last_error_text ? runtime->live.last_error_text : "");
        } else if (strcmp(call->name, "runtime.widgetCount") == 0) {
            *out_value = er_value_make_number((double) runtime->app.node_count);
        } else {
            *out_value = er_value_make_number((double) runtime->timer_count);
        }

        return true;
    }

    if (strcmp(call->name, "runtime.hasVar") == 0 ||
        strcmp(call->name, "runtime.getVar") == 0 ||
        strcmp(call->name, "runtime.widgetExists") == 0) {
        char *name_text = NULL;
        ErRuntimeVar *var;
        ErUiNode *node;

        if (!er_runtime_native_expect_arg_range(call, 1, 1, error)) {
            return false;
        }
        if (!er_runtime_value_to_string(runtime, &call->args.items[0], &name_text, error)) {
            return false;
        }

        if (strcmp(call->name, "runtime.hasVar") == 0) {
            var = er_runtime_find_variable(runtime, name_text ? name_text : "");
            *out_value = er_value_make_bool(var != NULL);
        } else if (strcmp(call->name, "runtime.getVar") == 0) {
            var = er_runtime_find_variable(runtime, name_text ? name_text : "");
            if (var) {
                *out_value = er_value_clone(&var->value);
            } else {
                *out_value = er_value_make_string("");
            }
        } else {
            node = runtime->app_initialized ? er_ui_app_find_node(&runtime->app, name_text ? name_text : "") : NULL;
            *out_value = er_value_make_bool(node != NULL);
        }

        free(name_text);
        return true;
    }

    if (strcmp(call->name, "dialog.openFiles") == 0 ||
        strcmp(call->name, "dialog.openFile") == 0 ||
        strcmp(call->name, "dialog.saveFile") == 0) {
        char *title_text = NULL;
        char *filter_text = NULL;
        char *default_name_text = NULL;
        char *result_text = NULL;

        if (strcmp(call->name, "dialog.openFiles") == 0) {
            if (!er_runtime_native_expect_arg_range(call, 0, 2, error)) {
                return false;
            }
            if (call->args.count >= 1 &&
                !er_runtime_value_to_string(runtime, &call->args.items[0], &title_text, error)) {
                return false;
            }
            if (call->args.count >= 2 &&
                !er_runtime_value_to_string(runtime, &call->args.items[1], &filter_text, error)) {
                free(title_text);
                return false;
            }
            result_text = er_runtime_dialog_open_file(runtime, true, title_text, filter_text, error);
        } else if (strcmp(call->name, "dialog.openFile") == 0) {
            if (!er_runtime_native_expect_arg_range(call, 0, 2, error)) {
                return false;
            }
            if (call->args.count >= 1 &&
                !er_runtime_value_to_string(runtime, &call->args.items[0], &title_text, error)) {
                return false;
            }
            if (call->args.count >= 2 &&
                !er_runtime_value_to_string(runtime, &call->args.items[1], &filter_text, error)) {
                free(title_text);
                return false;
            }
            result_text = er_runtime_dialog_open_file(runtime, false, title_text, filter_text, error);
        } else {
            if (!er_runtime_native_expect_arg_range(call, 0, 3, error)) {
                return false;
            }
            if (call->args.count >= 1 &&
                !er_runtime_value_to_string(runtime, &call->args.items[0], &title_text, error)) {
                return false;
            }
            if (call->args.count >= 2 &&
                !er_runtime_value_to_string(runtime, &call->args.items[1], &default_name_text, error)) {
                free(title_text);
                return false;
            }
            if (call->args.count >= 3 &&
                !er_runtime_value_to_string(runtime, &call->args.items[2], &filter_text, error)) {
                free(title_text);
                free(default_name_text);
                return false;
            }
            result_text = er_runtime_dialog_save_file(runtime, title_text, default_name_text, filter_text, error);
        }

        free(title_text);
        free(filter_text);
        free(default_name_text);
        if (!result_text) {
            return false;
        }
        *out_value = er_value_make_string(result_text);
        free(result_text);
        return true;
    }

    if (strcmp(call->name, "storage.read") == 0 ||
        strcmp(call->name, "storage.exists") == 0) {
        char *key_text = NULL;
        char *stored_text = NULL;
        bool exists;

        if (!er_runtime_native_expect_arg_range(call, 1, 1, error)) {
            return false;
        }
        if (!er_runtime_value_to_string(runtime, &call->args.items[0], &key_text, error)) {
            return false;
        }

        if (strcmp(call->name, "storage.exists") == 0) {
            char path[1024];
            if (!er_runtime_storage_key_path(runtime, key_text ? key_text : "", path, sizeof(path), error)) {
                free(key_text);
                return false;
            }
            exists = er_file_exists(path);
            *out_value = er_value_make_bool(exists);
            free(key_text);
            return true;
        }

        if (!er_runtime_storage_read_text(runtime, key_text ? key_text : "", &stored_text, error)) {
            free(key_text);
            return false;
        }
        *out_value = er_value_make_string(stored_text ? stored_text : "");
        free(key_text);
        free(stored_text);
        return true;
    }

    if (strcmp(call->name, "media.state") == 0) {
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }
        *out_value = er_value_make_string(er_media_player_state(&runtime->media));
        return true;
    }

    if (strcmp(call->name, "media.position") == 0 ||
        strcmp(call->name, "media.duration") == 0 ||
        strcmp(call->name, "media.volume") == 0 ||
        strcmp(call->name, "media.count") == 0 ||
        strcmp(call->name, "media.index") == 0) {
        double number_value = 0.0;

        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }

        if (strcmp(call->name, "media.position") == 0) {
            number_value = (double) er_media_player_position_ms(&runtime->media);
        } else if (strcmp(call->name, "media.duration") == 0) {
            number_value = (double) er_media_player_duration_ms(&runtime->media);
        } else if (strcmp(call->name, "media.volume") == 0) {
            number_value = (double) er_media_player_volume(&runtime->media);
        } else if (strcmp(call->name, "media.count") == 0) {
            number_value = (double) er_media_player_count(&runtime->media);
        } else {
            number_value = (double) er_media_player_current_index(&runtime->media);
        }

        *out_value = er_value_make_number(number_value);
        return true;
    }

    if (strcmp(call->name, "media.muted") == 0 || strcmp(call->name, "media.shuffle") == 0) {
        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }
        *out_value = er_value_make_bool(
            strcmp(call->name, "media.muted") == 0
                ? er_media_player_is_muted(&runtime->media)
                : er_media_player_shuffle_enabled(&runtime->media)
        );
        return true;
    }

    if (strcmp(call->name, "media.repeat") == 0 ||
        strcmp(call->name, "media.currentName") == 0 ||
        strcmp(call->name, "media.currentTitle") == 0 ||
        strcmp(call->name, "media.currentArtist") == 0 ||
        strcmp(call->name, "media.currentYear") == 0 ||
        strcmp(call->name, "media.currentBitrate") == 0 ||
        strcmp(call->name, "media.currentSampleRate") == 0 ||
        strcmp(call->name, "media.currentArt") == 0) {
        const char *text_value = "";

        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }

        if (strcmp(call->name, "media.repeat") == 0) {
            text_value = er_media_player_repeat_mode_text(&runtime->media);
        } else if (strcmp(call->name, "media.currentName") == 0) {
            text_value = er_media_player_current_name(&runtime->media);
        } else if (strcmp(call->name, "media.currentTitle") == 0) {
            text_value = er_media_player_current_title(&runtime->media);
        } else if (strcmp(call->name, "media.currentArtist") == 0) {
            text_value = er_media_player_current_artist(&runtime->media);
        } else if (strcmp(call->name, "media.currentYear") == 0) {
            text_value = er_media_player_current_year(&runtime->media);
        } else if (strcmp(call->name, "media.currentBitrate") == 0) {
            text_value = er_media_player_current_bitrate(&runtime->media);
        } else if (strcmp(call->name, "media.currentSampleRate") == 0) {
            text_value = er_media_player_current_sample_rate(&runtime->media);
        } else {
            text_value = er_media_player_current_art(&runtime->media);
        }

        *out_value = er_value_make_string(text_value ? text_value : "");
        return true;
    }

    if (strcmp(call->name, "media.positionText") == 0 ||
        strcmp(call->name, "media.remainingText") == 0 ||
        strcmp(call->name, "media.durationText") == 0 ||
        strcmp(call->name, "media.playlistText") == 0) {
        char *text_value = NULL;

        if (!er_runtime_native_expect_arg_range(call, 0, 0, error)) {
            return false;
        }

        if (strcmp(call->name, "media.positionText") == 0) {
            text_value = er_media_player_position_text(&runtime->media);
        } else if (strcmp(call->name, "media.remainingText") == 0) {
            text_value = er_media_player_remaining_text(&runtime->media);
        } else if (strcmp(call->name, "media.durationText") == 0) {
            text_value = er_media_player_duration_text(&runtime->media);
        } else {
            text_value = er_media_player_playlist_text(&runtime->media, error);
        }

        if (!text_value) {
            if (er_error_has(error)) {
                return false;
            }
            *out_value = er_value_make_string("");
            return true;
        }
        *out_value = er_value_make_string(text_value);
        free(text_value);
        return true;
    }

    if (strcmp(call->name, "text.upper") == 0 ||
        strcmp(call->name, "text.lower") == 0 ||
        strcmp(call->name, "text.title") == 0) {
        char *text = NULL;

        if (!er_runtime_native_expect_arg_range(call, 1, 1, error)) {
            return false;
        }
        if (!er_runtime_value_to_string(runtime, &call->args.items[0], &text, error)) {
            return false;
        }
        if (strcmp(call->name, "text.upper") == 0) {
            er_runtime_ascii_upper(text);
        } else if (strcmp(call->name, "text.lower") == 0) {
            er_runtime_ascii_lower(text);
        } else {
            er_runtime_ascii_title(text);
        }
        *out_value = er_value_make_string(text ? text : "");
        free(text);
        return true;
    }

    if (strcmp(call->name, "text.length") == 0) {
        char *text = NULL;

        if (!er_runtime_native_expect_arg_range(call, 1, 1, error)) {
            return false;
        }
        if (!er_runtime_value_to_string(runtime, &call->args.items[0], &text, error)) {
            return false;
        }
        *out_value = er_value_make_number((double) strlen(text ? text : ""));
        free(text);
        return true;
    }

    if (strcmp(call->name, "text.contains") == 0) {
        char *text = NULL;
        char *needle = NULL;
        bool found;

        if (!er_runtime_native_expect_arg_range(call, 2, 2, error)) {
            return false;
        }
        if (!er_runtime_value_to_string(runtime, &call->args.items[0], &text, error) ||
            !er_runtime_value_to_string(runtime, &call->args.items[1], &needle, error)) {
            free(text);
            free(needle);
            return false;
        }
        found = text && needle && strstr(text, needle) != NULL;
        *out_value = er_value_make_bool(found);
        free(text);
        free(needle);
        return true;
    }

    if (strcmp(call->name, "text.concat") == 0) {
        char *buffer = NULL;
        size_t length = 0;
        size_t capacity = 0;

        if (!er_runtime_native_expect_min_args(call, 1, error)) {
            return false;
        }

        for (i = 0; i < call->args.count; ++i) {
            char *part = NULL;
            if (!er_runtime_value_to_string(runtime, &call->args.items[i], &part, error)) {
                free(buffer);
                return false;
            }
            if (!er_runtime_buffer_append_text(&buffer, &length, &capacity, part ? part : "", error)) {
                free(part);
                free(buffer);
                return false;
            }
            free(part);
        }

        *out_value = er_value_make_string(buffer ? buffer : "");
        free(buffer);
        return true;
    }

    if (strncmp(call->name, "math.", 5) == 0) {
        double accumulator = 0.0;
        double current = 0.0;

        if (!er_runtime_native_expect_min_args(call, 1, error)) {
            return false;
        }

        if (!er_runtime_value_to_number(runtime, &call->args.items[0], &accumulator, error)) {
            er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
            return false;
        }

        if (strcmp(call->name, "math.add") == 0) {
            for (i = 1; i < call->args.count; ++i) {
                if (!er_runtime_value_to_number(runtime, &call->args.items[i], &current, error)) {
                    er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
                    return false;
                }
                accumulator += current;
            }
            *out_value = er_value_make_number(accumulator);
            return true;
        }

        if (strcmp(call->name, "math.sub") == 0) {
            for (i = 1; i < call->args.count; ++i) {
                if (!er_runtime_value_to_number(runtime, &call->args.items[i], &current, error)) {
                    er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
                    return false;
                }
                accumulator -= current;
            }
            *out_value = er_value_make_number(accumulator);
            return true;
        }

        if (strcmp(call->name, "math.mul") == 0) {
            for (i = 1; i < call->args.count; ++i) {
                if (!er_runtime_value_to_number(runtime, &call->args.items[i], &current, error)) {
                    er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
                    return false;
                }
                accumulator *= current;
            }
            *out_value = er_value_make_number(accumulator);
            return true;
        }

        if (strcmp(call->name, "math.div") == 0) {
            if (!er_runtime_native_expect_min_args(call, 2, error)) {
                return false;
            }
            for (i = 1; i < call->args.count; ++i) {
                if (!er_runtime_value_to_number(runtime, &call->args.items[i], &current, error)) {
                    er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
                    return false;
                }
                if (current == 0.0) {
                    er_error_set(error, 0, 0, "Division by zero in '%s'", call->name);
                    return false;
                }
                accumulator /= current;
            }
            *out_value = er_value_make_number(accumulator);
            return true;
        }

        if (strcmp(call->name, "math.min") == 0) {
            for (i = 1; i < call->args.count; ++i) {
                if (!er_runtime_value_to_number(runtime, &call->args.items[i], &current, error)) {
                    er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
                    return false;
                }
                if (current < accumulator) {
                    accumulator = current;
                }
            }
            *out_value = er_value_make_number(accumulator);
            return true;
        }

        if (strcmp(call->name, "math.max") == 0) {
            for (i = 1; i < call->args.count; ++i) {
                if (!er_runtime_value_to_number(runtime, &call->args.items[i], &current, error)) {
                    er_error_set(error, 0, 0, "Native math call '%s' requires numeric arguments", call->name);
                    return false;
                }
                if (current > accumulator) {
                    accumulator = current;
                }
            }
            *out_value = er_value_make_number(accumulator);
            return true;
        }
    }

    er_error_set(error, 0, 0, "Unsupported native call: %s", call->name);
    return false;
}

static bool er_runtime_evaluate_value(ErRuntime *runtime, const ErValue *value, ErValue *out_value, ErError *error) {
    ErRuntimeVar *var;
    ErRuntimePyResult result;

    memset(out_value, 0, sizeof(*out_value));

    if (!value) {
        *out_value = er_value_make_string("");
        return true;
    }

    if (value->type == ER_VALUE_VARIABLE) {
        var = er_runtime_find_variable(runtime, value->as.variable);
        if (!var) {
            *out_value = er_value_make_string("");
            return true;
        }
        *out_value = er_value_clone(&var->value);
        return true;
    }

    if (value->type == ER_VALUE_CALL) {
        if (value->as.call && value->as.call->name && strcmp(value->as.call->name, "py.call") == 0) {
            if (!er_runtime_execute_python_call(runtime, value->as.call, &result, error)) {
                return false;
            }
            *out_value = result.value;
            free(result.json);
            return true;
        }
        return er_runtime_execute_native_call(runtime, value->as.call, out_value, error);
    }

    *out_value = er_value_clone(value);
    return true;
}

static bool er_runtime_value_to_string(
    ErRuntime *runtime,
    const ErValue *value,
    char **out_text,
    ErError *error
) {
    ErValue resolved;
    char buffer[128];

    *out_text = NULL;
    memset(&resolved, 0, sizeof(resolved));

    if (!er_runtime_evaluate_value(runtime, value, &resolved, error)) {
        return false;
    }

    switch (resolved.type) {
        case ER_VALUE_STRING:
            *out_text = er_rt_dup(resolved.as.string ? resolved.as.string : "");
            break;
        case ER_VALUE_SYMBOL:
            *out_text = er_rt_dup(resolved.as.symbol ? resolved.as.symbol : "");
            break;
        case ER_VALUE_BOOL:
            *out_text = er_rt_dup(resolved.as.boolean ? "true" : "false");
            break;
        case ER_VALUE_NUMBER:
            snprintf(buffer, sizeof(buffer), "%g", resolved.as.number);
            *out_text = er_rt_dup(buffer);
            break;
        default:
            *out_text = er_rt_dup("");
            break;
    }

    er_value_free(&resolved);
    if (!*out_text) {
        er_error_set(error, 0, 0, "Out of memory while converting value to text");
        return false;
    }
    return true;
}

static bool er_runtime_value_to_number(ErRuntime *runtime, const ErValue *value, double *out_number, ErError *error) {
    ErValue resolved;

    memset(&resolved, 0, sizeof(resolved));
    if (!er_runtime_evaluate_value(runtime, value, &resolved, error)) {
        return false;
    }
    if (resolved.type == ER_VALUE_NUMBER) {
        *out_number = resolved.as.number;
        er_value_free(&resolved);
        return true;
    }
    if (resolved.type == ER_VALUE_BOOL) {
        *out_number = resolved.as.boolean ? 1.0 : 0.0;
        er_value_free(&resolved);
        return true;
    }
    er_value_free(&resolved);
    return false;
}

static bool er_runtime_text_means_true(const char *text) {
    if (!text) {
        return false;
    }

    return strcmp(text, "true") == 0 ||
           strcmp(text, "loop") == 0 ||
           strcmp(text, "repeat") == 0 ||
           strcmp(text, "infinite") == 0 ||
           strcmp(text, "yes") == 0;
}

static bool er_runtime_parse_color_text(const char *text, unsigned int *out_rgb) {
    unsigned int value;

    if (!text || text[0] != '#') {
        return false;
    }
    if (strlen(text) != 7) {
        return false;
    }
    if (sscanf(text + 1, "%x", &value) != 1) {
        return false;
    }
    *out_rgb = value & 0xffffffu;
    return true;
}

static bool er_runtime_parse_text_align_text(const char *text, ErUiTextAlign *out_align) {
    if (!text || !out_align) {
        return false;
    }

    if (strcmp(text, "left") == 0 || strcmp(text, "start") == 0) {
        *out_align = ER_UI_TEXT_ALIGN_START;
        return true;
    }
    if (strcmp(text, "center") == 0 || strcmp(text, "middle") == 0) {
        *out_align = ER_UI_TEXT_ALIGN_CENTER;
        return true;
    }
    if (strcmp(text, "right") == 0 || strcmp(text, "end") == 0) {
        *out_align = ER_UI_TEXT_ALIGN_END;
        return true;
    }

    return false;
}

static bool er_runtime_compare_values(
    ErRuntime *runtime,
    const ErCondition *condition,
    bool *out_result,
    ErError *error
) {
    ErValue left;
    ErValue right;
    double left_num = 0.0;
    double right_num = 0.0;
    bool numeric = false;
    int cmp = 0;
    char *left_text = NULL;
    char *right_text = NULL;
    bool result = false;

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));

    if (!er_runtime_evaluate_value(runtime, &condition->left, &left, error) ||
        !er_runtime_evaluate_value(runtime, &condition->right, &right, error)) {
        er_value_free(&left);
        er_value_free(&right);
        return false;
    }

    if ((left.type == ER_VALUE_NUMBER || left.type == ER_VALUE_BOOL) &&
        (right.type == ER_VALUE_NUMBER || right.type == ER_VALUE_BOOL)) {
        left_num = left.type == ER_VALUE_NUMBER ? left.as.number : (left.as.boolean ? 1.0 : 0.0);
        right_num = right.type == ER_VALUE_NUMBER ? right.as.number : (right.as.boolean ? 1.0 : 0.0);
        numeric = true;
    }

    if (numeric) {
        switch (condition->op) {
            case ER_COMPARE_EQ: result = left_num == right_num; break;
            case ER_COMPARE_NEQ: result = left_num != right_num; break;
            case ER_COMPARE_LT: result = left_num < right_num; break;
            case ER_COMPARE_LTE: result = left_num <= right_num; break;
            case ER_COMPARE_GT: result = left_num > right_num; break;
            case ER_COMPARE_GTE: result = left_num >= right_num; break;
        }
        er_value_free(&left);
        er_value_free(&right);
        *out_result = result;
        return true;
    }

    if (!er_runtime_value_to_string(runtime, &left, &left_text, error) ||
        !er_runtime_value_to_string(runtime, &right, &right_text, error)) {
        free(left_text);
        free(right_text);
        er_value_free(&left);
        er_value_free(&right);
        return false;
    }
    cmp = strcmp(left_text ? left_text : "", right_text ? right_text : "");

    switch (condition->op) {
        case ER_COMPARE_EQ: result = cmp == 0; break;
        case ER_COMPARE_NEQ: result = cmp != 0; break;
        case ER_COMPARE_LT: result = cmp < 0; break;
        case ER_COMPARE_LTE: result = cmp <= 0; break;
        case ER_COMPARE_GT: result = cmp > 0; break;
        case ER_COMPARE_GTE: result = cmp >= 0; break;
    }

    free(left_text);
    free(right_text);
    er_value_free(&left);
    er_value_free(&right);
    *out_result = result;
    return true;
}

static bool er_runtime_grow_bindings(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErEventBinding **new_bindings;

    if (runtime->binding_count < runtime->binding_capacity) {
        return true;
    }

    new_capacity = runtime->binding_capacity == 0 ? 8 : runtime->binding_capacity * 2;
    new_bindings = (ErEventBinding **) realloc(runtime->bindings, new_capacity * sizeof(ErEventBinding *));
    if (!new_bindings) {
        er_error_set(error, 0, 0, "Out of memory while growing event binding table");
        return false;
    }

    runtime->bindings = new_bindings;
    runtime->binding_capacity = new_capacity;
    return true;
}

static ErEventBinding *er_runtime_make_binding(
    ErRuntime *runtime,
    ErStatementArray *body,
    const char *bound_variable,
    ErError *error
) {
    ErEventBinding *binding;

    if (!er_runtime_grow_bindings(runtime, error)) {
        return NULL;
    }

    binding = (ErEventBinding *) calloc(1, sizeof(ErEventBinding));
    if (!binding) {
        er_error_set(error, 0, 0, "Out of memory while creating event binding");
        return NULL;
    }

    binding->runtime = runtime;
    binding->body = body;
    binding->bound_variable = er_rt_dup(bound_variable);
    runtime->bindings[runtime->binding_count++] = binding;
    return binding;
}

static bool er_runtime_grow_timers(ErRuntime *runtime, ErError *error) {
    size_t new_capacity;
    ErRuntimeTimer *new_timers;

    if (runtime->timer_count < runtime->timer_capacity) {
        return true;
    }

    new_capacity = runtime->timer_capacity == 0 ? 8 : runtime->timer_capacity * 2;
    new_timers = (ErRuntimeTimer *) realloc(runtime->timers, new_capacity * sizeof(ErRuntimeTimer));
    if (!new_timers) {
        er_error_set(error, 0, 0, "Out of memory while growing runtime timer table");
        return false;
    }

    runtime->timers = new_timers;
    runtime->timer_capacity = new_capacity;
    return true;
}

static ErRuntimeTimer *er_runtime_find_timer(ErRuntime *runtime, const char *name) {
    size_t i;

    if (!runtime || !name) {
        return NULL;
    }

    for (i = 0; i < runtime->timer_count; ++i) {
        if (runtime->timers[i].name && strcmp(runtime->timers[i].name, name) == 0) {
            return &runtime->timers[i];
        }
    }

    return NULL;
}

static bool er_runtime_execute_statement(ErRuntime *runtime, ErStatement *statement, ErError *error);

static bool er_runtime_execute_block(ErRuntime *runtime, ErStatementArray *body, ErError *error) {
    size_t i;

    if (!body) {
        return true;
    }

    for (i = 0; i < body->count; ++i) {
        if (!er_runtime_execute_statement(runtime, body->items[i], error)) {
            return false;
        }
    }
    return true;
}

static void er_runtime_store_ui_number(ErRuntime *runtime, const char *name, double number) {
    ErValue value;
    ErError ignored;

    if (!runtime || !name || name[0] == '\0') {
        return;
    }

    er_error_clear(&ignored);
    value = er_value_make_number(number);
    er_runtime_set_variable(runtime, name, &value, &ignored);
    er_value_free(&value);
}

static void er_runtime_on_click(ErUiApp *app, ErUiNode *node, void *user_data) {
    ErEventBinding *binding = (ErEventBinding *) user_data;
    ErError error;

    if (!binding || !binding->runtime) {
        return;
    }

    if (binding && binding->runtime) {
        int click_x = app ? app->last_click_x : 0;
        int click_y = app ? app->last_click_y : 0;
        int node_x = node ? node->x : 0;
        int node_y = node ? node->y : 0;
        int node_w = node ? node->w : 0;
        int node_h = node ? node->h : 0;

        er_runtime_store_ui_number(binding->runtime, "ui_click_x", (double) click_x);
        er_runtime_store_ui_number(binding->runtime, "ui_click_y", (double) click_y);
        er_runtime_store_ui_number(binding->runtime, "ui_click_local_x", (double) (click_x - node_x));
        er_runtime_store_ui_number(binding->runtime, "ui_click_local_y", (double) (click_y - node_y));
        er_runtime_store_ui_number(binding->runtime, "ui_target_x", (double) node_x);
        er_runtime_store_ui_number(binding->runtime, "ui_target_y", (double) node_y);
        er_runtime_store_ui_number(binding->runtime, "ui_target_w", (double) node_w);
        er_runtime_store_ui_number(binding->runtime, "ui_target_h", (double) node_h);
    }

    er_error_clear(&error);
    if (!er_runtime_execute_block(binding->runtime, binding->body, &error)) {
#ifdef _WIN32
        MessageBoxA(NULL, error.message, "Erire Runtime Error", MB_ICONERROR | MB_OK);
#endif
    }
}

static void er_runtime_on_change(ErUiApp *app, ErUiNode *node, void *user_data) {
    ErEventBinding *binding = (ErEventBinding *) user_data;
    ErError error;
    char *text;
    ErValue value;
    (void) app;

    if (!binding || !binding->runtime) {
        return;
    }

    er_error_clear(&error);

    if (binding->bound_variable) {
        text = er_ui_node_dup_text(node);
        if (text) {
            value = er_value_make_string(text);
            er_runtime_set_variable(binding->runtime, binding->bound_variable, &value, &error);
            er_value_free(&value);
            free(text);
        }
    }

    if (!er_runtime_execute_block(binding->runtime, binding->body, &error)) {
#ifdef _WIN32
        MessageBoxA(NULL, error.message, "Erire Runtime Error", MB_ICONERROR | MB_OK);
#endif
    }
}

static void er_runtime_on_timer(ErUiApp *app, unsigned int timer_id, void *user_data) {
    ErEventBinding *binding = (ErEventBinding *) user_data;
    ErError error;
    (void) app;
    (void) timer_id;
    if (!binding || !binding->runtime) {
        return;
    }
    er_error_clear(&error);
    if (!er_runtime_execute_block(binding->runtime, binding->body, &error)) {
#ifdef _WIN32
        MessageBoxA(NULL, error.message, "Erire Runtime Error", MB_ICONERROR | MB_OK);
#endif
    }
}

static void er_runtime_on_media_sync_timer(ErUiApp *app, unsigned int timer_id, void *user_data) {
    ErRuntime *runtime = (ErRuntime *) user_data;
    (void) app;
    (void) timer_id;
    er_media_player_sync(&runtime->media);
}

static bool er_runtime_register_timer(
    ErRuntime *runtime,
    const char *name,
    double interval_value,
    ErEventBinding *binding,
    ErError *error
) {
    ErRuntimeTimer *timer;
    unsigned int interval_ms;

    if (!runtime || !name || name[0] == '\0' || !binding) {
        er_error_set(error, 0, 0, "Runtime timer registration requires a timer name and block");
        return false;
    }

    if (interval_value <= 0.0) {
        er_error_set(error, 0, 0, "Timer interval must be greater than zero");
        return false;
    }

    if (!runtime->app_initialized && !er_runtime_ensure_app(runtime, error)) {
        return false;
    }

    interval_ms = (unsigned int) interval_value;
    timer = er_runtime_find_timer(runtime, name);
    if (!timer) {
        if (!er_runtime_grow_timers(runtime, error)) {
            return false;
        }
        timer = &runtime->timers[runtime->timer_count++];
        memset(timer, 0, sizeof(*timer));
        timer->name = er_rt_dup(name);
        if (!timer->name) {
            er_error_set(error, 0, 0, "Out of memory while storing timer name");
            return false;
        }
        timer->timer_id = runtime->next_runtime_timer_id++;
    }

    if (timer->binding && timer->binding != binding) {
        er_runtime_destroy_binding(runtime, timer->binding);
    }
    timer->binding = binding;
    return er_ui_app_set_timer(&runtime->app, timer->timer_id, interval_ms, er_runtime_on_timer, binding, error);
}

static void er_runtime_stop_timer(ErRuntime *runtime, const char *name) {
    size_t i;

    if (!runtime || !name) {
        return;
    }

    for (i = 0; i < runtime->timer_count; ++i) {
        if (runtime->timers[i].name && strcmp(runtime->timers[i].name, name) == 0) {
            er_ui_app_clear_timer(&runtime->app, runtime->timers[i].timer_id);
            er_runtime_destroy_binding(runtime, runtime->timers[i].binding);
            free(runtime->timers[i].name);
            if (i + 1 < runtime->timer_count) {
                runtime->timers[i] = runtime->timers[runtime->timer_count - 1];
            }
            runtime->timer_count--;
            return;
        }
    }
}

static const ErProperty *er_runtime_find_property(const ErElement *element, const char *name) {
    size_t i;
    for (i = 0; i < element->properties.count; ++i) {
        if (strcmp(element->properties.items[i].name, name) == 0) {
            return &element->properties.items[i];
        }
    }
    return NULL;
}

static bool er_runtime_value_to_int(
    ErRuntime *runtime,
    const ErValue *value,
    int fallback,
    int *out_int,
    ErError *error
) {
    double number;
    if (!er_runtime_value_to_number(runtime, value, &number, error)) {
        *out_int = fallback;
        return !er_error_has(error);
    }
    *out_int = (int) number;
    return true;
}

static void er_runtime_release_node_bindings(ErRuntime *runtime, ErUiNode *node) {
    ErEventBinding *click_binding = NULL;
    ErEventBinding *change_binding = NULL;

    if (!runtime || !node) {
        return;
    }

    if (node->on_click == er_runtime_on_click) {
        click_binding = (ErEventBinding *) node->on_click_user_data;
    }
    if (node->on_change == er_runtime_on_change) {
        change_binding = (ErEventBinding *) node->on_change_user_data;
    }

    if (click_binding) {
        er_runtime_destroy_binding(runtime, click_binding);
    }
    if (change_binding && change_binding != click_binding) {
        er_runtime_destroy_binding(runtime, change_binding);
    }
}

static bool er_runtime_element_get_id(ErRuntime *runtime, const ErElement *element, char **out_id, ErError *error) {
    const ErProperty *id_property;

    *out_id = NULL;

    if (!runtime || !element || !out_id) {
        er_error_set(error, 0, 0, "Live element id lookup requires a valid element");
        return false;
    }

    id_property = er_runtime_find_property(element, "id");
    if (!id_property || id_property->values.count == 0) {
        return true;
    }

    return er_runtime_value_to_string(runtime, &id_property->values.items[0], out_id, error);
}

static bool er_runtime_live_enable(
    ErRuntime *runtime,
    const char *entry_path,
    unsigned int poll_ms,
    ErError *error
) {
    char path_buffer[1024];

    if (!runtime || !entry_path || entry_path[0] == '\0') {
        er_error_set(error, 0, 0, "Live runtime requires a valid entry file path");
        return false;
    }

    runtime->live.enabled = true;
    runtime->live.poll_ms = poll_ms > 0 ? poll_ms : 300u;
    runtime->live.entry_path = er_rt_dup(entry_path);
    if (!runtime->live.entry_path) {
        er_error_set(error, 0, 0, "Out of memory while enabling live runtime");
        return false;
    }

    if (runtime->source_dir && runtime->source_dir[0] != '\0') {
        er_path_join(runtime->source_dir, "backend.er", path_buffer, sizeof(path_buffer));
        runtime->live.backend_path = er_rt_dup(path_buffer);
        if (!runtime->live.backend_path) {
            er_error_set(error, 0, 0, "Out of memory while enabling live backend watcher");
            return false;
        }

        er_path_join(runtime->source_dir, "app.json", path_buffer, sizeof(path_buffer));
        runtime->live.app_json_path = er_rt_dup(path_buffer);
        if (!runtime->live.app_json_path) {
            er_error_set(error, 0, 0, "Out of memory while enabling live app.json watcher");
            return false;
        }
    }

    er_runtime_live_refresh_file_stamps(runtime);
    er_runtime_live_set_status_text(runtime, "live patch idle");
    er_runtime_live_set_last_error_text(runtime, "");
    er_runtime_live_publish_state(runtime);
    return true;
}

static bool er_runtime_live_snapshot_initial_state(
    ErRuntime *runtime,
    const ErProgram *program,
    ErError *error
) {
    size_t i;

    if (!runtime || !program) {
        return false;
    }

    for (i = 0; i < program->statements.count; ++i) {
        ErStatement *statement = program->statements.items[i];

        if (!statement) {
            continue;
        }

        if (statement->type == ER_STMT_SCREEN_ADD &&
            er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE) {
            char *id_text = NULL;

            if (!er_runtime_element_get_id(runtime, statement->as.add.element, &id_text, error)) {
                return false;
            }
            if (id_text && id_text[0] != '\0' && !er_runtime_live_touch_node(runtime, id_text, error)) {
                free(id_text);
                return false;
            }
            free(id_text);
        } else if (statement->type == ER_STMT_TIMER_EVERY) {
            if (!er_runtime_live_touch_timer(runtime, statement->as.timer_stmt.name, error)) {
                return false;
            }
        }
    }

    return true;
}

static bool er_runtime_add_element(ErRuntime *runtime, const ErElement *element, ErError *error) {
    ErUiNodeSpec spec;
    ErUiNode *node;
    size_t i;
    char *text_value = NULL;
    char *hint_value = NULL;
    char *id_value = NULL;
    char *page_value = NULL;
    char *preset_value = NULL;
    unsigned int rgb;
    const ErProperty *on_load = NULL;
    const ErProperty *on_click = NULL;
    const ErProperty *on_change = NULL;
    const ErProperty *bind_property = NULL;
    char *bind_name = NULL;
    bool ok = false;

    memset(&spec, 0, sizeof(spec));
    spec.x = 20;
    spec.y = 20;
    spec.w = 180;
    spec.h = 42;
    spec.font_size = 16;
    spec.padding = -1;
    spec.border_width = -1;
    spec.border_radius = -1;
    spec.shadow_size = -1;
    spec.surface_style = ER_UI_SURFACE_STYLE_DEFAULT;
    spec.text_align = ER_UI_TEXT_ALIGN_DEFAULT;
    spec.image_fit = ER_UI_IMAGE_FIT_CONTAIN;

    if (strcmp(element->kind, "text") == 0 || strcmp(element->kind, "label") == 0) {
        spec.kind = ER_UI_NODE_TEXT;
        spec.h = 28;
    } else if (strcmp(element->kind, "btn") == 0 || strcmp(element->kind, "button") == 0) {
        spec.kind = ER_UI_NODE_BUTTON;
    } else if (strcmp(element->kind, "input") == 0) {
        spec.kind = ER_UI_NODE_INPUT;
    } else if (strcmp(element->kind, "image") == 0) {
        spec.kind = ER_UI_NODE_IMAGE;
    } else if (strcmp(element->kind, "webview") == 0) {
        spec.kind = ER_UI_NODE_WEBVIEW;
    } else if (strcmp(element->kind, "box") == 0 || strcmp(element->kind, "card") == 0 || strcmp(element->kind, "panel") == 0) {
        spec.kind = ER_UI_NODE_BOX;
        if (strcmp(element->kind, "card") == 0) {
            spec.surface_style = ER_UI_SURFACE_STYLE_CARD;
        } else if (strcmp(element->kind, "panel") == 0) {
            spec.surface_style = ER_UI_SURFACE_STYLE_PANEL;
        } else {
            spec.surface_style = ER_UI_SURFACE_STYLE_BOX;
        }
    } else {
        er_error_set(error, 0, 0, "Unknown UI element kind: %s", element->kind);
        goto cleanup;
    }

    if (element->base_property && element->base_args.count > 0) {
        if (strcmp(element->base_property, "text") == 0 || strcmp(element->base_property, "value") == 0) {
            if (!er_runtime_value_to_string(runtime, &element->base_args.items[0], &text_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(element->base_property, "placeholder") == 0) {
            if (!er_runtime_value_to_string(runtime, &element->base_args.items[0], &hint_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(element->base_property, "url") == 0) {
            free(text_value);
            text_value = NULL;
            if (!er_runtime_value_to_string(runtime, &element->base_args.items[0], &text_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(element->base_property, "src") == 0) {
            free((char *) spec.asset_path);
            spec.asset_path = NULL;
            if (!er_runtime_value_to_string(runtime, &element->base_args.items[0], (char **) &spec.asset_path, error)) {
                goto cleanup;
            }
        }
    }

    on_load = er_runtime_find_property(element, "onLoad");
    on_click = er_runtime_find_property(element, "onClick");
    on_change = er_runtime_find_property(element, "onChange");
    bind_property = er_runtime_find_property(element, "bind");

    if (bind_property && bind_property->values.count > 0) {
        if (!er_runtime_value_to_string(runtime, &bind_property->values.items[0], &bind_name, error)) {
            goto cleanup;
        }
        if (!bind_name || bind_name[0] == '\0') {
            er_error_set(error, 0, 0, "Input bind property cannot be empty");
            goto cleanup;
        }
    }

    for (i = 0; i < element->properties.count; ++i) {
        const ErProperty *property = &element->properties.items[i];

        if (property->is_event || property->values.count == 0) {
            continue;
        }

        if (strcmp(property->name, "x") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.x, &spec.x, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "y") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.y, &spec.y, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "w") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.w, &spec.w, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "h") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.h, &spec.h, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "size") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.font_size, &spec.font_size, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "id") == 0) {
            free(id_value);
            id_value = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &id_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "page") == 0) {
            free(page_value);
            page_value = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &page_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "preset") == 0) {
            free(preset_value);
            preset_value = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &preset_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "text") == 0 || strcmp(property->name, "value") == 0) {
            free(text_value);
            text_value = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &text_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "placeholder") == 0) {
            free(hint_value);
            hint_value = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &hint_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "url") == 0) {
            free(text_value);
            text_value = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &text_value, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "src") == 0) {
            free((char *) spec.asset_path);
            spec.asset_path = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], (char **) &spec.asset_path, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "icon") == 0) {
            free((char *) spec.icon_path);
            spec.icon_path = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], (char **) &spec.icon_path, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "iconSize") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.icon_size, &spec.icon_size, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "multiline") == 0) {
            if (!er_runtime_value_to_flag(runtime, &property->values.items[0], &spec.multiline, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "readonly") == 0 || strcmp(property->name, "readOnly") == 0) {
            if (!er_runtime_value_to_flag(runtime, &property->values.items[0], &spec.read_only, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "fit") == 0) {
            char *fit_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &fit_text, error)) {
                goto cleanup;
            }
            if (fit_text) {
                if (strcmp(fit_text, "contain") == 0) {
                    spec.image_fit = ER_UI_IMAGE_FIT_CONTAIN;
                } else if (strcmp(fit_text, "cover") == 0) {
                    spec.image_fit = ER_UI_IMAGE_FIT_COVER;
                } else if (strcmp(fit_text, "stretch") == 0) {
                    spec.image_fit = ER_UI_IMAGE_FIT_STRETCH;
                } else if (strcmp(fit_text, "center") == 0) {
                    spec.image_fit = ER_UI_IMAGE_FIT_CENTER;
                }
            }
            free(fit_text);
        } else if (strcmp(property->name, "color") == 0) {
            char *color_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &color_text, error)) {
                goto cleanup;
            }
            if (er_runtime_parse_color_text(color_text, &rgb)) {
                spec.has_text_color = true;
                spec.text_color_rgb = rgb;
            }
            free(color_text);
        } else if (strcmp(property->name, "bg") == 0) {
            char *color_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &color_text, error)) {
                goto cleanup;
            }
            if (er_runtime_parse_color_text(color_text, &rgb)) {
                spec.has_bg_color = true;
                spec.bg_color_rgb = rgb;
            }
            free(color_text);
        } else if (strcmp(property->name, "bg2") == 0) {
            char *color_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &color_text, error)) {
                goto cleanup;
            }
            if (er_runtime_parse_color_text(color_text, &rgb)) {
                spec.has_bg_alt_color = true;
                spec.bg_alt_color_rgb = rgb;
            }
            free(color_text);
        } else if (strcmp(property->name, "borderColor") == 0) {
            char *color_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &color_text, error)) {
                goto cleanup;
            }
            if (er_runtime_parse_color_text(color_text, &rgb)) {
                spec.has_border_color = true;
                spec.border_color_rgb = rgb;
            }
            free(color_text);
        } else if (strcmp(property->name, "padding") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.padding, &spec.padding, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "border") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.border_width, &spec.border_width, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "radius") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.border_radius, &spec.border_radius, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "shadow") == 0) {
            if (!er_runtime_value_to_int(runtime, &property->values.items[0], spec.shadow_size, &spec.shadow_size, error)) {
                goto cleanup;
            }
        } else if (strcmp(property->name, "shadowColor") == 0) {
            char *color_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &color_text, error)) {
                goto cleanup;
            }
            if (er_runtime_parse_color_text(color_text, &rgb)) {
                spec.has_shadow_color = true;
                spec.shadow_color_rgb = rgb;
            }
            free(color_text);
        } else if (strcmp(property->name, "align") == 0) {
            char *align_text = NULL;
            if (!er_runtime_value_to_string(runtime, &property->values.items[0], &align_text, error)) {
                goto cleanup;
            }
            er_runtime_parse_text_align_text(align_text, &spec.text_align);
            free(align_text);
        }
    }

    spec.id = id_value;
    spec.page = page_value;
    spec.text = text_value ? text_value : "";
    spec.hint = hint_value;
    spec.preset = preset_value;

    if (!er_runtime_ensure_app(runtime, error)) {
        goto cleanup;
    }

    if (on_click) {
        ErEventBinding *binding = er_runtime_make_binding(runtime, (ErStatementArray *) &on_click->block, NULL, error);
        if (!binding) {
            goto cleanup;
        }
        spec.on_click = er_runtime_on_click;
        spec.on_click_user_data = binding;
    }

    if (on_change || (spec.kind == ER_UI_NODE_INPUT && bind_name)) {
        ErStatementArray *on_change_body = on_change ? (ErStatementArray *) &on_change->block : NULL;
        ErEventBinding *binding = er_runtime_make_binding(runtime, on_change_body, bind_name, error);
        if (!binding) {
            goto cleanup;
        }
        spec.on_change = er_runtime_on_change;
        spec.on_change_user_data = binding;
    }

    node = er_ui_app_add_node(&runtime->app, &spec, error);
    if (!node) {
        goto cleanup;
    }

    if (spec.kind == ER_UI_NODE_INPUT && bind_name) {
        ErValue initial_value = er_value_make_string(spec.text ? spec.text : "");
        if (!er_runtime_set_variable(runtime, bind_name, &initial_value, error)) {
            er_value_free(&initial_value);
            goto cleanup;
        }
        er_value_free(&initial_value);
    }

    if (on_load) {
        if (runtime->defer_on_load) {
            if (!er_runtime_queue_on_load(runtime, (ErStatementArray *) &on_load->block, error)) {
                goto cleanup;
            }
        } else if (!er_runtime_execute_block(runtime, (ErStatementArray *) &on_load->block, error)) {
            goto cleanup;
        }
    }

    ok = true;

cleanup:
    free(text_value);
    free(hint_value);
    free(id_value);
    free(page_value);
    free(preset_value);
    free(bind_name);
    free((char *) spec.asset_path);
    free((char *) spec.icon_path);
    return ok;
}

static bool er_runtime_apply_screen_create(ErRuntime *runtime, const ErCommand *command, ErError *error) {
    if (command->args.count >= 6) {
        if (!er_runtime_value_to_int(runtime, &command->args.items[2], runtime->x, &runtime->x, error) ||
            !er_runtime_value_to_int(runtime, &command->args.items[3], runtime->y, &runtime->y, error) ||
            !er_runtime_value_to_int(runtime, &command->args.items[4], runtime->w, &runtime->w, error) ||
            !er_runtime_value_to_int(runtime, &command->args.items[5], runtime->h, &runtime->h, error)) {
            return false;
        }
        return true;
    }
    if (command->args.count >= 4) {
        if (!er_runtime_value_to_int(runtime, &command->args.items[0], runtime->x, &runtime->x, error) ||
            !er_runtime_value_to_int(runtime, &command->args.items[1], runtime->y, &runtime->y, error) ||
            !er_runtime_value_to_int(runtime, &command->args.items[2], runtime->w, &runtime->w, error) ||
            !er_runtime_value_to_int(runtime, &command->args.items[3], runtime->h, &runtime->h, error)) {
            return false;
        }
        return true;
    }
    return true;
}

static bool er_runtime_execute_statement(ErRuntime *runtime, ErStatement *statement, ErError *error) {
    char *text = NULL;
    ErValue resolved;
    unsigned int rgb;
    bool condition = false;

    /* Execution is strictly source-order. Each statement mutates runtime state
       immediately so event blocks observe prior state changes deterministically. */
    memset(&resolved, 0, sizeof(resolved));

    switch (statement->type) {
        case ER_STMT_IMPORT:
            if (statement->as.import_directive.type == ER_IMPORT_DIRECTIVE_PYTHON) {
                return er_runtime_register_python_import(
                    runtime,
                    statement->as.import_directive.alias,
                    statement->as.import_directive.path,
                    error
                );
            }
            return true;
        case ER_STMT_SCREEN_CREATE:
            return er_runtime_apply_screen_create(runtime, &statement->as.command, error);
        case ER_STMT_SCREEN_TITLE:
            if (statement->as.command.args.count > 0) {
                if (runtime->title_locked) {
                    return true;
                }
                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &text, error)) {
                    return false;
                }
                free(runtime->title);
                runtime->title = er_rt_dup(text ? text : "Erire App");
                if (runtime->app_initialized) {
                    er_ui_app_set_title(&runtime->app, runtime->title);
                }
                free(text);
            }
            return true;
        case ER_STMT_SCREEN_BG:
            if (statement->as.command.args.count > 0) {
                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &text, error)) {
                    return false;
                }
                if (er_runtime_parse_color_text(text, &rgb)) {
                    runtime->background_rgb = rgb;
                    if (runtime->app_initialized) {
                        er_ui_app_set_background(&runtime->app, rgb);
                    }
                }
                free(text);
            }
            return true;
        case ER_STMT_SCREEN_ADD:
            return er_runtime_add_element(runtime, statement->as.add.element, error);
        case ER_STMT_SCREEN_SHOW:
            if (statement->as.command.args.count > 0) {
                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &text, error)) {
                    return false;
                }
                free(runtime->current_page);
                runtime->current_page = er_rt_dup(text ? text : "main");
                if (runtime->app_initialized) {
                    er_ui_app_show_page(&runtime->app, runtime->current_page);
                }
                free(text);
            }
            return true;
        case ER_STMT_SCREEN_SETTEXT:
            if (!er_runtime_value_to_string(runtime, &statement->as.set_text.element_id, &text, error)) {
                return false;
            }
            if (!er_runtime_evaluate_value(runtime, &statement->as.set_text.value, &resolved, error)) {
                free(text);
                return false;
            }
            {
                char *value_text = NULL;
                if (!er_runtime_value_to_string(runtime, &resolved, &value_text, error)) {
                    free(text);
                    er_value_free(&resolved);
                    return false;
                }
                if (!runtime->app_initialized || !er_ui_app_set_text(&runtime->app, text, value_text)) {
                    er_error_set(error, statement->line, statement->column, "Could not set text for element '%s'", text);
                    free(text);
                    free(value_text);
                    er_value_free(&resolved);
                    return false;
                }
                free(value_text);
            }
            free(text);
            er_value_free(&resolved);
            return true;
        case ER_STMT_VAR_SET:
            if (statement->as.var_decl.value.type == ER_VALUE_CALL &&
                statement->as.var_decl.value.as.call &&
                strcmp(statement->as.var_decl.value.as.call->name, "py.call") == 0) {
                ErRuntimePyResult result;
                if (!er_runtime_execute_python_call(runtime, statement->as.var_decl.value.as.call, &result, error)) {
                    return false;
                }
                if (!er_runtime_set_variable(runtime, statement->as.var_decl.name, &result.value, error)) {
                    er_value_free(&result.value);
                    free(result.json);
                    return false;
                }
                if (result.structured &&
                    !er_runtime_store_structured_json(runtime, statement->as.var_decl.name, result.json, error)) {
                    er_value_free(&result.value);
                    free(result.json);
                    return false;
                }
                er_value_free(&result.value);
                free(result.json);
                return true;
            }

            if (!er_runtime_evaluate_value(runtime, &statement->as.var_decl.value, &resolved, error)) {
                return false;
            }
            if (!er_runtime_set_variable(runtime, statement->as.var_decl.name, &resolved, error)) {
                er_value_free(&resolved);
                return false;
            }
            er_value_free(&resolved);
            return true;
        case ER_STMT_IF:
            if (!er_runtime_compare_values(runtime, &statement->as.if_stmt.condition, &condition, error)) {
                return false;
            }
            if (condition) {
                return er_runtime_execute_block(runtime, &statement->as.if_stmt.body, error);
            }
            {
                size_t else_if_index;
                for (else_if_index = 0; else_if_index < statement->as.if_stmt.else_ifs.count; ++else_if_index) {
                    if (!er_runtime_compare_values(
                            runtime,
                            &statement->as.if_stmt.else_ifs.items[else_if_index].condition,
                            &condition,
                            error
                        )) {
                        return false;
                    }
                    if (condition) {
                        return er_runtime_execute_block(runtime, &statement->as.if_stmt.else_ifs.items[else_if_index].body, error);
                    }
                }
            }
            if (statement->as.if_stmt.has_else) {
                return er_runtime_execute_block(runtime, &statement->as.if_stmt.else_body, error);
            }
            return true;
        case ER_STMT_WHILE: {
            size_t guard = 0;

            for (;;) {
                if (!er_runtime_compare_values(runtime, &statement->as.while_stmt.condition, &condition, error)) {
                    return false;
                }
                if (!condition) {
                    return true;
                }
                if (++guard > 1000000u) {
                    er_error_set(error, statement->line, statement->column, "while loop exceeded the safety iteration limit");
                    return false;
                }
                if (!er_runtime_execute_block(runtime, &statement->as.while_stmt.body, error)) {
                    return false;
                }
            }
        }
        case ER_STMT_FOR: {
            double start = 0.0;
            double end = 0.0;
            double step = 1.0;
            double value_number;
            double step_magnitude;
            size_t guard = 0;
            ErValue iterator_value;

            memset(&iterator_value, 0, sizeof(iterator_value));

            if (!er_runtime_value_to_number(runtime, &statement->as.for_stmt.start, &start, error) ||
                !er_runtime_value_to_number(runtime, &statement->as.for_stmt.end, &end, error)) {
                er_error_set(error, statement->line, statement->column, "for loop bounds must be numeric");
                return false;
            }
            if (statement->as.for_stmt.has_step &&
                !er_runtime_value_to_number(runtime, &statement->as.for_stmt.step, &step, error)) {
                er_error_set(error, statement->line, statement->column, "for loop step must be numeric");
                return false;
            }

            if (step == 0.0) {
                er_error_set(error, statement->line, statement->column, "for loop step cannot be zero");
                return false;
            }

            step_magnitude = step < 0.0 ? -step : step;
            if (step_magnitude == 0.0) {
                step_magnitude = 1.0;
            }

            if (start <= end) {
                for (value_number = start; value_number <= end; value_number += step_magnitude) {
                    if (++guard > 1000000u) {
                        er_error_set(error, statement->line, statement->column, "for loop exceeded the safety iteration limit");
                        return false;
                    }
                    iterator_value = er_value_make_number(value_number);
                    if (!er_runtime_set_variable(runtime, statement->as.for_stmt.iterator_name, &iterator_value, error)) {
                        er_value_free(&iterator_value);
                        return false;
                    }
                    er_value_free(&iterator_value);
                    if (!er_runtime_execute_block(runtime, &statement->as.for_stmt.body, error)) {
                        return false;
                    }
                }
            } else {
                for (value_number = start; value_number >= end; value_number -= step_magnitude) {
                    if (++guard > 1000000u) {
                        er_error_set(error, statement->line, statement->column, "for loop exceeded the safety iteration limit");
                        return false;
                    }
                    iterator_value = er_value_make_number(value_number);
                    if (!er_runtime_set_variable(runtime, statement->as.for_stmt.iterator_name, &iterator_value, error)) {
                        er_value_free(&iterator_value);
                        return false;
                    }
                    er_value_free(&iterator_value);
                    if (!er_runtime_execute_block(runtime, &statement->as.for_stmt.body, error)) {
                        return false;
                    }
                }
            }
            return true;
        }
        case ER_STMT_TIMER_EVERY: {
            double interval_value = 0.0;
            ErEventBinding *binding;

            if (!er_runtime_value_to_number(runtime, &statement->as.timer_stmt.interval, &interval_value, error)) {
                er_error_set(error, statement->line, statement->column, "timer.every interval must be numeric");
                return false;
            }

            binding = er_runtime_make_binding(runtime, &statement->as.timer_stmt.body, NULL, error);
            if (!binding) {
                return false;
            }

            return er_runtime_register_timer(runtime, statement->as.timer_stmt.name, interval_value, binding, error);
        }
        case ER_STMT_GENERIC:
            if (!statement->as.command.name) {
                return true;
            }
            if (strcmp(statement->as.command.name, "screen.setImage") == 0) {
                char *id_text = NULL;
                char *path_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &path_text, error)) {
                    free(id_text);
                    free(path_text);
                    return false;
                }

                ok = runtime->app_initialized && er_ui_app_set_image(&runtime->app, id_text, path_text, error);
                free(id_text);
                free(path_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not update image node");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "screen.setBounds") == 0) {
                char *id_text = NULL;
                int x = 0;
                int y = 0;
                int w = 0;
                int h = 0;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_int(runtime, &statement->as.command.args.items[1], 0, &x, error) ||
                    !er_runtime_value_to_int(runtime, &statement->as.command.args.items[2], 0, &y, error) ||
                    !er_runtime_value_to_int(runtime, &statement->as.command.args.items[3], 1, &w, error) ||
                    !er_runtime_value_to_int(runtime, &statement->as.command.args.items[4], 1, &h, error)) {
                    free(id_text);
                    return false;
                }

                ok = runtime->app_initialized && er_ui_app_set_bounds(&runtime->app, id_text, x, y, w, h, error);
                free(id_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not update node bounds");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "storage.write") == 0) {
                char *key_text = NULL;
                char *value_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &key_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &value_text, error)) {
                    free(key_text);
                    free(value_text);
                    return false;
                }

                ok = er_runtime_storage_write_text(runtime, key_text ? key_text : "", value_text ? value_text : "", error);
                free(key_text);
                free(value_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not write storage value");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "shortcut.bind") == 0) {
                char *combo_text = NULL;
                char *target_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &combo_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &target_text, error)) {
                    free(combo_text);
                    free(target_text);
                    return false;
                }

                ok = runtime->app_initialized &&
                     er_ui_app_add_shortcut(&runtime->app, combo_text ? combo_text : "", target_text ? target_text : "", error);
                free(combo_text);
                free(target_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not register shortcut");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "webview.open") == 0) {
                char *id_text = NULL;
                char *url_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &url_text, error)) {
                    free(id_text);
                    free(url_text);
                    return false;
                }

                ok = runtime->app_initialized && er_ui_app_webview_navigate(&runtime->app, id_text, url_text);
                free(id_text);
                free(url_text);
                if (!ok) {
                    er_error_set(error, statement->line, statement->column, "Could not navigate webview");
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "webview.back") == 0 ||
                strcmp(statement->as.command.name, "webview.forward") == 0 ||
                strcmp(statement->as.command.name, "webview.reload") == 0) {
                char *id_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error)) {
                    return false;
                }

                if (strcmp(statement->as.command.name, "webview.back") == 0) {
                    ok = runtime->app_initialized && er_ui_app_webview_back(&runtime->app, id_text);
                } else if (strcmp(statement->as.command.name, "webview.forward") == 0) {
                    ok = runtime->app_initialized && er_ui_app_webview_forward(&runtime->app, id_text);
                } else {
                    ok = runtime->app_initialized && er_ui_app_webview_reload(&runtime->app, id_text);
                }

                free(id_text);
                if (!ok) {
                    er_error_set(error, statement->line, statement->column, "Could not execute webview navigation command");
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "webview.runJS") == 0) {
                char *id_text = NULL;
                char *script_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &script_text, error)) {
                    free(id_text);
                    free(script_text);
                    return false;
                }

                ok = runtime->app_initialized && er_ui_app_webview_run_script(&runtime->app, id_text, script_text);
                free(id_text);
                free(script_text);
                if (!ok) {
                    er_error_set(error, statement->line, statement->column, "Could not execute JavaScript in webview");
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "anim.play") == 0) {
                char *id_text = NULL;
                char *preset_text = NULL;
                char *loop_text = NULL;
                double duration_value = 1800.0;
                bool ok = false;
                bool loop = true;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &preset_text, error)) {
                    free(id_text);
                    free(preset_text);
                    return false;
                }

                if (statement->as.command.args.count >= 3 &&
                    !er_runtime_value_to_number(runtime, &statement->as.command.args.items[2], &duration_value, error)) {
                    free(id_text);
                    free(preset_text);
                    return false;
                }
                if (statement->as.command.args.count >= 4) {
                    if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[3], &loop_text, error)) {
                        free(id_text);
                        free(preset_text);
                        return false;
                    }
                    loop = er_runtime_text_means_true(loop_text);
                }

                ok = runtime->app_initialized &&
                    er_ui_app_animation_play(
                        &runtime->app,
                        id_text,
                        preset_text,
                        (int) duration_value,
                        loop,
                        error
                    );
                free(id_text);
                free(preset_text);
                free(loop_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not start preset animation");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "anim.oscillate") == 0) {
                char *id_text = NULL;
                char *property_text = NULL;
                char *loop_text = NULL;
                double amplitude_value = 0.0;
                double duration_value = 1800.0;
                bool ok = false;
                bool loop = true;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &property_text, error) ||
                    !er_runtime_value_to_number(runtime, &statement->as.command.args.items[2], &amplitude_value, error) ||
                    !er_runtime_value_to_number(runtime, &statement->as.command.args.items[3], &duration_value, error)) {
                    free(id_text);
                    free(property_text);
                    return false;
                }

                if (statement->as.command.args.count >= 5) {
                    if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[4], &loop_text, error)) {
                        free(id_text);
                        free(property_text);
                        return false;
                    }
                    loop = er_runtime_text_means_true(loop_text);
                }

                ok = runtime->app_initialized &&
                    er_ui_app_animation_oscillate(
                        &runtime->app,
                        id_text,
                        property_text,
                        amplitude_value,
                        (int) duration_value,
                        loop,
                        error
                    );
                free(id_text);
                free(property_text);
                free(loop_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not start oscillation animation");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "anim.keyframes") == 0) {
                char *id_text = NULL;
                char *property_text = NULL;
                char *frames_text = NULL;
                char *loop_text = NULL;
                double duration_value = 1800.0;
                bool ok = false;
                bool loop = true;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[1], &property_text, error) ||
                    !er_runtime_value_to_string(runtime, &statement->as.command.args.items[2], &frames_text, error) ||
                    !er_runtime_value_to_number(runtime, &statement->as.command.args.items[3], &duration_value, error)) {
                    free(id_text);
                    free(property_text);
                    free(frames_text);
                    return false;
                }

                if (statement->as.command.args.count >= 5) {
                    if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[4], &loop_text, error)) {
                        free(id_text);
                        free(property_text);
                        free(frames_text);
                        return false;
                    }
                    loop = er_runtime_text_means_true(loop_text);
                }

                ok = runtime->app_initialized &&
                    er_ui_app_animation_keyframes(
                        &runtime->app,
                        id_text,
                        property_text,
                        frames_text,
                        (int) duration_value,
                        loop,
                        error
                    );
                free(id_text);
                free(property_text);
                free(frames_text);
                free(loop_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not start keyframe animation");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "anim.stop") == 0) {
                char *id_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &id_text, error)) {
                    return false;
                }

                ok = runtime->app_initialized && er_ui_app_animation_stop(&runtime->app, id_text, error);
                free(id_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not stop animation");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "window.icon") == 0 ||
                strcmp(statement->as.command.name, "screen.icon") == 0) {
                char *path_text = NULL;
                bool ok = false;

                if (runtime->icon_locked) {
                    return true;
                }
                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &path_text, error)) {
                    return false;
                }

                free(runtime->icon_path);
                runtime->icon_path = er_rt_dup(path_text ? path_text : "");
                ok = runtime->icon_path != NULL;
                if (ok && runtime->app_initialized) {
                    ok = er_ui_app_set_icon(&runtime->app, runtime->icon_path, error);
                }
                if (ok) {
                    free(runtime->media.default_art_path);
                    runtime->media.default_art_path = er_rt_dup(runtime->icon_path);
                }
                free(path_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not set the window icon");
                    }
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "window.caption") == 0) {
                char *caption_text = NULL;

                if (runtime->title_locked) {
                    return true;
                }
                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &caption_text, error)) {
                    return false;
                }

                free(runtime->title);
                runtime->title = caption_text;
                if (runtime->app_initialized) {
                    er_ui_app_set_title(&runtime->app, runtime->title);
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "timer.stop") == 0) {
                char *timer_name = NULL;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &timer_name, error)) {
                    return false;
                }

                er_runtime_stop_timer(runtime, timer_name);
                free(timer_name);
                return true;
            }
            if (strcmp(statement->as.command.name, "media.addFiles") == 0 ||
                strcmp(statement->as.command.name, "media.openPlaylist") == 0 ||
                strcmp(statement->as.command.name, "media.savePlaylist") == 0) {
                char *path_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &path_text, error)) {
                    return false;
                }

                if (strcmp(statement->as.command.name, "media.addFiles") == 0) {
                    ok = er_media_player_add_paths(&runtime->media, path_text, error);
                } else if (strcmp(statement->as.command.name, "media.openPlaylist") == 0) {
                    ok = er_media_player_open_playlist(&runtime->media, path_text, error);
                } else {
                    ok = er_media_player_save_playlist(&runtime->media, path_text, error);
                }

                free(path_text);
                if (!ok) {
                    return false;
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "media.clear") == 0) {
                er_media_player_clear(&runtime->media);
                return true;
            }
            if (strcmp(statement->as.command.name, "media.play") == 0) {
                return er_media_player_play(&runtime->media, error);
            }
            if (strcmp(statement->as.command.name, "media.pause") == 0) {
                return er_media_player_pause(&runtime->media, error);
            }
            if (strcmp(statement->as.command.name, "media.playPause") == 0) {
                return er_media_player_play_pause(&runtime->media, error);
            }
            if (strcmp(statement->as.command.name, "media.stop") == 0) {
                return er_media_player_stop(&runtime->media, error);
            }
            if (strcmp(statement->as.command.name, "media.next") == 0) {
                return er_media_player_next(&runtime->media, error);
            }
            if (strcmp(statement->as.command.name, "media.previous") == 0) {
                return er_media_player_previous(&runtime->media, error);
            }
            if (strcmp(statement->as.command.name, "media.toggleMute") == 0) {
                er_media_player_toggle_mute(&runtime->media);
                return true;
            }
            if (strcmp(statement->as.command.name, "media.toggleShuffle") == 0) {
                er_media_player_toggle_shuffle(&runtime->media);
                return true;
            }
            if (strcmp(statement->as.command.name, "media.cycleRepeat") == 0) {
                er_media_player_cycle_repeat(&runtime->media);
                return true;
            }
            if (strcmp(statement->as.command.name, "media.seek") == 0 ||
                strcmp(statement->as.command.name, "media.seekRelative") == 0 ||
                strcmp(statement->as.command.name, "media.setVolume") == 0 ||
                strcmp(statement->as.command.name, "media.changeVolume") == 0) {
                double value_number = 0.0;

                if (!er_runtime_value_to_number(runtime, &statement->as.command.args.items[0], &value_number, error)) {
                    return false;
                }

                if (strcmp(statement->as.command.name, "media.seek") == 0) {
                    return er_media_player_seek(&runtime->media, (int) value_number, error);
                }
                if (strcmp(statement->as.command.name, "media.seekRelative") == 0) {
                    return er_media_player_seek_relative(&runtime->media, (int) value_number, error);
                }
                if (strcmp(statement->as.command.name, "media.setVolume") == 0) {
                    er_media_player_set_volume(&runtime->media, (int) value_number);
                } else {
                    er_media_player_change_volume(&runtime->media, (int) value_number);
                }
                return true;
            }
            if (strcmp(statement->as.command.name, "console.open") == 0) {
                char *profile_text = NULL;
                bool ok = false;

                if (!er_runtime_value_to_string(runtime, &statement->as.command.args.items[0], &profile_text, error)) {
                    return false;
                }

                ok = er_console_launch_profile(profile_text, error);
                free(profile_text);
                if (!ok) {
                    if (!er_error_has(error)) {
                        er_error_set(error, statement->line, statement->column, "Could not launch console profile");
                    }
                    return false;
                }
                return true;
            }
            return true;
    }

    return true;
}

static void er_runtime_live_remove_node_record(ErRuntime *runtime, size_t index) {
    if (!runtime || index >= runtime->live.node_count) {
        return;
    }

    free(runtime->live.nodes[index].id);
    if (index + 1 < runtime->live.node_count) {
        runtime->live.nodes[index] = runtime->live.nodes[runtime->live.node_count - 1];
    }
    runtime->live.node_count--;
}

static void er_runtime_live_remove_timer_record(ErRuntime *runtime, size_t index) {
    if (!runtime || index >= runtime->live.timer_count) {
        return;
    }

    free(runtime->live.timers[index].name);
    if (index + 1 < runtime->live.timer_count) {
        runtime->live.timers[index] = runtime->live.timers[runtime->live.timer_count - 1];
    }
    runtime->live.timer_count--;
}

static bool er_runtime_live_remove_node(ErRuntime *runtime, const char *id, ErError *error) {
    ErUiNode *node;

    if (!runtime || !id || id[0] == '\0') {
        return true;
    }

    if (!runtime->app_initialized) {
        return true;
    }

    node = er_ui_app_find_node(&runtime->app, id);
    if (!node) {
        return true;
    }

    er_runtime_release_node_bindings(runtime, node);
    return er_ui_app_remove_node(&runtime->app, id, error);
}

static void er_runtime_live_drop_unseen_nodes(ErRuntime *runtime) {
    size_t i = 0;
    ErError error;

    if (!runtime) {
        return;
    }

    er_error_clear(&error);
    while (i < runtime->live.node_count) {
        if (runtime->live.nodes[i].seen_in_patch) {
            ++i;
            continue;
        }

        if (!er_runtime_live_remove_node(runtime, runtime->live.nodes[i].id, &error)) {
            er_runtime_live_set_last_error_text(runtime, error.message);
            er_error_clear(&error);
            ++i;
            continue;
        }

        er_runtime_live_remove_node_record(runtime, i);
    }
}

static void er_runtime_live_drop_unseen_timers(ErRuntime *runtime) {
    size_t i = 0;

    if (!runtime) {
        return;
    }

    while (i < runtime->live.timer_count) {
        if (runtime->live.timers[i].seen_in_patch) {
            ++i;
            continue;
        }

        er_runtime_stop_timer(runtime, runtime->live.timers[i].name);
        er_runtime_live_remove_timer_record(runtime, i);
    }
}

static bool er_runtime_apply_live_statement(
    ErRuntime *runtime,
    ErStatement *statement,
    unsigned int *out_applied,
    unsigned int *out_skipped,
    ErError *error
) {
    if (!runtime || !statement) {
        return true;
    }

    if (er_compiler_statement_live_change_support(statement) != ER_COMPILER_LIVE_CHANGE_SAFE) {
        if (out_skipped) {
            (*out_skipped)++;
        }
        return true;
    }

    if (statement->type == ER_STMT_SCREEN_ADD) {
        char *id_text = NULL;

        if (!er_runtime_element_get_id(runtime, statement->as.add.element, &id_text, error)) {
            return false;
        }
        if (!id_text || id_text[0] == '\0') {
            free(id_text);
            if (out_skipped) {
                (*out_skipped)++;
            }
            return true;
        }

        if (!er_runtime_live_remove_node(runtime, id_text, error) ||
            !er_runtime_add_element(runtime, statement->as.add.element, error) ||
            !er_runtime_live_touch_node(runtime, id_text, error)) {
            free(id_text);
            return false;
        }

        free(id_text);
        if (out_applied) {
            (*out_applied)++;
        }
        return true;
    }

    if (statement->type == ER_STMT_TIMER_EVERY) {
        if (!er_runtime_execute_statement(runtime, statement, error) ||
            !er_runtime_live_touch_timer(runtime, statement->as.timer_stmt.name, error)) {
            return false;
        }
        if (out_applied) {
            (*out_applied)++;
        }
        return true;
    }

    if (!er_runtime_execute_statement(runtime, statement, error)) {
        return false;
    }

    if (out_applied) {
        (*out_applied)++;
    }
    return true;
}

static bool er_runtime_apply_live_unit(
    ErRuntime *runtime,
    ErFrontendUnit *unit,
    unsigned int *out_applied,
    unsigned int *out_skipped,
    ErError *error
) {
    size_t i;

    if (!runtime || !unit || !unit->program) {
        er_error_set(error, 0, 0, "Live patch requires a compiled frontend unit");
        return false;
    }

    er_runtime_live_begin_patch(runtime);

    for (i = 0; i < unit->program->statements.count; ++i) {
        if (!er_runtime_apply_live_statement(
                runtime,
                unit->program->statements.items[i],
                out_applied,
                out_skipped,
                error
            )) {
            return false;
        }
    }

    er_runtime_live_drop_unseen_nodes(runtime);
    er_runtime_live_drop_unseen_timers(runtime);
    runtime->program = unit->program;
    return true;
}

static void er_runtime_on_live_patch_timer(ErUiApp *app, unsigned int timer_id, void *user_data) {
    ErRuntime *runtime = (ErRuntime *) user_data;
    ErError error;
    ErFrontendUnit patch_unit;
    unsigned long long entry_stamp;
    unsigned long long backend_stamp;
    unsigned long long app_json_stamp;
    bool source_changed;
    bool app_json_changed;
    unsigned int applied_count = 0;
    unsigned int skipped_count = 0;

    (void) app;
    (void) timer_id;

    if (!runtime || !runtime->live.enabled) {
        return;
    }

    entry_stamp = er_file_last_write_time(runtime->live.entry_path);
    backend_stamp = er_file_last_write_time(runtime->live.backend_path);
    app_json_stamp = er_file_last_write_time(runtime->live.app_json_path);
    source_changed = entry_stamp != runtime->live.entry_stamp || backend_stamp != runtime->live.backend_stamp;
    app_json_changed = app_json_stamp != runtime->live.app_json_stamp;

    if (!source_changed && !app_json_changed) {
        return;
    }

    er_error_clear(&error);
    if (app_json_changed && !er_runtime_apply_app_json_config(runtime, &error)) {
        runtime->live.entry_stamp = entry_stamp;
        runtime->live.backend_stamp = backend_stamp;
        runtime->live.app_json_stamp = app_json_stamp;
        er_runtime_live_set_statusf(runtime, "live patch blocked by app.json error");
        er_runtime_live_set_last_error_text(runtime, error.message);
        er_runtime_live_publish_state(runtime);
        fprintf(stderr, "live patch app.json error: %s\n", error.message);
        return;
    }

    if (!source_changed) {
        runtime->live.app_json_stamp = app_json_stamp;
        er_runtime_live_set_statusf(runtime, "live app.json reload applied");
        er_runtime_live_set_last_error_text(runtime, "");
        er_runtime_live_publish_state(runtime);
        return;
    }

    memset(&patch_unit, 0, sizeof(patch_unit));
    if (!er_frontend_load_file(runtime->live.entry_path, &patch_unit, &error)) {
        runtime->live.entry_stamp = entry_stamp;
        runtime->live.backend_stamp = backend_stamp;
        runtime->live.app_json_stamp = app_json_stamp;
        er_runtime_live_set_statusf(runtime, "live patch parse failed");
        er_runtime_live_set_last_error_text(runtime, error.message);
        er_runtime_live_publish_state(runtime);
        fprintf(stderr, "live patch parse error: %s\n", error.message);
        return;
    }

    if (!er_runtime_live_keep_unit(runtime, &patch_unit, &error)) {
        er_frontend_unit_free(&patch_unit);
        runtime->live.entry_stamp = entry_stamp;
        runtime->live.backend_stamp = backend_stamp;
        runtime->live.app_json_stamp = app_json_stamp;
        er_runtime_live_set_statusf(runtime, "live patch storage failed");
        er_runtime_live_set_last_error_text(runtime, error.message);
        er_runtime_live_publish_state(runtime);
        fprintf(stderr, "live patch storage error: %s\n", error.message);
        return;
    }

    er_error_clear(&error);
    if (!er_runtime_apply_live_unit(
            runtime,
            &runtime->live.units[runtime->live.unit_count - 1],
            &applied_count,
            &skipped_count,
            &error
        )) {
        runtime->live.entry_stamp = entry_stamp;
        runtime->live.backend_stamp = backend_stamp;
        runtime->live.app_json_stamp = app_json_stamp;
        er_runtime_live_set_statusf(runtime, "live patch failed after partial apply");
        er_runtime_live_set_last_error_text(runtime, error.message);
        er_runtime_live_publish_state(runtime);
        fprintf(stderr, "live patch apply error: %s\n", error.message);
        return;
    }

    runtime->live.patch_version++;
    runtime->live.entry_stamp = entry_stamp;
    runtime->live.backend_stamp = backend_stamp;
    runtime->live.app_json_stamp = app_json_stamp;
    er_runtime_live_set_statusf(
        runtime,
        "live patch %u applied (%u updated, %u skipped)",
        runtime->live.patch_version,
        applied_count,
        skipped_count
    );
    er_runtime_live_set_last_error_text(runtime, "");
    er_runtime_live_publish_state(runtime);
    fprintf(
        stdout,
        "live patch %u applied: %u updated, %u skipped\n",
        runtime->live.patch_version,
        applied_count,
        skipped_count
    );
}

static bool er_runtime_flush_pending_on_loads(ErRuntime *runtime, ErError *error) {
    size_t i;

    if (!runtime) {
        return true;
    }

    runtime->defer_on_load = false;

    for (i = 0; i < runtime->pending_on_load_count; ++i) {
        if (!er_runtime_execute_block(runtime, runtime->pending_on_loads[i], error)) {
            return false;
        }
    }

    runtime->pending_on_load_count = 0;
    return true;
}

bool er_runtime_check_source(const char *source_name, const char *source, ErError *error) {
    ErFrontendUnit unit;

    if (!er_frontend_load_source(source_name, source, &unit, error)) {
        return false;
    }

    er_frontend_unit_free(&unit);
    return true;
}

bool er_runtime_check_file(const char *path, ErError *error) {
    ErFrontendUnit unit;

    if (!er_frontend_load_file(path, &unit, error)) {
        return false;
    }

    er_frontend_unit_free(&unit);
    return true;
}

static int er_runtime_run_loaded_file(
    ErFrontendUnit *unit,
    bool live_enabled,
    unsigned int poll_ms,
    ErError *error
) {
    ErRuntime runtime;
    size_t i;
    int exit_code;

    er_runtime_init(
        &runtime,
        unit->program,
        unit->graph.modules[unit->graph.entry_index].normalized_path,
        unit->graph.modules[unit->graph.entry_index].directory_path
    );

    if (live_enabled) {
        if (!er_runtime_live_enable(
                &runtime,
                unit->graph.modules[unit->graph.entry_index].normalized_path,
                poll_ms,
                error
            ) ||
            !er_runtime_live_keep_unit(&runtime, unit, error)) {
            er_runtime_free(&runtime);
            return 1;
        }
        runtime.program = runtime.live.units[runtime.live.unit_count - 1].program;
    }

    if (!er_runtime_apply_app_json_config(&runtime, error)) {
        er_runtime_free(&runtime);
        if (!live_enabled) {
            er_frontend_unit_free(unit);
        }
        return 1;
    }

    for (i = 0; i < runtime.program->statements.count; ++i) {
        if (!er_runtime_execute_statement(&runtime, runtime.program->statements.items[i], error)) {
            er_runtime_free(&runtime);
            if (!live_enabled) {
                er_frontend_unit_free(unit);
            }
            return 1;
        }
    }

    if (!er_runtime_ensure_app(&runtime, error)) {
        er_runtime_free(&runtime);
        if (!live_enabled) {
            er_frontend_unit_free(unit);
        }
        return 1;
    }

    if (!er_runtime_flush_pending_on_loads(&runtime, error)) {
        er_runtime_free(&runtime);
        if (!live_enabled) {
            er_frontend_unit_free(unit);
        }
        return 1;
    }

    if (live_enabled &&
        !er_runtime_live_snapshot_initial_state(&runtime, runtime.program, error)) {
        er_runtime_free(&runtime);
        return 1;
    }

    exit_code = er_ui_app_run(&runtime.app);
    er_runtime_free(&runtime);
    if (!live_enabled) {
        er_frontend_unit_free(unit);
    }
    return exit_code;
}

int er_runtime_run_source(const char *source_name, const char *source, ErError *error) {
    ErFrontendUnit unit;
    ErRuntime runtime;
    char *source_dir = NULL;
    size_t i;
    int exit_code = 0;

    if (!er_frontend_load_source(source_name, source, &unit, error)) {
        return 1;
    }

    source_dir = er_runtime_derive_source_dir(source_name);
    er_runtime_init(&runtime, unit.program, source_name, source_dir ? source_dir : ".");
    free(source_dir);

    if (!er_runtime_apply_app_json_config(&runtime, error)) {
        er_runtime_free(&runtime);
        er_frontend_unit_free(&unit);
        return 1;
    }

    for (i = 0; i < unit.program->statements.count; ++i) {
        if (!er_runtime_execute_statement(&runtime, unit.program->statements.items[i], error)) {
            er_runtime_free(&runtime);
            er_frontend_unit_free(&unit);
            return 1;
        }
    }

    if (!er_runtime_ensure_app(&runtime, error)) {
        er_runtime_free(&runtime);
        er_frontend_unit_free(&unit);
        return 1;
    }

    if (!er_runtime_flush_pending_on_loads(&runtime, error)) {
        er_runtime_free(&runtime);
        er_frontend_unit_free(&unit);
        return 1;
    }

    exit_code = er_ui_app_run(&runtime.app);
    er_runtime_free(&runtime);
    er_frontend_unit_free(&unit);
    return exit_code;
}

int er_runtime_run_file(const char *path, ErError *error) {
    ErFrontendUnit unit;

    if (!er_frontend_load_file(path, &unit, error)) {
        return 1;
    }

    return er_runtime_run_loaded_file(&unit, false, 0u, error);
}

int er_runtime_run_file_live(const char *path, unsigned int poll_ms, ErError *error) {
    ErFrontendUnit unit;

    if (!er_frontend_load_file(path, &unit, error)) {
        return 1;
    }

    return er_runtime_run_loaded_file(&unit, true, poll_ms, error);
}
