#include <stdio.h>
#include "../../include/execution_engine.h"
#include "../../include/data_types.h"

/* Temporary placeholder function for schema_tests.c compilation.
 *
 * (NOTE: Support for OP_MODULO or OP_BITWISE_NOT wasn't even thought of
 * when the structure for basic execution engine operations were made.
 * Executing operations should be based on Value structs. There should
 * be more support added to src/data_types/* so that the Execution Engine
 * can actually execute operations). */
Value *execute_operation(Value *left_val, Value *right_val, OperatorType type) {

    // Cannot execute operations with NULL values. (extra safeguard)
    if (left_val->null_val || right_val->null_val) {
        value_free(left_val);
        value_free(right_val);
        
        return NULL;
    }

    bool temp = true;
    return value_create(BOOL, &temp);
}
