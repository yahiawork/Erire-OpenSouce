#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

typedef struct ErParser {
    ErTokenArray tokens;
    size_t current;
    ErError *error;
} ErParser;

static char *er_parser_dup_range(const char *start, size_t length) {
    char *text = (char *) malloc(length + 1);
    if (!text) {
        return NULL;
    }
    memcpy(text, start, length);
    text[length] = '\0';
    return text;
}

static char *er_parser_dup_token(ErToken token) {
    return er_parser_dup_range(token.start, token.length);
}

static const ErToken *er_parser_peek(const ErParser *parser) {
    return &parser->tokens.items[parser->current];
}

static const ErToken *er_parser_previous(const ErParser *parser) {
    return &parser->tokens.items[parser->current - 1];
}

static bool er_parser_is_at_end(const ErParser *parser) {
    return er_parser_peek(parser)->type == ER_TOKEN_EOF;
}

static const ErToken *er_parser_advance(ErParser *parser) {
    if (!er_parser_is_at_end(parser)) {
        parser->current++;
    }
    return er_parser_previous(parser);
}

static bool er_parser_check(const ErParser *parser, ErTokenType type) {
    return er_parser_peek(parser)->type == type;
}

static bool er_parser_match(ErParser *parser, ErTokenType type) {
    if (!er_parser_check(parser, type)) {
        return false;
    }
    er_parser_advance(parser);
    return true;
}

static void er_parser_skip_newlines(ErParser *parser) {
    while (er_parser_match(parser, ER_TOKEN_NEWLINE)) {
    }
}

static void er_parser_skip_statement_separators(ErParser *parser) {
    while (er_parser_match(parser, ER_TOKEN_NEWLINE) || er_parser_match(parser, ER_TOKEN_SEMICOLON)) {
    }
}

static bool er_parser_fail_at(ErParser *parser, const ErToken *token, const char *message) {
    er_error_set(parser->error, token->line, token->column, "%s", message);
    return false;
}

static bool er_parser_expect(ErParser *parser, ErTokenType type, const char *message) {
    if (er_parser_match(parser, type)) {
        return true;
    }
    return er_parser_fail_at(parser, er_parser_peek(parser), message);
}

static bool er_parser_parse_qualified_name(ErParser *parser, char **out_name, bool allow_if_keyword);
static bool er_parser_parse_value_list(ErParser *parser, ErValueArray *values, ErTokenType closing);
static bool er_parser_parse_block(ErParser *parser, ErStatementArray *out_body);

static bool er_parser_parse_call_value(ErParser *parser, char *name, ErValue *out_value) {
    ErCallExpression *call = er_call_expression_create(name);

    if (!call) {
        free(name);
        return er_parser_fail_at(parser, er_parser_peek(parser), "Out of memory while creating call expression");
    }

    free(name);

    if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after call expression")) {
        er_call_expression_free(call);
        return false;
    }
    if (!er_parser_parse_value_list(parser, &call->args, ER_TOKEN_RBRACKET)) {
        er_call_expression_free(call);
        return false;
    }
    if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after call arguments")) {
        er_call_expression_free(call);
        return false;
    }

    *out_value = er_value_make_call(call);
    return true;
}

static bool er_parser_parse_value(ErParser *parser, ErValue *out_value) {
    const ErToken *token;
    char *text;
    char *name;

    er_parser_skip_newlines(parser);

    if (er_parser_match(parser, ER_TOKEN_DOLLAR)) {
        if (!er_parser_parse_qualified_name(parser, &name, false)) {
            return false;
        }
        *out_value = er_value_make_variable(name);
        free(name);
        return true;
    }

    token = er_parser_peek(parser);

    switch (token->type) {
        case ER_TOKEN_STRING:
            er_parser_advance(parser);
            text = er_parser_dup_range(token->start + 1, token->length >= 2 ? token->length - 2 : 0);
            if (!text) {
                return er_parser_fail_at(parser, token, "Out of memory while reading string");
            }
            *out_value = er_value_make_string(text);
            free(text);
            return true;
        case ER_TOKEN_NUMBER:
            er_parser_advance(parser);
            text = er_parser_dup_token(*token);
            if (!text) {
                return er_parser_fail_at(parser, token, "Out of memory while reading number");
            }
            *out_value = er_value_make_number(strtod(text, NULL));
            free(text);
            return true;
        case ER_TOKEN_TRUE:
            er_parser_advance(parser);
            *out_value = er_value_make_bool(true);
            return true;
        case ER_TOKEN_FALSE:
            er_parser_advance(parser);
            *out_value = er_value_make_bool(false);
            return true;
        case ER_TOKEN_IDENTIFIER:
            if (!er_parser_parse_qualified_name(parser, &name, false)) {
                return false;
            }
            if (er_parser_check(parser, ER_TOKEN_LBRACKET)) {
                return er_parser_parse_call_value(parser, name, out_value);
            }
            *out_value = er_value_make_symbol(name);
            free(name);
            return true;
        default:
            return er_parser_fail_at(parser, token, "Expected a literal value");
    }
}

static bool er_parser_parse_value_list(ErParser *parser, ErValueArray *values, ErTokenType closing) {
    ErValue value;

    er_parser_skip_newlines(parser);
    if (er_parser_check(parser, closing)) {
        return true;
    }

    do {
        if (!er_parser_parse_value(parser, &value)) {
            return false;
        }
        if (!er_value_array_push(values, value)) {
            er_value_free(&value);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Out of memory while growing value list");
        }
        er_parser_skip_newlines(parser);
    } while (er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA));

    return true;
}

static bool er_parser_parse_qualified_name(ErParser *parser, char **out_name, bool allow_if_keyword) {
    char *name = NULL;
    size_t total = 0;
    size_t count = 0;
    ErToken parts[16];
    size_t i;
    char *cursor;

    er_parser_skip_newlines(parser);

    if (!(er_parser_check(parser, ER_TOKEN_IDENTIFIER) || (allow_if_keyword && er_parser_check(parser, ER_TOKEN_IF)))) {
        return er_parser_fail_at(parser, er_parser_peek(parser), "Expected identifier");
    }

    parts[count++] = *er_parser_advance(parser);

    while (er_parser_match(parser, ER_TOKEN_DOT)) {
        if (!er_parser_expect(parser, ER_TOKEN_IDENTIFIER, "Expected identifier after '.'")) {
            return false;
        }
        if (count >= 16) {
            return er_parser_fail_at(parser, er_parser_previous(parser), "Qualified name is too long");
        }
        parts[count++] = *er_parser_previous(parser);
    }

    for (i = 0; i < count; ++i) {
        total += parts[i].length;
    }
    total += count > 0 ? count - 1 : 0;

    name = (char *) malloc(total + 1);
    if (!name) {
        return er_parser_fail_at(parser, er_parser_peek(parser), "Out of memory while building name");
    }

    cursor = name;
    for (i = 0; i < count; ++i) {
        memcpy(cursor, parts[i].start, parts[i].length);
        cursor += parts[i].length;
        if (i + 1 < count) {
            *cursor++ = '.';
        }
    }
    *cursor = '\0';

    *out_name = name;
    return true;
}

static bool er_parser_parse_condition(ErParser *parser, ErCondition *condition) {
    const ErToken *token;

    if (!er_parser_parse_value(parser, &condition->left)) {
        return false;
    }

    token = er_parser_peek(parser);
    switch (token->type) {
        case ER_TOKEN_EQUAL_EQUAL: condition->op = ER_COMPARE_EQ; break;
        case ER_TOKEN_BANG_EQUAL: condition->op = ER_COMPARE_NEQ; break;
        case ER_TOKEN_LESS: condition->op = ER_COMPARE_LT; break;
        case ER_TOKEN_LESS_EQUAL: condition->op = ER_COMPARE_LTE; break;
        case ER_TOKEN_GREATER: condition->op = ER_COMPARE_GT; break;
        case ER_TOKEN_GREATER_EQUAL: condition->op = ER_COMPARE_GTE; break;
        default:
            er_value_free(&condition->left);
            return er_parser_fail_at(parser, token, "Expected comparison operator");
    }
    er_parser_advance(parser);

    if (!er_parser_parse_value(parser, &condition->right)) {
        er_value_free(&condition->left);
        return false;
    }

    return true;
}

static bool er_parser_parse_condition_block(
    ErParser *parser,
    ErCondition *condition,
    ErStatementArray *body,
    const char *keyword
) {
    char message[96];

    er_parser_skip_newlines(parser);
    snprintf(message, sizeof(message), "Expected '[' after '%s'", keyword);
    if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, message)) {
        return false;
    }
    if (!er_parser_parse_condition(parser, condition)) {
        return false;
    }
    if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after condition")) {
        return false;
    }
    er_parser_skip_newlines(parser);
    snprintf(message, sizeof(message), "Expected '[' before %s block", keyword);
    if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, message)) {
        return false;
    }
    return er_parser_parse_block(parser, body);
}

static bool er_parser_parse_statement(ErParser *parser, ErStatement **out_statement);

static bool er_parser_parse_block(ErParser *parser, ErStatementArray *out_body) {
    ErStatement *statement;

    er_parser_skip_statement_separators(parser);
    while (!er_parser_check(parser, ER_TOKEN_RBRACKET) && !er_parser_is_at_end(parser)) {
        if (!er_parser_parse_statement(parser, &statement)) {
            return false;
        }
        if (statement) {
            if (!er_statement_array_push(out_body, statement)) {
                er_statement_free(statement);
                return er_parser_fail_at(parser, er_parser_peek(parser), "Out of memory while growing statement block");
            }
        }
        er_parser_skip_statement_separators(parser);
    }

    return er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after block");
}

static bool er_parser_is_event_name(const char *name) {
    return strcmp(name, "onClick") == 0 || strcmp(name, "onChange") == 0 || strcmp(name, "onLoad") == 0;
}

static bool er_parser_parse_element(ErParser *parser, ErElement **out_element) {
    ErElement *element = er_element_create();
    char *text;

    if (!element) {
        return er_parser_fail_at(parser, er_parser_peek(parser), "Out of memory while creating element");
    }

    er_parser_skip_newlines(parser);
    if (!er_parser_expect(parser, ER_TOKEN_IDENTIFIER, "Expected UI element kind")) {
        er_element_free(element);
        return false;
    }

    text = er_parser_dup_token(*er_parser_previous(parser));
    element->kind = text;

    if (er_parser_match(parser, ER_TOKEN_DOT)) {
        if (!er_parser_expect(parser, ER_TOKEN_IDENTIFIER, "Expected property name after '.'")) {
            er_element_free(element);
            return false;
        }
        element->base_property = er_parser_dup_token(*er_parser_previous(parser));

        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after base property")) {
            er_element_free(element);
            return false;
        }
        if (!er_parser_parse_value_list(parser, &element->base_args, ER_TOKEN_RBRACKET)) {
            er_element_free(element);
            return false;
        }
        if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after base property arguments")) {
            er_element_free(element);
            return false;
        }
    }

    for (;;) {
        ErProperty property;
        memset(&property, 0, sizeof(property));

        er_parser_skip_newlines(parser);
        if (!er_parser_match(parser, ER_TOKEN_DOT)) {
            break;
        }

        if (!er_parser_expect(parser, ER_TOKEN_IDENTIFIER, "Expected property name after '.'")) {
            er_element_free(element);
            return false;
        }

        property.name = er_parser_dup_token(*er_parser_previous(parser));
        property.is_event = er_parser_is_event_name(property.name);

        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after property name")) {
            free(property.name);
            er_element_free(element);
            return false;
        }

        if (property.is_event) {
            if (!er_parser_parse_block(parser, &property.block)) {
                free(property.name);
                er_element_free(element);
                return false;
            }
        } else {
            if (!er_parser_parse_value_list(parser, &property.values, ER_TOKEN_RBRACKET)) {
                free(property.name);
                er_element_free(element);
                return false;
            }
            if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after property values")) {
                free(property.name);
                er_element_free(element);
                return false;
            }
        }

        if (!er_property_array_push(&element->properties, property)) {
            size_t j;
            free(property.name);
            for (j = 0; j < property.values.count; ++j) {
                er_value_free(&property.values.items[j]);
            }
            free(property.values.items);
            for (j = 0; j < property.block.count; ++j) {
                er_statement_free(property.block.items[j]);
            }
            free(property.block.items);
            er_element_free(element);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Out of memory while growing property list");
        }
    }

    *out_element = element;
    return true;
}

static ErStatementType er_parser_statement_type_for_name(const char *name) {
    if (strcmp(name, "screen.create") == 0) return ER_STMT_SCREEN_CREATE;
    if (strcmp(name, "screen.title") == 0) return ER_STMT_SCREEN_TITLE;
    if (strcmp(name, "screen.bg") == 0) return ER_STMT_SCREEN_BG;
    if (strcmp(name, "screen.show") == 0) return ER_STMT_SCREEN_SHOW;
    return ER_STMT_GENERIC;
}

static bool er_parser_parse_import(ErParser *parser, ErStatement **out_statement) {
    ErStatement *statement = er_statement_create(ER_STMT_IMPORT, er_parser_previous(parser)->line, er_parser_previous(parser)->column);

    if (!statement) {
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing import");
    }

    if (!er_parser_expect(parser, ER_TOKEN_FROM, "Expected 'from' after '@imp'")) {
        er_statement_free(statement);
        return false;
    }

    statement->as.import_directive.type = ER_IMPORT_DIRECTIVE_LEGACY;

    if (!er_parser_parse_qualified_name(parser, &statement->as.import_directive.provider, true)) {
        er_statement_free(statement);
        return false;
    }

    if (!er_parser_expect(parser, ER_TOKEN_LPAREN, "Expected '(' after import provider")) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_parse_value_list(parser, &statement->as.import_directive.args, ER_TOKEN_RPAREN)) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_expect(parser, ER_TOKEN_RPAREN, "Expected ')' after import")) {
        er_statement_free(statement);
        return false;
    }

    *out_statement = statement;
    return true;
}

static bool er_parser_parse_python_import(ErParser *parser, ErStatement **out_statement) {
    ErStatement *statement = er_statement_create(ER_STMT_IMPORT, er_parser_previous(parser)->line, er_parser_previous(parser)->column);
    const ErToken *path_token;

    if (!statement) {
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing import");
    }

    statement->as.import_directive.type = ER_IMPORT_DIRECTIVE_PYTHON;
    statement->as.import_directive.provider = er_parser_dup_range("python", 6);
    if (!statement->as.import_directive.provider) {
        er_statement_free(statement);
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing import");
    }

    if (!er_parser_expect(
        parser,
        ER_TOKEN_PYTHON,
        "Expected 'python' after 'import'. Erire loads backend.er automatically when it is beside main.er"
    )) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_expect(parser, ER_TOKEN_STRING, "Expected Python script path after 'import python'")) {
        er_statement_free(statement);
        return false;
    }

    path_token = er_parser_previous(parser);
    statement->as.import_directive.path = er_parser_dup_range(
        path_token->start + 1,
        path_token->length >= 2 ? path_token->length - 2 : 0
    );
    if (!statement->as.import_directive.path) {
        er_statement_free(statement);
        return er_parser_fail_at(parser, path_token, "Out of memory while reading import path");
    }

    if (!er_parser_expect(parser, ER_TOKEN_AS, "Expected 'as' after Python import path")) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_expect(parser, ER_TOKEN_IDENTIFIER, "Expected alias after 'as'")) {
        er_statement_free(statement);
        return false;
    }

    statement->as.import_directive.alias = er_parser_dup_token(*er_parser_previous(parser));
    if (!statement->as.import_directive.alias) {
        er_statement_free(statement);
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while reading import alias");
    }

    *out_statement = statement;
    return true;
}

static bool er_parser_parse_if_statement(ErParser *parser, ErStatement **out_statement) {
    ErStatement *statement = er_statement_create(ER_STMT_IF, er_parser_previous(parser)->line, er_parser_previous(parser)->column);

    if (!statement) {
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing if");
    }

    if (!er_parser_parse_condition_block(parser, &statement->as.if_stmt.condition, &statement->as.if_stmt.body, "if")) {
        er_statement_free(statement);
        return false;
    }

    for (;;) {
        ErElseIfClause clause;
        memset(&clause, 0, sizeof(clause));

        er_parser_skip_statement_separators(parser);
        if (!er_parser_match(parser, ER_TOKEN_ELSE)) {
            break;
        }

        if (statement->as.if_stmt.has_else) {
            er_statement_free(statement);
            return er_parser_fail_at(parser, er_parser_previous(parser), "else cannot appear after another else block");
        }

        if (er_parser_match(parser, ER_TOKEN_DOT)) {
            if (!er_parser_expect(parser, ER_TOKEN_IF, "Expected 'if' after 'else.'")) {
                er_statement_free(statement);
                return false;
            }
            if (!er_parser_parse_condition_block(parser, &clause.condition, &clause.body, "else.if")) {
                er_statement_free(statement);
                return false;
            }
            if (!er_else_if_clause_array_push(&statement->as.if_stmt.else_ifs, clause)) {
                er_statement_free(statement);
                return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while growing else.if list");
            }
            continue;
        }

        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' before else block")) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_parse_block(parser, &statement->as.if_stmt.else_body)) {
            er_statement_free(statement);
            return false;
        }
        statement->as.if_stmt.has_else = true;
        break;
    }

    *out_statement = statement;
    return true;
}

static bool er_parser_parse_while_statement(ErParser *parser, ErStatement **out_statement) {
    ErStatement *statement = er_statement_create(ER_STMT_WHILE, er_parser_previous(parser)->line, er_parser_previous(parser)->column);

    if (!statement) {
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing while");
    }

    if (!er_parser_parse_condition_block(parser, &statement->as.while_stmt.condition, &statement->as.while_stmt.body, "while")) {
        er_statement_free(statement);
        return false;
    }

    *out_statement = statement;
    return true;
}

static bool er_parser_parse_for_statement(ErParser *parser, ErStatement **out_statement) {
    ErStatement *statement = er_statement_create(ER_STMT_FOR, er_parser_previous(parser)->line, er_parser_previous(parser)->column);
    ErValue iterator_name;
    const char *name_text;

    if (!statement) {
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing for");
    }

    er_parser_skip_newlines(parser);
    if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after 'for'")) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_parse_value(parser, &iterator_name)) {
        er_statement_free(statement);
        return false;
    }

    if (iterator_name.type != ER_VALUE_SYMBOL && iterator_name.type != ER_VALUE_STRING) {
        er_value_free(&iterator_name);
        er_statement_free(statement);
        return er_parser_fail_at(parser, er_parser_peek(parser), "for iterator name must be a symbol or string");
    }

    name_text = iterator_name.type == ER_VALUE_SYMBOL ? iterator_name.as.symbol : iterator_name.as.string;
    statement->as.for_stmt.iterator_name = er_parser_dup_range(name_text, strlen(name_text));
    er_value_free(&iterator_name);
    if (!statement->as.for_stmt.iterator_name) {
        er_statement_free(statement);
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while reading for iterator name");
    }

    if (!(er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA))) {
        er_statement_free(statement);
        return er_parser_fail_at(parser, er_parser_peek(parser), "Expected ';' after for iterator name");
    }
    if (!er_parser_parse_value(parser, &statement->as.for_stmt.start)) {
        er_statement_free(statement);
        return false;
    }
    if (!(er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA))) {
        er_statement_free(statement);
        return er_parser_fail_at(parser, er_parser_peek(parser), "Expected ';' after for start value");
    }
    if (!er_parser_parse_value(parser, &statement->as.for_stmt.end)) {
        er_statement_free(statement);
        return false;
    }
    if (er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA)) {
        if (!er_parser_parse_value(parser, &statement->as.for_stmt.step)) {
            er_statement_free(statement);
            return false;
        }
        statement->as.for_stmt.has_step = true;
    }
    if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after for range")) {
        er_statement_free(statement);
        return false;
    }
    er_parser_skip_newlines(parser);
    if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' before for block")) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_parse_block(parser, &statement->as.for_stmt.body)) {
        er_statement_free(statement);
        return false;
    }

    *out_statement = statement;
    return true;
}

static bool er_parser_parse_command_statement(ErParser *parser, ErStatement **out_statement) {
    char *name = NULL;
    ErStatement *statement;
    ErValue name_value;

    if (!er_parser_parse_qualified_name(parser, &name, true)) {
        return false;
    }

    if (strcmp(name, "screen.add") == 0) {
        statement = er_statement_create(ER_STMT_SCREEN_ADD, er_parser_previous(parser)->line, er_parser_previous(parser)->column);
        free(name);
        if (!statement) {
            return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing screen.add");
        }
        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after screen.add")) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_parse_element(parser, &statement->as.add.element)) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after element")) {
            er_statement_free(statement);
            return false;
        }
        *out_statement = statement;
        return true;
    }

    if (strcmp(name, "screen.setText") == 0) {
        statement = er_statement_create(ER_STMT_SCREEN_SETTEXT, er_parser_previous(parser)->line, er_parser_previous(parser)->column);
        free(name);
        if (!statement) {
            return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing screen.setText");
        }
        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after screen.setText")) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_parse_value(parser, &statement->as.set_text.element_id)) {
            er_statement_free(statement);
            return false;
        }
        if (!(er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA))) {
            er_statement_free(statement);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Expected ';' between screen.setText arguments");
        }
        if (!er_parser_parse_value(parser, &statement->as.set_text.value)) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after screen.setText")) {
            er_statement_free(statement);
            return false;
        }
        *out_statement = statement;
        return true;
    }

    if (strcmp(name, "var.set") == 0) {
        statement = er_statement_create(ER_STMT_VAR_SET, er_parser_previous(parser)->line, er_parser_previous(parser)->column);
        free(name);
        if (!statement) {
            return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing var.set");
        }
        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after var.set")) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_parse_value(parser, &name_value)) {
            er_statement_free(statement);
            return false;
        }
        if (name_value.type != ER_VALUE_SYMBOL && name_value.type != ER_VALUE_STRING) {
            er_value_free(&name_value);
            er_statement_free(statement);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Variable name must be a symbol or string");
        }
        statement->as.var_decl.name = er_parser_dup_range(
            name_value.type == ER_VALUE_SYMBOL ? name_value.as.symbol : name_value.as.string,
            strlen(name_value.type == ER_VALUE_SYMBOL ? name_value.as.symbol : name_value.as.string)
        );
        er_value_free(&name_value);
        if (!(er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA))) {
            er_statement_free(statement);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Expected ';' between var.set arguments");
        }
        if (!er_parser_parse_value(parser, &statement->as.var_decl.value)) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after var.set")) {
            er_statement_free(statement);
            return false;
        }
        *out_statement = statement;
        return true;
    }

    if (strcmp(name, "timer.every") == 0) {
        statement = er_statement_create(ER_STMT_TIMER_EVERY, er_parser_previous(parser)->line, er_parser_previous(parser)->column);
        free(name);
        if (!statement) {
            return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing timer.every");
        }
        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after timer.every")) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_parse_value(parser, &name_value)) {
            er_statement_free(statement);
            return false;
        }
        if (name_value.type != ER_VALUE_SYMBOL && name_value.type != ER_VALUE_STRING) {
            er_value_free(&name_value);
            er_statement_free(statement);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Timer name must be a symbol or string");
        }
        statement->as.timer_stmt.name = er_parser_dup_range(
            name_value.type == ER_VALUE_SYMBOL ? name_value.as.symbol : name_value.as.string,
            strlen(name_value.type == ER_VALUE_SYMBOL ? name_value.as.symbol : name_value.as.string)
        );
        er_value_free(&name_value);
        if (!(er_parser_match(parser, ER_TOKEN_SEMICOLON) || er_parser_match(parser, ER_TOKEN_COMMA))) {
            er_statement_free(statement);
            return er_parser_fail_at(parser, er_parser_peek(parser), "Expected ';' between timer.every arguments");
        }
        if (!er_parser_parse_value(parser, &statement->as.timer_stmt.interval)) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after timer.every arguments")) {
            er_statement_free(statement);
            return false;
        }
        er_parser_skip_newlines(parser);
        if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' before timer.every block")) {
            er_statement_free(statement);
            return false;
        }
        if (!er_parser_parse_block(parser, &statement->as.timer_stmt.body)) {
            er_statement_free(statement);
            return false;
        }
        *out_statement = statement;
        return true;
    }

    statement = er_statement_create(er_parser_statement_type_for_name(name), er_parser_previous(parser)->line, er_parser_previous(parser)->column);
    if (!statement) {
        free(name);
        return er_parser_fail_at(parser, er_parser_previous(parser), "Out of memory while parsing command");
    }
    statement->as.command.name = name;

    er_parser_skip_newlines(parser);
    if (!er_parser_expect(parser, ER_TOKEN_LBRACKET, "Expected '[' after command name")) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_parse_value_list(parser, &statement->as.command.args, ER_TOKEN_RBRACKET)) {
        er_statement_free(statement);
        return false;
    }
    if (!er_parser_expect(parser, ER_TOKEN_RBRACKET, "Expected ']' after command")) {
        er_statement_free(statement);
        return false;
    }

    *out_statement = statement;
    return true;
}

static bool er_parser_parse_statement(ErParser *parser, ErStatement **out_statement) {
    *out_statement = NULL;
    er_parser_skip_statement_separators(parser);

    if (er_parser_is_at_end(parser)) {
        return true;
    }
    if (er_parser_match(parser, ER_TOKEN_AT_IMP)) {
        return er_parser_parse_import(parser, out_statement);
    }
    if (er_parser_match(parser, ER_TOKEN_IMPORT)) {
        return er_parser_parse_python_import(parser, out_statement);
    }
    if (er_parser_match(parser, ER_TOKEN_IF)) {
        return er_parser_parse_if_statement(parser, out_statement);
    }
    if (er_parser_match(parser, ER_TOKEN_WHILE)) {
        return er_parser_parse_while_statement(parser, out_statement);
    }
    if (er_parser_match(parser, ER_TOKEN_FOR)) {
        return er_parser_parse_for_statement(parser, out_statement);
    }
    if (er_parser_check(parser, ER_TOKEN_IDENTIFIER)) {
        return er_parser_parse_command_statement(parser, out_statement);
    }
    return er_parser_fail_at(parser, er_parser_peek(parser), "Unexpected token");
}

bool er_parse_program(
    const char *source_name,
    const char *source,
    ErProgram **out_program,
    ErError *error
) {
    ErParser parser;
    ErProgram *program;
    ErStatement *statement;

    (void) source_name;
    er_error_clear(error);
    *out_program = NULL;

    if (!er_lexer_tokenize(source_name, source, &parser.tokens, error)) {
        return false;
    }

    parser.current = 0;
    parser.error = error;
    program = er_program_create();
    if (!program) {
        er_token_array_free(&parser.tokens);
        er_error_set(error, 0, 0, "Out of memory while creating program");
        return false;
    }

    while (!er_parser_is_at_end(&parser)) {
        if (!er_parser_parse_statement(&parser, &statement)) {
            er_program_free(program);
            er_token_array_free(&parser.tokens);
            return false;
        }
        if (statement) {
            if (!er_statement_array_push(&program->statements, statement)) {
                er_statement_free(statement);
                er_program_free(program);
                er_token_array_free(&parser.tokens);
                er_error_set(error, 0, 0, "Out of memory while growing program statements");
                return false;
            }
        }
        er_parser_skip_statement_separators(&parser);
    }

    er_token_array_free(&parser.tokens);
    *out_program = program;
    return true;
}
