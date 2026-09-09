#ifndef EXPRESSIONS_H_
#define EXPRESSIONS_H_

#include <stdint.h>

typedef struct database Database;
typedef struct transaction Transaction;
typedef struct value Value;
typedef struct table Table;
typedef struct row Row;
typedef struct expression_node ExpressionNode;

/* Enum of potential expression entities */
typedef enum expression_type {
    EXPR_LITERAL = 0,
    EXPR_COLUMN_REF = 1,
    EXPR_TABLE_REF = 2,
    EXPR_UNARY = 3,
    EXPR_BINARY = 4,
    EXPR_IS_NULL = 5,
    EXPR_IS_NOT_NULL = 6,
    EXPR_IN = 7,
    EXPR_BETWEEN = 8,
    EXPR_FUNCTIONS = 9
} ExpressionType;

/* Enum of all operator types 
 * Arithmetic operators: +, -, *, /
 * Comparison operators: =, <>, <, <=, >, >= 
 * Logical operators: AND, OR, NOT */
typedef enum operator_type {
    OP_EQ = 0,
    OP_NEQ = 1,
    OP_LT = 2,
    OP_LTE = 3,
    OP_GT = 4,
    OP_GTE = 5,
    OP_AND = 6,
    OP_OR = 7,
    OP_NOT = 8,
    OP_ADD = 9,
    OP_SUB = 10,
    OP_MUL = 11,
    OP_DIV = 12,
    OP_ERROR = 13
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
    SUM = 0,
    COUNT = 1,
    AVG = 2,
    MIN = 3,
    MAX = 4
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