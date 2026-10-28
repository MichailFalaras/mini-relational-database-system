#ifndef SEMANTIC_BINDER_UTILS_H_
#define SEMANTIC_BINDER_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct database Database;
typedef struct abstract_syntax_tree_node ASTNode;
typedef struct binding_context BindingContext;
typedef struct expression_node ExpressionNode;
typedef struct table Table;

typedef struct ast_projection ProjectionNode;
typedef struct ast_from FromNode;
typedef struct ast_as AsNode;
typedef struct ast_join JoinNode;
typedef struct ast_on OnNode;
typedef struct ast_where WhereNode;
typedef struct ast_group_by GroupByNode;
typedef struct ast_having HavingNode;
typedef struct ast_order_by OrderByNode;
typedef struct ast_limit LimitNode;
typedef struct ast_offset OffsetNode;
typedef struct ast_into IntoNode;
typedef struct ast_values ValuesNode;
typedef struct ast_set SetNode;
typedef struct ast_columns ColumnsNode;
typedef struct ast_column_def ColumnDefNode;
typedef struct ast_constraint ConstraintNode;
typedef struct ast_alter_action AlterActionNode;
typedef struct bound_expression_type BoundExpressionType;
typedef struct bound_projection BoundProjection;
typedef struct bound_insertion_values BoundInsertionValues;


/* ---------- Binding of ExpressionNode ---------- */

bool bind_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a literal expression
bool bind_literal(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a column reference expression
bool bind_column_ref(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a table reference expression
bool bind_table_ref(ExpressionNode *expr, BindingContext *context, Table **out_table);

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

bool bind_select(ASTNode *root, BindingContext *context, BoundProjection *out_projection);

bool bind_insert(ASTNode *root, BindingContext *context, BoundInsertionValues *out_values);

bool bind_update(ASTNode *root, BindingContext *context, BoundUpdateValues *out_values);

bool bind_delete(ASTNode *root, BindingContext *context, BoundDeleteValues *out_values);

bool bind_create_table(ASTNode *root, BindingContext *context);

bool bind_drop_table(ASTNode *root, BindingContext *context);

bool bind_truncate_table(ASTNode *root, BindingContext *context);

bool bind_alter_table(ASTNode *root, BindingContext *context);

bool bind_create_index(ASTNode *root, BindingContext *context);

bool bind_drop_index(ASTNode *root, BindingContext *context);

/* ---------- Parsing of Inner Query Components ---------- */

bool bind_joins(JoinNode **joins, uint32_t num_joins, BindingContext *context);

bool bind_on(OnNode *on, BindingContext *context);

bool bind_projection(ProjectionNode *projection, BindingContext *context, BoundProjection *out_projection);

bool bind_from(FromNode *from, BindingContext *context);

bool bind_where(WhereNode *where, BindingContext *context);

bool bind_group_by(GroupByNode *group_by, BindingContext *context);

bool bind_having(HavingNode *having, BindingContext *context);

bool bind_order_by(OrderByNode *order_by, BindingContext *context);

bool bind_into(IntoNode *into, BindingContext *context);

bool bind_values(ValuesNode *values, BindingContext *context, BoundExpressionType *out_types);

bool bind_set(SetNode *set, BindingContext *context, BoundExpressionType *out_types);

bool bind_columns(ColumnsNode *columns, BindingContext *context);

bool bind_column_def(ColumnDefNode *column_def, BindingContext *context);

bool bind_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_primary_key_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_unique_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_not_null_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_foreign_key_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_check_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_default_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_alter_action(AlterActionNode *action, BindingContext *context);

bool bind_alter_add_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_drop_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_rename_table(AlterActionNode *action, BindingContext *context);

bool bind_alter_rename_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_modify_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_add_constraint(AlterActionNode *action, BindingContext *context);

bool bind_alter_drop_constraint(AlterActionNode *action, BindingContext *context);

/* ---------- Cross-Clause Validation Helpers ---------- */

// Register a table in the binding context, used by FROM and JOIN binding
bool register_binding_relation(const char *table_name, const char *alias, BindingContext *context);

// Validate Projection expressions against GROUP BY expressions
bool validate_grouped_projection(ProjectionNode *projection, GroupByNode *group_by, 
    BindingContext *context);

// Validate that an expression is legal in a grouped query
bool validate_grouped_expression(ExpressionNode *expr, GroupByNode *group_by);

// Determine whether 2 expressions represent the same grouping expression
bool grouped_expressions_equal(const ExpressionNode *left, const ExpressionNode *right);

// Validate if a expression contains a nested aggregate expression
bool expression_contains_aggregate(const ExpressionNode *expr);

// Validate ORDER BY clause in a grouped query
bool validate_grouped_order_by(OrderByNode *order_by, GroupByNode *group_by);

// Check assignment compatibility between columns and corresponding inserted values
bool validate_insert_assignments(IntoNode *into, ValuesNode *values,
    const BoundExpressionType *value_types, BindingContext *context);

// Build insertion values tuple
bool build_bound_insertion_values(IntoNode *into, ValuesNode *values,
    const BoundExpressionType *value_types, BindingContext *context, BoundInsertionValues *out_values);

// Check assignment compatibility between columns and corresponding update values
bool validate_update_assignments(SetNode *set, const BoundExpressionType *value_types, 
    BindingContext *context);

// Build updated values tuple
bool build_bound_update_values(SetNode *set, const BoundExpressionType *value_types, 
    BindingContext *context, BoundUpdateValues *out_values);

// Build delete output
bool build_bound_delete_values(WhereNode *where, BindingContext *context, 
    BoundDeleteValues *out_values);

#endif