#include "token.h"

#include <stdio.h>

const char *er_token_type_name(ErTokenType type) {
    switch (type) {
        case ER_TOKEN_EOF: return "EOF";
        case ER_TOKEN_ERROR: return "ERROR";
        case ER_TOKEN_NEWLINE: return "NEWLINE";
        case ER_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case ER_TOKEN_STRING: return "STRING";
        case ER_TOKEN_NUMBER: return "NUMBER";
        case ER_TOKEN_TRUE: return "TRUE";
        case ER_TOKEN_FALSE: return "FALSE";
        case ER_TOKEN_IMPORT: return "IMPORT";
        case ER_TOKEN_PYTHON: return "PYTHON";
        case ER_TOKEN_AS: return "AS";
        case ER_TOKEN_FROM: return "FROM";
        case ER_TOKEN_IF: return "IF";
        case ER_TOKEN_ELSE: return "ELSE";
        case ER_TOKEN_WHILE: return "WHILE";
        case ER_TOKEN_FOR: return "FOR";
        case ER_TOKEN_AT: return "AT";
        case ER_TOKEN_AT_IMP: return "AT_IMP";
        case ER_TOKEN_DOLLAR: return "DOLLAR";
        case ER_TOKEN_DOT: return "DOT";
        case ER_TOKEN_COMMA: return "COMMA";
        case ER_TOKEN_SEMICOLON: return "SEMICOLON";
        case ER_TOKEN_LBRACKET: return "LBRACKET";
        case ER_TOKEN_RBRACKET: return "RBRACKET";
        case ER_TOKEN_LPAREN: return "LPAREN";
        case ER_TOKEN_RPAREN: return "RPAREN";
        case ER_TOKEN_EQUAL: return "EQUAL";
        case ER_TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case ER_TOKEN_BANG_EQUAL: return "BANG_EQUAL";
        case ER_TOKEN_LESS: return "LESS";
        case ER_TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case ER_TOKEN_GREATER: return "GREATER";
        case ER_TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        default: return "UNKNOWN";
    }
}

ErToken er_token_make(
    ErTokenType type,
    const char *start,
    size_t length,
    size_t offset,
    int line,
    int column
) {
    ErToken token;
    token.type = type;
    token.start = start;
    token.length = length;
    token.offset = offset;
    token.line = line;
    token.column = column;
    return token;
}

void er_token_print(FILE *out, ErToken token) {
    fprintf(
        out,
        "%-14s line=%d col=%d len=%zu lexeme=\"%.*s\"\n",
        er_token_type_name(token.type),
        token.line,
        token.column,
        token.length,
        (int) token.length,
        token.start ? token.start : ""
    );
}
