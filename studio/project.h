#ifndef ERIRE_STUDIO_PROJECT_H
#define ERIRE_STUDIO_PROJECT_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"
#include "util.h"

typedef struct StudioProject {
    bool is_open;
    char root_path[STUDIO_MAX_PATH];
    char name[STUDIO_MAX_NAME];
    char app_json_path[STUDIO_MAX_PATH];
    char entry_file[STUDIO_MAX_PATH];
} StudioProject;

bool studio_project_detect_root(const char *path, char *out_root, size_t out_capacity);
bool studio_project_open(StudioProject *project, const char *path, ErError *error);
void studio_project_close(StudioProject *project);
char *studio_project_tree_json(const StudioProject *project);
bool studio_project_read_file(const char *path, char **out_data, size_t *out_size, ErError *error);
bool studio_project_write_file(const char *path, const char *content, ErError *error);
bool studio_project_rename_entry(const char *path, const char *new_name, char *out_new_path, size_t out_capacity, ErError *error);
bool studio_project_delete_entry(const char *path, ErError *error);

#endif
