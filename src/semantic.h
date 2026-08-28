#ifndef ERIRE_SEMANTIC_H
#define ERIRE_SEMANTIC_H

#include "ast.h"
#include "error.h"

bool er_semantic_analyze_program(const char *source_name, const ErProgram *program, ErError *error);

#endif
