#include "lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char er_lexer_peek(const ErLexer *lexer) {
    return *lexer->cursor;
}

static char er_lexer_peek_next(const ErLexer *lexer) {
    return lexer->cursor[0] == '\0' ? '\0' : lexer->cursor[1];
}

static char er_lexer_advance(ErLexer *lexer) {
    char c = *lexer->cursor;
    if (c == '\0') {
        return c;
    }
    lexer->cursor++;
    lexer->offset++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static bool er_lexer_match(ErLexer *lexer, char expected) {
    if (er_lexer_peek(lexer) != expected) {
        return false;
    }
    er_lexer_advance(lexer);
    return true;
}

static bool er_lexer_is_ident_start(char c) {
    return isalpha((unsigned char) c) || c == '_';
}

static bool er_lexer_is_ident_continue(char c) {
    return isalnum((unsigned char) c) || c == '_' || c == '-';
}

static bool er_token_array_push(ErTokenArray *tokens, ErToken token) {
    size_t new_capacity;
    ErToken *new_items;

    if (tokens->count < tokens->capacity) {
        tokens->items[tokens->count++] = token;
        return true;
    }

    new_capacity = tokens->capacity == 0 ? 32 : tokens->capacity * 2;
    new_items = (ErToken *) realloc(tokens->items, new_capacity * sizeof(ErToken));
    if (!new_items) {
        return false;
    }

    tokens->items = new_items;
    tokens->capacity = new_capacity;
    tokens->items[tokens->count++] = token;
    return true;
}

static ErToken er_make_lexer_token(
    ErTokenType type,
    const char *start,
    size_t length,
    size_t offset,
    int line,
    int column
) {
    return er_token_make(type, start, length, offset, line, column);
}

static void er_lexer_skip_layout(ErLexer *lexer) {
    for (;;) {
        char c = er_lexer_peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            er_lexer_advance(lexer);
            continue;
        }
        if (c == '/' && er_lexer_peek_next(lexer) == '/') {
            while (er_lexer_peek(lexer) != '\0' && er_lexer_peek(lexer) != '\n') {
                er_lexer_advance(lexer);
            }
            continue;
        }
        if (c == '/' && er_lexer_peek_next(lexer) == '*') {
            er_lexer_advance(lexer);
            er_lexer_advance(lexer);
            while (er_lexer_peek(lexer) != '\0') {
                if (er_lexer_peek(lexer) == '*' && er_lexer_peek_next(lexer) == '/') {
                    er_lexer_advance(lexer);
                    er_lexer_advance(lexer);
                    break;
                }
                er_lexer_advance(lexer);
            }
            continue;
        }
        break;
    }
}

static ErTokenType er_lexer_keyword_type(const char *start, size_t length) {
    if (length == 6 && strncmp(start, "import", 6) == 0) {
        return ER_TOKEN_IMPORT;
    }
    if (length == 6 && strncmp(start, "python", 6) == 0) {
        return ER_TOKEN_PYTHON;
    }
    if (length == 2 && strncmp(start, "as", 2) == 0) {
        return ER_TOKEN_AS;
    }
    if (length == 4 && strncmp(start, "from", 4) == 0) {
        return ER_TOKEN_FROM;
    }
    if (length == 4 && strncmp(start, "true", 4) == 0) {
        return ER_TOKEN_TRUE;
    }
    if (length == 5 && strncmp(start, "false", 5) == 0) {
        return ER_TOKEN_FALSE;
    }
    if (length == 2 && strncmp(start, "if", 2) == 0) {
        return ER_TOKEN_IF;
    }
    if (length == 4 && strncmp(start, "else", 4) == 0) {
        return ER_TOKEN_ELSE;
    }
    if (length == 5 && strncmp(start, "while", 5) == 0) {
        return ER_TOKEN_WHILE;
    }
    if (length == 3 && strncmp(start, "for", 3) == 0) {
        return ER_TOKEN_FOR;
    }
    return ER_TOKEN_IDENTIFIER;
}

static ErToken er_lexer_identifier(ErLexer *lexer, const char *start, size_t offset, int line, int column) {
    while (er_lexer_is_ident_continue(er_lexer_peek(lexer))) {
        er_lexer_advance(lexer);
    }
    return er_make_lexer_token(
        er_lexer_keyword_type(start, (size_t) (lexer->cursor - start)),
        start,
        (size_t) (lexer->cursor - start),
        offset,
        line,
        column
    );
}

static ErToken er_lexer_number(ErLexer *lexer, const char *start, size_t offset, int line, int column) {
    while (isdigit((unsigned char) er_lexer_peek(lexer))) {
        er_lexer_advance(lexer);
    }
    if (er_lexer_peek(lexer) == '.' && isdigit((unsigned char) er_lexer_peek_next(lexer))) {
        er_lexer_advance(lexer);
        while (isdigit((unsigned char) er_lexer_peek(lexer))) {
            er_lexer_advance(lexer);
        }
    }
    return er_make_lexer_token(
        ER_TOKEN_NUMBER,
        start,
        (size_t) (lexer->cursor - start),
        offset,
        line,
        column
    );
}

static ErToken er_lexer_string(ErLexer *lexer, const char *start, size_t offset, int line, int column) {
    while (er_lexer_peek(lexer) != '\0' && er_lexer_peek(lexer) != '"') {
        if (er_lexer_peek(lexer) == '\\' && er_lexer_peek_next(lexer) != '\0') {
            er_lexer_advance(lexer);
        }
        er_lexer_advance(lexer);
    }

    if (er_lexer_peek(lexer) != '"') {
        return er_make_lexer_token(ER_TOKEN_ERROR, "Unterminated string literal", 27, offset, line, column);
    }

    er_lexer_advance(lexer);
    return er_make_lexer_token(
        ER_TOKEN_STRING,
        start,
        (size_t) (lexer->cursor - start),
        offset,
        line,
        column
    );
}

void er_lexer_init(ErLexer *lexer, const char *source_name, const char *source) {
    lexer->source_name = source_name;
    lexer->source = source;
    lexer->cursor = source;
    lexer->offset = 0;
    lexer->line = 1;
    lexer->column = 1;
}

ErToken er_lexer_next(ErLexer *lexer) {
    const char *start;
    size_t offset;
    int line;
    int column;
    char c;

    er_lexer_skip_layout(lexer);

    start = lexer->cursor;
    offset = lexer->offset;
    line = lexer->line;
    column = lexer->column;
    c = er_lexer_advance(lexer);

    switch (c) {
        case '\0':
            return er_make_lexer_token(ER_TOKEN_EOF, start, 0, offset, line, column);
        case '\n':
            return er_make_lexer_token(ER_TOKEN_NEWLINE, start, 1, offset, line, column);
        case '[':
            return er_make_lexer_token(ER_TOKEN_LBRACKET, start, 1, offset, line, column);
        case ']':
            return er_make_lexer_token(ER_TOKEN_RBRACKET, start, 1, offset, line, column);
        case '(':
            return er_make_lexer_token(ER_TOKEN_LPAREN, start, 1, offset, line, column);
        case ')':
            return er_make_lexer_token(ER_TOKEN_RPAREN, start, 1, offset, line, column);
        case '.':
            return er_make_lexer_token(ER_TOKEN_DOT, start, 1, offset, line, column);
        case ',':
            return er_make_lexer_token(ER_TOKEN_COMMA, start, 1, offset, line, column);
        case ';':
            return er_make_lexer_token(ER_TOKEN_SEMICOLON, start, 1, offset, line, column);
        case '$':
            return er_make_lexer_token(ER_TOKEN_DOLLAR, start, 1, offset, line, column);
        case '@':
            if (strncmp(lexer->cursor, "imp", 3) == 0 && !er_lexer_is_ident_continue(lexer->cursor[3])) {
                er_lexer_advance(lexer);
                er_lexer_advance(lexer);
                er_lexer_advance(lexer);
                return er_make_lexer_token(ER_TOKEN_AT_IMP, start, 4, offset, line, column);
            }
            return er_make_lexer_token(ER_TOKEN_AT, start, 1, offset, line, column);
        case '=':
            return er_make_lexer_token(
                er_lexer_match(lexer, '=') ? ER_TOKEN_EQUAL_EQUAL : ER_TOKEN_EQUAL,
                start,
                (size_t) (lexer->cursor - start),
                offset,
                line,
                column
            );
        case '!':
            if (er_lexer_match(lexer, '=')) {
                return er_make_lexer_token(ER_TOKEN_BANG_EQUAL, start, 2, offset, line, column);
            }
            return er_make_lexer_token(ER_TOKEN_ERROR, "Unexpected '!'", 14, offset, line, column);
        case '<':
            return er_make_lexer_token(
                er_lexer_match(lexer, '=') ? ER_TOKEN_LESS_EQUAL : ER_TOKEN_LESS,
                start,
                (size_t) (lexer->cursor - start),
                offset,
                line,
                column
            );
        case '>':
            return er_make_lexer_token(
                er_lexer_match(lexer, '=') ? ER_TOKEN_GREATER_EQUAL : ER_TOKEN_GREATER,
                start,
                (size_t) (lexer->cursor - start),
                offset,
                line,
                column
            );
        case '"':
            return er_lexer_string(lexer, start, offset, line, column);
        default:
            if (isdigit((unsigned char) c)) {
                return er_lexer_number(lexer, start, offset, line, column);
            }
            if (er_lexer_is_ident_start(c)) {
                return er_lexer_identifier(lexer, start, offset, line, column);
            }
            return er_make_lexer_token(ER_TOKEN_ERROR, "Unexpected character", 20, offset, line, column);
    }
}

bool er_lexer_tokenize(
    const char *source_name,
    const char *source,
    ErTokenArray *out_tokens,
    ErError *error
) {
    ErLexer lexer;
    ErToken token;

    er_error_clear(error);
    out_tokens->items = NULL;
    out_tokens->count = 0;
    out_tokens->capacity = 0;

    er_lexer_init(&lexer, source_name, source);

    for (;;) {
        token = er_lexer_next(&lexer);
        if (!er_token_array_push(out_tokens, token)) {
            er_error_set(error, token.line, token.column, "Out of memory while growing token list");
            er_token_array_free(out_tokens);
            return false;
        }

        if (token.type == ER_TOKEN_ERROR) {
            er_error_set(error, token.line, token.column, "%.*s", (int) token.length, token.start);
            er_token_array_free(out_tokens);
            return false;
        }
        if (token.type == ER_TOKEN_EOF) {
            return true;
        }
    }
}

void er_lexer_dump(FILE *out, const ErTokenArray *tokens) {
    size_t i;
    for (i = 0; i < tokens->count; ++i) {
        er_token_print(out, tokens->items[i]);
    }
}

void er_token_array_free(ErTokenArray *tokens) {
    if (!tokens) {
        return;
    }
    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}
