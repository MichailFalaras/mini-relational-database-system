#ifndef PARSER_H_
#define PARSER_H_

#include <stdint.h>
#include "tokenizer.h"
#include "ast.h"
#include "database.h"

typedef enum statement_type {
    STMT_CREATE_TABLE = 0,
    STMT_DROP_TABLE = 1,
    STMT_ALTER_TABLE = 2,
    STMT_TRUNCATE_TABLE = 3,
    STMT_CREATE_INDEX = 4,
    STMT_DROP_INDEX = 5,
    STMT_SELECT = 6,
    STMT_INSERT = 7,
    STMT_UPDATE = 8,
    STMT_DELETE = 9,
    STMT_ERROR = 10
} StatementType;

typedef struct statement {
    ASTNode *root;
    StatementType type;
} Statement;

/* Parser component containing:
 * token_array: array of query's tokens
 * current_position: current position in that token array. */
typedef struct parser {
    TokenArray *token_array;
    uint32_t current_position;
} Parser;

/* Allocate and initialize Parser component. */
extern Parser *parser_init(TokenArray *token_array);

/* Parsing orchestration function. */
extern Statement *parse_query(Parser *parser, Database *db);

/* Parse top-level keyword and create Abstract Syntax Tree. */
extern ASTNode *parse(Parser *parser);

/* Parse Expression and create Expression Tree. */
ExpressionNode *parse_expression(Parser *parser);

/* Allocate and initialize Statement. */
extern Statement *statement_init(ASTNode *root, StatementType type);

/* Inside parse_query, last check of AST validity. */
extern bool bind_statement(ASTNode *root, Database *db);

/* Deallocate Parser component*/
extern void parser_free(Parser *parser);

#endif