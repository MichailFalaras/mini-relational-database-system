#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../../include/query_optimizer.h"
#include "../../include/schema.h"
#include "../../include/constraints.h"
#include "../../include/index.h"
#include "../../include/expressions.h"

/* Create PlanNode struct and initialize common values. */
PlanNode *plan_node_create(PlanNodeTypes type) {
    if (type > PLAN_OFFSET) {
        return NULL;
    }

    PlanNode *plan_node = (PlanNode *) calloc(1, sizeof(PlanNode));
    if (!plan_node) {
        return NULL;
    }

    plan_node->type = type;
    plan_node->estimated_cost = 0;
    plan_node->next = NULL;

    return plan_node;
}

/* Append PlanNode to QueryPlan's PlanNode list.
 *
 * (NOTE: if PlanNode getting appended is already connected to other
 *  PlanNodes, it's currently denied. Each PlanNode should only be appended
 *  and connected to QueryPlan's list) */
bool query_plan_append_node(QueryPlan *query_plan, PlanNode *new_node) {
    if (!query_plan || !new_node
        || new_node->type > PLAN_OFFSET
        || new_node->next != NULL) {
        return false;
    }

    /* Empty list. */
    PlanNode *curr = query_plan->head;
    if (!curr) {
        query_plan->head = new_node;
        query_plan->amount_plan_nodes++;
        return true;
    }

    PlanNode *fast = curr;
    while (curr->next != NULL) {
        curr = curr->next;

        /* To prevent against list becoming cyclic
         * and running in an infinite loop. */
        if (fast != NULL && fast->next != NULL) {
            fast = fast->next->next;
        }

        if (fast != NULL && fast == curr) {
            return false;
        }
    }

    /* Append and update QueryPlan metadata. */
    curr->next = new_node;
    query_plan->amount_plan_nodes++;

    return true;
}

/* A PlanNode only stores the references to objects.
 * Should not actually free those objects, just nullify the pointer
 * referencing them. */
void plan_node_free(PlanNode *plan_node) {
    if (!plan_node) {
        return;
    }

    switch (plan_node->type) {
        case PLAN_CREATE_TABLE:
            plan_node->operation.plan_create_table.schema = NULL;
            break;

        case PLAN_DROP_TABLE:
            plan_node->operation.plan_drop_table.table = NULL;
            break;
            
        case PLAN_ALTER_RENAME_TABLE:
            plan_node->operation.plan_alter_rename_table.table = NULL;
            break;

        case PLAN_TRUNCATE_TABLE:
            plan_node->operation.plan_truncate_table.table = NULL;
            break;

        case PLAN_ALTER_ADD_COLUMN:
            plan_node->operation.plan_alter_add_column.table = NULL;
            plan_node->operation.plan_alter_add_column.new_column_definition = NULL;

            for (uint32_t i = 0; i < plan_node->operation.plan_alter_add_column.num_constraints; i++) {
                plan_node->operation.plan_alter_add_column.new_constraints[i] = NULL;
            }
            free(plan_node->operation.plan_alter_add_column.new_constraints);
            break;

        case PLAN_ALTER_DROP_COLUMN:
            plan_node->operation.plan_alter_drop_column.column.table = NULL;
            break;

        case PLAN_ALTER_RENAME_COLUMN:
            plan_node->operation.plan_alter_rename_column.column.table = NULL;
            break;

        case PLAN_ALTER_MODIFY_COLUMN:
            plan_node->operation.plan_alter_modify_column.column.table = NULL;
            plan_node->operation.plan_alter_modify_column.new_column_definition = NULL;
            break;

        case PLAN_ALTER_ADD_CONSTRAINT:
            plan_node->operation.plan_alter_add_constraint.table = NULL;
            plan_node->operation.plan_alter_add_constraint.constraint = NULL;
            break;

        case PLAN_ALTER_DROP_CONSTRAINT:
            plan_node->operation.plan_alter_drop_constraint.table = NULL;
            break;
        
        case PLAN_CREATE_INDEX:
            plan_node->operation.plan_create_index.table = NULL;
            plan_node->operation.plan_create_index.new_index = NULL;
            break;
            
        case PLAN_DROP_INDEX:
            plan_node->operation.plan_drop_index.table = NULL;
            plan_node->operation.plan_drop_index.index = NULL;
            break;

        case PLAN_SEQ_SCAN:
            plan_node->operation.plan_seq_scan.table = NULL;
            break;

        case PLAN_INDEX_SCAN:
            plan_node->operation.plan_index_scan.table = NULL;
            plan_node->operation.plan_index_scan.index = NULL;
            break;

        case PLAN_FILTER:
            plan_node->operation.plan_filter.expression = NULL;

            for (uint32_t i = 0; i < plan_node->operation.plan_filter.amount_column; i++) {
                plan_node->operation.plan_filter.columns[i]->table = NULL;
                free(plan_node->operation.plan_filter.columns[i]);
            }
            free(plan_node->operation.plan_filter.columns);
            break;

        case PLAN_PROJECT:
            for (uint32_t i = 0; i < plan_node->operation.plan_project.num_expressions; i++) {
                plan_node->operation.plan_project.expressions[i] = NULL;
            }
            free(plan_node->operation.plan_project.expressions);

            for (uint32_t i = 0; i < plan_node->operation.plan_project.amount_columns; i++) {
                plan_node->operation.plan_project.columns[i]->table = NULL;
                free(plan_node->operation.plan_project.columns[i]);
            }
            free(plan_node->operation.plan_project.columns);
            break;

        case PLAN_JOIN:
            plan_node->operation.plan_join.left_table = NULL;
            plan_node->operation.plan_join.right_table = NULL;
            filter_free(plan_node->operation.plan_join.on_node);
            break;
        
        case PLAN_INSERT:
            plan_node->operation.plan_insert.table = NULL;

            for (uint32_t i = 0; i < plan_node->operation.plan_insert.num_columns; i++) {
                plan_node->operation.plan_insert.columns[i]->table = NULL;
                free(plan_node->operation.plan_insert.columns[i]);
            }
            free(plan_node->operation.plan_insert.columns);

            for (uint32_t i = 0; i < plan_node->operation.plan_insert.num_rows; i++) {
                plan_node->operation.plan_insert.rows[i] = NULL;
            }
            free(plan_node->operation.plan_insert.rows);
            break;

        case PLAN_UPDATE:
            plan_node->operation.plan_update.table = NULL;

            for (uint32_t i = 0; i < plan_node->operation.plan_update.num_columns; i++) {
                plan_node->operation.plan_update.columns[i]->table = NULL;
                free(plan_node->operation.plan_update.columns[i]);
            }
            free(plan_node->operation.plan_update.columns);

            for (uint32_t i = 0; i < plan_node->operation.plan_update.num_columns; i++) {
                plan_node->operation.plan_update.new_values[i] = NULL;
            }
            free(plan_node->operation.plan_update.new_values);

            filter_free(plan_node->operation.plan_update.filter);
            break;

        case PLAN_DELETE:
            plan_node->operation.plan_delete.table = NULL;
            filter_free(plan_node->operation.plan_delete.filter);
            break;

        case PLAN_GROUP_BY:
            for (uint32_t i = 0; i < plan_node->operation.plan_group_by.num_columns; i++) {
                plan_node->operation.plan_group_by.columns[i]->table = NULL;
                free(plan_node->operation.plan_group_by.columns[i]);
            }
            free(plan_node->operation.plan_group_by.columns);
            break;

        case PLAN_ORDER_BY:
            plan_node->operation.plan_order_by.expr = NULL;
            for (uint32_t i = 0; i < plan_node->operation.plan_order_by.num_columns; i++) {
                plan_node->operation.plan_order_by.columns[i]->table = NULL;
                free(plan_node->operation.plan_order_by.columns[i]);
            }
            free(plan_node->operation.plan_order_by.columns);

            free(plan_node->operation.plan_order_by.types);
            break;

        case PLAN_LIMIT:
        case PLAN_OFFSET:
            break;

        default:
            fprintf(stderr, "Error, invalid PlanNodeType.\n");
    }

    free(plan_node);
}

void filter_free(Filter *filter) {
    if (filter != NULL) {
        filter->expression = NULL;

        for (uint32_t i = 0; i < filter->amount_column; i++) {
            filter->columns[i]->table = NULL;
            free(filter->columns[i]);
        }
        free(filter->columns);

        free(filter);
    }
}