#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/execution_engine.h"
#include "../../include/expressions.h"
#include "../../include/database.h"
#include "../../include/transaction.h"
#include "../../include/data_types.h"
#include "../../include/table.h"
#include "../../include/row.h"


/* Create ExpressionNode. */
ExpressionNode *expression_node_create(ExpressionType type) {

    ExpressionNode *expr_node = (ExpressionNode*) malloc(sizeof(ExpressionNode));
    if (expr_node == NULL) {
        perror("ExpressionNode");
        exit(1);
    }
    expr_node->type = type;

    /* Fill with unresolved values.
     * Semantic Binder is going to initialize these values. */
    if (expr_node->type == EXPR_COLUMN_REF) {
        expr_node->expression_data.column_value.column_index = -1;
        expr_node->expression_data.column_value.relation_index = -1;
    }

    return expr_node;
}

/* Get operator type from operator token. */
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

/* ExecutionContext gets allocated by ExecutionEngine.
 * RelationContext also gets allocated ONCE by ExecutionEngine. */

/* -- First version of evaluate_expression, subject to change. */
Value *evaluate_expression(const ExpressionNode *expr, const EvaluationContext *context) {

    switch (expr->type) {
        case EXPR_LITERAL: 
            Value *final = value_copy(expr->expression_data.literal_value.literal);
            if (final == NULL) {
                return NULL;
            }

            return final;
        case EXPR_COLUMN_REF: {
            int32_t rel_index = expr->expression_data.column_value.relation_index;
            if (rel_index == -1) {
                return NULL;
            }
            int32_t column_index = expr->expression_data.column_value.column_index;
            if (column_index == -1) {
                return NULL;
            }

            Value *final = value_copy(context->relation_context[rel_index].row->values[column_index]);
            if (final == NULL) {
                return NULL;
            }

            return final;
        }
        case EXPR_UNARY: {
            Value *val = evaluate_expression((const ExpressionNode *) expr->expression_data.unary_expr.operand, context);

            if (val == NULL) {
                return NULL;
            }

            Value *final = execute_operation(val,  NULL, OP_NOT);
            value_free(val);
            if (final == NULL) {
                return NULL;
            }
            return final;
        }
        case EXPR_BINARY: {
            Value *left_val = evaluate_expression((const ExpressionNode *) expr->expression_data.binary_expr.left_operand, context);
            Value *right_val = evaluate_expression((const ExpressionNode *) expr->expression_data.binary_expr.right_operand, context);

            if (left_val == NULL || right_val == NULL) {

                if (left_val != NULL) {
                    value_free(left_val);
                }
                if (right_val != NULL) {
                    value_free(right_val);
                }
                return NULL;
            }

            Value *final = execute_operation(left_val, right_val, expr->expression_data.binary_expr.op);
            value_free(left_val);
            value_free(right_val);
            if (final == NULL) {
                return NULL;
            }
            return final;
        }  
        case EXPR_IS_NULL: {
            Value *val = evaluate_expression((const ExpressionNode *) expr->expression_data.is_null_expr.operand, context);

            if (val == NULL) {
                return NULL;
            }

            bool res = false;
            if (val->type == NULL_TYPE && val->value.null_val == true) {
                res = true;
            }

            value_free(val);
            Value *final = value_create(BOOL, &res);
            if (final == NULL) {
                return NULL;
            }
            return final;
        }  
        case EXPR_IS_NOT_NULL: {
            Value *val = evaluate_expression((const ExpressionNode *) expr->expression_data.is_not_null_expr.operand, context);

            if (val == NULL) {
                return NULL;
            }

            bool res = false;
            if (val->type != NULL_TYPE && val->value.null_val == false) {
                res = true;
            }

            value_free(val);
            Value *final = value_create(BOOL, &res);
            if (final == NULL) {
                return NULL;
            }
            return final;
        }
        case EXPR_IN: {
            Value *val = evaluate_expression(expr->expression_data.in_expr.operand, context);

            if (val == NULL) {
                return NULL;
            }

            bool result = false;
            int compare_result = -2;
            for (uint32_t i = 0; i < expr->expression_data.in_expr.option_count; i++) {
                Value *option = evaluate_expression(expr->expression_data.in_expr.set_options[i], context);
                value_compare(val, option, &compare_result);
                if (compare_result == 0) {
                    result = true;
                    value_free(option);
                    break;
                }
                value_free(option);
            }

            value_free(val);
            Value *final = value_create(BOOL, &result);
            if (final == NULL) {
                return NULL;
            }
            return final;
        }    
        case EXPR_BETWEEN: {
            Value *val = evaluate_expression(expr->expression_data.between_expr.operand, context);
            Value *lower_limit = evaluate_expression(expr->expression_data.between_expr.lower, context);
            Value *upper_limit = evaluate_expression(expr->expression_data.between_expr.upper, context);

            if (val == NULL || lower_limit == NULL || upper_limit == NULL) {
                return NULL;
            }

            Value *result = execute_operation(val, lower_limit, OP_GTE);

            if (result == NULL || result->value.bool_val == false) {
                value_free(result);
                value_free(val);
                value_free(lower_limit);
                value_free(upper_limit);
                return NULL;
            }
            value_free(result);
            result = execute_operation(val, upper_limit, OP_LTE);
            if (result == NULL || result->value.bool_val == false) {
                value_free(result);
                value_free(val);
                value_free(lower_limit);
                value_free(upper_limit);
                return NULL;
            }

            bool temp = result->value.bool_val;
            value_free(result);
            value_free(val);
            value_free(lower_limit);
            value_free(upper_limit);
            Value *final = value_create(BOOL, &temp);
            if (final == NULL) {
                return NULL;
            }
            return final;
        }
        case EXPR_FUNCTIONS: /* Accumulator calls evaluate_expression */
        default: 
            printf("expr type doesn't match with ExpressionNodeTypes\n");
            break;
    }

    return NULL;
}

/* ExpressionNode Free. */
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

        free(expr);
    }
}