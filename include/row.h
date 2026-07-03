#ifndef ROW_H_
#define ROW_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"

/* Row structure that contains:
 * is_deleted: tombstone flag
 * n_columns: amount of columns
 * values: array of multiple data type values. */
typedef struct row {
    bool is_deleted;
    uint32_t n_columns;
    Value *values;
} Row;

#endif