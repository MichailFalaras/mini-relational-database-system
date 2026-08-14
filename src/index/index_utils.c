#include <stdio.h>
#include <stdlib.h>
#include "../../include/index.h"
#include "../../include/row.h"
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


/* ---------- IndexRangeResult helpers ---------- */

bool index_range_result_init(IndexRangeResult *result) {
    if (!result) {
        return false;
    }

    result->entries = (IndexEntry *) calloc(INDEX_RANGE_INITIAL_CAPACITY, sizeof(IndexEntry));
    
    if (!result->entries) {
        result->count = 0;
        result->capacity = 0;
        return false;
    }

    result->count = 0;
    result->capacity = INDEX_RANGE_INITIAL_CAPACITY;
    return true;
}

bool index_range_result_append(IndexRangeResult *result, Row *row) {
    if (!result || !row) {
        return false;
    }

    if (result->count == result->capacity) {
        uint32_t new_capacity = 
                        result->capacity == 0
                            ? INDEX_RANGE_INITIAL_CAPACITY
                            : result->capacity * 2;

        if (new_capacity < result->capacity) {
            return false;
        }

        IndexEntry *new_entries = (IndexEntry *) realloc(result->entries, new_capacity * sizeof(IndexEntry));

        if (!new_entries) {
            return false;
        }

        result->entries = new_entries;
        result->capacity = new_capacity;
    }

    result->entries[result->count].row = row;
    result->count++;

    return true;
}

bool index_entry_free(IndexEntry *entry) {
    if (!entry) {
        return false;
    }

    if (entry->row) {
        row_free(entry->row);
        entry->row = NULL;
    }

    return true;
}

bool index_range_result_free(IndexRangeResult *result) {
    if (!result) {
        return false;
    }

    for (uint32_t i = 0; i < result->count; i++) {
        index_entry_free(&result->entries[i]);
    }

    free(result->entries);

    result->entries = NULL;
    result->count = 0;
    result->capacity = 0;

    return true;
}