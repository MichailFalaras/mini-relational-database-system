#ifndef QUERY_OPTIMIZER_UTILS_H_
#define QUERY_OPTIMIZER_UTILS_H_

#include <stdbool.h>

typedef struct plan_node PlanNode;
typedef enum plan_node_types PlanNodeTypes;
typedef struct filter Filter;

/* ---------- PlanNode Utilities ---------- */

/* Create PlanNode struct and initialize common values. */
PlanNode *plan_node_create(PlanNodeTypes type);

/* Append PlanNode to QueryPlan's PlanNode list
 * (NOTE: PlanNode->next should be NULL) */
bool query_plan_append_node(QueryPlan *query_plan, PlanNode *new_node);

/* Free PlanNode, its internal objects and nullify pointers. */
void plan_node_free(PlanNode *plan_node);

/* Filter-specific free, freeing of internal objects and nullifying pointers. */
void filter_free(Filter *filter);

#endif