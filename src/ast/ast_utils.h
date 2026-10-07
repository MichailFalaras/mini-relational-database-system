#ifndef AST_UTILS_H
#define AST_UTILS_H

#include <stdint.h>

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

void ast_free_constraint(ConstraintNode *constraint);

void ast_free_constraints(ConstraintNode **constraints, uint32_t count);

void ast_free_constraint_contents(ConstraintNode *constraint);

void ast_free_alter_action_contents(AlterActionNode *action);

void ast_free_alter_actions(AlterActionNode *actions, uint32_t num_actions);

/* Internal helper that loops over Expressions and frees them */
void ast_free_expression_array(ExpressionNode **expressions, uint32_t count);

#endif