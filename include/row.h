#ifndef ROW_H_
#define ROW_H_

#include <stdint.h>
#include <stdbool.h>

/* Forward Declarations. */
typedef struct expression_node ExpressionNode;
typedef struct value Value;

/* Row structure that contains:
 * is_deleted: tombstone flag
 * n_columns: amount of columns
 * values: array of multiple data type values. */
typedef struct row {
    bool is_deleted;
    uint32_t n_columns;
    Value **values;
} Row;

extern Row *row_create(ExpressionNode **values, uint32_t n_columns);

extern bool row_mark_deleted(Row *row);

extern Value *row_get_value(const Row *row, uint32_t column_pos);

extern bool row_set_value(Row *row, uint32_t column_pos, const Value *new_val);

extern void row_free(Row *row);

#endif