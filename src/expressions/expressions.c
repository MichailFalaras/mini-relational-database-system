#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/expressions.h"

ExpressionNode *expression_node_create(ExpressionType type) {

    ExpressionNode *expr_node = (ExpressionNode*) malloc(sizeof(ExpressionNode));
    if (expr_node == NULL) {
        perror("ExpressionNode");
        exit(1);
    }
    expr_node->type = type;

    return expr_node;
}

OperatorType get_operator_type(char *operator_token) {
    OperatorType type;

    if (!strcmp(operator_token, "=")) {
        type = OP_EQ;
    } else if (!strcmp(operator_token, "!=")) {
        type = OP_NEQ;
    } else if (!strcmp(operator_token, "<")) {
        type = OP_LT;
    } else if (!strcmp(operator_token, "<=")) {
        type = OP_LTE;
    } else if (!strcmp(operator_token, ">")) {
        type = OP_GT;
    } else if (!strcmp(operator_token, ">=")) {
        type = OP_GTE;
    } else if (!strcmp(operator_token, "AND")) {
        type = OP_AND;
    } else if (!strcmp(operator_token, "OR")) {
        type = OP_OR;
    } else if (!strcmp(operator_token, "NOT")) {
        type = OP_NOT;
    } else if (!strcmp(operator_token, "+")) {
        type = OP_ADD;
    } else if (!strcmp(operator_token, "-")) {
        type = OP_SUB;
    } else if (!strcmp(operator_token, "*")) {
        type = OP_MUL;
    } else if (!strcmp(operator_token, "/")) {
        type = OP_DIV;
    } // don't know why modulo isnt supported

    return type;
}

/* Deep-Copy ExpressionNode for each ExpressionType. */
ExpressionNode *expression_node_copy(const ExpressionNode *source) {

    if (source == NULL) {
        return NULL;
    }

    ExpressionNode *copy = expression_node_create(source->type);

    switch (source->type) {
        case EXPR_LITERAL:
            copy->expression_data.literal_value.literal = value_copy(source->expression_data.literal_value.literal);
            break;
        case EXPR_COLUMN_REF:
            copy->expression_data.column_value = source->expression_data.column_value;
            break;
        case EXPR_TABLE_REF:
            copy->expression_data.table_value = source->expression_data.table_value;
            break;
        case EXPR_UNARY:
            copy->expression_data.unary_expr.op = source->expression_data.unary_expr.op;
            copy->expression_data.unary_expr.operand = expression_node_copy(source->expression_data.unary_expr.operand);
            break;
        case EXPR_BINARY:
            copy->expression_data.binary_expr.op = source->expression_data.binary_expr.op;
            copy->expression_data.binary_expr.left_operand = expression_node_copy(source->expression_data.binary_expr.left_operand);
            copy->expression_data.binary_expr.right_operand = expression_node_copy(source->expression_data.binary_expr.right_operand);
            break;
        case EXPR_IS_NULL:
            copy->expression_data.is_null_expr.operand = expression_node_copy(source->expression_data.is_null_expr.operand);
            break;
        case EXPR_IS_NOT_NULL:
            copy->expression_data.is_not_null_expr.operand = expression_node_copy(source->expression_data.is_not_null_expr.operand);
            break;
        case EXPR_IN:
            copy->expression_data.in_expr.option_count = source->expression_data.in_expr.option_count;
            copy->expression_data.in_expr.operand = expression_node_copy(source->expression_data.in_expr.operand);
            copy->expression_data.in_expr.set_options = (ExpressionNode **) malloc(source->expression_data.in_expr.option_count*sizeof(ExpressionNode *));
            if (copy->expression_data.in_expr.set_options == NULL) {
                perror("expression_node_copy");
                exit(1);
            }

            for (uint32_t i = 0; i < source->expression_data.in_expr.option_count; i++) {
                copy->expression_data.in_expr.set_options[i] = expression_node_copy(source->expression_data.in_expr.set_options[i]);
            }

            break;
        case EXPR_BETWEEN:
            copy->expression_data.between_expr.operand = expression_node_copy(source->expression_data.between_expr.operand);
            copy->expression_data.between_expr.lower = expression_node_copy(source->expression_data.between_expr.lower);
            copy->expression_data.between_expr.upper = expression_node_copy(source->expression_data.between_expr.upper);
            break;
        case EXPR_FUNCTIONS:
            copy->expression_data.aggregate_func_expr.type = source->expression_data.aggregate_func_expr.type;
            copy->expression_data.aggregate_func_expr.expression = expression_node_copy(source->expression_data.aggregate_func_expr.expression);
            break;
        default:
            printf("Source type doesn't match with ExpressionNode types\n");
            return NULL;
    }

    return copy;
}

void expression_node_free(ExpressionNode *expr) {
    if (expr != NULL) {
        switch (expr->type) {
        case EXPR_LITERAL:
            value_free(expr->expression_data.literal_value.literal);
            break;
        case EXPR_COLUMN_REF:
        case EXPR_TABLE_REF:
            break;
        case EXPR_UNARY:
            expression_node_free(expr->expression_data.unary_expr.operand);
            break;
        case EXPR_BINARY:
            expression_node_free(expr->expression_data.binary_expr.left_operand);
            expression_node_free(expr->expression_data.binary_expr.right_operand);
            break;
        case EXPR_IS_NULL:
            expression_node_free(expr->expression_data.is_null_expr.operand);
            break;
        case EXPR_IS_NOT_NULL:
            expression_node_free(expr->expression_data.is_not_null_expr.operand);
            break;
        case EXPR_IN:
            expression_node_free(expr->expression_data.in_expr.operand);
            for (uint32_t i = 0; i < expr->expression_data.in_expr.option_count; i++) {
                expression_node_free(expr->expression_data.in_expr.set_options[i]);
            }
            free(expr->expression_data.in_expr.set_options);
            break;
        case EXPR_BETWEEN:
            expression_node_free(expr->expression_data.between_expr.operand);
            expression_node_free(expr->expression_data.between_expr.lower);
            expression_node_free(expr->expression_data.between_expr.upper);
            break;
        case EXPR_FUNCTIONS:
            expression_node_free(expr->expression_data.aggregate_func_expr.expression);
            break;
        default:
            printf("Source type doesn't match with ExpressionNode types\n");
            return;
        }
    }
}