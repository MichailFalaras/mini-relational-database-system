#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "parser_utils.h"
#include "../../include/parser.h"
#include "../../include/tokenizer.h"
#include "../../include/expressions.h"

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

void consume_token(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens) {
        return;
    }

    if (parser->current_position+1 < parser->token_array->amount_tokens) {
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

        ExpressionNode *new_left = create_binary_expression(left_operand, OP_OR, right_operand);
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

        ExpressionNode *new_left = create_binary_expression(left_operand, OP_AND, right_operand);
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

    ExpressionNode *comparison_expr = create_binary_expression(left_operand, op, right_operand);
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

        ExpressionNode *new_left = create_binary_expression(left_operand, op, right_operand);
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

        ExpressionNode *new_left = create_binary_expression(left_operand, op, right_operand);
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

        consume_token(parser);
        return parentheses_expr;
    }

    if (curr_token->type == IDENTIFIER) {
        ExpressionNode *identifier_expr = expression_node_create(EXPR_COLUMN_REF);
        if (!identifier_expr) {
            return NULL;
        }

        strncpy(identifier_expr->expression_data.column_value.column_name,
                curr_token->token, 64);
        identifier_expr->expression_data.column_value.column_name[63] = '\0';
        consume_token(parser);
        return identifier_expr;
    }

    ExpressionNode *literal_expr = expression_node_create(EXPR_LITERAL);
    if (!literal_expr) {
        return NULL;
    }

    Value *literal = NULL;
    if (curr_token->type == NUMBER) {
        literal = create_number_literal(parser);
    } else if (curr_token->type == STRING) {
        literal = create_string_literal(parser);
    } else if (curr_token->type == KEYWORD &&
               (!strcasecmp(curr_token->token, "TRUE") ||
                !strcasecmp(curr_token->token, "FALSE"))) {
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
    consume_token(parser);
    return literal_expr;
}

/* Identify INTEGER or NUMERIC literal. 
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

    return value_create(INTEGER, &number);
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
    char trailing;

    if (sscanf(token_string, "'%4d-%2d-%2d %2d:%2d:%2d'%c",
               &year, &month, &day, &hour, &minute, &second, &trailing) == 6) {
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

    if (sscanf(token_string, "'%4d-%2d-%2d'%c", &year, &month, &day, &trailing) == 3) {
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
    char *content = (char *) malloc(content_len + 1);
    if (!content) {
        return NULL;
    }

    memcpy(content, token_string + 1, content_len);
    content[content_len] = '\0';

    char_n_t char_n = {0};
    char_n.n = (uint32_t) content_len;
    char_n.string = content;

    Value *char_value = value_create(CHAR, &char_n);
    free(content);
    return char_value;
}