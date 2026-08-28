#ifndef ERIRE_PACKAGER_H
#define ERIRE_PACKAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

typedef struct ErPackagedApp {
    char *extract_root;
    char *entry_path;
} ErPackagedApp;

typedef struct ErPackagerBuildOptions {
    const char *icon_path_override;
    const char *win_title_override;
    bool onefile;
} ErPackagerBuildOptions;

bool er_packager_build(
    const char *source_path,
    const char *output_path,
    const ErPackagerBuildOptions *options,
    ErError *error
);
bool er_packager_extract_embedded_app(const char *exe_path, ErPackagedApp *out_app, ErError *error);
void er_packaged_app_free(ErPackagedApp *app);
bool er_packager_extract_embedded_source(
    const char *exe_path,
    char **out_source,
    size_t *out_size,
    ErError *error
);

#endif
