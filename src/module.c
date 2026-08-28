#include "module.h"

#include <stdlib.h>
#include <string.h>

#include "fileio.h"

#ifdef _WIN32
#define ER_MODULE_FULLPATH _fullpath
#else
#include <unistd.h>
#endif

static char *er_module_dup(const char *text) {
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

static bool er_module_normalize_path(const char *path, char **out_path, ErError *error) {
    char buffer[1024];

    *out_path = NULL;

    if (!path || path[0] == '\0') {
        er_error_set(error, 0, 0, "Expected a module path");
        return false;
    }

#ifdef _WIN32
    if (!ER_MODULE_FULLPATH(buffer, path, sizeof(buffer))) {
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

    *out_path = er_module_dup(buffer);
    if (!*out_path) {
        er_error_set(error, 0, 0, "Out of memory while normalizing path");
        return false;
    }

    return true;
}

static bool er_module_graph_push(ErModuleGraph *graph, ErModuleSource module, ErError *error) {
    ErModuleSource *new_items;
    size_t new_capacity;

    if (graph->count < graph->capacity) {
        graph->modules[graph->count++] = module;
        return true;
    }

    new_capacity = graph->capacity == 0 ? 4 : graph->capacity * 2;
    new_items = (ErModuleSource *) realloc(graph->modules, new_capacity * sizeof(ErModuleSource));
    if (!new_items) {
        er_error_set(error, 0, 0, "Out of memory while growing module graph");
        return false;
    }

    graph->modules = new_items;
    graph->capacity = new_capacity;
    graph->modules[graph->count++] = module;
    return true;
}

bool er_module_load_entry(const char *entry_path, ErModuleSource *out_module, ErError *error) {
    char *normalized = NULL;
    char *source = NULL;
    size_t source_size = 0;
    ErModuleSource module;

    memset(out_module, 0, sizeof(*out_module));

    if (!er_path_has_extension(entry_path, ".er")) {
        er_error_set(error, 0, 0, "Expected an .er file: %s", entry_path);
        return false;
    }

    if (!er_module_normalize_path(entry_path, &normalized, error)) {
        return false;
    }

    if (!er_file_read_all(normalized, &source, &source_size, error)) {
        free(normalized);
        return false;
    }

    memset(&module, 0, sizeof(module));
    module.requested_path = er_module_dup(entry_path);
    module.normalized_path = normalized;
    module.source = source;
    module.source_size = source_size;

    if (!module.requested_path) {
        er_module_source_free(&module);
        er_error_set(error, 0, 0, "Out of memory while recording module path");
        return false;
    }

    module.directory_path = (char *) malloc(strlen(module.normalized_path) + 1);
    if (!module.directory_path) {
        er_module_source_free(&module);
        er_error_set(error, 0, 0, "Out of memory while recording module directory");
        return false;
    }
    er_path_dirname(module.normalized_path, module.directory_path, strlen(module.normalized_path) + 1);

    *out_module = module;
    return true;
}

bool er_module_graph_build_entry(const char *entry_path, ErModuleGraph *out_graph, ErError *error) {
    ErModuleSource module;
    char backend_path[1024];

    memset(out_graph, 0, sizeof(*out_graph));

    if (!er_module_load_entry(entry_path, &module, error)) {
        return false;
    }

    if (!er_module_graph_push(out_graph, module, error)) {
        er_module_source_free(&module);
        return false;
    }

    out_graph->entry_index = 0;

    if (module.directory_path && module.directory_path[0] != '\0') {
        ErModuleSource backend_module;

        memset(&backend_module, 0, sizeof(backend_module));
        er_path_join(module.directory_path, "backend.er", backend_path, sizeof(backend_path));
        if (er_file_exists(backend_path) && strcmp(backend_path, module.normalized_path) != 0) {
            if (!er_module_load_entry(backend_path, &backend_module, error)) {
                er_module_graph_free(out_graph);
                return false;
            }
            if (!er_module_graph_push(out_graph, backend_module, error)) {
                er_module_source_free(&backend_module);
                er_module_graph_free(out_graph);
                return false;
            }
        }
    }

    return true;
}

void er_module_source_free(ErModuleSource *module) {
    if (!module) {
        return;
    }

    free(module->requested_path);
    free(module->normalized_path);
    free(module->directory_path);
    free(module->source);
    memset(module, 0, sizeof(*module));
}

void er_module_graph_free(ErModuleGraph *graph) {
    size_t i;

    if (!graph) {
        return;
    }

    for (i = 0; i < graph->count; ++i) {
        er_module_source_free(&graph->modules[i]);
    }

    free(graph->modules);
    memset(graph, 0, sizeof(*graph));
}
