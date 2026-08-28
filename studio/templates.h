#ifndef ERIRE_STUDIO_TEMPLATES_H
#define ERIRE_STUDIO_TEMPLATES_H

#include <stdbool.h>
#include <stddef.h>

#include "error.h"

bool studio_templates_create_project(
    const char *project_name,
    const char *location,
    const char *template_name,
    char *out_root,
    size_t out_capacity,
    ErError *error
);

#endif
