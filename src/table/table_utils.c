
#include "./table_utils.h"
#include "../../include/table.h"
#include "../../include/index.h"
#include "../../include/schema.h"

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