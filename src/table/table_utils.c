#include <stdlib.h>
#include "./table_utils.h"
#include "../../include/table.h"
#include "../../include/index.h"
#include "../../include/schema.h"
#include "../../include/row.h"

// Helper that validates a table's logical index metadata
bool table_validate_logical_index(const Table *table, const Index *index, IndexType expected_type) {
    if (!table || 
        !table->table_schema || 
        !index ||
        !index->key ||
        !index->key->column_index_array ||
        index->key->num_columns == 0 ) {

        return false;
    }

    if (index->type != expected_type) {
        return false;
    }

    // At this point, an Index is not physically created yet. 
    // So it must have the invalid root page number assigned as its page
    if (index->root_page_num != INVALID_ROOT_PAGE) {
        return false;
    }

    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        uint32_t column_position = index->key->column_index_array[i];

        // Checking for invalid column positions in Index Key
        if (column_position >= table->table_schema->num_columns) {
            return false;
        }

        for (uint32_t j = i+1; j < index->key->num_columns; j++) {

            // Checking for duplicate columns in the Index Key
            if (column_position == index->key->column_index_array[j]) {
                return false;
            }
        }
    }

    return true;
}

/* ---------- TableResult helpers ---------- */

bool table_row_result_init(TableRowResult *result) {
    if (!result) {
        return false;
    }

    // Result must be in an empty, uninitialized state
    if (result->rows ||result->count != 0 || result->capacity != 0) {
        return false;
    }

    result->rows = (Row **) calloc(TABLE_RANGE_INITIAL_CAPACITY, sizeof(Row *));
    
    if (!result->rows) {
        result->count = 0;
        result->capacity = 0;
        return false;
    } 

    result->count = 0;
    result->capacity = TABLE_RANGE_INITIAL_CAPACITY;
    return true;
}

// On success, ownership of Row is transferred to the result structure
// On failure, ownership remains with the caller
bool table_row_result_append(TableRowResult *result, Row *row) {
    if (!result || !row) {
        return false;
    }

    if (result->count == result->capacity) {
        uint32_t new_capacity = 
                        result->capacity == 0
                            ? TABLE_RANGE_INITIAL_CAPACITY
                            : result->capacity * 2;

        if (new_capacity < result->capacity) {
            return false;
        }

        Row **result_rows = (Row **) realloc(result->rows, new_capacity * sizeof(Row *));

        if (!result_rows) {
            return false;
        }

        result->rows = result_rows;
        result->capacity = new_capacity;
    }

    result->rows[result->count] = row;
    result->count++;
    
    return true;
}

void table_row_result_free(TableRowResult *result) {
    if (!result) {
        return;
    }

    if (result->rows) {
        for (uint32_t i = 0; i < result->count; i++) {
            if (result->rows[i]) {
                row_free(result->rows[i]);
                result->rows[i] = NULL;
            }
            
        }

        free(result->rows);
    }

    result->rows = NULL;
    result->count = 0;
    result->capacity = 0;
}