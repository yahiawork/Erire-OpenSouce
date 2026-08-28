#ifndef ERIRE_FRONTEND_H
#define ERIRE_FRONTEND_H

#include "ast.h"
#include "error.h"
#include "module.h"

typedef struct ErFrontendUnit {
    ErModuleGraph graph;
    ErProgram *program;
} ErFrontendUnit;

bool er_frontend_load_file(const char *entry_path, ErFrontendUnit *out_unit, ErError *error);
bool er_frontend_load_source(const char *source_name, const char *source, ErFrontendUnit *out_unit, ErError *error);
void er_frontend_unit_free(ErFrontendUnit *unit);

#endif
