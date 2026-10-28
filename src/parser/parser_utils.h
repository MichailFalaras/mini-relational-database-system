#ifndef PARSER_UTILS_H_
#define PARSER_UTILS_H_


#include <stdint.h>
#include <stdbool.h>

typedef enum data_types DataType;
typedef struct value Value;
typedef struct token_struct Token;
typedef struct parser Parser;
typedef enum statement_type StatementType;

typedef struct ast_projection ProjectionNode;
typedef struct ast_from FromNode;
typedef struct ast_as AsNode;
typedef struct ast_join JoinNode;
typedef struct ast_on OnNode;
typedef struct ast_where WhereNode;
typedef struct ast_group_by GroupByNode;
typedef struct ast_having HavingNode;
typedef struct ast_order_by OrderByNode;
typedef struct ast_limit LimitNode;
typedef struct ast_offset OffsetNode;
typedef struct ast_into IntoNode;
typedef struct ast_values ValuesNode;
typedef struct ast_set SetNode;
typedef struct ast_columns ColumnsNode;
typedef struct ast_column_def ColumnDefNode;
typedef struct ast_constraint ConstraintNode;
typedef struct ast_alter_action AlterActionNode;
typedef enum ast_node_type ASTNodeType;
typedef struct abstract_syntax_tree_node ASTNode;

typedef enum operator_type OperatorType;
typedef enum ast_join_types JoinType;
typedef struct expression_node ExpressionNode;

typedef enum postfix_parse_status {
    POSTFIX_PARSE_ERROR = -1,
    POSTFIX_PARSE_NONE = 0,
    POSTFIX_PARSE_SUCCESS = 1
} PostfixParseStatus;

/* ASTNodeType to StatementType. */
StatementType ast_to_statement_type(ASTNodeType type);

/* Token string to OperatorType. */
OperatorType token_str_to_operator_type(char *token_str);

/* Token string to DataType */
DataType token_str_to_data_type(char *token_str);

/* Token string to JoinType */
JoinType token_str_to_join_type(char *token_str);

/* Check if Token string is a specific OperatorType. */
bool is_token_operator(char *token_str, OperatorType type);

/* Check if Token string is specifically a comparison operator. */
bool is_token_comparison_operator(char *token_str);

/* Check if current token is the start of a column-level constraint */
bool is_column_constraint_start(Token *token);

/* Check if current token is the start of a table-level constraint */
bool is_table_constraint_start(Token *token);

/* Validate that there is a token for Parser's current position in the TokenArray.
 * Then return Token* .*/
Token *get_current_token(Parser *parser);

/* Consume token if possible. */
void consume_token(Parser *parser);

/* ----- EXPRESSION PARSING ----- */

/* Parse OR Expression. */
ExpressionNode *parse_or(Parser *parser);

/* Parse AND Expression. */
ExpressionNode *parse_and(Parser *parser);

/* Parse NOT Expression. */
ExpressionNode *parse_not(Parser *parser);

/* Parse Comparison Expression. */
ExpressionNode *parse_comparison(Parser *parser);

/* Parse Addition/Subtraction Expression. */
ExpressionNode *parse_addition(Parser *parser);

/* Parse Multiplication/Division/Modulo Expression. */
ExpressionNode *parse_multiplication(Parser *parser);

/* Parse Unary (+, -, ~) Expression. */
ExpressionNode *parse_unary(Parser *parser);

/* Parse Identifier/Literal/Parentheses Expression. */
ExpressionNode *parse_primary_expression(Parser *parser);

/* Parse postfix expressions like: IS NULL / IS NOT NULL / IN / BETWEEN
 * that require their own unique parsing. */
PostfixParseStatus parse_postfix_expression(Parser *parser, ExpressionNode **operand);

/* Parse literal expressions. */
ExpressionNode *parse_literal_expression(Parser *parser);

/* Identify UNSIGNED INTEGER or NUMERIC literal. */
Value *create_number_literal(Parser *parser);

/* Identify if string is CHAR(n), DATE, TIMESTAMP or BOOL. */
Value *create_string_literal(Parser *parser);

/* ---------- Parsing of the available Query Types ---------- */

ASTNode *parse_select(Parser *parser);

ASTNode *parse_update(Parser *parser);

ASTNode *parse_insert(Parser *parser);

ASTNode *parse_delete(Parser *parser);

ASTNode *parse_create_table(Parser *parser);

ASTNode *parse_drop_table(Parser *parser);

ASTNode *parse_truncate_table(Parser *parser);

ASTNode *parse_alter_table(Parser *parser);

ASTNode *parse_create_index(Parser *parser);

ASTNode *parse_drop_index(Parser *parser);

/* ---------- Parsing of Inner Query Components ---------- */

JoinNode **parse_joins(Parser *parser, uint32_t *num_joins);

OnNode *parse_on(Parser *parser);

ProjectionNode *parse_projection(Parser *parser);

FromNode *parse_from(Parser *parser);

AsNode *parse_as(Parser *parser);

WhereNode *parse_where(Parser *parser);

GroupByNode *parse_group_by(Parser *parser);

HavingNode *parse_having(Parser *parser);

OrderByNode *parse_order_by(Parser *parser);

LimitNode *parse_limit(Parser *parser);

OffsetNode *parse_offset(Parser *parser);

bool parse_uint32_token(const Token *token, uint32_t *out_value);

IntoNode *parse_into(Parser *parser);

ValuesNode *parse_values(Parser *parser);

SetNode *parse_set(Parser *parser);

ColumnsNode *parse_columns(Parser *parser);

ColumnDefNode *parse_column_def(Parser *parser);

ConstraintNode *parse_constraint(Parser *parser, const char *column_name);

bool parse_primary_key_constraint(Parser *parser, ConstraintNode *constraint, const char *column_name);

bool parse_unique_constraint(Parser *parser, ConstraintNode *constraint, const char *column_name);

bool parse_not_null_constraint(Parser *parser, ConstraintNode *constraint, const char *column_name);

bool parse_foreign_key_constraint(Parser *parser, ConstraintNode *constraint, const char *column_name);

bool parse_check_constraint(Parser *parser, ConstraintNode *constraint);

bool parse_default_constraint(Parser *parser, ConstraintNode *constraint, const char *column_name);

bool parse_constraint_column_list(Parser *parser, ExpressionNode ***column_refs, uint32_t *num_columns);

bool parse_alter_add_col(Parser *parser, AlterActionNode *action);

bool parse_alter_drop_col(Parser *parser, AlterActionNode *action);

bool parse_alter_rename_table(Parser *parser, AlterActionNode *action);

bool parse_alter_rename_col(Parser *parser, AlterActionNode *action);

bool parse_alter_modify_col(Parser *parser, AlterActionNode *action);

bool parse_alter_add_constraint(Parser *parser, AlterActionNode *action);

bool parse_alter_drop_constraint(Parser *parser, AlterActionNode *action);

#endif