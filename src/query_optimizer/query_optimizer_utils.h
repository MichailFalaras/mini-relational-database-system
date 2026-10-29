#ifndef QUERY_OPTIMIZER_UTILS_H_
#define QUERY_OPTIMIZER_UTILS_H_

typedef struct filter Filter;
typedef struct plan_node PlanNode;

void plan_node_free(PlanNode *plan_node);

void filter_free(Filter *filter);

#endif