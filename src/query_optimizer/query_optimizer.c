#include <stdio.h>
#include <stdlib.h>
#include "../../include/query_optimizer.h"
#include "query_optimizer_utils.h"

/* Allocate QueryPlan. */
QueryPlan *query_plan_create() {
    QueryPlan *plan = (QueryPlan *) calloc(1, sizeof(QueryPlan));
    if (!plan) {
        printf("query_plan_init: Error allocating memory for query plan.");
        return NULL;
    }

    plan->head = NULL;
    plan->amount_plan_nodes = 0;

    return plan;
}

/* Deallocate QueryPlan and free linked PlanNodes. */
void query_plan_free(QueryPlan *plan) {
    if (plan != NULL) {
        PlanNode *curr = plan->head, *prev = NULL;

        while (curr != NULL) {
            prev = curr;
            curr = curr->next;

            plan_node_free(prev);
        }

        free(plan);
    }
}