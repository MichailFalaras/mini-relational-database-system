#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "parser_utils.h"
#include "../../include/parser.h"
#include "../../include/tokenizer.h"
#include "../../include/expressions.h"
#include "../../include/ast.h"
#include "../ast/ast_utils.h"


/* ASTNodeType to StatementType. */
StatementType ast_to_statement_type(ASTNodeType type) {
    switch (type) {
        case AST_SELECT:
            return STMT_SELECT;

        case AST_UPDATE:
            return STMT_UPDATE;

        case AST_INSERT:
            return STMT_INSERT;

        case AST_DELETE:
            return STMT_DELETE;
            
        case AST_CREATE_TABLE:
            return STMT_CREATE_TABLE;

        case AST_DROP_TABLE:
            return STMT_DROP_TABLE;

        case AST_ALTER_TABLE:
            return STMT_ALTER_TABLE;

        case AST_TRUNCATE_TABLE:
            return STMT_TRUNCATE_TABLE;

        case AST_CREATE_INDEX:
            return STMT_CREATE_INDEX;

        case AST_DROP_INDEX:
            return STMT_DROP_INDEX;

        default: 
            printf("ASTNodeType doesn't exist\n");
            return STMT_ERROR;
    }
}

/* Token string to OperatorType. */
OperatorType token_str_to_operator_type(char *token_str) {
    if (!token_str) {
        return OP_ERROR;
    }

    if (!strcasecmp(token_str, "=")) {
        return OP_EQ;
    } else if (!strcmp(token_str, "!=") || !strcmp(token_str, "<>")) {
        return OP_NEQ;
    } else if (!strcmp(token_str, "<")) {
        return OP_LT;
    } else if (!strcmp(token_str, "<=")) {
        return OP_LTE;
    } else if (!strcmp(token_str, ">")) {
        return OP_GT;
    } else if (!strcmp(token_str, ">=")) {
        return OP_GTE;
    } else if (!strcasecmp(token_str, "AND")) {
        return OP_AND;
    } else if (!strcasecmp(token_str, "OR")) {
        return OP_OR;
    } else if (!strcasecmp(token_str, "NOT")) {
        return OP_NOT;
    } else if (!strcmp(token_str, "+")) {
        return OP_ADD;
    } else if (!strcmp(token_str, "-")) {
        return OP_SUB;
    } else if (!strcmp(token_str, "*")) {
        return OP_MUL;
    } else if (!strcmp(token_str, "/")) {
        return OP_DIV;
    } else if (!strcmp(token_str, "%")) {
        return OP_MODULO;
    } else if (!strcmp(token_str, "~")) {
        return OP_BITWISE_NOT;
    }
    
    return OP_ERROR;
}

/* Check if Token string is a specific OperatorType. */
bool is_token_operator(char *token_str, OperatorType type) {
    if (!token_str || type >= OP_ERROR) {
        return false;
    }

    OperatorType token_operator_type = token_str_to_operator_type(token_str);
    if (token_operator_type == OP_ERROR) {
        return false;
    } 

    if (token_operator_type == type) {
        return true;
    }

    return false;
}

/* Check if Token string is specifically a comparison operator. */
bool is_token_comparison_operator(char *token_str) {
    if (!token_str) {
        return false;
    }

    OperatorType token_operator_type = token_str_to_operator_type(token_str);
    if (token_operator_type == OP_ERROR) {
        return false;
    }

    return token_operator_type == OP_EQ  || token_operator_type == OP_NEQ ||
       token_operator_type == OP_LT  || token_operator_type == OP_LTE ||
       token_operator_type == OP_GT  || token_operator_type == OP_GTE;
} 

/* Validate that there is a token for Parser's current position in the TokenArray.
 * Then return Token* .*/
Token *get_current_token(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens) {
        return NULL;
    }

    if (parser->current_position >= parser->token_array->amount_tokens) {
        return NULL;
    }

    return parser->token_array->tokens[parser->current_position];
}

/* Consume token if possible. */
void consume_token(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens) {
        return;
    }

    if (parser->current_position < parser->token_array->amount_tokens) {
        parser->current_position++;
    }
}

/* ----- EXPRESSION PARSING ----- */

/* Parse OR Expression. */
ExpressionNode *parse_or(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }
    
    ExpressionNode *left_operand = parse_and(parser);
    if (!left_operand) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    while (curr_token && is_token_operator(curr_token->token, OP_OR)) {
        consume_token(parser);

        ExpressionNode *right_operand = parse_and(parser);
        if (!right_operand) {
            expression_node_free(left_operand);
            return NULL;
        }

        ExpressionNode *new_left = expression_create_binary_tree(left_operand, OP_OR, right_operand);
        if (!new_left) {
            expression_node_free(left_operand);
            expression_node_free(right_operand);
            return NULL;
        }

        left_operand = new_left;
        curr_token = get_current_token(parser);
    }

    return left_operand;
}

/* Parse AND Expression. */
ExpressionNode *parse_and(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    ExpressionNode *left_operand = parse_not(parser);
    if (!left_operand) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    while (curr_token && is_token_operator(curr_token->token, OP_AND)) {
        consume_token(parser);

        ExpressionNode *right_operand = parse_not(parser);
        if (!right_operand) {
            expression_node_free(left_operand);
            return NULL;
        }

        ExpressionNode *new_left = expression_create_binary_tree(left_operand, OP_AND, right_operand);
        if (!new_left) {
            expression_node_free(left_operand);
            expression_node_free(right_operand);
            return NULL;
        }

        left_operand = new_left;
        curr_token = get_current_token(parser);
    }

    return left_operand;
}

/* Parse NOT Expression. */
ExpressionNode *parse_not(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    if (is_token_operator(curr_token->token, OP_NOT)) {
        consume_token(parser);

        ExpressionNode *not_expr = expression_node_create(EXPR_UNARY);
        if (!not_expr) {
            return NULL;
        }

        not_expr->expression_data.unary_expr.op = OP_NOT;
        not_expr->expression_data.unary_expr.operand = parse_not(parser);
        if (!not_expr->expression_data.unary_expr.operand) {
            expression_node_free(not_expr);
            return NULL;
        }

        return not_expr;
    }

    return parse_comparison(parser);
}

/* Parse Comparison Expression. */
ExpressionNode *parse_comparison(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    ExpressionNode *left_operand = parse_addition(parser);
    if (!left_operand) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    if (!curr_token || !is_token_comparison_operator(curr_token->token)) {
        return left_operand;
    }

    OperatorType op = token_str_to_operator_type(curr_token->token);
    consume_token(parser);

    ExpressionNode *right_operand = parse_addition(parser);
    if (!right_operand) {
        expression_node_free(left_operand);
        return NULL;
    }

    ExpressionNode *comparison_expr = expression_create_binary_tree(left_operand, op, right_operand);
    if (!comparison_expr) {
        expression_node_free(left_operand);
        expression_node_free(right_operand);
        return NULL;
    }

    return comparison_expr;
}

/* Parse Addition/Subtraction Expression. */
ExpressionNode *parse_addition(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    ExpressionNode *left_operand = parse_multiplication(parser);
    if (!left_operand) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    while (curr_token &&
           (is_token_operator(curr_token->token, OP_ADD) ||
            is_token_operator(curr_token->token, OP_SUB))) {
        OperatorType op = token_str_to_operator_type(curr_token->token);
        consume_token(parser);

        ExpressionNode *right_operand = parse_multiplication(parser);
        if (!right_operand) {
            expression_node_free(left_operand);
            return NULL;
        }

        ExpressionNode *new_left = expression_create_binary_tree(left_operand, op, right_operand);
        if (!new_left) {
            expression_node_free(left_operand);
            expression_node_free(right_operand);
            return NULL;
        }

        left_operand = new_left;
        curr_token = get_current_token(parser);
    }

    return left_operand;
}

/* Parse Multiplication/Division/Modulo Expression. */
ExpressionNode *parse_multiplication(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    ExpressionNode *left_operand = parse_unary(parser);
    if (!left_operand) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    while (curr_token &&
           (is_token_operator(curr_token->token, OP_MUL) ||
            is_token_operator(curr_token->token, OP_DIV) ||
            is_token_operator(curr_token->token, OP_MODULO))) {
        OperatorType op = token_str_to_operator_type(curr_token->token);
        consume_token(parser);

        ExpressionNode *right_operand = parse_unary(parser);
        if (!right_operand) {
            expression_node_free(left_operand);
            return NULL;
        }

        ExpressionNode *new_left = expression_create_binary_tree(left_operand, op, right_operand);
        if (!new_left) {
            expression_node_free(left_operand);
            expression_node_free(right_operand);
            return NULL;
        }

        left_operand = new_left;
        curr_token = get_current_token(parser);
    }

    return left_operand;
}

/* Parse Unary (+, -, ~) Expression. */
ExpressionNode *parse_unary(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    if (is_token_operator(curr_token->token, OP_ADD) ||
        is_token_operator(curr_token->token, OP_SUB) ||
        is_token_operator(curr_token->token, OP_BITWISE_NOT)) {
        OperatorType op = token_str_to_operator_type(curr_token->token);
        consume_token(parser);

        ExpressionNode *unary_expr = expression_node_create(EXPR_UNARY);
        if (!unary_expr) {
            return NULL;
        }

        unary_expr->expression_data.unary_expr.op = op;
        unary_expr->expression_data.unary_expr.operand = parse_unary(parser);
        if (!unary_expr->expression_data.unary_expr.operand) {
            expression_node_free(unary_expr);
            return NULL;
        }

        return unary_expr;
    }

    return parse_primary_expression(parser);
}

/* Parse Identifier/Literal/Parentheses Expression.
 *
 * Supported literal types are:
 * INTEGER
 * NUMERIC
 * CHAR(n), where n = strlen(string_literal)
 * DATE
 * TIMESTAMP
 * BOOL. */
ExpressionNode *parse_primary_expression(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }
    ExpressionNode *expr = NULL;

    Token *curr_token = get_current_token(parser);
    if (!strcmp(curr_token->token, "(")) {
        consume_token(parser);

        ExpressionNode *parentheses_expr = parse_expression(parser);
        if (!parentheses_expr) {
            return NULL;
        }

        curr_token = get_current_token(parser);
        if (!curr_token || strcmp(curr_token->token, ")") != 0) {
            expression_node_free(parentheses_expr);
            return NULL;
        }

        expr = parentheses_expr;
        goto parse_postfix_expression;
    }

    if (curr_token->type == IDENTIFIER) {

        /* Normally, aggregate functions are categorized as IDENTIFIERs.
         *
         * (NOTE: Select or other keyword parsing should be handling
         * "AS alias" separately) */
        if (!strcasecmp(curr_token->token, "SUM")
            || !strcasecmp(curr_token->token, "COUNT")
            || !strcasecmp(curr_token->token, "AVG")
            || !strcasecmp(curr_token->token, "MIN")
            || !strcasecmp(curr_token->token, "MAX")) {
            consume_token(parser);

            ExpressionNode *aggregate_funct_expr = expression_node_create(EXPR_FUNCTIONS);
            if (!aggregate_funct_expr) {
                return NULL;
            }

            aggregate_funct_expr->expression_data.aggregate_func_expr.type = get_aggregate_function_type_from_str(curr_token->token);

            
            curr_token = get_current_token(parser);
            
            if (curr_token->type != PUNCTUATION || strcmp(curr_token->token, "(") != 0) {
                expression_node_free(aggregate_funct_expr);
                return NULL;
            }
            consume_token(parser);

            curr_token = get_current_token(parser);
            if (aggregate_funct_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT
                && curr_token->type == OPERATOR && !strcmp(curr_token->token, "*")) {
                aggregate_funct_expr->expression_data.aggregate_func_expr.wildcard = true;

                consume_token(parser);
            } else {
                /* Parse addition to allow for operations inside aggregate functions but not
                 * OR/AND/NOT. */
                aggregate_funct_expr->expression_data.aggregate_func_expr.expression = parse_addition(parser);
                if (!aggregate_funct_expr->expression_data.aggregate_func_expr.expression) {
                    expression_node_free(aggregate_funct_expr);
                    return NULL;
                }
            }

            curr_token = get_current_token(parser);
            if (curr_token->type != PUNCTUATION || strcmp(curr_token->token, ")") != 0) {
                expression_node_free(aggregate_funct_expr);
                return NULL;
            }
            consume_token(parser);
            
            return aggregate_funct_expr;
        }

        ExpressionNode *identifier_expr = expression_node_create(EXPR_COLUMN_REF);
        if (!identifier_expr) {
            return NULL;
        }

        strncpy(identifier_expr->expression_data.column_value.column_name,
                curr_token->token, 64);
        identifier_expr->expression_data.column_value.column_name[63] = '\0';

        expr = identifier_expr;
        goto parse_postfix_expression;
    }

    ExpressionNode *literal_expr = parse_literal_expression(parser);
    if (!literal_expr) {
        return NULL;
    }
    consume_token(parser);

    expr = literal_expr;
    return expr;

    /* Parse postfix expression KEYWORDs that come right after parentheses/identifiers. */
parse_postfix_expression: 
    consume_token(parser);
    ExpressionNode *postfix_expr = parse_postfix_expression(parser, &expr);
    if (postfix_expr != NULL) {
        expr = postfix_expr;
    }
    
    return expr;
}

/* Parse postfix expressions like: IS NULL / IS NOT NULL / IN / BETWEEN
 * that require their own unique parsing. 
 *
 * Pass parenthesized expression or identifier as operand for complete parsing. */
ExpressionNode *parse_postfix_expression(Parser *parser, ExpressionNode **operand) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);

    /* Create IS NULL/IS NOT NULL expression node that contains the identifier. */
    if (curr_token->type == KEYWORD && !strcasecmp(curr_token->token, "IS")) {
        ExpressionType expr_type = EXPR_IS_NULL;

        consume_token(parser);
        curr_token = get_current_token(parser);
        if (curr_token->type == KEYWORD && !strcasecmp(curr_token->token, "NOT")) {
            consume_token(parser);
            curr_token = get_current_token(parser);

            expr_type = EXPR_IS_NOT_NULL;
        } 

        if (curr_token->type != KEYWORD || strcasecmp(curr_token->token, "NULL") != 0) {
            expression_node_free(*operand);
            *operand = NULL;
            return NULL;
        }
        consume_token(parser);

        ExpressionNode *is_expr = expression_node_create(expr_type);
        if (!is_expr) {
            expression_node_free(*operand);
            *operand = NULL;
            return NULL;
        }

        if (expr_type == EXPR_IS_NULL) {
            is_expr->expression_data.is_null_expr.operand = *operand;
        } else {
            is_expr->expression_data.is_not_null_expr.operand = *operand;
        }

        *operand = NULL;

        return is_expr;
    }

    if (curr_token->type == KEYWORD && !strcasecmp(curr_token->token, "IN")) {
        ExpressionNode *in_expr = expression_node_create(EXPR_IN);
        if (!in_expr) {
            expression_node_free(*operand);
            *operand = NULL;
            return NULL;
        }

        consume_token(parser);
        curr_token = get_current_token(parser);

        in_expr->expression_data.in_expr.operand = *operand;
        *operand = NULL;
        if (strcmp(curr_token->token, "(") != 0) {
            expression_node_free(in_expr);
            *operand = NULL;
            return NULL;
        }

        /* Require another expression immediately.
         * Used to reject lists like (1, 2,).  */
        do {
            consume_token(parser);

            uint32_t option_count = ++in_expr->expression_data.in_expr.option_count; 
            ExpressionNode **new_set_options = (ExpressionNode **) realloc(in_expr->expression_data.in_expr.set_options,
                                                                    option_count*sizeof(ExpressionNode *));
            if (!new_set_options) {
                expression_node_free(in_expr);
                *operand = NULL;
                return NULL;
            }
            in_expr->expression_data.in_expr.set_options = new_set_options;

            in_expr->expression_data.in_expr.set_options[option_count-1] = parse_expression(parser);
            if (!in_expr->expression_data.in_expr.set_options[option_count-1]) {
                expression_node_free(in_expr);
                *operand = NULL;
                return NULL;
            }

            curr_token = get_current_token(parser);
        } while (strcmp(curr_token->token, ",") == 0);

        curr_token = get_current_token(parser);
        if (strcmp(curr_token->token, ")") != 0) {
            expression_node_free(in_expr);
            *operand = NULL;
            return NULL;
        }

        /* If empty list is given in queries like:
         * number in ()
         *
         * It should be rejected. */
        if (in_expr->expression_data.in_expr.option_count == 0) {
            expression_node_free(in_expr);
            *operand = NULL;
            return NULL;
        }

        consume_token(parser);
        curr_token = get_current_token(parser);


        return in_expr;
    }
    
    if (curr_token->type == KEYWORD && !strcasecmp(curr_token->token, "BETWEEN")) {

        ExpressionNode *between_expr = expression_node_create(EXPR_BETWEEN);
        if (!between_expr) {
            expression_node_free(*operand);
            *operand = NULL;
            return NULL;
        }
        consume_token(parser);

        between_expr->expression_data.between_expr.operand = *operand;
        *operand = NULL;

        /* Parse lower and upper limit separately to avoid chaining ANDs. */
        between_expr->expression_data.between_expr.lower = parse_not(parser);
        if (!between_expr->expression_data.between_expr.lower) {
            expression_node_free(between_expr);
            *operand = NULL;
            return NULL;
        }

        curr_token = get_current_token(parser);
        if (!curr_token || curr_token->type != KEYWORD
            || strcasecmp(curr_token->token, "AND") != 0) {
            expression_node_free(between_expr);
            *operand = NULL;
            return NULL;
        }
        consume_token(parser);

        between_expr->expression_data.between_expr.upper = parse_not(parser);
        if (!between_expr->expression_data.between_expr.upper) {
            expression_node_free(between_expr);
            *operand = NULL;
            return NULL;
        }

        return between_expr;
    }

    return NULL;
}

/* Parse literal expressions. */
ExpressionNode *parse_literal_expression(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) {
        return NULL;
    }

    Token *curr_token = get_current_token(parser);
    ExpressionNode *literal_expr = expression_node_create(EXPR_LITERAL);
    if (!literal_expr) {
        return NULL;
    }

    Value *literal = NULL;
    if (curr_token->type == NUMBER) {
        literal = create_number_literal(parser);

    } else if (curr_token->type == STRING) {
        literal = create_string_literal(parser);
        
    } else if (curr_token->type == KEYWORD) {

        if (strcasecmp(curr_token->token, "TRUE") != 0
            && strcasecmp(curr_token->token, "FALSE") != 0) {
            expression_node_free(literal_expr);
            return NULL;
        }
        
        literal = create_string_literal(parser);

    } else {
        expression_node_free(literal_expr);
        return NULL;
    }

    if (!literal) {
        expression_node_free(literal_expr);
        return NULL;
    }

    literal_expr->expression_data.literal_value.literal = literal;
    return literal_expr;
}

/* Identify UNSIGNED INTEGER or NUMERIC literal. 
 *
 * (NOTE: INTEGER/NUMERIC sign is parsed through parse_unary). */
Value *create_number_literal(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->current_position 
        || !parser->token_array->tokens) {
       return NULL;
    }
    Token *token = get_current_token(parser);
    if (!token || token->type != NUMBER || !token->token) {
        return NULL;
    }

    const char *token_string = token->token;
    if (*token_string == '\0') {
        return NULL;
    }

    bool is_numeric = false;
    uint32_t scale = 0;
    uint32_t number = 0;

    for (uint32_t i = 0; token_string[i] != '\0'; i++) {
        if (token_string[i] == '.') {
            if (is_numeric || i == 0 || token_string[i + 1] == '\0') {
                return NULL;
            }
            is_numeric = true;
            continue;
        }

        if (!isdigit((unsigned char) token_string[i])) {
            return NULL;
        }

        number = (number * 10) + (uint32_t)(token_string[i] - '0');
        if (is_numeric) {
            scale++;
        }
    }

    if (is_numeric) {
        numeric_t numeric_val = {0};
        numeric_val.scale = scale;
        numeric_val.val = number;
        return value_create(NUMERIC, &numeric_val);
    }

    return value_create(UNSIGNED_INTEGER, &number);
}

/* Identify if string is CHAR(n), DATE, TIMESTAMP or BOOL. */
Value *create_string_literal(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->current_position 
        || !parser->token_array->tokens) {
       return NULL;
    }

    Token *token = get_current_token(parser);
    if (!token || !token->token) {
        return NULL;
    }

    const char *token_string = token->token;

    if (token->type == KEYWORD) {
        if (!strcasecmp(token_string, "TRUE") || !strcasecmp(token_string, "FALSE")) {
            bool bool_val = !strcasecmp(token_string, "TRUE");
            return value_create(BOOL, &bool_val);
        }
        return NULL;
    }

    if (token->type != STRING) {
        return NULL;
    }

    size_t token_len = strlen(token_string);
    if (token_len < 2 || token_string[0] != '\'' || token_string[token_len - 1] != '\'') {
        return NULL;
    }

    /* Keep the tokenizer-owned string representation here. The Value layer
     * is responsible for copying/normalizing CHAR storage. Temporal detection
     * is intentionally strict so ordinary strings containing digits are not
     * silently converted to DATE/TIMESTAMP. */
    int year, month, day, hour, minute, second;

    int chars_read = 0;
    if (sscanf(token_string, "'%4d-%2d-%2d %2d:%2d:%2d'%n",
               &year, &month, &day, &hour, &minute, &second, &chars_read) == 6
        && chars_read == strlen(token_string)) {
        struct tm tm_value = {0};
        tm_value.tm_year = year - 1900;
        tm_value.tm_mon = month - 1;
        tm_value.tm_mday = day;
        tm_value.tm_hour = hour;
        tm_value.tm_min = minute;
        tm_value.tm_sec = second;

        __time64_t unix_time = _mkgmtime64(&tm_value);
        if (unix_time >= 0 &&
            tm_value.tm_year == year - 1900 && tm_value.tm_mon == month - 1 &&
            tm_value.tm_mday == day && tm_value.tm_hour == hour &&
            tm_value.tm_min == minute && tm_value.tm_sec == second) {
            uint64_t epoch = (uint64_t) unix_time;
            return value_create(TIMESTAMP, &epoch);
        }
    }

    chars_read = 0;
    if (sscanf(token_string, "'%4d-%2d-%2d'%n", &year, &month, &day, &chars_read) == 3
        && chars_read == strlen(token_string)) {
        struct tm tm_value = {0};
        tm_value.tm_year = year - 1900;
        tm_value.tm_mon = month - 1;
        tm_value.tm_mday = day;

        __time64_t unix_time = _mkgmtime64(&tm_value);
        if (unix_time >= 0 &&
            tm_value.tm_year == year - 1900 && tm_value.tm_mon == month - 1 &&
            tm_value.tm_mday == day) {
            uint64_t epoch = (uint64_t) unix_time;
            return value_create(DATE, &epoch);
        }
    }

    size_t content_len = token_len - 2;

    char *content = malloc(content_len + 1);
    if (!content) {
        return NULL;
    }

    memcpy(content, token_string + 1, content_len);
    content[content_len] = '\0';

    char_n_t char_n = {0};
    char_n.n = (uint32_t)content_len;
    char_n.string = content;

    Value *char_value = value_create(CHAR, &char_n);

    free(content);
    return char_value;
}

/* ---------- Parsing of the available Query Types ---------- */

/* Parse UPDATE query */
ASTNode *parse_update(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse UPDATE token
    Token *update_token = get_current_token(parser);

    if (!update_token ||
        update_token->type != KEYWORD ||
        strcasecmp(update_token->token, "UPDATE")) {
        printf("parse_update: UPDATE keyword doesn't exist.");
        return NULL;
    }

    consume_token(parser);

    // Parse table name after UPDATE,
    Token *table_token = get_current_token(parser);

    if (!table_token || table_token->type != IDENTIFIER) {
        printf("parse_update: Table identifier name doesn't exist after UPDATE.");
        return NULL;
    }

    ASTNode *root = (ASTNode *) calloc(1, sizeof(ASTNode));

    if (!root) {
        return NULL;
    }

    root->type = AST_UPDATE;

    // and if it exists, add it to the new AST UPDATE node
    strncpy(
        root->node_contents.update.table_name, 
        table_token->token, 
        sizeof(root->node_contents.update.table_name) - 1
    );

    consume_token(parser);

    // Parse SET clause
    root->node_contents.update.set = parse_set(parser);
    if (!root->node_contents.update.set) {
        printf("parse_update: AST SET node is NULL");
        ast_free_node(root);
        return NULL;
    }

    // Parse optional WHERE clause
    Token *current = get_current_token(parser);

    if (current && current->type == KEYWORD && !strcasecmp(current->token, "WHERE")) {
        root->node_contents.update.where = parse_where(parser);

        if (!root->node_contents.update.where) {
            printf("parse_update: Invalid WHERE clause.");
            ast_free_node(root);
            return NULL;
        }
    }

    return root;
}

/* Parse INSERT INTO query */
ASTNode *parse_insert(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse INSERT token
    Token *insert_token = get_current_token(parser);

    if (!insert_token || 
        insert_token->type != KEYWORD || 
        strcasecmp(insert_token->token, "INSERT")) {
        printf("parse_insert: INSERT keyword doesn't exist.");
        return NULL;
    }

    consume_token(parser);

    ASTNode *root = (ASTNode *) calloc(1, sizeof(ASTNode));

    if (!root) {
        return NULL;
    }

    root->type = AST_INSERT;

    // Parse INTO clause
    root->node_contents.insert.into = parse_into(parser);
    if (!root->node_contents.insert.into) {
        printf("parse_insert: AST INTO node is NULL");
        ast_free_node(root);
        return NULL;
    }

    // Parse VALUES clause
    root->node_contents.insert.values = parse_values(parser);
    if (!root->node_contents.insert.values) {
        printf("parse_insert: AST VALUES node is NULL");
        ast_free_node(root);
        return NULL;
    }

    return root;
}

/* Parse DELETE query */
ASTNode *parse_delete(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    Token *delete_token = get_current_token(parser);

    if (!delete_token ||
        delete_token->type != KEYWORD ||
        strcasecmp(delete_token->token, "DELETE")) {
        printf("parse_delete: DELETE keyword doesn't exist.");
        return NULL;
    }

    consume_token(parser);

    ASTNode *root = (ASTNode *) calloc(1, sizeof(ASTNode));

    if (!root) {
        return NULL;
    }

    root->type = AST_DELETE;

    root->node_contents.delete.from = parse_from(parser);
    if (!root->node_contents.delete.from) {
        printf("parse_delete: AST FROM node is NULL");
        ast_free_node(root);
        return NULL;
    }

    // A DELETE FROM statement should only have one table reference
    if (root->node_contents.delete.from->num_expressions != 1) {
        printf("parse_delete: DELETE requires exactly one target table.");
        ast_free_node(root);
        return NULL;
    }

    // Parse optional WHERE clause
    Token *current = get_current_token(parser);

    if (current && current->type == KEYWORD && !strcasecmp(current->token, "WHERE")) {
        root->node_contents.delete.where = parse_where(parser);

        if (!root->node_contents.delete.where) {
            printf("parse_delete: Invalid WHERE clause.");
            ast_free_node(root);
            return NULL;
        }
    }

    return root;
}

/* ---------- Parsing of Inner Query Components ---------- */

/* Parse FROM clause */
FromNode *parse_from(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse FROM token
    Token *from_token = get_current_token(parser);

    if (!from_token ||
        from_token->type != KEYWORD ||
        strcasecmp(from_token->token, "FROM")) {
        printf("parse_from: FROM token is NULL.");
        return NULL;
    }

    consume_token(parser);

    FromNode *from = (FromNode *) calloc(1, sizeof(FromNode));

    if (!from) {
        return NULL;
    }

    // Parse one or more table identifier names
    while (true) {
        Token *table_token = get_current_token(parser);

        if (!table_token || table_token->type != IDENTIFIER) {
            printf("parse_from: Expected table identifier.");
            ast_free_from(from);
            return NULL;
        }

        ExpressionNode *table = expression_node_create(EXPR_COLUMN_REF);

        if (!table) {
            ast_free_from(from);
            return NULL;
        }

        strncpy(
            table->expression_data.column_value.column_name,
            table_token->token,
            sizeof(table->expression_data.column_value.column_name) - 1
        );

        consume_token(parser);

        ExpressionNode **new_expressions = (ExpressionNode **) realloc(
            from->expressions,
            (from->num_expressions + 1) * sizeof(ExpressionNode *)
        );

        if (!new_expressions) {
            expression_node_free(table);
            ast_free_from(from);
            return NULL;
        }

        from->expressions = new_expressions;
        from->expressions[from->num_expressions] = table;
        from->num_expressions++;

        Token *current = get_current_token(parser);  
        
        if (!current) {
            ast_free_from(from);
            return NULL;
        }

        // Checking for a ',' that separates table references
        // If there is no ',', leave the current token untouched for the parent parser to handle
        if (strcmp(current->token, ",")) {
            break;
        }

        consume_token(parser);
    }

    return from;
}

/* Parse WHERE clause */
WhereNode *parse_where(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse WHERE token
    Token *where_token = get_current_token(parser);

    if (!where_token ||
        where_token->type != KEYWORD ||
        strcasecmp(where_token->token, "WHERE")) {
        printf("parse_where: WHERE token is NULL.");
        return NULL;
    }

    consume_token(parser);

    WhereNode *where = (WhereNode *) calloc(1, sizeof(WhereNode));
    
    if (!where) {
        return NULL;
    }

    // Parse WHERE's expression
    where->expression = parse_expression(parser);

    if (!where->expression) {
        ast_free_where(where);
        return NULL;
    }

    return where;
}

/* Parse INTO clause in INSERT INTO */
IntoNode *parse_into(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse INTO token
    Token *into_token = get_current_token(parser);

    if (!into_token ||
        into_token->type != KEYWORD ||
        strcasecmp(into_token->token, "INTO")) {
        printf("parse_into: INTO keyword doesn't exist.");
        return NULL;
    }
    
    consume_token(parser);

    // Parse table name after INTO,
    Token *table_token = get_current_token(parser);

    if (!table_token || table_token->type != IDENTIFIER) {
        printf("parse_into: Table identifier name doesn't exist after INTO.");
        return NULL;
    }

    IntoNode *into = (IntoNode *) calloc(1, sizeof(IntoNode));

    if (!into) {
        return NULL;
    }

    // and if it exists, add it to the new AST INTO node
    strncpy(into->table_name, table_token->token, sizeof(into->table_name) - 1);

    consume_token(parser);

    Token *current = get_current_token(parser);

    if (!current) {
        ast_free_into(into);
        return NULL;
    }

    // If there's no opening "(" that lists the column names, 
    // return and parse the VALUES component
    if (strcmp(current->token, "(")) {
        return into;
    }

    consume_token(parser);

    // Parse all Column references
    while (true) {
        Token *column_token = get_current_token(parser);

        if (!column_token || column_token->type != IDENTIFIER) {
            printf("parse_into: Expected column identifier.");
            ast_free_into(into);
            return NULL;
        }

        ExpressionNode *column = expression_node_create(EXPR_COLUMN_REF);

        if (!column) {
            ast_free_into(into);
            return NULL;
        }

        // Copy column name to column reference node
        strncpy(
            column->expression_data.column_value.column_name, 
            column_token->token,
            sizeof(column->expression_data.column_value.column_name) - 1
        );

        consume_token(parser);

        // Append the new expression node to the array of expression node pointers
        ExpressionNode **new_refs = (ExpressionNode **) realloc(
            into->column_refs,
            (into->num_column_refs + 1) * sizeof(ExpressionNode *)
        );

        if (!new_refs) {
            expression_node_free(column);
            ast_free_into(into);
            return NULL;
        }

        into->column_refs = new_refs;
        into->column_refs[into->num_column_refs] = column;
        into->num_column_refs++;

        // Check if we've reached the closing ")" of the column names list
        current = get_current_token(parser);

        if (!current) {
            ast_free_into(into);
            return NULL;
        }

        // If so, exit the loop
        if(!strcmp(current->token, ")")) {
            consume_token(parser);
            break;
        }

        // Otherwise, a comma is required after a column name
        if (strcmp(current->token, ",")) {
            printf("parse_into: expected ',' or ')'.");
            ast_free_into(into);
            return NULL;
        }

        consume_token(parser);
    }

    return into;
}

/* Parse VALUES clause in INSERT INTO */
ValuesNode *parse_values(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse VALUES token
    Token *values_token = get_current_token(parser);

    if (!values_token ||
        values_token->type != KEYWORD ||
        strcasecmp(values_token->token, "VALUES")) {
        printf("parse_values: VALUES keyword doesn't exist.");
        return NULL;
    }
    
    consume_token(parser);

    Token *current = get_current_token(parser);

    // If there's no opening "(" that lists the values expressions, we have invalid syntax 
    if (!current || strcmp(current->token, "(")) {
        printf("parse_values: Expected '('.");
        return NULL;
    }    

    consume_token(parser);

    ValuesNode *values = (ValuesNode *) calloc(1, sizeof(ValuesNode));

    if (!values) {
        return NULL;
    }

    // The 'VALUES ()' syntax is invalid
    current = get_current_token(parser);

    if (!current || !strcmp(current->token, ")")) {
        printf("parse_values: Expected at least one value expression.");
        ast_free_values(values);
        return NULL;
    }

    // Parse all Values expressions
    while (true) {
        ExpressionNode *expression = parse_expression(parser);

        if (!expression) {
            printf("parse_values: Invalid value expression.");
            ast_free_values(values);
            return NULL;
        }

        ExpressionNode **new_values = (ExpressionNode **) realloc(
            values->values,
            (values->num_values + 1) * sizeof(ExpressionNode *)
        );

        if (!new_values) {
            expression_node_free(expression);
            ast_free_values(values);
            return NULL;
        }

        values->values = new_values;
        values->values[values->num_values] = expression;
        values->num_values++;

        current = get_current_token(parser);

        if (!current) {
            printf("parse_values: Unexpected end of VALUES clause.");
            ast_free_values(values);
            return NULL;
        }

        // Check for the end of the VALUES list with a ")"
        if (!strcmp(current->token, ")")) {
            consume_token(parser);
            break;
        }

        // Otherwise, a comma must follow the latest parsed expression
        if(strcmp(current->token, ",")) {
            printf("parse_values: Expected ',' or ')'.");
            ast_free_values(values);
            return NULL;
        }
        
        consume_token(parser);
    }

    return values;
}

/* Parse SET clause in UPDATE */
SetNode *parse_set(Parser *parser) {
    if (!parser || 
        !parser->token_array || 
        !parser->token_array->amount_tokens || 
        parser->current_position >= parser->token_array->amount_tokens || 
        !parser->token_array->tokens) {
       return NULL;
    }

    // Parse SET token
    Token *set_token = get_current_token(parser);

    if (!set_token ||
        set_token->type != KEYWORD ||
        strcasecmp(set_token->token, "SET")) {
        printf("parse_set: SET token is NULL.");
        return NULL;
    }

    consume_token(parser);

    SetNode *set = (SetNode *) calloc(1, sizeof(SetNode));
    
    if (!set) {
        return NULL;
    }

    // Parse one or more update assignments
    while (true) {
        // Extract column that is being assigned
        Token *column_token = get_current_token(parser);

        if (!column_token || column_token->type != IDENTIFIER) {
            printf("parse_set: Expected column identifier.");
            ast_free_set(set);
            return NULL;
        }

        AssignmentNode assignment = {0};

        strncpy(assignment.column_name, column_token->token, sizeof(assignment.column_name) - 1);

        consume_token(parser);

        // Parse mandatory '=' in assignment expression
        Token *equals_token = get_current_token(parser);

        if (!equals_token ||
            equals_token->type != OPERATOR ||
            strcmp(equals_token->token, "=")) {
            printf("parse_set: Expected '=' after column identifier.");
            ast_free_set(set);
            return NULL;
        }

        consume_token(parser);
        
        // Parse right-hand side expression
        assignment.value = parse_expression(parser);

        if (!assignment.value) {
            printf("parse_set: Invalid assignment expression.");
            ast_free_set(set);
            return NULL;
        }

        AssignmentNode *new_assignments = (AssignmentNode *) realloc(
            set->assignments,
            (set->num_assignments + 1) * sizeof(AssignmentNode)
        );

        if (!new_assignments) {
            expression_node_free(assignment.value);
            ast_free_set(set);
            return NULL;
        }

        set->assignments = new_assignments;
        set->assignments[set->num_assignments] = assignment;
        set->num_assignments++;

        Token *current = get_current_token(parser);

        if (!current) {
            ast_free_set(set);
            return NULL;
        }

        // Check for a comma ',' that separates SET assignments
        // If there's no ',', leave the current token untouched
        // so parse_update() can handle WHERE or the terminating ';'
        if (strcmp(current->token, ",")) {
            break;
        }

        consume_token(parser);
    }

    return set;
}