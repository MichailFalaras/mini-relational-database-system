#ifndef EXECUTION_ENGINE_H_
#define  EXECUTION_ENGINE_H_

#include <stdint.h>
#include "database.h"
#include "row.h"
#include "query_optimizer.h"
#include "transaction.h"
#include "data_types.h"

typedef enum execution_status {
    EXECUTION_SUCCESS,
    EXECUTION_ERROR,
    EXECUTION_EMPTY
} ExecutionStatus;

typedef struct execution_engine {
    Database *database;
    Transaction *current_transaction;
} ExecutionEngine;

/* Returned rows by SELECT. */
typedef struct query_result {
    Schema *schema;

    Row *rows;
    uint32_t rows_returned;
} QueryResult;

/* Overall Query Result. */
typedef struct execution_result {
    ExecutionStatus status;
    char error_msg[256];

    /* If not SELECT, query_result is NULL. */
    QueryResult *query_result;
    uint32_t rows_affected;
} ExecutionResult;

extern ExecutionEngine *execution_engine_init(Database *db);

extern ExecutionResult *execution_engine_query(const QueryPlan *query_plan);


/* -- First version of execute_operation for evaluate_expression, subject to change.*/
/* Switch case with all different types of operations from expression.h*/

/* Only if:
 * left_val == NULL && right_val == NULL there's error, because
 * its possibly a unary/NOT operation with only one value. */
extern Value *execute_operation(Value *left_val, Value *right_val, OperatorType type);
/* Each operation handler function. (Probably private)*/

/* Comparisons return Value with type BOOL.*/
Value *execute_equality(Value *left_val, Value *right_val);
Value *execute_inequality(Value *left_val, Value *right_val);
Value *execute_less_than(Value *left_val, Value *right_val);
Value *execute_less_than_equal(Value *left_val, Value *right_val);
Value *execute_greater_than(Value *left_val, Value *right_val);
Value *execute_greater_than_equal(Value *left_val, Value *right_val);
Value *execute_logical_and(Value *left_val, Value *right_val);
Value *execute_logical_or(Value *left_val, Value *right_val);
Value *execute_logical_not(Value *val);
Value *execute_addition(Value *left_val, Value *right_val);
Value *execute_subtraction(Value *left_val, Value *right_val);
Value *execute_multiplication(Value *left_val, Value *right_val);
Value *execute_division(Value *left_val, Value *right_val);

/* TODO: static functions in .c file */
ExecutionResult *execute_create_table(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_drop_table(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_truncate_table(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_action(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_add_column(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_drop_column(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_rename_table(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_rename_column(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_modify_column(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_add_constraint(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_alter_drop_constraint(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_create_index(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_drop_index(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_filter(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_project(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_join(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_insert(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_delete(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_update(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_seq_scan(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *execute_index_scan(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *plan_node_group_by(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *plan_node_order_by(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *plan_node_having(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *plan_node_limit(ExecutionEngine *engine, PlanNode *plan_node);
ExecutionResult *plan_node_offset(ExecutionEngine *engine, PlanNode *plan_node);

extern void execution_engine_result_free(ExecutionResult *execution_result);

extern void execution_engine_free(ExecutionEngine *engine);

#endif