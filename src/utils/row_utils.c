#include <stdio.h>
#include <stdlib.h>
#include "../../include/row.h"
#include "../../include/expressions.h"

/* Create Row Struct & Initialize with Value copies. */
Row *row_create(ExpressionNode **values, uint32_t n_columns) {

    if (values == NULL) {
        return NULL;
    }

    Row *row = (Row *) malloc(sizeof(Row));
    if (row == NULL) {
        perror("row_create");
        exit(1);
    }
    row->is_deleted = false;
    row->n_columns = n_columns;

    row->values = (Value **) malloc(row->n_columns*sizeof(Value *));
    if (row->values == NULL) {
        perror("row_create");
        exit(1);
    }

    for (uint32_t i = 0; i < row->n_columns; i++) {
        Value *copy = value_copy(values[i]->expression_data.literal_value.literal);
        row->values[i] = copy;
    }

    return row;
}

/* Mark Row as deleted without freeing it/completely removing
it from the database. */
bool row_mark_deleted(Row *row) {
    if (row == NULL) {
        return false;
    }

    row->is_deleted = true;

    return true;
}

/* Row return Value * from values array index. */
Value *row_get_value(const Row *row, uint32_t column_pos) {

    if (row == NULL) {
        return NULL;
    }

    if (column_pos > row->n_columns || column_pos == 0) {
        return NULL;
    }

    return row->values[column_pos-1];
}

/* Change Row Value pointer to a copy of a different value. */
bool row_set_value(Row *row, uint32_t column_pos, const Value *new_val) {

    if (new_val == NULL || row == NULL) {
        return false;
    }

    if (column_pos > row->n_columns || column_pos == 0) {
        return false;
    }

    Value *temp = row->values[column_pos-1];
     
    Value *copy = value_copy(new_val);
    if (copy == NULL) {
        return false;
    }

    row->values[column_pos-1] = copy;
    
    value_free(temp);
    return true;
}

/* Free Row, Values and Values' internals. */
void row_free(Row *row) {
    if (row != NULL) {
        for (uint32_t i = 0; i < row->n_columns; i++) {
            value_free(row->values[i]);
        }

        free(row->values);
        free(row);
    }
}
