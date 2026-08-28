#ifndef ERIRE_COMPILER_H
#define ERIRE_COMPILER_H

#include <stdbool.h>
#include <stdio.h>

#include "ast.h"
#include "error.h"

typedef enum ErCompilerTarget {
    ER_COMPILER_TARGET_C_STUB,
    ER_COMPILER_TARGET_BYTECODE_VM
} ErCompilerTarget;

typedef enum ErCompilerLiveChangeSupport {
    ER_COMPILER_LIVE_CHANGE_UNSAFE = 0,
    ER_COMPILER_LIVE_CHANGE_SAFE = 1
} ErCompilerLiveChangeSupport;

ErCompilerLiveChangeSupport er_compiler_statement_live_change_support(const ErStatement *statement);
const char *er_compiler_statement_live_change_reason(const ErStatement *statement);

bool er_compiler_emit_stub(
    const ErProgram *program,
    ErCompilerTarget target,
    FILE *out,
    ErError *error
);

#endif
