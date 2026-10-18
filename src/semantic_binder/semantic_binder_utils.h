#ifndef SEMANTIC_BINDER_UTILS_H_
#define SEMANTIC_BINDER_UTILS_H_

#include <stdbool.h>

typedef struct database Database;
typedef struct abstract_syntax_tree_node ASTNode;
typedef struct binding_context BindingContext;
typedef struct expression_node ExpressionNode;
typedef enum data_types DataType;

typedef struct bound_expression_type {
    DataType type;
    bool is_untyped_null;
} BoundExpressionType;

/* ---------- Binding of ExpressionNode ---------- */

bool bind_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a literal expression
bool bind_literal(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a column reference expression
bool bind_column_ref(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a table reference expression
bool bind_table_ref(ExpressionNode *expr, BindingContext *context);

// Helper that rejects table references inside scalar expressions
bool bind_scalar_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a unary expression
bool bind_unary(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a binary expression
bool bind_binary(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind an IS NULL expression
bool bind_is_null(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind an IS NOT NULL expression
bool bind_is_not_null(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind an IN expression
bool bind_in(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a BETWEEN expression
bool bind_between(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a function expression
bool bind_function(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

/* ---------- Binding of each ASTNodeType ---------- */

bool bind_select(ASTNode *root, BindingContext *context);

bool bind_insert(ASTNode *root, BindingContext *context);

bool bind_update(ASTNode *root, BindingContext *context);

bool bind_delete(ASTNode *root, BindingContext *context);

bool bind_create_table(ASTNode *root, BindingContext *context);

bool bind_drop_table(ASTNode *root, BindingContext *context);

bool bind_alter_table(ASTNode *root, BindingContext *context);

bool bind_truncate_table(ASTNode *root, BindingContext *context);

bool bind_create_index(ASTNode *root, BindingContext *context);

bool bind_drop_index(ASTNode *root, BindingContext *context);

#endif