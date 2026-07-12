#include <stdio.h>
#include <stdlib.h>
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