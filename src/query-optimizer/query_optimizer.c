#include <stdio.h>
#include <stdlib.h>
#include "../../include/query_optimizer.h"
#include "../../include/row.h"

// Forward declarations of static helper functions
static void query_plan_free_nested_filter(Filter *filter, bool free_node);
static void query_plan_free_nested(PlanNode *node);


// Interface function -- Create empty query plan
QueryPlan *query_plan_init() {
    QueryPlan *plan = (QueryPlan *) malloc(sizeof(QueryPlan));
    if (!plan) {
        printf("query_plan_init: Error allocating memory for query plan.");
        return NULL;
    }

    plan->head = NULL;
    plan->amount_plan_nodes = 0;
    return plan;
}

// Interface function -- Deallocate query plan
void query_plan_free(QueryPlan *plan) {
    if (!plan) {
        printf("query_plan_free: Plan structure is NULL.");
        return;
    }

    PlanNode *curr = plan->head;

    while (curr) {
        PlanNode *next = curr->next;

        // Free any nested node fields that were allocated by the query optimizer
        query_plan_free_nested(curr);
        // Before freeing the node itself
        free(curr);

        curr = next;
    }

    free(plan);
}


// Private helper function that frees a nested Filter node
static void query_plan_free_nested_filter(Filter *filter, bool free_node) {
    if (!filter) {
        printf("query_plan_free_nested_filter: Filter node is NULL.");
        return;
    }

    free(filter->columns);
    filter->columns = NULL;
    filter->amount_column_refs = 0;

    filter->expression = NULL;

    if (free_node) {
        free(filter);
    }
}

// Private helper function that frees any nested fields owned by the query optimizer
static void query_plan_free_nested(PlanNode *node) {
    if (!node) {
        printf("query_plan_free_nested: Plan node is NULL.");
        return;
    }

    switch (node->type) {
        case PLAN_FILTER:
            // Free Filter's column array
            query_plan_free_nested_filter(&node->operation.plan_filter, false);
            break;

        case PLAN_PROJECT:
            // Free Project's columns array, and expressions array of pointers
            free(node->operation.plan_project.columns);
            free(node->operation.plan_project.expressions);

            node->operation.plan_project.columns = NULL;
            node->operation.plan_project.expressions = NULL;
            node->operation.plan_project.num_columns = 0;
            node->operation.plan_project.num_expressions = 0;
            break;

        case PLAN_JOIN:
            // Free Join's inner Filter node, and inner Filter's column array
            query_plan_free_nested_filter(&node->operation.plan_join.on_node, true);
            node->operation.plan_join.on_node = NULL;
            break;

        case PLAN_INSERT:
            // Free Insert's column array
            free(node->operation.plan_insert.columns);
            node->operation.plan_insert.columns = NULL;
            node->operation.plan_insert.num_columns = 0;

            // Free each Row struct, as INSERT rows are managed by the Query Optimizer
            Insert *insert = &node->operation.plan_insert;

            for (uint32_t i = 0; i < insert->num_rows; i++) {
                row_free(insert->rows[i]);
            }    

            free(insert->rows);
            insert->columns = NULL;
            insert->rows = NULL;
            insert->num_columns = 0;
            insert->num_rows = 0;
            break;

        case PLAN_UPDATE:
            // Free Update's column array, inner Filter node, and inner Filter's column array
            free(node->operation.plan_update.columns);
            node->operation.plan_update.columns = NULL;
            node->operation.plan_update.num_columns = 0;

            query_plan_free_nested_filter(&node->operation.plan_update.filter, true);
            node->operation.plan_update.filter = NULL;
            break;

        case PLAN_DELETE:
            // Free Delete's inner Filter node, and inner Filter's column array
            query_plan_free_nested_filter(&node->operation.plan_delete.filter, true);
            node->operation.plan_delete.filter = NULL;
            break;
        
        default:
            break;
    }
}