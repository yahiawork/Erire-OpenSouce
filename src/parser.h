#ifndef ERIRE_PARSER_H
#define ERIRE_PARSER_H

#include <stdbool.h>

#include "ast.h"
#include "error.h"

bool er_parse_program(
    const char *source_name,
    const char *source,
    ErProgram **out_program,
    ErError *error
);

#endif
