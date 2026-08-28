#ifndef ERIRE_AST_H
#define ERIRE_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum ErValueType {
    ER_VALUE_STRING,
    ER_VALUE_NUMBER,
    ER_VALUE_BOOL,
    ER_VALUE_SYMBOL,
    ER_VALUE_VARIABLE,
    ER_VALUE_CALL
} ErValueType;

struct ErCallExpression;

typedef struct ErValue {
    ErValueType type;
    union {
        char *string;
        double number;
        bool boolean;
        char *symbol;
        char *variable;
        struct ErCallExpression *call;
    } as;
} ErValue;

typedef enum ErCompareOp {
    ER_COMPARE_EQ,
    ER_COMPARE_NEQ,
    ER_COMPARE_LT,
    ER_COMPARE_LTE,
    ER_COMPARE_GT,
    ER_COMPARE_GTE
} ErCompareOp;

typedef struct ErCondition {
    ErValue left;
    ErCompareOp op;
    ErValue right;
} ErCondition;

typedef struct ErValueArray {
    ErValue *items;
    size_t count;
    size_t capacity;
} ErValueArray;

typedef struct ErCallExpression {
    char *name;
    ErValueArray args;
} ErCallExpression;

struct ErStatement;

typedef struct ErStatementArray {
    struct ErStatement **items;
    size_t count;
    size_t capacity;
} ErStatementArray;

typedef struct ErProperty {
    char *name;
    bool is_event;
    ErValueArray values;
    ErStatementArray block;
} ErProperty;

typedef struct ErPropertyArray {
    ErProperty *items;
    size_t count;
    size_t capacity;
} ErPropertyArray;

typedef struct ErElement {
    char *kind;
    char *base_property;
    ErValueArray base_args;
    ErPropertyArray properties;
} ErElement;

typedef enum ErImportDirectiveType {
    ER_IMPORT_DIRECTIVE_LEGACY,
    ER_IMPORT_DIRECTIVE_PYTHON
} ErImportDirectiveType;

typedef struct ErImportDirective {
    ErImportDirectiveType type;
    char *provider;
    char *path;
    char *alias;
    ErValueArray args;
} ErImportDirective;

typedef struct ErCommand {
    char *name;
    ErValueArray args;
} ErCommand;

typedef struct ErAddCommand {
    ErElement *element;
} ErAddCommand;

typedef struct ErSetTextCommand {
    ErValue element_id;
    ErValue value;
} ErSetTextCommand;

typedef struct ErVarDecl {
    char *name;
    ErValue value;
} ErVarDecl;

typedef struct ErElseIfClause {
    ErCondition condition;
    ErStatementArray body;
} ErElseIfClause;

typedef struct ErElseIfClauseArray {
    ErElseIfClause *items;
    size_t count;
    size_t capacity;
} ErElseIfClauseArray;

typedef struct ErIfStatement {
    ErCondition condition;
    ErStatementArray body;
    ErElseIfClauseArray else_ifs;
    ErStatementArray else_body;
    bool has_else;
} ErIfStatement;

typedef struct ErWhileStatement {
    ErCondition condition;
    ErStatementArray body;
} ErWhileStatement;

typedef struct ErForStatement {
    char *iterator_name;
    ErValue start;
    ErValue end;
    ErValue step;
    bool has_step;
    ErStatementArray body;
} ErForStatement;

typedef struct ErTimerStatement {
    char *name;
    ErValue interval;
    ErStatementArray body;
} ErTimerStatement;

typedef enum ErStatementType {
    ER_STMT_IMPORT,
    ER_STMT_SCREEN_CREATE,
    ER_STMT_SCREEN_TITLE,
    ER_STMT_SCREEN_BG,
    ER_STMT_SCREEN_ADD,
    ER_STMT_SCREEN_SHOW,
    ER_STMT_SCREEN_SETTEXT,
    ER_STMT_VAR_SET,
    ER_STMT_IF,
    ER_STMT_WHILE,
    ER_STMT_FOR,
    ER_STMT_TIMER_EVERY,
    ER_STMT_GENERIC
} ErStatementType;

typedef struct ErStatement {
    ErStatementType type;
    int line;
    int column;
    union {
        ErImportDirective import_directive;
        ErCommand command;
        ErAddCommand add;
        ErSetTextCommand set_text;
        ErVarDecl var_decl;
        ErIfStatement if_stmt;
        ErWhileStatement while_stmt;
        ErForStatement for_stmt;
        ErTimerStatement timer_stmt;
    } as;
} ErStatement;

typedef struct ErProgram {
    ErStatementArray statements;
} ErProgram;

bool er_value_array_push(ErValueArray *array, ErValue value);
bool er_statement_array_push(ErStatementArray *array, ErStatement *statement);
bool er_property_array_push(ErPropertyArray *array, ErProperty property);
bool er_else_if_clause_array_push(ErElseIfClauseArray *array, ErElseIfClause clause);

ErValue er_value_make_string(const char *text);
ErValue er_value_make_symbol(const char *text);
ErValue er_value_make_variable(const char *text);
ErValue er_value_make_number(double number);
ErValue er_value_make_bool(bool value);
ErValue er_value_make_call(ErCallExpression *call);
ErValue er_value_clone(const ErValue *value);
void er_value_free(ErValue *value);

ErCallExpression *er_call_expression_create(const char *name);
void er_call_expression_free(ErCallExpression *call);

ErElement *er_element_create(void);
void er_element_free(ErElement *element);

ErStatement *er_statement_create(ErStatementType type, int line, int column);
void er_statement_free(ErStatement *statement);

ErProgram *er_program_create(void);
void er_program_free(ErProgram *program);
void er_program_dump(FILE *out, const ErProgram *program);

#endif
