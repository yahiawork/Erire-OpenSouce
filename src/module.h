#ifndef ERIRE_MODULE_H
#define ERIRE_MODULE_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

typedef struct ErModuleSource {
    char *requested_path;
    char *normalized_path;
    char *directory_path;
    char *source;
    size_t source_size;
} ErModuleSource;

typedef struct ErModuleGraph {
    ErModuleSource *modules;
    size_t count;
    size_t capacity;
    size_t entry_index;
} ErModuleGraph;

bool er_module_load_entry(const char *entry_path, ErModuleSource *out_module, ErError *error);
bool er_module_graph_build_entry(const char *entry_path, ErModuleGraph *out_graph, ErError *error);
void er_module_source_free(ErModuleSource *module);
void er_module_graph_free(ErModuleGraph *graph);

#endif
