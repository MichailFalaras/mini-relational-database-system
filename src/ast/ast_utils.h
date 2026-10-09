#ifndef AST_UTILS_H
#define AST_UTILS_H

#include <stdint.h>

typedef struct ast_select SelectNode;
typedef struct ast_insert InsertNode;
typedef struct ast_update UpdateNode;
typedef struct ast_delete DeleteNode;
typedef struct ast_create_table CreateTableNode;
typedef struct ast_alter_table AlterTableNode;
typedef struct ast_create_index CreateIndexNode;
typedef struct ast_projection ProjectionNode;
typedef struct ast_from FromNode;
typedef struct ast_where WhereNode;
typedef struct ast_on OnNode;
typedef struct ast_join JoinNode;
typedef struct ast_group_by GroupByNode;
typedef struct ast_having HavingNode;
typedef struct ast_order_by OrderByNode;
typedef struct ast_limit LimitNode;
typedef struct ast_offset OffsetNode;
typedef struct ast_into IntoNode;
typedef struct ast_values ValuesNode;
typedef struct ast_set SetNode;
typedef struct ast_assignment AssignmentNode;
typedef struct ast_columns ColumnsNode;
typedef struct ast_column_def ColumnDefNode;
typedef struct ast_constraints ConstraintsNode;
typedef struct ast_alter_action AlterActionNode;


/* ---------- Deallocation Helpers ---------- */

/* Top-level Deallocation Helpers */

void ast_free_select(SelectNode *select);

void ast_free_insert(InsertNode *insert);

void ast_free_update(UpdateNode *update);

void ast_free_delete(DeleteNode *delete);

void ast_free_create_table(CreateTableNode *create_table);

void ast_free_alter_table(AlterTableNode *alter_table);

void ast_free_create_index(CreateIndexNode *create_index);

/* DROP TABLE, TRUNCATE TABLE, and DROP INDEX contain only fixed-size char[64] fields,
   so their top-level destructors actually have nothing to do */

/* Inner-query Deallocation Helpers */

void ast_free_projection(ProjectionNode *projection);

void ast_free_from(FromNode *from);

void ast_free_where(WhereNode *where);
    
void ast_free_on(OnNode *on);

void ast_free_join(JoinNode *join);

void ast_free_joins(JoinNode **joins, uint32_t num_joins);
    
void ast_free_group_by(GroupByNode *group_by);

void ast_free_having(HavingNode *having);

void ast_free_order_by(OrderByNode *order_by);

void ast_free_limit(LimitNode *limit);

void ast_free_offset(OffsetNode *offset);

void ast_free_into(IntoNode *into);

void ast_free_values(ValuesNode *values);

void ast_free_set(SetNode *set);

void ast_free_assignments(AssignmentNode *assignments, uint32_t num_assignments);

void ast_free_columns(ColumnsNode *columns);

void ast_free_column_def(ColumnDefNode *column_def);

void ast_free_column_constraints(ConstraintsNode *constraints, uint32_t count);

void ast_free_constraints(ConstraintsNode **constraints, uint32_t count);

void ast_free_constraint_contents(ConstraintsNode *constraint);

void ast_free_alter_actions(AlterActionNode *actions, uint32_t num_actions);

/* Internal helper that loops over Expressions and frees them */
void ast_free_expression_array(ExpressionNode **expressions, uint32_t count);

#endif