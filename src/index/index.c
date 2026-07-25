#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/index.h"


/* Create Index key */
IndexKey *index_key_create(const uint32_t *column_indexes, uint32_t num_columns) {
    if (!column_indexes) {
        printf("index_key_create: Column indexes array is NULL.\n");
        return NULL;
    }

    if (num_columns == 0) {
        printf("index_key_create: Invalid number of columns.\n");
        return NULL;
    }

    IndexKey *new_key = (IndexKey *) calloc(1, sizeof(IndexKey));
    if (!new_key) {
        printf("index_key_create: New Index key could not be allocated.\n");
        return NULL;
    }

    new_key->column_index_array = (uint32_t *) malloc(num_columns * sizeof(uint32_t));
    if (!new_key->column_index_array) {
        printf("index_key_create: Columns index array could not be allocated.\n");
        index_key_free(new_key);
        return NULL;
    }

    memcpy(new_key->column_index_array, column_indexes, num_columns * sizeof(uint32_t));
    new_key->num_columns = num_columns;

    return new_key;
}

/* Free Index key */
void index_key_free(IndexKey *key) {
    if (!key) {
        printf("index_key_free: Input key is NULL.\n");
        return;
    }    

    free(key->column_index_array);
    free(key);
}

/* Creation of logical Index struct */
Index *index_metadata_create(const char *index_name, IndexType type, const IndexKey *key, uint32_t root_page_num) {
    if (!index_name || index_name[0] == '\0') {
        printf("index_metadata_create: Invalid Index name.\n");
        return NULL;
    }

    if (type != PRIMARY_INDEX && type != SECONDARY_INDEX) {
        printf("index_metadata_create: Invalid index type.\n");
        return NULL;
    }

    if (!key || !key->column_index_array || key->num_columns == 0) {
        printf("index_metadata_create: Invalid Index key.\n");
        return NULL;
    }

    if (root_page_num <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("index_metaadata_create: Invalid root page number.\n");
        return NULL;
    }

    // Allocating new logical Index structure
    Index *new_index = (Index *) calloc(1, sizeof(Index));
    if (!new_index) {
        printf("index_metadata_create: New Index could not be allocated.\n");
        return NULL;
    }

    // Deep-copying the input parameters in the new Index structure
    if (strlen(index_name) >= sizeof(new_index->name)) {
        printf("index_metadata_create: Input name exceeds the length limit.\n");
        index_free(new_index);
        return NULL;
    }

    strcpy(new_index->name, index_name);
    
    new_index->key = index_key_create(key->column_index_array, key->num_columns);
    if (!new_index->key) {
        printf("index_metadata_create: Index key could not be deep-copied.\n");
        index_free(new_index);
        return NULL;
    }

    new_index->type = type;
    new_index->root_page_num = root_page_num;
    
    return new_index;
}

/* Deallocation of logical Index struct */
void index_free(Index *index) {
    if (!index) {
        printf("index_free: Index struct is NULL.\n");
        return;
    }

    index_key_free(index->key);
    free(index);
}


/* Check if Index key contains a particular column */
bool index_key_has_column(const Index *index, uint32_t index_key) {
    if (!index || 
        !index->key || 
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        printf("index_key_has_column: Invalid input Index.\n");
        return false;
    }

    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        if (index->key->column_index_array[i] == index_key)
            return true;
    }

    return false;
}

/* Check if Index key matches all of its fields */
bool index_key_matches_key(const Index *index, const uint32_t *column_ids, uint32_t num_columns) {
    if (!index || 
        !index->key || 
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        printf("index_key_matches_key: Invalid input Index.\n");
        return false;
    }

    if (!column_ids) {
        printf("index_key_matches_key: Input columns index array is NULL.\n");
        return false;
    }

    if (num_columns == 0) {
        printf("index_key_matches_key: Invalid number of columns.\n");
        return false;
    }

    // Number of columns doesn't match
    if (index->key->num_columns != num_columns) {
        return false;
    }

    // Matches full Index key in the same order of columns
    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        if (index->key->column_index_array[i] != column_ids[i]) {
            return false;
        }
    }

    return true;
}

/* Check if Index key matches a prefix of columns */
bool index_key_matches_prefix( const Index *index, const uint32_t *column_ids, uint32_t num_columns) {
    if (!index || 
        !index->key || 
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        printf("index_key_matches_prefix: Invalid input Index.\n");
        return false;
    }

    if (!column_ids) {
        printf("index_key_matches_prefix: Input columns index array is NULL.\n");
        return false;
    }

    if (num_columns == 0) {
        printf("index_key_matches_prefix: Invalid number of columns.\n");
        return false;
    }

    // Number of prefix columns shouldn't exceed number of index columns
    if (num_columns > index->key->num_columns) {
        return false;
    }

    // Matches prefix Index key in the same order of columns
    for (uint32_t i = 0; i < num_columns; i++) {
        if (index->key->column_index_array[i] != column_ids[i]) {
            return false;
        }
    }

    return true;
}

