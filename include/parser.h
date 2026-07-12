#ifndef PARSER_H_
#define PARSER_H_

#include <stdint.h>
#include "tokenizer.h"
#include "ast.h"
#include "database.h"

typedef enum statement_type {
    STMT_CREATE_TABLE,
    STMT_DROP_TABLE,
    STMT_ALTER_TABLE,
    STMT_TRUNCATE_TABLE,
    STMT_CREATE_INDEX,
    STMT_DROP_INDEX,
    STMT_SELECT,
    STMT_INSERT,
    STMT_UPDATE,
    STMT_DELETE
} StatementType;

typedef struct statement {
    ASTNode *root;
    StatementType type;
} Statement;

/* Parser component containing:
 * query_tokens: array of tokens
 * amount_tokens: amount of tokens in array
 * current_position: current position while traversing array. */
typedef struct parser {
    TokenArray *token_array;
    uint32_t current_position;
} Parser;

extern Parser *parser_init(TokenArray *token_array);

/* Identify Top-Level Keyword*/
extern Statement *parse_query(Parser *parser, Database *db);
// Database working like System Catalog. Needed for semantic binder

extern ASTNode *parse(Parser *parser);

/* Inside parse_query, last check of AST validity. */
extern bool bind_statement(ASTNode *root, Database *db);

extern void parser_free(Parser *parser);

#endif