#ifndef PARSER_UTILS_H_
#define PARSER_UTILS_H_

typedef struct parser Parser;
#include "../../include/ast.h"
#include "../../include/expressions.h"

/* ASTNodeType to StatementType. */
StatementType ast_to_statement_type(ASTNodeType type);

/* Token string to OperatorType. */
OperatorType token_str_to_operator_type(char *token_str);

/* Check if Token string is a specific OperatorType. */
bool is_token_operator(char *token_str, OperatorType type);

/* Check if Token string is specifically a comparison operator. */
bool is_token_comparison_operator(char *token_str);

/* Validate that there is a token for Parser's current position in the TokenArray.
 * Then return Token* .*/
Token *get_current_token(Parser *parser);

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

/* Identify INTEGER or NUMERIC literal. */
Value *create_number_literal(Parser *parser);

/* Identify if string is CHAR(n), DATE, TIMESTAMP or BOOL. */
Value *create_string_literal(Parser *parser);

ASTNode *parse_select(Parser *parser);

ASTNode *parse_update(Parser *parser);

ASTNode *parse_insert(Parser *parser);

ASTNode *parse_delete(Parser *parser);

ASTNode *parse_create_table(Parser *parser);

ASTNode *parse_drop_table(Parser *parser);

ASTNode *parse_alter_table(Parser *parser);

ASTNode *parse_alter_table(Parser *parser);

ASTNode *parse_create_index(Parser *parser);

ASTNode *parse_drop_index(Parser *parser);

#endif