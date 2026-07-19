#ifndef PARSER_H_
#define PARSER_H_

#include <stdint.h>
#include "tokenizer.h"
#include "ast.h"

/* Forward Declarations. */
typedef struct database Database;

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
    uint32_t amount_tokens;
    uint32_t current_position;
} Parser;

extern Parser *parser_init(TokenArray *token_array);

/* Identify Top-Level Keyword*/
extern Statement *parse_query(Parser *parser, Database *db);
// Database working like System Catalog. Needed for semantic binder

extern ASTNode *parse(Parser *parser);

/* TODO: Move functions to parser.c */
/* SELECT */
extern ASTNode *parse_select(Parser *parser);
extern ASTNode *parse_projection(Parser *parser);
extern ASTNode *parse_from(Parser *parser);
extern ASTNode *parse_where(Parser *parser);
extern ASTNode *parse_group_by(Parser *parser);
extern ASTNode *parse_having(Parser *parser);
extern ASTNode *parse_order_by(Parser *parser);
// Might need to check how many JOINs there are
extern ASTNode *parse_joins(Parser *parser);
extern ASTNode *parse_limit(Parser *parser);
extern ASTNode *parse_offset(Parser *parser);

/* INSERT */
extern ASTNode *parse_insert(Parser *parser);
extern ASTNode *parse_into(Parser *parser);
extern ASTNode *parse_values(Parser *parser);

/* UPDATE */
extern ASTNode *parse_update(Parser *parser);
extern ASTNode *parse_set(Parser *parser);

/* DELETE */
extern ASTNode *parse_delete(Parser *parser);

/* CREATE TABLE */
extern ASTNode *parse_create_table(Parser *parser);
extern ASTNode *parse_columns(Parser *parser);
extern ASTNode *parse_column_defs(Parser *parser);
extern ASTNode *parse_constraints(Parser *parser);

/* DROP TABLE */
extern ASTNode *parse_drop_table(Parser *parser);

/* ALTER TABLE */
extern ASTNode *parse_alter_table(Parser *parser);
extern ASTNode *parse_alter_add_col(Parser *parser);
extern ASTNode *parse_alter_drop_col(Parser *parser);
extern ASTNode *parse_alter_rename_table(Parser *parser);
extern ASTNode *parse_alter_modify_col(Parser *parser);
extern ASTNode *parse_alter_add_constraint(Parser *parser);
extern ASTNode *parse_alter_drop_constraint(Parser *parser);

/* CREATE INDEX */
extern ASTNode *parse_create_index(Parser *parser);

/* DROP INDEX */
extern ASTNode *parse_drop_index(Parser *parser);

/* Inside parse_query, last check of AST validity. */
extern bool semantic_binder(ASTNode *root, Database *db);

extern void parser_free(Parser *parser);

#endif