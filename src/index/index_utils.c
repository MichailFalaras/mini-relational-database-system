#include "../../include/index.h"
#include "index_utils.h"

/* Decrement Index Key columns' position index after removing a column */
bool index_key_shift_after_column_drop(Index *index, uint32_t col_pos) {
    if (!index || !index->key) {
        printf("index_key_shift_after_column_drop: Invalid input Index.\n");
        return false;
    }

    if (index->key->num_columns > 0 && !index->key->column_index_array) {
        printf("index_key_shift_after_column_drop: Index key column array is NULL.\n");
        return false;
    }

    // Checking if the dropped column still exists in the index key
    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        if (index->key->column_index_array[i] == col_pos) {
            printf("index_key_shift_after_column_drop: Dropped column belongs to the index key.\n");
            return false;
        }
    }

    // Safely decrementing subsequency columns' positions
    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        if (index->key->column_index_array[i] > col_pos) {
            index->key->column_index_array[i]--;
        }
    }

    return true;
}