#include "compiler.h"

#include <stdio.h>
#include <string.h>

static const ErProperty *er_compiler_find_property(const ErElement *element, const char *name) {
    size_t i;

    if (!element || !name) {
        return NULL;
    }

    for (i = 0; i < element->properties.count; ++i) {
        if (element->properties.items[i].name && strcmp(element->properties.items[i].name, name) == 0) {
            return &element->properties.items[i];
        }
    }

    return NULL;
}

static bool er_compiler_is_pure_live_call_name(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }

    if (strncmp(name, "text.", 5) == 0 || strncmp(name, "math.", 5) == 0) {
        return true;
    }

    return strcmp(name, "time.now") == 0 ||
        strcmp(name, "sys.cwd") == 0 ||
        strcmp(name, "sys.hostname") == 0 ||
        strcmp(name, "sys.platform") == 0 ||
        strcmp(name, "runtime.liveEnabled") == 0 ||
        strcmp(name, "runtime.liveVersion") == 0 ||
        strcmp(name, "runtime.liveStatus") == 0 ||
        strcmp(name, "runtime.liveError") == 0 ||
        strcmp(name, "runtime.widgetCount") == 0 ||
        strcmp(name, "runtime.timerCount") == 0 ||
        strcmp(name, "runtime.hasVar") == 0 ||
        strcmp(name, "runtime.getVar") == 0 ||
        strcmp(name, "runtime.widgetExists") == 0;
}

static bool er_compiler_value_is_live_safe(const ErValue *value) {
    size_t i;
    const ErCallExpression *call;

    if (!value) {
        return false;
    }

    switch (value->type) {
        case ER_VALUE_STRING:
        case ER_VALUE_NUMBER:
        case ER_VALUE_BOOL:
        case ER_VALUE_SYMBOL:
        case ER_VALUE_VARIABLE:
            return true;
        case ER_VALUE_CALL:
            call = value->as.call;
            if (!call || !er_compiler_is_pure_live_call_name(call->name)) {
                return false;
            }
            for (i = 0; i < call->args.count; ++i) {
                if (!er_compiler_value_is_live_safe(&call->args.items[i])) {
                    return false;
                }
            }
            return true;
    }

    return false;
}

static bool er_compiler_value_array_is_live_safe(const ErValueArray *values) {
    size_t i;

    if (!values) {
        return true;
    }

    for (i = 0; i < values->count; ++i) {
        if (!er_compiler_value_is_live_safe(&values->items[i])) {
            return false;
        }
    }

    return true;
}

static bool er_compiler_element_has_stable_id(const ErElement *element) {
    const ErProperty *id_property;

    id_property = er_compiler_find_property(element, "id");
    return id_property && id_property->values.count > 0;
}

static bool er_compiler_element_is_live_safe(const ErElement *element) {
    size_t i;

    if (!element || !er_compiler_element_has_stable_id(element)) {
        return false;
    }

    if (!er_compiler_value_array_is_live_safe(&element->base_args)) {
        return false;
    }

    for (i = 0; i < element->properties.count; ++i) {
        const ErProperty *property = &element->properties.items[i];
        if (property->is_event) {
            continue;
        }
        if (!er_compiler_value_array_is_live_safe(&property->values)) {
            return false;
        }
    }

    return true;
}

ErCompilerLiveChangeSupport er_compiler_statement_live_change_support(const ErStatement *statement) {
    size_t i;

    if (!statement) {
        return ER_COMPILER_LIVE_CHANGE_UNSAFE;
    }

    switch (statement->type) {
        case ER_STMT_IMPORT:
            return statement->as.import_directive.type == ER_IMPORT_DIRECTIVE_PYTHON
                ? ER_COMPILER_LIVE_CHANGE_SAFE
                : ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_SCREEN_TITLE:
        case ER_STMT_SCREEN_BG:
        case ER_STMT_SCREEN_SHOW:
            return er_compiler_value_array_is_live_safe(&statement->as.command.args)
                ? ER_COMPILER_LIVE_CHANGE_SAFE
                : ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_SCREEN_SETTEXT:
            return er_compiler_value_is_live_safe(&statement->as.set_text.element_id) &&
                er_compiler_value_is_live_safe(&statement->as.set_text.value)
                ? ER_COMPILER_LIVE_CHANGE_SAFE
                : ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_TIMER_EVERY:
            return er_compiler_value_is_live_safe(&statement->as.timer_stmt.interval)
                ? ER_COMPILER_LIVE_CHANGE_SAFE
                : ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_SCREEN_ADD:
            return er_compiler_element_is_live_safe(statement->as.add.element)
                ? ER_COMPILER_LIVE_CHANGE_SAFE
                : ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_VAR_SET:
            return er_compiler_value_is_live_safe(&statement->as.var_decl.value)
                ? ER_COMPILER_LIVE_CHANGE_SAFE
                : ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_GENERIC:
            if (!statement->as.command.name) {
                return ER_COMPILER_LIVE_CHANGE_UNSAFE;
            }
            if (strcmp(statement->as.command.name, "screen.setImage") == 0 ||
                strcmp(statement->as.command.name, "screen.setBounds") == 0 ||
                strcmp(statement->as.command.name, "window.icon") == 0 ||
                strcmp(statement->as.command.name, "window.caption") == 0 ||
                strcmp(statement->as.command.name, "timer.stop") == 0) {
                for (i = 0; i < statement->as.command.args.count; ++i) {
                    if (!er_compiler_value_is_live_safe(&statement->as.command.args.items[i])) {
                        return ER_COMPILER_LIVE_CHANGE_UNSAFE;
                    }
                }
                return ER_COMPILER_LIVE_CHANGE_SAFE;
            }
            return ER_COMPILER_LIVE_CHANGE_UNSAFE;
        case ER_STMT_SCREEN_CREATE:
        case ER_STMT_IF:
        case ER_STMT_WHILE:
        case ER_STMT_FOR:
            return ER_COMPILER_LIVE_CHANGE_UNSAFE;
    }

    return ER_COMPILER_LIVE_CHANGE_UNSAFE;
}

const char *er_compiler_statement_live_change_reason(const ErStatement *statement) {
    if (!statement) {
        return "missing statement";
    }

    switch (statement->type) {
        case ER_STMT_IMPORT:
            return statement->as.import_directive.type == ER_IMPORT_DIRECTIVE_PYTHON
                ? "python import path rebinding is live-safe"
                : "legacy imports are not re-applied during live patching";
        case ER_STMT_SCREEN_CREATE:
            return "window creation and geometry replay are UNSAFE FOR LIVE PATCHING";
        case ER_STMT_SCREEN_TITLE:
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "window title updates are applied in place"
                : "screen.title is skipped when it depends on side-effectful expressions";
        case ER_STMT_SCREEN_BG:
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "window background updates are applied in place"
                : "screen.bg is skipped when it depends on side-effectful expressions";
        case ER_STMT_SCREEN_ADD:
            if (!er_compiler_element_has_stable_id(statement->as.add.element)) {
                return "screen.add requires id[] for live replacement";
            }
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "widget reload uses id[] as the stable patch target"
                : "screen.add properties must remain side-effect-free during live patching";
        case ER_STMT_SCREEN_SHOW:
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "page switching is applied in place"
                : "screen.show is skipped when it depends on side-effectful expressions";
        case ER_STMT_SCREEN_SETTEXT:
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "text updates are applied in place"
                : "screen.setText is skipped when it depends on side-effectful expressions";
        case ER_STMT_VAR_SET:
            return er_compiler_value_is_live_safe(&statement->as.var_decl.value)
                ? "pure var.set values can be re-evaluated safely"
                : "var.set is skipped when it depends on side-effectful calls";
        case ER_STMT_TIMER_EVERY:
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "named timers are rebound in place"
                : "timer.every interval must remain side-effect-free during live patching";
        case ER_STMT_GENERIC:
            return er_compiler_statement_live_change_support(statement) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "generic command is whitelisted for live patching"
                : "generic command is UNSAFE FOR LIVE PATCHING";
        case ER_STMT_IF:
        case ER_STMT_WHILE:
        case ER_STMT_FOR:
            return "control-flow replay is DO NOT IMPLEMENT IN V1";
    }

    return "statement is UNSAFE FOR LIVE PATCHING";
}

bool er_compiler_emit_stub(
    const ErProgram *program,
    ErCompilerTarget target,
    FILE *out,
    ErError *error
) {
    size_t i;

    (void) error;

    fprintf(out, "/* Erire compiler skeleton target=%d statements=%zu */\n", (int) target, program->statements.count);
    fprintf(out, "/* v1 recommendation: keep packaging source-in-runner; v2 can lower AST to bytecode. */\n");
    for (i = 0; i < program->statements.count; ++i) {
        fprintf(out, "/* stmt[%zu] type=%d */\n", i, (int) program->statements.items[i]->type);
        fprintf(
            out,
            "/* stmt[%zu] live_change=%s reason=%s */\n",
            i,
            er_compiler_statement_live_change_support(program->statements.items[i]) == ER_COMPILER_LIVE_CHANGE_SAFE
                ? "safe"
                : "unsafe",
            er_compiler_statement_live_change_reason(program->statements.items[i])
        );
    }
    return true;
}
