#ifndef ERIRE_LEXER_H
#define ERIRE_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "error.h"
#include "token.h"

typedef struct ErLexer {
    const char *source_name;
    const char *source;
    const char *cursor;
    size_t offset;
    int line;
    int column;
} ErLexer;

typedef struct ErTokenArray {
    ErToken *items;
    size_t count;
    size_t capacity;
} ErTokenArray;

void er_lexer_init(ErLexer *lexer, const char *source_name, const char *source);
ErToken er_lexer_next(ErLexer *lexer);
bool er_lexer_tokenize(
    const char *source_name,
    const char *source,
    ErTokenArray *out_tokens,
    ErError *error
);
void er_lexer_dump(FILE *out, const ErTokenArray *tokens);
void er_token_array_free(ErTokenArray *tokens);

#endif
