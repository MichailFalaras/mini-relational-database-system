#ifndef PARSER_UTILS_H_
#define PARSER_UTILS_H_

typedef enum data_types DataType;
typedef struct parser Parser;
#include "../../include/ast.h"
#include "../../include/expressions.h"

/* ASTNodeType to StatementType. */
StatementType ast_to_statement_type(ASTNodeType type);

/* Token string to OperatorType. */
OperatorType token_str_to_operator_type(char *token_str);

/* Token string to DataType */
DataType token_str_to_data_type(char *token_str);

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
ExpressionNode *parse_postfix_expression(Parser *parser, ExpressionNode **operand);

/* Parse literal expressions. */
ExpressionNode *parse_literal_expression(Parser *parser);

/* Identify INTEGER or NUMERIC literal. */
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

FromNode *parse_from(Parser *parser);

WhereNode *parse_where(Parser *parser);

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