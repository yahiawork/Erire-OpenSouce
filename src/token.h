#ifndef ERIRE_TOKEN_H
#define ERIRE_TOKEN_H

#include <stddef.h>
#include <stdio.h>

typedef enum ErTokenType {
    ER_TOKEN_EOF = 0,
    ER_TOKEN_ERROR,
    ER_TOKEN_NEWLINE,
    ER_TOKEN_IDENTIFIER,
    ER_TOKEN_STRING,
    ER_TOKEN_NUMBER,
    ER_TOKEN_TRUE,
    ER_TOKEN_FALSE,
    ER_TOKEN_IMPORT,
    ER_TOKEN_PYTHON,
    ER_TOKEN_AS,
    ER_TOKEN_FROM,
    ER_TOKEN_IF,
    ER_TOKEN_ELSE,
    ER_TOKEN_WHILE,
    ER_TOKEN_FOR,
    ER_TOKEN_AT,
    ER_TOKEN_AT_IMP,
    ER_TOKEN_DOLLAR,
    ER_TOKEN_DOT,
    ER_TOKEN_COMMA,
    ER_TOKEN_SEMICOLON,
    ER_TOKEN_LBRACKET,
    ER_TOKEN_RBRACKET,
    ER_TOKEN_LPAREN,
    ER_TOKEN_RPAREN,
    ER_TOKEN_EQUAL,
    ER_TOKEN_EQUAL_EQUAL,
    ER_TOKEN_BANG_EQUAL,
    ER_TOKEN_LESS,
    ER_TOKEN_LESS_EQUAL,
    ER_TOKEN_GREATER,
    ER_TOKEN_GREATER_EQUAL
} ErTokenType;

typedef struct ErToken {
    ErTokenType type;
    const char *start;
    size_t length;
    size_t offset;
    int line;
    int column;
} ErToken;

const char *er_token_type_name(ErTokenType type);
ErToken er_token_make(
    ErTokenType type,
    const char *start,
    size_t length,
    size_t offset,
    int line,
    int column
);
void er_token_print(FILE *out, ErToken token);

#endif
