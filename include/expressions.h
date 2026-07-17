#ifndef EXPRESSIONS_H_
#define EXPRESSIONS_H_

#include <stdint.h>
#include "data_types.h"
#include "database.h"
#include "execution_engine.h"
#include "transaction.h"

typedef struct expression_node ExpressionNode;

/* Enum of potential expression entities */
typedef enum expression_type {
    EXPR_LITERAL,
    EXPR_COLUMN_REF,
    EXPR_TABLE_REF,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_IS_NULL,
    EXPR_IS_NOT_NULL,
    EXPR_IN,
    EXPR_BETWEEN,
    EXPR_FUNCTIONS
} ExpressionType;

/* Enum of all operator types 
 * Arithmetic operators: +, -, *, /
 * Comparison operators: =, <>, <, <=, >, >= 
 * Logical operators: AND, OR, NOT */
typedef enum operator_type {
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_LTE,
    OP_GT,
    OP_GTE,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV
} OperatorType;

/* Literal value in expression, e.g., integer, float, string */
typedef struct literal {
    Value *literal;
} Literal;

/* Column name reference in the expression */
typedef struct column_ref {
    char column_name[64];

    int32_t relation_index;
    int32_t column_index;
} ColumnRef;

typedef struct table_ref {
    char table_name[64];
} TableRef;

/* Unary expression, including operator + operand */
typedef struct unary {
    ExpressionNode *operand;
    OperatorType op;
} Unary;

/* Binary expression, including left operand + operator + right operand */
typedef struct binary {
    ExpressionNode *left_operand;
    ExpressionNode *right_operand;
    OperatorType op;
} Binary;

/* <column> IS NULL expression */
typedef struct is_null {
    ExpressionNode *operand;
} IsNull;

/* <column> IS NOT NULL expression */
typedef struct is_not_null {
    ExpressionNode *operand;
} IsNotNull;

/* <column> IN <list_of_values> expression */
typedef struct in {
    ExpressionNode *operand;
    ExpressionNode **set_options;
    uint32_t option_count;
} In;

/* <column> BETWEEN <lower> AND <upper> expression */
typedef struct between {
    ExpressionNode *operand;
    ExpressionNode *lower;
    ExpressionNode *upper;
} Between;

typedef enum aggregate_function_types {
    SUM,
    COUNT,
    AVG,
    MIN,
    MAX
} AggregateFunctionTypes;

typedef struct aggregate_function {
    AggregateFunctionTypes type;
    ExpressionNode *expression;
} AggregateFunction;

/* Generic expression struct that can be any of the above expression entities */
typedef struct expression_node {
    ExpressionType type;
    union {
        Literal literal_value;
        ColumnRef column_value;
        TableRef table_value;
        Unary unary_expr;
        Binary binary_expr;
        IsNull is_null_expr;
        IsNotNull is_not_null_expr;
        In in_expr;
        Between between_expr;
        AggregateFunction aggregate_func_expr;
    } expression_data;
} ExpressionNode;

/* Subject to change. */
typedef struct relation_context {
    Table *table;
    Row *row;
} RelationContext;

typedef struct evaluation_context {
    Database *db;
    RelationContext *relation_context;
    uint32_t relations_count;
    Transaction *transaction;
} EvaluationContext;

extern ExpressionNode *expression_node_create(ExpressionType type);

extern OperatorType get_operator_type(char *operator_token);

extern ExpressionNode *expression_node_copy(const ExpressionNode *source);

Value *evaluate_expression(const ExpressionNode *expr, const EvaluationContext *context);

extern void expression_node_free(ExpressionNode *expr);

#endif