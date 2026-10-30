#ifndef QUERY_OPTIMIZER_UTILS_H_
#define QUERY_OPTIMIZER_UTILS_H_

#include <stdbool.h>

typedef struct plan_node PlanNode;
typedef enum plan_node_types PlanNodeTypes;
typedef struct filter Filter;

PlanNode *plan_node_create(PlanNodeTypes type);

void plan_node_free(PlanNode *plan_node);

void filter_free(Filter *filter);

#endif