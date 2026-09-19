#include <stdlib.h>
#include "./table_utils.h"
#include "../../include/table.h"
#include "../../include/index.h"
#include "../../include/schema.h"
#include "../../include/row.h"
#include "../../include/catalog.h"


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

// Update index's root page number in corresponding catalog record
TableMutationStatus table_sync_index_catalog_root(Table *table, Index *index, Catalog *catalog,
    uint32_t old_root_page_num) {

    if (!table || !index || !catalog) {
        return TABLE_MUTATION_INVALID_ARGUMENTS;
    }

    // Avoid an unnecessary catalog update if the root did not change
    if (old_root_page_num == index->root_page_num) {
        return TABLE_MUTATION_SUCCESS;
    }

    CatalogRecordInfo record_info = {0};

    strncpy(record_info.table_name, table->name, sizeof(record_info.table_name) - 1);
    record_info.table_name[sizeof(record_info.table_name) - 1] = '\0';

    strncpy(record_info.object_name, index->name, sizeof(record_info.object_name) - 1);
    record_info.object_name[sizeof(record_info.object_name) - 1] = '\0';

    record_info.type = CATALOG_INDEX;
    record_info.root_page_num = old_root_page_num;
    record_info.object.index = index;

    CatalogStatus status = catalog_update_record(catalog, &record_info, index->root_page_num);

    return catalog_mutation_to_table_mutation_status(status);
}

// Update table's root page number in corresponding catalog record
TableMutationStatus table_sync_table_catalog_root(Table *table, Catalog *catalog,
    uint32_t old_root_page_num) {

    if (!table || !catalog || !table->primary_index) {
        return TABLE_MUTATION_INVALID_ARGUMENTS;
    }

    if (old_root_page_num == table->primary_index->root_page_num) {
        return TABLE_MUTATION_SUCCESS;
    }

    CatalogRecordInfo record_info = {0};

    strncpy(record_info.table_name, table->name, sizeof(record_info.table_name) - 1);
    record_info.table_name[sizeof(record_info.table_name) - 1] = '\0';

    strncpy(record_info.object_name, table->name, sizeof(record_info.object_name) - 1);
    record_info.object_name[sizeof(record_info.object_name) - 1] = '\0';

    record_info.type = CATALOG_TABLE;
    record_info.root_page_num = old_root_page_num;
    record_info.object.table = table;

    CatalogStatus status = catalog_update_record(catalog, &record_info, table->primary_index->root_page_num);

    return catalog_mutation_to_table_mutation_status(status);
}

// Conversion from index mutation status to table mutation status
TableMutationStatus index_mutation_to_table_mutation_status(IndexMutationStatus status) {
    switch (status) {
        case INDEX_MUTATION_SUCCESS:
            return TABLE_MUTATION_SUCCESS;

        case INDEX_MUTATION_DUPLICATE_KEY:
            return TABLE_MUTATION_DUPLICATE_KEY;

        case INDEX_MUTATION_NOT_FOUND:
            return TABLE_MUTATION_NOT_FOUND;

        case INDEX_MUTATION_INVALID_ARGUMENTS:
            return TABLE_MUTATION_INVALID_ARGUMENTS;

        case INDEX_MUTATION_ERROR:
        default:
            return TABLE_MUTATION_ERROR;
    }
}

// Conversion from catalog status to table mutation status
TableMutationStatus catalog_mutation_to_table_mutation_status(CatalogStatus status) {
    switch (status) {
        case CATALOG_SUCCESS:
            return TABLE_MUTATION_SUCCESS;

        case CATALOG_DUPLICATE_KEY:
            return TABLE_MUTATION_DUPLICATE_KEY;

        case CATALOG_NOT_FOUND:
            return TABLE_MUTATION_NOT_FOUND;

        case CATALOG_INVALID_ARGUMENTS:
            return TABLE_MUTATION_INVALID_ARGUMENTS;

        case CATALOG_ERROR:
        default:
            return TABLE_MUTATION_ERROR;
    }
}