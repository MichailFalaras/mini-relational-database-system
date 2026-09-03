#include <stdio.h>
#include "../../include/execution_engine.h"
#include "../../include/data_types.h"

/* Temporary placeholder function for schema_tests.c compilation. */
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
