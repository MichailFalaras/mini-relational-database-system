#include <stdio.h>
#include <string.h>
#include "../include/tokenizer.h"
#include "../include/parser.h"
#include "../src/parser/parser_utils.h"
#include "../include/expressions.h"
#include "../include/data_types.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return 1; \
    }

/* ---------- Parser Lifecycle Tests ---------- */

static int test_parser_init() {
    char *query = strdup("SELECT * FROM table;");

    Tokenizer *tokenizer = tokenizer_init(query);
    if (!tokenizer) { return -1; }

    TokenArray *token_array = tokenize_query(tokenizer);
    if (!token_array) { return -1; }

    Parser *parser = parser_init(token_array);
    ASSERT(parser != NULL);

    ASSERT(parser->token_array == token_array);
    ASSERT(parser->current_position == 0);

    parser_free(parser);
    tokenizer_free(tokenizer);
    return 0;
}

/* ---------- Expression Parsing Tests ---------- */
/* (NOTE: Since we check to make sure the query doesn't start with
 *  an expression, we check !parser->current_position inside any parsing expression helper.
 *  Therefore, we use a placeholder identifier token in the beginning to resolve that). */

static int test_parsing_literal_expressions() {

    /* ----- PARSING LITERAL NUMBERS ----- */
    {
        char *query = strdup("placeholder 123 12.3 0.5 400;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == UNSIGNED_INTEGER);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.uint32_val == 123);
        ASSERT(parser->current_position == 1);
        expression_node_free(literal_expr);

        consume_token(parser);

        literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == NUMERIC);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.numeric_val.scale == 1);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.numeric_val.val == 123);
        ASSERT(parser->current_position == 2);
        expression_node_free(literal_expr);

        consume_token(parser);

        literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == NUMERIC);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.numeric_val.scale == 1);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.numeric_val.val == 5);
        ASSERT(parser->current_position == 3);
        expression_node_free(literal_expr);

        consume_token(parser);

        literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == UNSIGNED_INTEGER);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.uint32_val == 400);
        ASSERT(parser->current_position == 4);
        expression_node_free(literal_expr);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PARSING STRING/CHAR LITERAL ----- */
    {
        char *query = strdup("placeholder 'John Doe';");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == CHAR);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.char_val.n == 8);
        ASSERT(strcmp(literal_expr->expression_data.literal_value.literal->value.char_val.string, "John Doe") == 0);
        ASSERT(parser->current_position == 1);

        expression_node_free(literal_expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PARSING BOOLEAN LITERALS ----- */
    {
        char *query = strdup("placeholder TRUE FALSE;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == BOOL);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.bool_val == true);
        expression_node_free(literal_expr);

        consume_token(parser);

        literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == BOOL);
        ASSERT(literal_expr->expression_data.literal_value.literal->value.bool_val == false);
        expression_node_free(literal_expr);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PARSING DATE LITERAL ----- */
    {
        char *query = strdup("placeholder '2026-09-21';");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == DATE);
        ASSERT(parser->current_position == 1);

        expression_node_free(literal_expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PARSING TIMESTAMP LITERAL ----- */
    {
        char *query = strdup("placeholder '2026-09-21 15:00:00';");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == TIMESTAMP);
        ASSERT(parser->current_position == 1);

        expression_node_free(literal_expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }
        
    /* ----- ORDINARY DATE-LIKE STRING MUST REMAIN CHAR ----- */
    {
        char *query = strdup("placeholder '2026-09-21-extra';");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == CHAR);
        ASSERT(!strcmp(literal_expr->expression_data.literal_value.literal->value.char_val.string,
                      "2026-09-21-extra"));
                    

        expression_node_free(literal_expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID DATE-LIKE STRING MUST REMAIN CHAR ----- */
    {
        char *query = strdup("placeholder '2026-02-30';");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *literal_expr = parse_literal_expression(parser);
        ASSERT(literal_expr != NULL);
        ASSERT(literal_expr->expression_data.literal_value.literal->type == CHAR);
        ASSERT(literal_expr->type == EXPR_LITERAL);
        ASSERT(strcmp(literal_expr->expression_data.literal_value.literal->value.char_val.string,
                      "2026-02-30") == 0);

        expression_node_free(literal_expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID LITERAL: IDENTIFIER ----- */
    {
        char *query = strdup("placeholder column_name;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_literal_expression(parser) == NULL);
        ASSERT(parser->current_position == 1);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID LITERAL: NON-BOOLEAN KEYWORD ----- */
    {
        char *query = strdup("placeholder FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }

        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }

        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_literal_expression(parser) == NULL);
        ASSERT(parser->current_position == 1);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }
    
    return 0;
}

static int test_parsing_primary_expressions() {

    /* ----- COLUMN REFERENCE ----- */
    {
        char *query = strdup("placeholder salary;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(expr->expression_data.column_value.column_name, "salary") == 0);
        ASSERT(parser->current_position == 2);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- LITERAL PRIMARY EXPRESSION ----- */
    {
        char *query = strdup("placeholder 15 FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.literal_value.literal->type == UNSIGNED_INTEGER);
        ASSERT(expr->expression_data.literal_value.literal->value.uint32_val == 15);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PARENTHESIZED LITERAL ----- */
    {
        char *query = strdup("placeholder (123) FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.literal_value.literal->value.uint32_val == 123);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- NESTED PARENTHESES ----- */
    {
        char *query = strdup("placeholder (((column_name))) ORDER BY column_name;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(expr->expression_data.column_value.column_name, "column_name") == 0);
        ASSERT(strcmp(get_current_token(parser)->token, "ORDER BY") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PARENTHESIZED ARITHMETIC EXPRESSION ----- */
    {
        char *query = strdup("placeholder (1 + 2 * 3) FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_MUL);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- AGGREGATE SUM ----- */
    {
        char *query = strdup("placeholder SUM(salary);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_SUM);
        ASSERT(expr->expression_data.aggregate_func_expr.wildcard == false);
        ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);
        ASSERT(expr->expression_data.aggregate_func_expr.expression->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(expr->expression_data.aggregate_func_expr.expression->expression_data.column_value.column_name,
                      "salary") == 0);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- AGGREGATE COUNT WILDCARD ----- */
    {
        char *query = strdup("placeholder COUNT(*) FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
        ASSERT(expr->expression_data.aggregate_func_expr.wildcard == true);
        ASSERT(expr->expression_data.aggregate_func_expr.expression == NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }


    /* ----- AGGREGATE COUNT COLUMN ----- */
    {
        char *query = strdup("placeholder COUNT(employee_id);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
        ASSERT(expr->expression_data.aggregate_func_expr.wildcard == false);
        ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);
        ASSERT(expr->expression_data.aggregate_func_expr.expression->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(expr->expression_data.aggregate_func_expr.expression->expression_data.column_value.column_name,
                      "employee_id") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- OTHER AGGREGATE TYPES ----- */
    {
        char *query = strdup("placeholder AVG(score) MIN(score) MAX(score);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_AVG);
        expression_node_free(expr);

        expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_MIN);
        expression_node_free(expr);

        expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_MAX);
        expression_node_free(expr);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- AGGREGATE ARITHMETIC ARGUMENT ----- */
    {
        char *query = strdup("placeholder SUM(salary + bonus * 2);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_FUNCTIONS);
        ASSERT(expr->expression_data.aggregate_func_expr.expression->type == EXPR_BINARY);
        ASSERT(expr->expression_data.aggregate_func_expr.expression->expression_data.binary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.aggregate_func_expr.expression->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.aggregate_func_expr.expression->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_MUL);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID AGGREGATE: MISSING OPENING PARENTHESIS ----- */
    {
        char *query = strdup("placeholder SUM salary;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_primary_expression(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID AGGREGATE: EMPTY ARGUMENT ----- */
    {
        char *query = strdup("placeholder SUM();");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_primary_expression(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID AGGREGATE: WILDCARD ONLY ALLOWED FOR COUNT ----- */
    {
        char *query = strdup("placeholder SUM(*);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_primary_expression(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID AGGREGATE: MISSING CLOSING PARENTHESIS ----- */
    {
        char *query = strdup("placeholder SUM(salary;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_primary_expression(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID PARENTHESIZED EXPRESSION: MISSING ')' ----- */
    {
        char *query = strdup("placeholder (1 + 2;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ASSERT(parse_primary_expression(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PRIMARY -> POSTFIX INTEGRATION: IDENTIFIER IS NULL ----- */
    {
        char *query = strdup("placeholder department IS NULL FROM employees;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_IS_NULL);
        ASSERT(expr->expression_data.is_null_expr.operand->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(expr->expression_data.is_null_expr.operand->expression_data.column_value.column_name,
                      "department") == 0);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PRIMARY -> POSTFIX INTEGRATION: PARENTHESIZED IS NOT NULL ----- */
    {
        char *query = strdup("placeholder (salary + bonus) IS NOT NULL;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_IS_NOT_NULL);
        ASSERT(expr->expression_data.is_not_null_expr.operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.is_not_null_expr.operand->expression_data.binary_expr.op == OP_ADD);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- PRIMARY PARSER MUST STOP AT RANDOM NON-EXPRESSION KEYWORD ----- */
    {
        char *query = strdup("placeholder column_name WHERE other = 1;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *expr = parse_primary_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(get_current_token(parser)->token, "WHERE") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_postfix_expressions() {

    /* ----- IS NULL ----- */
    {
        char *query = strdup("placeholder IS NULL;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }
        strncpy(operand->expression_data.column_value.column_name, "column_name", 64);

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_IS_NULL);
        ASSERT(expr->expression_data.is_null_expr.operand != NULL);
        ASSERT(expr->expression_data.is_null_expr.operand->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- IS NOT NULL ----- */
    {
        char *query = strdup("placeholder IS NOT NULL FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_IS_NOT_NULL);
        ASSERT(expr->expression_data.is_not_null_expr.operand != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID IS EXPRESSION ----- */
    {
        char *query = strdup("placeholder IS TRUE;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- IN WITH ONE VALUE ----- */
    {
        char *query = strdup("placeholder IN (1);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_IN);
        ASSERT(expr->expression_data.in_expr.option_count == 1);
        ASSERT(expr->expression_data.in_expr.set_options != NULL);
        ASSERT(expr->expression_data.in_expr.set_options[0]->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.in_expr.set_options[0]->expression_data.literal_value.literal->value.uint32_val == 1);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- IN WITH MULTIPLE VALUES/EXPRESSIONS ----- */
    {
        char *query = strdup("placeholder IN (1, 2 + 3, column_name);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);

        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_IN);
        ASSERT(expr->expression_data.in_expr.option_count == 3);
        ASSERT(expr->expression_data.in_expr.set_options[0]->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.in_expr.set_options[1]->type == EXPR_BINARY);
        ASSERT(expr->expression_data.in_expr.set_options[1]->expression_data.binary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.in_expr.set_options[2]->type == EXPR_COLUMN_REF);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    
    /* ----- INVALID IN: EMPTY LIST ----- */
    {
        char *query = strdup("placeholder IN ();");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID IN: TRAILING COMMA ----- */
    {
        char *query = strdup("placeholder IN (1,);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID IN: LEADING COMMA ----- */
    {
        char *query = strdup("placeholder IN (,1);");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID IN: MISSING PARENTHESES ----- */
    {
        char *query = strdup("placeholder IN 1;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- BETWEEN SIMPLE LIMITS ----- */
    {
        char *query = strdup("placeholder BETWEEN 10 AND 20;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_BETWEEN);
        ASSERT(expr->expression_data.between_expr.operand != NULL);
        ASSERT(expr->expression_data.between_expr.lower != NULL);
        ASSERT(expr->expression_data.between_expr.upper != NULL);
        ASSERT(expr->expression_data.between_expr.lower->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.between_expr.upper->type == EXPR_LITERAL);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- BETWEEN EXPRESSION LIMITS ----- */
    {
        char *query = strdup("placeholder BETWEEN 1 + 2 AND 10 * 3;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_BETWEEN);
        ASSERT(expr->expression_data.between_expr.lower->type == EXPR_BINARY);
        ASSERT(expr->expression_data.between_expr.lower->expression_data.binary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.between_expr.upper->type == EXPR_BINARY);
        ASSERT(expr->expression_data.between_expr.upper->expression_data.binary_expr.op == OP_MUL);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- BETWEEN MUST LEAVE OUTER AND UNCONSUMED ----- */
    {
        char *query = strdup("placeholder BETWEEN 1 AND 10 AND other = 5;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ExpressionNode *expr = parse_postfix_expression(parser, &operand);
        ASSERT(expr != NULL);
        ASSERT(operand == NULL);
        ASSERT(expr->type == EXPR_BETWEEN);
        ASSERT(strcmp(get_current_token(parser)->token, "AND") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID BETWEEN: MISSING AND ----- */
    {
        char *query = strdup("placeholder BETWEEN 1 10;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID BETWEEN: MISSING UPPER LIMIT ----- */
    {
        char *query = strdup("placeholder BETWEEN 1 AND;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- NON-POSTFIX TOKEN MUST REMAIN UNCONSUMED ----- */
    {
        char *query = strdup("placeholder FROM table;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }

        consume_token(parser);
        ExpressionNode *operand = expression_node_create(EXPR_COLUMN_REF);
        if (!operand) { return -1; }

        ASSERT(parse_postfix_expression(parser, &operand) == NULL);
        ASSERT(operand != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(operand);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_unary_expressions() {

    /* ----- UNARY PLUS ----- */
    {
        char *query = strdup("placeholder +5;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_unary(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_LITERAL);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- UNARY MINUS ----- */
    {
        char *query = strdup("placeholder -5;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_unary(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_SUB);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_LITERAL);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- BITWISE NOT ----- */
    {
        char *query = strdup("placeholder ~flags;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_unary(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_BITWISE_NOT);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_COLUMN_REF);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- CHAINED UNARY OPERATORS ----- */
    {
        char *query = strdup("placeholder -~+value;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_unary(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_SUB);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.operand->expression_data.unary_expr.op == OP_BITWISE_NOT);
        ASSERT(expr->expression_data.unary_expr.operand->expression_data.unary_expr.operand->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.operand->expression_data.unary_expr.operand->expression_data.unary_expr.op == OP_ADD);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- UNARY APPLIED TO PARENTHESIZED EXPRESSION ----- */
    {
        char *query = strdup("placeholder -(1 + 2) FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_unary(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.unary_expr.operand->expression_data.binary_expr.op == OP_ADD);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID UNARY: MISSING OPERAND ----- */
    {
        char *query = strdup("placeholder -;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_unary(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_multiplication_expressions() {

    /* ----- MULTIPLICATION ----- */
    {
        char *query = strdup("placeholder 2 * 3;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_MUL);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_LITERAL);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_LITERAL);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- DIVISION ----- */
    {
        char *query = strdup("placeholder 8 / 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->expression_data.binary_expr.op == OP_DIV);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MODULO ----- */
    {
        char *query = strdup("placeholder 8 % 3;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->expression_data.binary_expr.op == OP_MODULO);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- LEFT ASSOCIATIVITY ----- */
    {
        char *query = strdup("placeholder 16 / 4 / 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_DIV);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_DIV);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_LITERAL);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- UNARY HAS HIGHER PRECEDENCE ----- */
    {
        char *query = strdup("placeholder -2 * ~value;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_MUL);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_UNARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_UNARY);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT LOWER-PRECEDENCE '+' ----- */
    {
        char *query = strdup("placeholder 2 * 3 + 4;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->expression_data.binary_expr.op == OP_MUL);
        ASSERT(strcmp(get_current_token(parser)->token, "+") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT SQL KEYWORD ----- */
    {
        char *query = strdup("placeholder 2 * 3 FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_multiplication(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID MULTIPLICATION: MISSING RIGHT OPERAND ----- */
    {
        char *query = strdup("placeholder 2 *;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_multiplication(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_addition_expressions() {

    /* ----- ADDITION ----- */
    {
        char *query = strdup("placeholder 1 + 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_addition(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_ADD);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- SUBTRACTION ----- */
    {
        char *query = strdup("placeholder 5 - 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_addition(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->expression_data.binary_expr.op == OP_SUB);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- LEFT ASSOCIATIVITY ----- */
    {
        char *query = strdup("placeholder 10 - 3 - 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_addition(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_SUB);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_SUB);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MULTIPLICATION PRECEDENCE ----- */
    {
        char *query = strdup("placeholder 1 + 2 * 3 - 4 / 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_addition(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_SUB);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_MUL);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_DIV);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT COMPARISON ----- */
    {
        char *query = strdup("placeholder 1 + 2 >= 3;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_addition(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, ">=") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT ORDER BY ----- */
    {
        char *query = strdup("placeholder value + 1 ORDER BY value;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_addition(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "ORDER BY") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID ADDITION ----- */
    {
        char *query = strdup("placeholder 1 +;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_addition(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_comparison_expressions() {

    /* ----- ALL COMPARISON OPERATORS ----- */
    {
        char *query = strdup("placeholder a = b a != b a <> b a < b a <= b a > b a >= b;");

        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        OperatorType expected_ops[] = { OP_EQ, OP_NEQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE };

        for (uint32_t i = 0; i < 7; i++) {
            ExpressionNode *expr = parse_comparison(parser);
            ASSERT(expr != NULL);
            ASSERT(expr->type == EXPR_BINARY);
            ASSERT(expr->expression_data.binary_expr.op == expected_ops[i]);
            ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_COLUMN_REF);
            ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_COLUMN_REF);
            expression_node_free(expr);
        }

        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- ARITHMETIC OPERANDS ----- */
    {
        char *query = strdup("placeholder a + 1 >= b * 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_comparison(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_GTE);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_ADD);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_MUL);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- ONLY ONE COMPARISON IS PARSED ----- */
    {
        char *query = strdup("placeholder a < b < c;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_comparison(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_LT);
        ASSERT(strcmp(get_current_token(parser)->token, "<") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT NOT/AND/OR LEVELS ----- */
    {
        char *query = strdup("placeholder a = b AND c = d;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_comparison(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "AND") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT SQL KEYWORD ----- */
    {
        char *query = strdup("placeholder a = b FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_comparison(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID COMPARISON ----- */
    {
        char *query = strdup("placeholder a =;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_comparison(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_not_expressions() {

    /* ----- NOT EXPRESSION ----- */
    {
        char *query = strdup("placeholder NOT active;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_not(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_NOT);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_COLUMN_REF);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- CHAINED NOT ----- */
    {
        char *query = strdup("placeholder NOT NOT active;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_not(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_NOT);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.operand->expression_data.unary_expr.op == OP_NOT);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- COMPARISON HAS HIGHER PRECEDENCE THAN NOT ----- */
    {
        char *query = strdup("placeholder NOT age >= 18;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_not(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_NOT);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.unary_expr.operand->expression_data.binary_expr.op == OP_GTE);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }


    /* ----- POSTFIX HAS HIGHER PRECEDENCE THAN NOT ----- */
    {
        char *query = strdup("placeholder NOT department IS NULL;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_not(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_UNARY);
        ASSERT(expr->expression_data.unary_expr.op == OP_NOT);
        ASSERT(expr->expression_data.unary_expr.operand->type == EXPR_IS_NULL);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- NOT MUST STOP BEFORE AND ----- */
    {
        char *query = strdup("placeholder NOT active AND enabled;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_not(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "AND") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID NOT ----- */
    {
        char *query = strdup("placeholder NOT;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_not(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_and_expressions() {

    /* ----- SIMPLE AND ----- */
    {
        char *query = strdup("placeholder a AND b;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_and(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_AND);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_COLUMN_REF);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_COLUMN_REF);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- CHAINED AND IS LEFT ASSOCIATIVE ----- */
    {
        char *query = strdup("placeholder a AND b AND c;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_and(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_AND);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_AND);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- NOT/COMPARISON PRECEDENCE ----- */
    {
        char *query = strdup("placeholder NOT a = 1 AND b >= 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_and(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_AND);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_UNARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.unary_expr.op == OP_NOT);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.unary_expr.operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_GTE);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- BETWEEN'S INTERNAL AND MUST NOT STEAL OUTER AND ----- */
    {
        char *query = strdup("placeholder age BETWEEN 18 AND 65 AND active = TRUE;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_and(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_AND);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BETWEEN);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_EQ);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP BEFORE OR ----- */
    {
        char *query = strdup("placeholder a AND b OR c;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_and(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_AND);
        ASSERT(strcmp(get_current_token(parser)->token, "OR") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- MUST STOP AT ORDER BY ----- */
    {
        char *query = strdup("placeholder a AND b ORDER BY a;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_and(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, "ORDER BY") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID AND ----- */
    {
        char *query = strdup("placeholder a AND;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_and(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_or_expressions() {

    /* ----- SIMPLE OR ----- */
    {
        char *query = strdup("placeholder a OR b;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_or(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- CHAINED OR IS LEFT ASSOCIATIVE ----- */
    {
        char *query = strdup("placeholder a OR b OR c;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_or(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_OR);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- AND HAS HIGHER PRECEDENCE THAN OR ----- */
    {
        char *query = strdup("placeholder a OR b AND c;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_or(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_COLUMN_REF);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_AND);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- COMPLETE MIXED-PRECEDENCE EXPRESSION ----- */
    {
        char *query = strdup("placeholder NOT age + 1 >= 18 AND active = TRUE OR score * 2 > 100;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);

        ExpressionNode *left = expr->expression_data.binary_expr.left_operand;
        ExpressionNode *right = expr->expression_data.binary_expr.right_operand;

        ASSERT(left->type == EXPR_BINARY);
        ASSERT(left->expression_data.binary_expr.op == OP_AND);
        ASSERT(left->expression_data.binary_expr.left_operand->type == EXPR_UNARY);
        ASSERT(left->expression_data.binary_expr.left_operand->expression_data.unary_expr.op == OP_NOT);
        ASSERT(left->expression_data.binary_expr.left_operand->expression_data.unary_expr.operand->type == EXPR_BINARY);
        ASSERT(left->expression_data.binary_expr.left_operand->expression_data.unary_expr.operand->expression_data.binary_expr.op == OP_GTE);
        ASSERT(left->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(left->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_EQ);

        ASSERT(right->type == EXPR_BINARY);
        ASSERT(right->expression_data.binary_expr.op == OP_GT);
        ASSERT(right->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(right->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_MUL);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- POSTFIX + LOGICAL PRECEDENCE IN FULL EXPRESSION ----- */
    {
        char *query = strdup("placeholder department IS NOT NULL AND salary BETWEEN 1000 AND 2000 OR active = TRUE;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_AND);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.left_operand->type == EXPR_IS_NOT_NULL);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.right_operand->type == EXPR_BETWEEN);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_EQ);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT FROM ----- */
    {
        char *query = strdup("placeholder a + b * 2 FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_ADD);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT ORDER BY ----- */
    {
        char *query = strdup("placeholder score >= 10 ORDER BY score;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_GTE);
        ASSERT(strcmp(get_current_token(parser)->token, "ORDER BY") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT COMMA ----- */
    {
        char *query = strdup("placeholder a + 1, b + 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, ",") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT CLOSING PARENTHESIS ----- */
    {
        char *query = strdup("placeholder a + 1) FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, ")") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- INVALID OR ----- */
    {
        char *query = strdup("placeholder a OR;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ASSERT(parse_or(parser) == NULL);

        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}

static int test_parsing_expressions() {

    /* ----- COMPLETE MIXED-PRECEDENCE EXPRESSION ----- */
    {
        char *query = strdup("placeholder NOT age + 1 >= 18 AND active = TRUE OR score * 2 > 100;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);

        ExpressionNode *left = expr->expression_data.binary_expr.left_operand;
        ExpressionNode *right = expr->expression_data.binary_expr.right_operand;

        ASSERT(left->type == EXPR_BINARY);
        ASSERT(left->expression_data.binary_expr.op == OP_AND);
        ASSERT(left->expression_data.binary_expr.left_operand->type == EXPR_UNARY);
        ASSERT(left->expression_data.binary_expr.left_operand->expression_data.unary_expr.op == OP_NOT);
        ASSERT(left->expression_data.binary_expr.left_operand->expression_data.unary_expr.operand->type == EXPR_BINARY);
        ASSERT(left->expression_data.binary_expr.left_operand->expression_data.unary_expr.operand->expression_data.binary_expr.op == OP_GTE);
        ASSERT(left->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(left->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_EQ);

        ASSERT(right->type == EXPR_BINARY);
        ASSERT(right->expression_data.binary_expr.op == OP_GT);
        ASSERT(right->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(right->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_MUL);
        ASSERT(strcmp(get_current_token(parser)->token, ";") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- POSTFIX + LOGICAL PRECEDENCE IN FULL EXPRESSION ----- */
    {
        char *query = strdup("placeholder department IS NOT NULL AND salary BETWEEN 1000 AND 2000 OR active = TRUE;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_OR);
        ASSERT(expr->expression_data.binary_expr.left_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.op == OP_AND);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.left_operand->type == EXPR_IS_NOT_NULL);
        ASSERT(expr->expression_data.binary_expr.left_operand->expression_data.binary_expr.right_operand->type == EXPR_BETWEEN);
        ASSERT(expr->expression_data.binary_expr.right_operand->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.binary_expr.op == OP_EQ);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT FROM ----- */
    {
        char *query = strdup("placeholder a + b * 2 FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_ADD);
        ASSERT(strcmp(get_current_token(parser)->token, "FROM") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT ORDER BY ----- */
    {
        char *query = strdup("placeholder score >= 10 ORDER BY score;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(expr->type == EXPR_BINARY);
        ASSERT(expr->expression_data.binary_expr.op == OP_GTE);
        ASSERT(strcmp(get_current_token(parser)->token, "ORDER BY") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT COMMA ----- */
    {
        char *query = strdup("placeholder a + 1, b + 2;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, ",") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    /* ----- EXPRESSION MUST STOP AT CLOSING PARENTHESIS ----- */
    {
        char *query = strdup("placeholder a + 1) FROM table;");
        Tokenizer *tokenizer = tokenizer_init(query);
        if (!tokenizer) { return -1; }
        TokenArray *token_array = tokenize_query(tokenizer);
        if (!token_array) { return -1; }
        Parser *parser = parser_init(token_array);
        if (!parser) { return -1; }
        consume_token(parser);

        ExpressionNode *expr = parse_expression(parser);
        ASSERT(expr != NULL);
        ASSERT(strcmp(get_current_token(parser)->token, ")") == 0);

        expression_node_free(expr);
        tokenizer_free(tokenizer);
        parser_free(parser);
    }

    return 0;
}


/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;
    printf("> STARTING TESTS\n");

    result = test_parser_init();
    generate_output(result, 0, "test_parser_init");
    result = test_parsing_literal_expressions();
    generate_output(result, 1, "test_parsing_literal_expressions");
    result = test_parsing_primary_expressions();
    generate_output(result, 2, "test_parsing_primary_expressions");
    result = test_parsing_postfix_expressions();
    generate_output(result, 3, "test_parsing_postfix_expressions");
    result = test_parsing_unary_expressions();
    generate_output(result, 4, "test_parsing_unary_expressions");
    result = test_parsing_multiplication_expressions();
    generate_output(result, 5, "test_parsing_multiplication_expressions");
    result = test_parsing_addition_expressions();
    generate_output(result, 6, "test_parsing_addition_expressions");
    result = test_parsing_comparison_expressions();
    generate_output(result, 7, "test_parsing_comparison_expressions");
    result = test_parsing_not_expressions();
    generate_output(result, 8, "test_parsing_not_expressions");
    result = test_parsing_and_expressions();
    generate_output(result, 9, "test_parsing_and_expressions");
    result = test_parsing_or_expressions();
    generate_output(result, 10, "test_parsing_or_expressions");
    result = test_parsing_expressions();
    generate_output(result, 11, "test_parsing_expressions");

    printf("> TESTS RAN SUCCESSFULLY\n");
    return 0;
}
