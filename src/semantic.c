#include "semantic.h"

#include <stdlib.h>
#include <string.h>

typedef struct ErSemanticContext {
    char **ids;
    size_t id_count;
    size_t id_capacity;
} ErSemanticContext;

static char *er_semantic_dup(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *) malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void er_semantic_context_free(ErSemanticContext *context) {
    size_t i;

    if (!context) {
        return;
    }

    for (i = 0; i < context->id_count; ++i) {
        free(context->ids[i]);
    }
    free(context->ids);
    memset(context, 0, sizeof(*context));
}

static bool er_semantic_context_has_id(const ErSemanticContext *context, const char *id) {
    size_t i;

    if (!context || !id) {
        return false;
    }

    for (i = 0; i < context->id_count; ++i) {
        if (strcmp(context->ids[i], id) == 0) {
            return true;
        }
    }

    return false;
}

static bool er_semantic_context_register_id(
    const ErStatement *statement,
    ErSemanticContext *context,
    const char *id,
    ErError *error
) {
    size_t new_capacity;
    char **new_ids;
    char *copy;

    if (!context || !id) {
        return true;
    }

    if (er_semantic_context_has_id(context, id)) {
        er_error_set(
            error,
            statement ? statement->line : 0,
            statement ? statement->column : 0,
            "Duplicate element id '%s'",
            id
        );
        return false;
    }

    if (context->id_count >= context->id_capacity) {
        new_capacity = context->id_capacity == 0 ? 8 : context->id_capacity * 2;
        new_ids = (char **) realloc(context->ids, new_capacity * sizeof(char *));
        if (!new_ids) {
            er_error_set(
                error,
                statement ? statement->line : 0,
                statement ? statement->column : 0,
                "Out of memory while tracking element ids"
            );
            return false;
        }
        context->ids = new_ids;
        context->id_capacity = new_capacity;
    }

    copy = er_semantic_dup(id);
    if (!copy) {
        er_error_set(
            error,
            statement ? statement->line : 0,
            statement ? statement->column : 0,
            "Out of memory while storing element id"
        );
        return false;
    }

    context->ids[context->id_count++] = copy;
    return true;
}

static bool er_semantic_fail(const ErStatement *statement, ErError *error, const char *message) {
    er_error_set(
        error,
        statement ? statement->line : 0,
        statement ? statement->column : 0,
        "%s",
        message
    );
    return false;
}

static bool er_semantic_validate_arg_count(
    const ErStatement *statement,
    size_t count,
    size_t expected_min,
    size_t expected_max,
    ErError *error,
    const char *name
) {
    if (count < expected_min || count > expected_max) {
        if (expected_min == expected_max) {
            er_error_set(
                error,
                statement->line,
                statement->column,
                "%s expects %zu argument%s, got %zu",
                name,
                expected_min,
                expected_min == 1 ? "" : "s",
                count
            );
        } else {
            er_error_set(
                error,
                statement->line,
                statement->column,
                "%s expects between %zu and %zu arguments, got %zu",
                name,
                expected_min,
                expected_max,
                count
            );
        }
        return false;
    }

    return true;
}

static bool er_semantic_is_known_element_kind(const char *kind) {
    return strcmp(kind, "text") == 0 ||
           strcmp(kind, "label") == 0 ||
           strcmp(kind, "btn") == 0 ||
           strcmp(kind, "button") == 0 ||
           strcmp(kind, "input") == 0 ||
           strcmp(kind, "image") == 0 ||
           strcmp(kind, "webview") == 0 ||
           strcmp(kind, "box") == 0 ||
           strcmp(kind, "card") == 0 ||
           strcmp(kind, "panel") == 0;
}

static bool er_semantic_is_common_property(const char *name) {
    return strcmp(name, "x") == 0 ||
           strcmp(name, "y") == 0 ||
           strcmp(name, "w") == 0 ||
           strcmp(name, "h") == 0 ||
           strcmp(name, "id") == 0 ||
           strcmp(name, "page") == 0 ||
           strcmp(name, "preset") == 0 ||
           strcmp(name, "color") == 0 ||
           strcmp(name, "bg") == 0 ||
           strcmp(name, "bg2") == 0 ||
           strcmp(name, "border") == 0 ||
           strcmp(name, "borderColor") == 0 ||
           strcmp(name, "radius") == 0 ||
           strcmp(name, "padding") == 0 ||
           strcmp(name, "shadow") == 0 ||
           strcmp(name, "shadowColor") == 0 ||
           strcmp(name, "align") == 0 ||
           strcmp(name, "onLoad") == 0;
}

static bool er_semantic_is_property_allowed(const char *kind, const char *name) {
    if (er_semantic_is_common_property(name)) {
        return true;
    }

    if (strcmp(kind, "text") == 0 || strcmp(kind, "label") == 0) {
        return strcmp(name, "text") == 0 ||
               strcmp(name, "value") == 0 ||
               strcmp(name, "size") == 0;
    }

    if (strcmp(kind, "btn") == 0 || strcmp(kind, "button") == 0) {
        return strcmp(name, "text") == 0 ||
               strcmp(name, "value") == 0 ||
               strcmp(name, "icon") == 0 ||
               strcmp(name, "iconSize") == 0 ||
               strcmp(name, "size") == 0 ||
               strcmp(name, "onClick") == 0;
    }

    if (strcmp(kind, "input") == 0) {
        return strcmp(name, "text") == 0 ||
               strcmp(name, "value") == 0 ||
               strcmp(name, "placeholder") == 0 ||
               strcmp(name, "multiline") == 0 ||
               strcmp(name, "readonly") == 0 ||
               strcmp(name, "readOnly") == 0 ||
               strcmp(name, "bind") == 0 ||
               strcmp(name, "size") == 0 ||
               strcmp(name, "onChange") == 0 ||
               strcmp(name, "onClick") == 0;
    }

    if (strcmp(kind, "image") == 0) {
        return strcmp(name, "src") == 0 ||
               strcmp(name, "fit") == 0 ||
               strcmp(name, "iconSize") == 0 ||
               strcmp(name, "onClick") == 0;
    }

    if (strcmp(kind, "webview") == 0) {
        return strcmp(name, "url") == 0;
    }

    if (strcmp(kind, "box") == 0 || strcmp(kind, "card") == 0 || strcmp(kind, "panel") == 0) {
        return strcmp(name, "text") == 0 ||
               strcmp(name, "value") == 0 ||
               strcmp(name, "onClick") == 0;
    }

    return false;
}

static bool er_semantic_validate_property_counts(
    const ErStatement *statement,
    const ErElement *element,
    const ErProperty *property,
    ErError *error
) {
    size_t expected = property->is_event ? property->block.count : property->values.count;

    (void) element;

    if (property->is_event) {
        if (expected == 0) {
            er_error_set(error, statement->line, statement->column, "Event '%s' must contain at least one statement", property->name);
            return false;
        }
        return true;
    }

    if (expected != 1) {
        er_error_set(error, statement->line, statement->column, "Property '%s' expects exactly one value", property->name);
        return false;
    }

    return true;
}

static const ErProperty *er_semantic_find_property(const ErElement *element, const char *name) {
    size_t i;

    if (!element || !name) {
        return NULL;
    }

    for (i = 0; i < element->properties.count; ++i) {
        if (strcmp(element->properties.items[i].name, name) == 0) {
            return &element->properties.items[i];
        }
    }

    return NULL;
}

static const char *er_semantic_literal_text(const ErValue *value) {
    if (!value) {
        return NULL;
    }

    switch (value->type) {
        case ER_VALUE_STRING:
            return value->as.string;
        case ER_VALUE_SYMBOL:
            return value->as.symbol;
        default:
            return NULL;
    }
}

static bool er_semantic_validate_non_empty_literal_property(
    const ErStatement *statement,
    const char *element_kind,
    const ErProperty *property,
    ErError *error
) {
    const char *text;

    if (!property || property->values.count == 0) {
        return true;
    }

    text = er_semantic_literal_text(&property->values.items[0]);
    if (text && text[0] == '\0') {
        er_error_set(
            error,
            statement ? statement->line : 0,
            statement ? statement->column : 0,
            "Element '%s' property '%s' cannot be empty",
            element_kind ? element_kind : "<unknown>",
            property->name ? property->name : "<unknown>"
        );
        return false;
    }

    return true;
}

static bool er_semantic_validate_value(const ErStatement *statement, const ErValue *value, ErError *error) {
    size_t i;

    if (!value) {
        return er_semantic_fail(statement, error, "Encountered a null value in semantic analysis");
    }

    if (value->type != ER_VALUE_CALL) {
        return true;
    }

    if (!value->as.call || !value->as.call->name || value->as.call->name[0] == '\0') {
        return er_semantic_fail(statement, error, "Call expressions require a target name");
    }
    if (strcmp(value->as.call->name, "py.call") != 0 &&
        strcmp(value->as.call->name, "time.now") != 0 &&
        strcmp(value->as.call->name, "sys.cwd") != 0 &&
        strcmp(value->as.call->name, "sys.hostname") != 0 &&
        strcmp(value->as.call->name, "sys.platform") != 0 &&
        strcmp(value->as.call->name, "dialog.openFiles") != 0 &&
        strcmp(value->as.call->name, "dialog.openFile") != 0 &&
        strcmp(value->as.call->name, "dialog.saveFile") != 0 &&
        strcmp(value->as.call->name, "storage.read") != 0 &&
        strcmp(value->as.call->name, "storage.exists") != 0 &&
        strcmp(value->as.call->name, "text.upper") != 0 &&
        strcmp(value->as.call->name, "text.lower") != 0 &&
        strcmp(value->as.call->name, "text.title") != 0 &&
        strcmp(value->as.call->name, "text.length") != 0 &&
        strcmp(value->as.call->name, "text.contains") != 0 &&
        strcmp(value->as.call->name, "text.concat") != 0 &&
        strcmp(value->as.call->name, "math.add") != 0 &&
        strcmp(value->as.call->name, "math.sub") != 0 &&
        strcmp(value->as.call->name, "math.mul") != 0 &&
        strcmp(value->as.call->name, "math.div") != 0 &&
        strcmp(value->as.call->name, "math.min") != 0 &&
        strcmp(value->as.call->name, "math.max") != 0 &&
        strcmp(value->as.call->name, "media.state") != 0 &&
        strcmp(value->as.call->name, "media.position") != 0 &&
        strcmp(value->as.call->name, "media.duration") != 0 &&
        strcmp(value->as.call->name, "media.volume") != 0 &&
        strcmp(value->as.call->name, "media.muted") != 0 &&
        strcmp(value->as.call->name, "media.shuffle") != 0 &&
        strcmp(value->as.call->name, "media.repeat") != 0 &&
        strcmp(value->as.call->name, "media.count") != 0 &&
        strcmp(value->as.call->name, "media.index") != 0 &&
        strcmp(value->as.call->name, "media.currentName") != 0 &&
        strcmp(value->as.call->name, "media.currentTitle") != 0 &&
        strcmp(value->as.call->name, "media.currentArtist") != 0 &&
        strcmp(value->as.call->name, "media.currentYear") != 0 &&
        strcmp(value->as.call->name, "media.currentBitrate") != 0 &&
        strcmp(value->as.call->name, "media.currentSampleRate") != 0 &&
        strcmp(value->as.call->name, "media.currentArt") != 0 &&
        strcmp(value->as.call->name, "media.positionText") != 0 &&
        strcmp(value->as.call->name, "media.remainingText") != 0 &&
        strcmp(value->as.call->name, "media.durationText") != 0 &&
        strcmp(value->as.call->name, "media.playlistText") != 0) {
        er_error_set(
            error,
            statement->line,
            statement->column,
            "Unsupported value call: %s",
            value->as.call->name
        );
        return false;
    }
    if (strcmp(value->as.call->name, "py.call") == 0 && value->as.call->args.count == 0) {
        return er_semantic_fail(statement, error, "py.call expects at least one argument");
    }

    for (i = 0; i < value->as.call->args.count; ++i) {
        if (!er_semantic_validate_value(statement, &value->as.call->args.items[i], error)) {
            return false;
        }
    }

    return true;
}

static bool er_semantic_validate_value_array(
    const ErStatement *statement,
    const ErValueArray *values,
    ErError *error
) {
    size_t i;

    for (i = 0; i < values->count; ++i) {
        if (!er_semantic_validate_value(statement, &values->items[i], error)) {
            return false;
        }
    }

    return true;
}

static bool er_semantic_validate_condition(const ErStatement *statement, const ErCondition *condition, ErError *error) {
    if (!condition) {
        return er_semantic_fail(statement, error, "Condition cannot be empty");
    }

    return er_semantic_validate_value(statement, &condition->left, error) &&
           er_semantic_validate_value(statement, &condition->right, error);
}

static bool er_semantic_validate_generic_command(const ErStatement *statement, ErError *error) {
    const char *name;

    if (!statement || !statement->as.command.name) {
        return er_semantic_fail(statement, error, "Generic command is missing a name");
    }

    name = statement->as.command.name;

    if (strcmp(name, "webview.open") == 0 || strcmp(name, "webview.runJS") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 2, 2, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "webview.back") == 0 ||
        strcmp(name, "webview.forward") == 0 ||
        strcmp(name, "webview.reload") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "window.icon") == 0 || strcmp(name, "screen.icon") == 0 ||
        strcmp(name, "window.caption") == 0 ||
        strcmp(name, "timer.stop") == 0 ||
        strcmp(name, "console.open") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "screen.setImage") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 2, 2, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "storage.write") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 2, 2, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "shortcut.bind") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 2, 2, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "screen.setBounds") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 5, 5, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "media.addFiles") == 0 ||
        strcmp(name, "media.openPlaylist") == 0 ||
        strcmp(name, "media.savePlaylist") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "media.clear") == 0 ||
        strcmp(name, "media.play") == 0 ||
        strcmp(name, "media.pause") == 0 ||
        strcmp(name, "media.playPause") == 0 ||
        strcmp(name, "media.stop") == 0 ||
        strcmp(name, "media.next") == 0 ||
        strcmp(name, "media.previous") == 0 ||
        strcmp(name, "media.toggleMute") == 0 ||
        strcmp(name, "media.toggleShuffle") == 0 ||
        strcmp(name, "media.cycleRepeat") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 0, 0, error, name)) {
            return false;
        }
        return true;
    }

    if (strcmp(name, "media.seek") == 0 ||
        strcmp(name, "media.seekRelative") == 0 ||
        strcmp(name, "media.setVolume") == 0 ||
        strcmp(name, "media.changeVolume") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "anim.play") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 2, 4, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "anim.oscillate") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 4, 5, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "anim.keyframes") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 4, 5, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    if (strcmp(name, "anim.stop") == 0) {
        if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, name)) {
            return false;
        }
        return er_semantic_validate_value_array(statement, &statement->as.command.args, error);
    }

    er_error_set(
        error,
        statement->line,
        statement->column,
        "Unsupported command in Phase 1: %s",
        name
    );
    return false;
}

static bool er_semantic_validate_element(
    const ErStatement *statement,
    const ErElement *element,
    ErSemanticContext *context,
    ErError *error
);

static bool er_semantic_validate_block(
    const ErStatementArray *body,
    ErSemanticContext *context,
    ErError *error
) {
    size_t i;

    for (i = 0; i < body->count; ++i) {
        const ErStatement *statement = body->items[i];
        if (!statement) {
            er_error_set(error, 0, 0, "Encountered a null statement in semantic analysis");
            return false;
        }

        switch (statement->type) {
            case ER_STMT_IMPORT:
                if (statement->as.import_directive.type == ER_IMPORT_DIRECTIVE_PYTHON) {
                    if (!statement->as.import_directive.path || statement->as.import_directive.path[0] == '\0') {
                        return er_semantic_fail(statement, error, "Python import requires a script path");
                    }
                    if (!statement->as.import_directive.alias || statement->as.import_directive.alias[0] == '\0') {
                        return er_semantic_fail(statement, error, "Python import requires an alias");
                    }
                } else {
                    if (!statement->as.import_directive.provider || statement->as.import_directive.provider[0] == '\0') {
                        return er_semantic_fail(statement, error, "Import directive is missing a provider");
                    }
                    if (!er_semantic_validate_value_array(statement, &statement->as.import_directive.args, error)) {
                        return false;
                    }
                }
                break;
            case ER_STMT_SCREEN_CREATE:
                if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 4, 6, error, "screen.create")) {
                    return false;
                }
                if (!(statement->as.command.args.count == 4 || statement->as.command.args.count == 6)) {
                    return er_semantic_fail(statement, error, "screen.create currently supports 4 or 6 arguments only");
                }
                if (!er_semantic_validate_value_array(statement, &statement->as.command.args, error)) {
                    return false;
                }
                break;
            case ER_STMT_SCREEN_TITLE:
                if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, "screen.title")) {
                    return false;
                }
                if (!er_semantic_validate_value_array(statement, &statement->as.command.args, error)) {
                    return false;
                }
                break;
            case ER_STMT_SCREEN_BG:
                if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, "screen.bg")) {
                    return false;
                }
                if (!er_semantic_validate_value_array(statement, &statement->as.command.args, error)) {
                    return false;
                }
                break;
            case ER_STMT_SCREEN_SHOW:
                if (!er_semantic_validate_arg_count(statement, statement->as.command.args.count, 1, 1, error, "screen.show")) {
                    return false;
                }
                if (!er_semantic_validate_value_array(statement, &statement->as.command.args, error)) {
                    return false;
                }
                break;
            case ER_STMT_SCREEN_SETTEXT:
                if (!er_semantic_validate_value(statement, &statement->as.set_text.element_id, error) ||
                    !er_semantic_validate_value(statement, &statement->as.set_text.value, error)) {
                    return false;
                }
                break;
            case ER_STMT_VAR_SET:
                if (!statement->as.var_decl.name || statement->as.var_decl.name[0] == '\0') {
                    return er_semantic_fail(statement, error, "var.set requires a non-empty variable name");
                }
                if (!er_semantic_validate_value(statement, &statement->as.var_decl.value, error)) {
                    return false;
                }
                break;
            case ER_STMT_SCREEN_ADD:
                if (!statement->as.add.element) {
                    return er_semantic_fail(statement, error, "screen.add requires an element");
                }
                if (!er_semantic_validate_element(statement, statement->as.add.element, context, error)) {
                    return false;
                }
                break;
            case ER_STMT_IF:
                if (!er_semantic_validate_condition(statement, &statement->as.if_stmt.condition, error)) {
                    return false;
                }
                if (!er_semantic_validate_block(&statement->as.if_stmt.body, context, error)) {
                    return false;
                }
                {
                    size_t else_if_index;
                    for (else_if_index = 0; else_if_index < statement->as.if_stmt.else_ifs.count; ++else_if_index) {
                        if (!er_semantic_validate_condition(statement, &statement->as.if_stmt.else_ifs.items[else_if_index].condition, error)) {
                            return false;
                        }
                        if (!er_semantic_validate_block(&statement->as.if_stmt.else_ifs.items[else_if_index].body, context, error)) {
                            return false;
                        }
                    }
                }
                if (statement->as.if_stmt.has_else &&
                    !er_semantic_validate_block(&statement->as.if_stmt.else_body, context, error)) {
                    return false;
                }
                break;
            case ER_STMT_WHILE:
                if (!er_semantic_validate_condition(statement, &statement->as.while_stmt.condition, error)) {
                    return false;
                }
                if (!er_semantic_validate_block(&statement->as.while_stmt.body, context, error)) {
                    return false;
                }
                break;
            case ER_STMT_FOR:
                if (!statement->as.for_stmt.iterator_name || statement->as.for_stmt.iterator_name[0] == '\0') {
                    return er_semantic_fail(statement, error, "for requires a non-empty iterator name");
                }
                if (!er_semantic_validate_value(statement, &statement->as.for_stmt.start, error) ||
                    !er_semantic_validate_value(statement, &statement->as.for_stmt.end, error)) {
                    return false;
                }
                if (statement->as.for_stmt.has_step &&
                    !er_semantic_validate_value(statement, &statement->as.for_stmt.step, error)) {
                    return false;
                }
                if (!er_semantic_validate_block(&statement->as.for_stmt.body, context, error)) {
                    return false;
                }
                break;
            case ER_STMT_TIMER_EVERY:
                if (!statement->as.timer_stmt.name || statement->as.timer_stmt.name[0] == '\0') {
                    return er_semantic_fail(statement, error, "timer.every requires a non-empty timer name");
                }
                if (!er_semantic_validate_value(statement, &statement->as.timer_stmt.interval, error)) {
                    return false;
                }
                if (!er_semantic_validate_block(&statement->as.timer_stmt.body, context, error)) {
                    return false;
                }
                break;
            case ER_STMT_GENERIC:
                return er_semantic_validate_generic_command(statement, error);
        }
    }

    return true;
}

static bool er_semantic_validate_element(
    const ErStatement *statement,
    const ErElement *element,
    ErSemanticContext *context,
    ErError *error
) {
    size_t i;
    size_t j;
    const ErProperty *id_property;
    const ErProperty *bind_property;
    const ErProperty *page_property;
    const ErProperty *preset_property;
    const ErProperty *url_property;
    const ErProperty *src_property;
    const char *literal_id = NULL;

    if (!element->kind || element->kind[0] == '\0') {
        return er_semantic_fail(statement, error, "Element kind cannot be empty");
    }

    if (!er_semantic_is_known_element_kind(element->kind)) {
        er_error_set(error, statement->line, statement->column, "Unknown element kind: %s", element->kind);
        return false;
    }

    if (element->base_property && !er_semantic_is_property_allowed(element->kind, element->base_property)) {
        er_error_set(
            error,
            statement->line,
            statement->column,
            "Element '%s' does not support base property '%s'",
            element->kind,
            element->base_property
        );
        return false;
    }

    if (element->base_property && element->base_args.count != 1) {
        er_error_set(
            error,
            statement->line,
            statement->column,
            "Element '%s' base property '%s' expects exactly one value",
            element->kind,
            element->base_property
        );
        return false;
    }

    if (!er_semantic_validate_value_array(statement, &element->base_args, error)) {
        return false;
    }

    for (i = 0; i < element->properties.count; ++i) {
        const ErProperty *property = &element->properties.items[i];

        if (!er_semantic_is_property_allowed(element->kind, property->name)) {
            er_error_set(
                error,
                statement->line,
                statement->column,
                "Element '%s' does not support property '%s'",
                element->kind,
                property->name
            );
            return false;
        }

        if (!er_semantic_validate_property_counts(statement, element, property, error)) {
            return false;
        }

        if (property->is_event && !er_semantic_validate_block(&property->block, context, error)) {
            return false;
        }
        if (!property->is_event && !er_semantic_validate_value_array(statement, &property->values, error)) {
            return false;
        }

        for (j = i + 1; j < element->properties.count; ++j) {
            if (strcmp(property->name, element->properties.items[j].name) == 0) {
                er_error_set(
                    error,
                    statement->line,
                    statement->column,
                    "Duplicate property '%s' on element '%s'",
                    property->name,
                    element->kind
                );
                return false;
            }
        }
    }

    if ((strcmp(element->kind, "webview") == 0) &&
        !(element->base_property && strcmp(element->base_property, "url") == 0) &&
        !er_semantic_find_property(element, "url")) {
        er_error_set(error, statement->line, statement->column, "Element '%s' requires a url", element->kind);
        return false;
    }

    if ((strcmp(element->kind, "image") == 0) &&
        !(element->base_property && strcmp(element->base_property, "src") == 0) &&
        !er_semantic_find_property(element, "src")) {
        er_error_set(error, statement->line, statement->column, "Element '%s' requires a src", element->kind);
        return false;
    }

    id_property = er_semantic_find_property(element, "id");
    bind_property = er_semantic_find_property(element, "bind");
    page_property = er_semantic_find_property(element, "page");
    preset_property = er_semantic_find_property(element, "preset");
    url_property = er_semantic_find_property(element, "url");
    src_property = er_semantic_find_property(element, "src");

    if (!er_semantic_validate_non_empty_literal_property(statement, element->kind, id_property, error) ||
        !er_semantic_validate_non_empty_literal_property(statement, element->kind, bind_property, error) ||
        !er_semantic_validate_non_empty_literal_property(statement, element->kind, page_property, error) ||
        !er_semantic_validate_non_empty_literal_property(statement, element->kind, preset_property, error) ||
        !er_semantic_validate_non_empty_literal_property(statement, element->kind, url_property, error) ||
        !er_semantic_validate_non_empty_literal_property(statement, element->kind, src_property, error)) {
        return false;
    }

    if (id_property && id_property->values.count > 0) {
        literal_id = er_semantic_literal_text(&id_property->values.items[0]);
        if (literal_id && !er_semantic_context_register_id(statement, context, literal_id, error)) {
            return false;
        }
    }

    return true;
}

bool er_semantic_analyze_program(const char *source_name, const ErProgram *program, ErError *error) {
    ErSemanticContext context;
    bool ok;

    (void) source_name;

    er_error_clear(error);
    memset(&context, 0, sizeof(context));

    if (!program) {
        er_error_set(error, 0, 0, "Semantic analysis requires a program");
        return false;
    }

    ok = er_semantic_validate_block(&program->statements, &context, error);
    er_semantic_context_free(&context);
    return ok;
}
