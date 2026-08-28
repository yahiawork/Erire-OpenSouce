#include "ast.h"

#include <stdlib.h>
#include <string.h>

static char *er_ast_dup(const char *text) {
    char *copy;
    size_t len;

    if (!text) {
        return NULL;
    }

    len = strlen(text);
    copy = (char *) malloc(len + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, len + 1);
    return copy;
}

bool er_value_array_push(ErValueArray *array, ErValue value) {
    size_t new_capacity;
    ErValue *new_items;

    if (array->count < array->capacity) {
        array->items[array->count++] = value;
        return true;
    }

    new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
    new_items = (ErValue *) realloc(array->items, new_capacity * sizeof(ErValue));
    if (!new_items) {
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    array->items[array->count++] = value;
    return true;
}

bool er_statement_array_push(ErStatementArray *array, ErStatement *statement) {
    size_t new_capacity;
    ErStatement **new_items;

    if (array->count < array->capacity) {
        array->items[array->count++] = statement;
        return true;
    }

    new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
    new_items = (ErStatement **) realloc(array->items, new_capacity * sizeof(ErStatement *));
    if (!new_items) {
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    array->items[array->count++] = statement;
    return true;
}

bool er_property_array_push(ErPropertyArray *array, ErProperty property) {
    size_t new_capacity;
    ErProperty *new_items;

    if (array->count < array->capacity) {
        array->items[array->count++] = property;
        return true;
    }

    new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
    new_items = (ErProperty *) realloc(array->items, new_capacity * sizeof(ErProperty));
    if (!new_items) {
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    array->items[array->count++] = property;
    return true;
}

bool er_else_if_clause_array_push(ErElseIfClauseArray *array, ErElseIfClause clause) {
    size_t new_capacity;
    ErElseIfClause *new_items;

    if (array->count < array->capacity) {
        array->items[array->count++] = clause;
        return true;
    }

    new_capacity = array->capacity == 0 ? 4 : array->capacity * 2;
    new_items = (ErElseIfClause *) realloc(array->items, new_capacity * sizeof(ErElseIfClause));
    if (!new_items) {
        return false;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    array->items[array->count++] = clause;
    return true;
}

ErValue er_value_make_string(const char *text) {
    ErValue value;
    value.type = ER_VALUE_STRING;
    value.as.string = er_ast_dup(text);
    return value;
}

ErValue er_value_make_symbol(const char *text) {
    ErValue value;
    value.type = ER_VALUE_SYMBOL;
    value.as.symbol = er_ast_dup(text);
    return value;
}

ErValue er_value_make_variable(const char *text) {
    ErValue value;
    value.type = ER_VALUE_VARIABLE;
    value.as.variable = er_ast_dup(text);
    return value;
}

ErValue er_value_make_number(double number) {
    ErValue value;
    value.type = ER_VALUE_NUMBER;
    value.as.number = number;
    return value;
}

ErValue er_value_make_bool(bool boolean) {
    ErValue value;
    value.type = ER_VALUE_BOOL;
    value.as.boolean = boolean;
    return value;
}

ErCallExpression *er_call_expression_create(const char *name) {
    ErCallExpression *call = (ErCallExpression *) calloc(1, sizeof(ErCallExpression));
    if (!call) {
        return NULL;
    }
    call->name = er_ast_dup(name);
    if (name && !call->name) {
        free(call);
        return NULL;
    }
    return call;
}

static ErCallExpression *er_call_expression_clone(const ErCallExpression *call) {
    ErCallExpression *copy;
    size_t i;

    if (!call) {
        return NULL;
    }

    copy = er_call_expression_create(call->name);
    if (!copy) {
        return NULL;
    }

    for (i = 0; i < call->args.count; ++i) {
        ErValue cloned = er_value_clone(&call->args.items[i]);
        if (!er_value_array_push(&copy->args, cloned)) {
            er_value_free(&cloned);
            er_call_expression_free(copy);
            return NULL;
        }
    }

    return copy;
}

ErValue er_value_make_call(ErCallExpression *call) {
    ErValue value;
    value.type = ER_VALUE_CALL;
    value.as.call = call;
    return value;
}

ErValue er_value_clone(const ErValue *value) {
    switch (value->type) {
        case ER_VALUE_STRING: return er_value_make_string(value->as.string);
        case ER_VALUE_SYMBOL: return er_value_make_symbol(value->as.symbol);
        case ER_VALUE_VARIABLE: return er_value_make_variable(value->as.variable);
        case ER_VALUE_NUMBER: return er_value_make_number(value->as.number);
        case ER_VALUE_BOOL: return er_value_make_bool(value->as.boolean);
        case ER_VALUE_CALL: return er_value_make_call(er_call_expression_clone(value->as.call));
        default: return er_value_make_symbol("");
    }
}

void er_call_expression_free(ErCallExpression *call) {
    size_t i;

    if (!call) {
        return;
    }

    free(call->name);
    for (i = 0; i < call->args.count; ++i) {
        er_value_free(&call->args.items[i]);
    }
    free(call->args.items);
    free(call);
}

void er_value_free(ErValue *value) {
    if (!value) {
        return;
    }

    switch (value->type) {
        case ER_VALUE_STRING:
            free(value->as.string);
            break;
        case ER_VALUE_SYMBOL:
            free(value->as.symbol);
            break;
        case ER_VALUE_VARIABLE:
            free(value->as.variable);
            break;
        case ER_VALUE_CALL:
            er_call_expression_free(value->as.call);
            break;
        default:
            break;
    }

    memset(value, 0, sizeof(*value));
}

ErElement *er_element_create(void) {
    return (ErElement *) calloc(1, sizeof(ErElement));
}

static void er_property_free(ErProperty *property) {
    size_t i;

    free(property->name);
    for (i = 0; i < property->values.count; ++i) {
        er_value_free(&property->values.items[i]);
    }
    free(property->values.items);

    for (i = 0; i < property->block.count; ++i) {
        er_statement_free(property->block.items[i]);
    }
    free(property->block.items);
}

static void er_condition_free(ErCondition *condition) {
    if (!condition) {
        return;
    }

    er_value_free(&condition->left);
    er_value_free(&condition->right);
}

static void er_statement_array_free_contents(ErStatementArray *body) {
    size_t i;

    if (!body) {
        return;
    }

    for (i = 0; i < body->count; ++i) {
        er_statement_free(body->items[i]);
    }
    free(body->items);
    memset(body, 0, sizeof(*body));
}

void er_element_free(ErElement *element) {
    size_t i;

    if (!element) {
        return;
    }

    free(element->kind);
    free(element->base_property);

    for (i = 0; i < element->base_args.count; ++i) {
        er_value_free(&element->base_args.items[i]);
    }
    free(element->base_args.items);

    for (i = 0; i < element->properties.count; ++i) {
        er_property_free(&element->properties.items[i]);
    }
    free(element->properties.items);

    free(element);
}

ErStatement *er_statement_create(ErStatementType type, int line, int column) {
    ErStatement *statement = (ErStatement *) calloc(1, sizeof(ErStatement));
    if (!statement) {
        return NULL;
    }
    statement->type = type;
    statement->line = line;
    statement->column = column;
    return statement;
}

void er_statement_free(ErStatement *statement) {
    size_t i;

    if (!statement) {
        return;
    }

    switch (statement->type) {
        case ER_STMT_IMPORT:
            free(statement->as.import_directive.provider);
            free(statement->as.import_directive.path);
            free(statement->as.import_directive.alias);
            for (i = 0; i < statement->as.import_directive.args.count; ++i) {
                er_value_free(&statement->as.import_directive.args.items[i]);
            }
            free(statement->as.import_directive.args.items);
            break;
        case ER_STMT_SCREEN_CREATE:
        case ER_STMT_SCREEN_TITLE:
        case ER_STMT_SCREEN_BG:
        case ER_STMT_SCREEN_SHOW:
        case ER_STMT_GENERIC:
            free(statement->as.command.name);
            for (i = 0; i < statement->as.command.args.count; ++i) {
                er_value_free(&statement->as.command.args.items[i]);
            }
            free(statement->as.command.args.items);
            break;
        case ER_STMT_SCREEN_ADD:
            er_element_free(statement->as.add.element);
            break;
        case ER_STMT_SCREEN_SETTEXT:
            er_value_free(&statement->as.set_text.element_id);
            er_value_free(&statement->as.set_text.value);
            break;
        case ER_STMT_VAR_SET:
            free(statement->as.var_decl.name);
            er_value_free(&statement->as.var_decl.value);
            break;
        case ER_STMT_IF:
            er_condition_free(&statement->as.if_stmt.condition);
            er_statement_array_free_contents(&statement->as.if_stmt.body);
            for (i = 0; i < statement->as.if_stmt.else_ifs.count; ++i) {
                er_condition_free(&statement->as.if_stmt.else_ifs.items[i].condition);
                er_statement_array_free_contents(&statement->as.if_stmt.else_ifs.items[i].body);
            }
            free(statement->as.if_stmt.else_ifs.items);
            er_statement_array_free_contents(&statement->as.if_stmt.else_body);
            break;
        case ER_STMT_WHILE:
            er_condition_free(&statement->as.while_stmt.condition);
            er_statement_array_free_contents(&statement->as.while_stmt.body);
            break;
        case ER_STMT_FOR:
            free(statement->as.for_stmt.iterator_name);
            er_value_free(&statement->as.for_stmt.start);
            er_value_free(&statement->as.for_stmt.end);
            er_value_free(&statement->as.for_stmt.step);
            er_statement_array_free_contents(&statement->as.for_stmt.body);
            break;
        case ER_STMT_TIMER_EVERY:
            free(statement->as.timer_stmt.name);
            er_value_free(&statement->as.timer_stmt.interval);
            er_statement_array_free_contents(&statement->as.timer_stmt.body);
            break;
        default:
            break;
    }

    free(statement);
}

ErProgram *er_program_create(void) {
    return (ErProgram *) calloc(1, sizeof(ErProgram));
}

void er_program_free(ErProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->statements.count; ++i) {
        er_statement_free(program->statements.items[i]);
    }
    free(program->statements.items);
    free(program);
}

static void er_indent(FILE *out, int indent) {
    int i;
    for (i = 0; i < indent; ++i) {
        fputs("  ", out);
    }
}

static void er_dump_value(FILE *out, const ErValue *value) {
    switch (value->type) {
        case ER_VALUE_STRING: fprintf(out, "\"%s\"", value->as.string); break;
        case ER_VALUE_NUMBER: fprintf(out, "%g", value->as.number); break;
        case ER_VALUE_BOOL: fputs(value->as.boolean ? "true" : "false", out); break;
        case ER_VALUE_SYMBOL: fprintf(out, "%s", value->as.symbol); break;
        case ER_VALUE_VARIABLE: fprintf(out, "$%s", value->as.variable); break;
        case ER_VALUE_CALL: {
            size_t i;
            fprintf(out, "%s[", value->as.call && value->as.call->name ? value->as.call->name : "<call>");
            if (value->as.call) {
                for (i = 0; i < value->as.call->args.count; ++i) {
                    if (i > 0) {
                        fputs("; ", out);
                    }
                    er_dump_value(out, &value->as.call->args.items[i]);
                }
            }
            fputc(']', out);
            break;
        }
    }
}

static void er_dump_statement(FILE *out, const ErStatement *statement, int indent);

static void er_dump_element(FILE *out, const ErElement *element, int indent) {
    size_t i;

    er_indent(out, indent);
    fprintf(out, "%s", element->kind ? element->kind : "<element>");
    if (element->base_property) {
        fprintf(out, ".%s(", element->base_property);
        for (i = 0; i < element->base_args.count; ++i) {
            if (i > 0) {
                fputs(", ", out);
            }
            er_dump_value(out, &element->base_args.items[i]);
        }
        fputs(")", out);
    }
    fputc('\n', out);

    for (i = 0; i < element->properties.count; ++i) {
        size_t j;
        const ErProperty *property = &element->properties.items[i];
        er_indent(out, indent + 1);
        fprintf(out, ".%s", property->name);
        if (property->is_event) {
            fputs(" [event]\n", out);
            for (j = 0; j < property->block.count; ++j) {
                er_dump_statement(out, property->block.items[j], indent + 2);
            }
        } else {
            fputs("(", out);
            for (j = 0; j < property->values.count; ++j) {
                if (j > 0) {
                    fputs(", ", out);
                }
                er_dump_value(out, &property->values.items[j]);
            }
            fputs(")\n", out);
        }
    }
}

static void er_dump_statement(FILE *out, const ErStatement *statement, int indent) {
    size_t i;

    er_indent(out, indent);

    switch (statement->type) {
        case ER_STMT_IMPORT:
            if (statement->as.import_directive.type == ER_IMPORT_DIRECTIVE_PYTHON) {
                fprintf(
                    out,
                    "import python \"%s\" as %s\n",
                    statement->as.import_directive.path ? statement->as.import_directive.path : "",
                    statement->as.import_directive.alias ? statement->as.import_directive.alias : ""
                );
            } else {
                fprintf(out, "@imp from %s(", statement->as.import_directive.provider);
                for (i = 0; i < statement->as.import_directive.args.count; ++i) {
                    if (i > 0) {
                        fputs(", ", out);
                    }
                    er_dump_value(out, &statement->as.import_directive.args.items[i]);
                }
                fputs(")\n", out);
            }
            break;
        case ER_STMT_SCREEN_CREATE:
        case ER_STMT_SCREEN_TITLE:
        case ER_STMT_SCREEN_BG:
        case ER_STMT_SCREEN_SHOW:
        case ER_STMT_GENERIC:
            fprintf(out, "%s(", statement->as.command.name);
            for (i = 0; i < statement->as.command.args.count; ++i) {
                if (i > 0) {
                    fputs(", ", out);
                }
                er_dump_value(out, &statement->as.command.args.items[i]);
            }
            fputs(")\n", out);
            break;
        case ER_STMT_SCREEN_ADD:
            fputs("screen.add\n", out);
            er_dump_element(out, statement->as.add.element, indent + 1);
            break;
        case ER_STMT_SCREEN_SETTEXT:
            fputs("screen.setText(", out);
            er_dump_value(out, &statement->as.set_text.element_id);
            fputs(", ", out);
            er_dump_value(out, &statement->as.set_text.value);
            fputs(")\n", out);
            break;
        case ER_STMT_VAR_SET:
            fprintf(out, "var.set(%s, ", statement->as.var_decl.name);
            er_dump_value(out, &statement->as.var_decl.value);
            fputs(")\n", out);
            break;
        case ER_STMT_IF:
            fputs("if\n", out);
            for (i = 0; i < statement->as.if_stmt.body.count; ++i) {
                er_dump_statement(out, statement->as.if_stmt.body.items[i], indent + 1);
            }
            break;
        case ER_STMT_WHILE:
            fputs("while\n", out);
            for (i = 0; i < statement->as.while_stmt.body.count; ++i) {
                er_dump_statement(out, statement->as.while_stmt.body.items[i], indent + 1);
            }
            break;
        case ER_STMT_FOR:
            fprintf(out, "for %s\n", statement->as.for_stmt.iterator_name ? statement->as.for_stmt.iterator_name : "<iter>");
            for (i = 0; i < statement->as.for_stmt.body.count; ++i) {
                er_dump_statement(out, statement->as.for_stmt.body.items[i], indent + 1);
            }
            break;
        case ER_STMT_TIMER_EVERY:
            fprintf(out, "timer.every %s\n", statement->as.timer_stmt.name ? statement->as.timer_stmt.name : "<timer>");
            for (i = 0; i < statement->as.timer_stmt.body.count; ++i) {
                er_dump_statement(out, statement->as.timer_stmt.body.items[i], indent + 1);
            }
            break;
    }
}

void er_program_dump(FILE *out, const ErProgram *program) {
    size_t i;
    for (i = 0; i < program->statements.count; ++i) {
        er_dump_statement(out, program->statements.items[i], 0);
    }
}
