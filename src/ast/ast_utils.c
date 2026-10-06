#include <stdlib.h>
#include "../../include/ast.h"
#include "ast_utils.h"
#include "../../include/expressions.h"


/* Top-level Deallocation Helpers */

void ast_free_select(SelectNode *select) {
    if (!select) {
        return;
    }
    
    ast_free_projection(select->projection);
    ast_free_from(select->from);
    ast_free_where(select->where);
    ast_free_joins(select->joins, select->num_joins);
    ast_free_group_by(select->group_by);
    ast_free_having(select->having);
    ast_free_order_by(select->order_by);
    ast_free_limit(select->limit);
    ast_free_offset(select->offset);
}   

void ast_free_insert(InsertNode *insert) {
    if (!insert) {
        return;
    }

    ast_free_into(insert->into);
    ast_free_values(insert->values);
}

void ast_free_update(UpdateNode *update) {
    if (!update) {
        return;
    }

    ast_free_set(update->set);
    ast_free_where(update->where);
}

void ast_free_delete(DeleteNode *delete) {
    if (!delete) {
        return;
    }

    ast_free_from(delete->from);
    ast_free_where(delete->where);
}

void ast_free_create_table(CreateTableNode *create_table) {
    if (!create_table) {
        return;
    }

    ast_free_columns(create_table->columns);
    ast_free_constraints(create_table->constraints, create_table->num_constraints);
}

void ast_free_alter_table(AlterTableNode *alter_table) {
    if (!alter_table) {
        return;
    }

    ast_free_alter_actions(alter_table->actions, alter_table->num_actions);
}

void ast_free_create_index(CreateIndexNode *create_index) {
    if (!create_index) {
        return;
    }

    ast_free_expression_array(create_index->column_refs, create_index->num_column_refs);
}


/* Inner-query Deallocation Helpers */

void ast_free_projection(ProjectionNode *projection) {
    if (!projection) {
        return;
    }

    ast_free_expression_array(projection->expressions, projection->num_expressions);
    free(projection);
}

void ast_free_from(FromNode *from) {
    if (!from) {
        return;
    }

    ast_free_expression_array(from->expressions, from->num_expressions);
    free(from);
}

void ast_free_where(WhereNode *where) {
    if (!where) {
        return;
    }

    expression_node_free(where->expression);
    free(where);
}

void ast_free_on(OnNode *on) {
    if (!on) {
        return;
    }

    expression_node_free(on->expression);
    free(on);
}

void ast_free_join(JoinNode *join) {
    if (!join) {
        return;
    }

    ast_free_on(join->on);
    free(join);
}

void ast_free_joins(JoinNode **joins, uint32_t num_joins) {
    if (!joins) {
        return;
    }

    for (uint32_t i = 0; i < num_joins; i++) {
        ast_free_join(joins[i]);
    }

    free(joins);
}
    
void ast_free_group_by(GroupByNode *group_by) {
    if (!group_by) {
        return;
    }

    ast_free_expression_array(group_by->column_refs, group_by->num_column_refs);
    free(group_by);
}

void ast_free_having(HavingNode *having) {
    if (!having) {
        return;
    }

    expression_node_free(having->expression);
    free(having);
}

void ast_free_order_by(OrderByNode *order_by) {
    if (!order_by) {
        return;
    }

    ast_free_expression_array(order_by->column_refs, order_by->num_column_refs);
    free(order_by->type);
    free(order_by);
}

void ast_free_limit(LimitNode *limit) {
    free(limit);
}

void ast_free_offset(OffsetNode *offset) {
    free(offset);
}

void ast_free_into(IntoNode *into) {
    if (!into) {
        return;
    }
    
    ast_free_expression_array(into->column_refs, into->num_column_refs);
    free(into);
}

void ast_free_values(ValuesNode *values) {
    if (!values) {
        return;
    }

    ast_free_expression_array(values->values, values->num_values);
    free(values);
}

void ast_free_set(SetNode *set) {
    if (!set) {
        return;
    }

    ast_free_assignments(set->assignments, set->num_assignments);

    free(set);
}

void ast_free_assignments(AssignmentNode *assignments, uint32_t num_assignments) {
    if (!assignments) {
        return;
    }

    for (uint32_t i = 0; i < num_assignments; i++) {
        expression_node_free(assignments[i].value);
    }

    free(assignments);
}

void ast_free_columns(ColumnsNode *columns) {
    if (!columns) {
        return;
    }

    if (columns->column_defs) {
        for (uint32_t i = 0; i < columns->num_column_defs; i++) {
            ast_free_column_def(columns->column_defs[i]);
        }

        free(columns->column_defs);
    }

    free(columns);
}

void ast_free_column_def(ColumnDefNode *column_def) {
    if (!column_def) {
        return;
    }

    ast_free_constraints(column_def->constraints, column_def->num_constraints);

    free(column_def);
}

void ast_free_constraint(ConstraintNode *constraint) {
    if (!constraint) {
        return;
    }

    ast_free_constraint_contents(constraint);
    free(constraint);
}

void ast_free_constraints(ConstraintNode **constraints, uint32_t count) {
    if (!constraints) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (constraints[i]) {
            ast_free_constraint(constraints[i]);
        }
    }

    free(constraints);
}

void ast_free_constraint_contents(ConstraintNode *constraint) {
    if (!constraint) {
        return;
    }

    switch (constraint->type) {
        case AST_CONSTRAINT_PRIMARY_KEY:
            ast_free_expression_array(
                constraint->constraint_data.primary_key.column_refs,
                constraint->constraint_data.primary_key.num_columns
            );
            break;

        case AST_CONSTRAINT_UNIQUE:
            ast_free_expression_array(
                constraint->constraint_data.unique.column_refs,
                constraint->constraint_data.unique.num_columns
            );
            break;

        case AST_CONSTRAINT_NOT_NULL:
            expression_node_free(constraint->constraint_data.not_null.column_ref);
            break;

        case AST_CONSTRAINT_FOREIGN_KEY:
            ast_free_expression_array(
                constraint->constraint_data.foreign_key.local_column_refs,
                constraint->constraint_data.foreign_key.num_local_columns
            );

            ast_free_expression_array(
                constraint->constraint_data.foreign_key.referenced_column_refs,
                constraint->constraint_data.foreign_key.num_referenced_columns
            );
            break;

        case AST_CONSTRAINT_CHECK:
            expression_node_free(constraint->constraint_data.check.check_expr);
            break;

        case AST_CONSTRAINT_DEFAULT:
            expression_node_free(constraint->constraint_data.default_value.default_expr);
            break;
    }
}

void ast_free_alter_actions(AlterActionNode *actions, uint32_t num_actions) {
    if (!actions) {
        return;
    }

    for (uint32_t i = 0; i < num_actions; i++) {
        switch (actions[i].type) {
            case AST_ALTER_ADD:
                ast_free_column_def(actions[i].alter_contents.alter_add.column);
                break;

            case AST_ALTER_DROP:
            case AST_ALTER_RENAME:
            case AST_ALTER_MODIFY:
            case AST_ALTER_ADD_CONSTRAINT:
            case AST_ALTER_DROP_CONSTRAINT:
                break;
        }
    }

    free(actions);
}

/* Internal helper that loops over Expressions and frees them */
void ast_free_expression_array(ExpressionNode **expressions, uint32_t count) {
    if (!expressions) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        expression_node_free(expressions[i]);
    }

    free(expressions);
}