#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../include/row.h"
#include "../../include/expressions.h"
#include "../../include/data_types.h"
#include  "../data_types/data_types_utils.h"

/* Create Row Struct & Initialize with Value copies. */
Row *row_create(ExpressionNode **values, uint32_t n_columns) {
    if (!values || !n_columns) {
        return NULL;
    }

    Row *row = (Row *) calloc(1, sizeof(Row));
    if (row == NULL) {
        perror("row_create");
        return NULL;
    }

    row->is_deleted = false;
    row->n_columns = n_columns;

    row->values = (Value **) malloc(row->n_columns*sizeof(Value *));
    if (row->values == NULL) {
        perror("row_create");
        return NULL;
    }

    for (uint32_t i = 0; i < row->n_columns; i++) {
        row->values[i] = value_copy(values[i]->expression_data.literal_value.literal);
        if (!row->values[i]) {
            return NULL;
        }
    }

    return row;
}

/* Mark Row as deleted without freeing it/completely removing
it from the database. */
bool row_mark_deleted(Row *row) {
    if (!row) {
        return false;
    }

    row->is_deleted = true;

    return true;
}

/* Row return Value * from values array index. */
Value *row_get_value(const Row *row, uint32_t column_pos) {
    if (!row || column_pos >= row->n_columns) {
        return NULL;
    }

    return row->values[column_pos];
}

Value **row_get_values(const Row *row, uint32_t *column_index_array, uint32_t num_columns) {
    if (!row || !column_index_array || !num_columns) {
        return NULL;
    }

    Value **values = (Value **) malloc(num_columns*sizeof(Value *));
    if (!values) {
        return NULL;
    }

    for (uint32_t i = 0; i < num_columns; i++) {
        values[i] = value_copy(row->values[column_index_array[i]]);
        if (!values[i]) {
            value_free_array(values, i);
            return NULL;
        }
    }

    return values;
}

/* Change Row Value pointer to a copy of a different value. */
bool row_set_value(Row *row, uint32_t column_pos, const Value *new_val) {
    if (!row || column_pos >= row->n_columns || !new_val) {
        return false;
    }

    Value *temp = row->values[column_pos];
     
    Value *copy = value_copy(new_val);
    if (copy == NULL) {
        return false;
    }

    row->values[column_pos] = copy;
    
    value_free(temp);
    return true;
}

/* Free Row, Values and Values' internals. */
void row_free(Row *row) {
    if (row != NULL) {
        if (row->values != NULL) {
            for (uint32_t i = 0; i < row->n_columns; i++) {
                if (row->values[i] != NULL) {
                    value_free(row->values[i]);
                }
            }
        }
        
        free(row->values);
        free(row);
        row = NULL;
    }
}


// Compares 2 rows column-by-column
bool row_equals(const Row *left, const Row *right) {
    // Validate inputs
    if (!left || !left->values || !left->n_columns || left->is_deleted) {
        return false;
    }

    if (!right || !right->values || !right->n_columns || right->is_deleted) {
        return false;
    }

    if (left->n_columns != right->n_columns) {
        return false;
    }

    for (uint32_t i = 0; i < left->n_columns; i++) {
        int comp = 0;

        if (!value_compare(left->values[i], right->values[i], &comp)) {
            return false;
        }

        if (comp != 0) {
            return false;
        }
    }

    return true;
}
