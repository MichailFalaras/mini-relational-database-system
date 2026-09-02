#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/index.h"
#include "index_utils.h"
#include "../../include/pager.h"
#include "../../include/page.h"
#include "../../include/btree.h"
#include "../src/btree/btree_utils.h"
#include "../../include/schema.h"
#include "../../include/row.h"
#include "../../include/data_types.h"

/* Index metadata operations */

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
Index *index_metadata_create(const char *index_name, IndexType type, const IndexKey *key, 
    uint32_t root_page_num, bool is_unique) {

    if (!index_name || index_name[0] == '\0') {
        printf("index_metadata_create: Invalid Index name.\n");
        return NULL;
    }

    if (type != PRIMARY_INDEX && type != SECONDARY_INDEX) {
        printf("index_metadata_create: Invalid index type.\n");
        return NULL;
    }

    if (type == PRIMARY_INDEX && !is_unique) {
        printf("index_metadata_create: Primary index must be unique.\n");
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
    new_index->is_unique = is_unique;

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
bool index_key_matches_prefix(const Index *index, const uint32_t *column_ids, uint32_t num_columns) {
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


/* Index disk operations */

/* Create physical index */
Index *index_create(const char *index_name, IndexType type, const IndexKey *key, Pager *pager, bool is_unique) {
    // Input validation
    if (!index_name || index_name[0] == '\0') {
        printf("index_create: Invalid index name.\n");
        return NULL;
    }

    if (type != PRIMARY_INDEX && type != SECONDARY_INDEX) {
        printf("index_create: Invalid index type.\n");
        return NULL;
    }

    if (!key || !key->column_index_array || key->num_columns == 0) {
        printf("index_create: Invalid index key.\n");
        return NULL;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("index_create: Invalid or uninitialized pager.\n");
        return NULL;
    }

    // Temporarily assigned to 0
    uint32_t root_page_num = 0;
    bool is_page_allocated = false;

    // Determine page number for the index root
    if (!pager_allocate_page(pager, &root_page_num)) {
        printf("index_create: Page allocation failed.\n");
        return NULL;
    }

    is_page_allocated = true;

    // Get root page from cache or load it from the disk
    Page *index_root = pager_get_page(pager, root_page_num);
    if (!index_root) {
        printf("index_create: Index root retrieval failed.\n");
        goto rollback;
    }

    // Clear page's previous data
    if (!page_clear(pager, index_root)) {
        printf("index_create: Root clearing failed.\n");
        goto rollback;
    }

    // Initialize the BTreePage wrapper with the leaf root
    BTreePage btree_index_root = {0}; 

    btree_page_attach(&btree_index_root, index_root);
    
    if (btree_page_init_empty_leaf(&btree_index_root) != BTREE_SUCCESS) {
        printf("index_create: Root initialization failed.\n");
        goto rollback;
    }

    btree_page_sync(pager, &btree_index_root);

    // Create index metadata structure in memory
    Index *index = index_metadata_create(index_name, type, key, root_page_num, is_unique);
    if (!index) {
        printf("index_create: Index metadata creation failed.\n");
        goto rollback;
    }

    return index;


rollback:
    // Rollback: Releasing the currently allocated page 
    // if any processing step fails after its allocation
    if (is_page_allocated && !pager_release_page(pager, root_page_num)) {
        printf("index_create: Root page rollback failed.\n");
    }

    return NULL;

}

/* Truncate physical Index 

 * Failure behavior:
 *
 * Validation or traversal failure causes no modifications.
 *
 * The root is reinitialized before descendant pages are released,
 * ensuring that the index remains a valid empty tree.
 *
 * If one or more descendant releases fail, the function attempts all
 * remaining releases and returns false. Pages that could not be released
 * remain allocated but unreachable. Rollback is not currently supported.
 */
bool index_truncate(Index *index, Pager *pager) {
    if (!index || 
        !index->key || 
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        printf("index_truncate: Invalid input Index.\n");
        return false;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("index_truncate: Invalid or uninitialized pager.\n");
        return false;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        printf("index_truncate: Invalid root page number.\n");
        return false;
    }

    BTreePageCollection *visited_pages = (BTreePageCollection *) calloc(1, sizeof(BTreePageCollection));
    if (!visited_pages) {
        printf("index_truncate: Couldn't allocate page collection.\n");
        return false;
    }

    // Traverse Index B+ Tree
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    if (btree_traverse_reachable_pages(&btree, visited_pages) != BTREE_SUCCESS) {
        printf("index_truncate: Couldn't traverse Index B+ Tree.\n");
        free(visited_pages);
        return false;
    }

    
    Page *root_page = pager_get_page(pager, index->root_page_num);
    if (!root_page) {
        printf("index_truncate: Couldn't load root page.\n");
        free(visited_pages);
        return false;
    }

    // Clearing root page's data
    if (!page_clear(pager, root_page)) {
        printf("index_trucate: Index root page couldn't not be cleared.\n");
        free(visited_pages);
        return false;
    }

    // Re-initializing the index B+ Tree
    BTreePage btree_root_page = {0};

    btree_page_attach(&btree_root_page, root_page);

    if (btree_page_init_empty_leaf(&btree_root_page) != BTREE_SUCCESS) {
        printf("index_truncate: Could not clear root page.\n");
        free(visited_pages);
        return false;
    }

    btree_page_sync(pager, &btree_root_page);

    // Attempting to release all non-root pages
    // ROLLBACK IS CURRENTLY UNAVAILABLE -> Returns false if any page release fails
    bool all_released = true;
    for (uint32_t i = 0; i < visited_pages->count; i++) {
        uint32_t page_num = visited_pages->page_numbers[i];

        if (page_num == index->root_page_num) {
            continue;
        }

        if(!pager_release_page(pager, page_num)) {
            printf("index_truncate: Could not release page %u\n", page_num);
            all_released = false;
        }
        
    }

    free(visited_pages);
    return all_released;
}


/*
 * Drop physical index.
 *
 * Failure behavior:
 *
 * Validation or traversal failure causes no modifications.
 *
 * Pages are released in reverse traversal order, so descendants are
 * released before parents and the root is released last.
 *
 * Once release begins, rollback is unavailable. If any release fails,
 * all remaining releases are still attempted and the function returns
 * false. The index metadata is retained, but the partially released
 * physical index must not be used.
 *
 * Metadata is freed only after every index page is released successfully.
 */
bool index_drop(Index *index, Pager *pager) {
    if (!index || 
        !index->key || 
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        printf("index_drop: Invalid input Index.\n");
        return false;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("index_drop: Invalid or uninitialized pager.\n");
        return false;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        printf("index_drop: Invalid root page number.\n");
        return false;
    }

    BTreePageCollection *visited_pages = (BTreePageCollection *) calloc(1, sizeof(BTreePageCollection));
    if (!visited_pages) {
        printf("index_drop: Couldn't allocate page collection.\n");
        return false;
    }

    // Traverse Index B+ Tree
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;
    if (btree_traverse_reachable_pages(&btree, visited_pages) != BTREE_SUCCESS) {
        printf("index_drop: Couldn't traverse Index B+ Tree.\n");
        free(visited_pages);
        return false;
    }

    // Attempting to release all non-root pages
    // ROLLBACK IS CURRENTLY UNAVAILABLE -> Returns false if any page release fails
    // Releasing descendands nodes before root node
    bool all_released = true;
    
    for (uint32_t i = visited_pages->count; i > 0; i--) {
        uint32_t page_num = visited_pages->page_numbers[i - 1];

        if (!pager_release_page(pager, page_num)) {
            printf("index_drop: Could not release page %u.\n", page_num);
            all_released = false;
        }
    }

    free(visited_pages);

    if (!all_released) {
        return false;
    }

    // Free index metadata, only if the whole physical index B+ Tree was released successfully.
    index_free(index);
    return all_released;
}


// Find exact-match index entry
// Find all entries whose full IndexKey exactly matches the search key
IndexLookupStatus index_find_exact(const Index *index, Pager *pager, Schema *schema,
    Value **key_values, const uint32_t *column_ids, uint32_t num_columns, IndexRangeResult *result) {
    
    // Validate inputs
    if (!index || !index->key ||
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!pager ||
        pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!schema || !key_values || !column_ids || !result || num_columns == 0) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    // Exact full-key lookup requires every IndexKey column.
    if (num_columns != index->key->num_columns) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    //Search columns must exactly match the IndexKey, including composite-key order.
    if (!index_key_matches_key(index, column_ids, num_columns)) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    result->entries = NULL;
    result->count = 0;
    result->capacity = 0;

    // Build required B+ Tree structures
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    BTreeIndexSpec spec = {0};
    if (!btree_index_spec_init(index, schema, &spec)) {
        return INDEX_LOOKUP_ERROR;
    }

    BTreeSearchKey search_key = {0};
    search_key.index = &spec;
    search_key.target_key = key_values;
    search_key.num_target_keys = num_columns;

    BTreeSearchEntries btree_result = {0};
    BTreeStatus status;

    // For Unique or Primary index, at most one matching row can exist, so use the
    // optimized single-cell B+ Tree exact lookup.
    // For Non-Unique secondary index, multiple leaf cells can contain the exact same full key.
    // Search the inclusive range: key <= entry_key <= key
    if (index->is_unique) {
        BTreeSearchResult search_result = {0};
        status = btree_find_exact_key(&btree, &search_key, &search_result, &btree_result);
    }
    else {
        status = btree_find_range_keys(&btree, &spec, &search_key, true, &search_key, true, &btree_result);
    }

    // Evaluating search completion status code
    switch (status) {
        case BTREE_SUCCESS:
            break;

        case BTREE_NOT_FOUND:
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_NOT_FOUND;

        case BTREE_INVALID_ARGUMENTS:
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_INVALID_ARGUMENTS;

        default:
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_ERROR;
    }

    // No match was found
    if (btree_result.count == 0) {
        btree_search_entries_free(&btree_result);
        index_btree_spec_free(&spec);
        return INDEX_LOOKUP_NOT_FOUND;
    }

    // A match is found, but a unique index cannot return more than 1 matches
    if (index->is_unique && btree_result.count != 1) {
        btree_search_entries_free(&btree_result);
        index_btree_spec_free(&spec);
        return INDEX_LOOKUP_ERROR;
    }

    // Initialize index result structure
    if (!index_range_result_init(result)) {
        btree_cell_contents_free(&btree_result);
        index_btree_spec_free(&spec);
        return INDEX_LOOKUP_ERROR;
    }

    // Transfer Row ownership from BTreeSearchEntries to IndexRangeResult
    for (uint32_t i = 0; i < btree_result.count; i++) {
        Row *row = btree_result.entries[i].cell.BTreePayload.row;

        if (!row) {
            index_range_result_free(result);
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_ERROR;
        }

        if (!index_range_result_append(result, row)) {
            index_range_result_free(result);
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_ERROR;
        }

        btree_result.entries[i].cell.BTreePayload.row = NULL;
    }

    btree_search_entries_free(&btree_result);
    index_btree_spec_free(&spec);

    return INDEX_LOOKUP_SUCCESS;
}


IndexLookupStatus index_find_prefix(const Index *index, Pager *pager, Schema *schema, 
    Value **prefix_key_values, const uint32_t *prefix_column_ids, uint32_t prefix_num_columns,
    IndexRangeResult *result) {

    // Validate inputs
    if (!index || !index->key ||
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!schema || !prefix_key_values || !prefix_column_ids || prefix_num_columns == 0 ||
        prefix_num_columns > index->key->num_columns || !result) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!index_key_matches_prefix(index, prefix_column_ids, prefix_num_columns)) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    result->entries = NULL;
    result->count = 0;
    result->capacity = 0;

    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    BTreeIndexSpec spec = {0};
    if (!btree_index_spec_init(index, schema, &spec)) {
        return INDEX_LOOKUP_ERROR;
    }

    BTreeSearchKey prefix_key = {0};
    prefix_key.index = &spec;
    prefix_key.target_key = prefix_key_values;
    prefix_key.num_target_keys = prefix_num_columns;

    BTreeSearchEntries btree_result = {0};

    BTreeStatus status = btree_find_prefix_keys(&btree, &spec, &prefix_key, &btree_result);

    switch (status) {
        case BTREE_SUCCESS:
            break;

        case BTREE_NOT_FOUND:
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_NOT_FOUND;

        case BTREE_INVALID_ARGUMENTS:
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_INVALID_ARGUMENTS;

        default:
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_ERROR;
    }

    // No matches were found
    if (btree_result.count == 0) {
        btree_search_entries_free(&btree_result);
        index_btree_spec_free(&spec);
        return INDEX_LOOKUP_NOT_FOUND;
    }

    // Initialize the index result's structure
    if (!index_range_result_init(result)) {
        btree_search_entries_free(&btree_result);
        index_btree_spec_free(&spec);
        return INDEX_LOOKUP_ERROR;
    }

    // Transfer ownership of rows from BTreeSearchEntries to IndexRangeResult
    for (uint32_t i = 0; i < btree_result.count; i++) {
        Row *row = btree_result.entries[i].cell.BTreePayload.row;

        if (!row) {
            index_range_result_free(result);
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);
            return INDEX_LOOKUP_ERROR;
        }

        if (!index_range_result_append(result, row)) {
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);

            // If append fails, we also need to free the already transferred rows
            index_range_result_free(result);

            return INDEX_LOOKUP_ERROR;
        }

        // Ownership moved into IndexRangeResult.
         
        btree_result.entries[i].cell.BTreePayload.row = NULL;
    }

    btree_search_entries_free(&btree_result);
    index_btree_spec_free(&spec);

    return INDEX_LOOKUP_SUCCESS;
}


// Find range of index entries that match the range query bounds
IndexLookupStatus index_find_range(const Index *index, Pager *pager, Schema *schema,
    Value **start_key_values, const uint32_t *start_column_ids, uint32_t start_num_columns, bool include_start, 
    Value **end_key_values, const uint32_t *end_column_ids, uint32_t end_num_columns, bool include_end, 
    IndexRangeResult *result) {

    // Validate inputs
    if (!index || !index->key || 
        !index->key->column_index_array || 
        index->key->num_columns == 0) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!schema || !result) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    bool has_start = start_key_values != NULL || start_column_ids != NULL || start_num_columns != 0;
    bool has_end = end_key_values != NULL || end_column_ids != NULL || end_num_columns != 0;

    // Validate lower bound search key if it exists,
    if (has_start) {
        if (!start_key_values || !start_column_ids || 
            start_num_columns == 0 ||
            start_num_columns > index->key->num_columns) {
            return INDEX_LOOKUP_INVALID_ARGUMENTS;
        }
        
        // Range bounds may use an IndexKey prefix.
        if (!index_key_matches_prefix(index, start_column_ids, start_num_columns)) {
            return INDEX_LOOKUP_INVALID_ARGUMENTS;
        }
    }

    // Validate upper bound search key if it exists,
    if (has_end) {
        if (!end_key_values || !end_column_ids || 
            end_num_columns == 0 ||
            end_num_columns > index->key->num_columns) {
            return INDEX_LOOKUP_INVALID_ARGUMENTS;
        }

        // Range bounds may use an IndexKey prefix.
        if (!index_key_matches_prefix(index, end_column_ids, end_num_columns)) {
            return INDEX_LOOKUP_INVALID_ARGUMENTS;
        }
    }

    // If both bounds exist, they currently need to describe prefixes of the same length.
    if (has_start && has_end && start_num_columns != end_num_columns) {
        return INDEX_LOOKUP_INVALID_ARGUMENTS;
    }

    result->entries = NULL;
    result->count = 0;
    result->capacity = 0;

    // Initialize B+ Tree structures
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    BTreeIndexSpec spec = {0};
    if (!btree_index_spec_init(index, schema, &spec)) {
        return INDEX_LOOKUP_ERROR;
    }

    BTreeSearchKey start_search_key = {0};
    BTreeSearchKey end_search_key = {0};

    if (has_start) {
        start_search_key.index = &spec;
        start_search_key.target_key = start_key_values;
        start_search_key.num_target_keys = start_num_columns;
    }
    
    if (has_end) {
        end_search_key.index = &spec;
        end_search_key.target_key = end_key_values;
        end_search_key.num_target_keys = end_num_columns;
    }

    BTreeSearchKey *start_search_ptr = has_start ? &start_search_key : NULL;
    BTreeSearchKey *end_search_ptr = has_end ? &end_search_key : NULL;

    BTreeSearchEntries btree_result = {0};

    BTreeStatus status = btree_find_range_keys(&btree, &spec, 
        start_search_ptr, include_start, end_search_ptr, include_end, &btree_result);

    switch (status) {
        case BTREE_SUCCESS:
            break;

        case BTREE_NOT_FOUND:
            index_btree_spec_free(&spec);
            btree_search_entries_free(&btree_result);
            return INDEX_LOOKUP_NOT_FOUND;

        case BTREE_INVALID_ARGUMENTS:
            index_btree_spec_free(&spec);
            btree_search_entries_free(&btree_result);
            return INDEX_LOOKUP_INVALID_ARGUMENTS;

        default:
            index_btree_spec_free(&spec);
            btree_search_entries_free(&btree_result);
            return INDEX_LOOKUP_ERROR;
    }

    // An exact non-unique range lookup that found no matching rows should return a success status code
    if (btree_result.count == 0) {
        btree_search_entries_free(&btree_result);
        index_btree_spec_free(&spec);

        // result was already initialized to {NULL, 0, 0}
        return INDEX_LOOKUP_SUCCESS;
    }

    // Allocate the range result entries 
    if (!index_range_result_init(result)) {
        index_btree_spec_free(&spec);
        btree_search_entries_free(&btree_result);

        return INDEX_LOOKUP_ERROR;
    }

    // Transfer ownership of rows from BTreeSearchEntries to IndexRangeResult
    for (uint32_t i = 0; i < btree_result.count; i++) {
        Row *row = btree_result.entries[i].cell.BTreePayload.row;

        if (!row) {
            index_range_result_free(result);
            btree_search_entries_free(&btree_result);
            index_btree_spec_free(&spec);

            return INDEX_LOOKUP_ERROR;
        }

        if (!index_range_result_append(result, row)) {
            index_btree_spec_free(&spec);
            btree_search_entries_free(&btree_result);

            // If append fails, we also need to free the already transferred rows
            index_range_result_free(result);
            
            return INDEX_LOOKUP_ERROR;
        }
        
        // Ownership of current Row struct has been transferred
        btree_result.entries[i].cell.BTreePayload.row = NULL;
    }

    index_btree_spec_free(&spec);
    btree_search_entries_free(&btree_result);
    
    return INDEX_LOOKUP_SUCCESS;
}


// Full Index scan
IndexLookupStatus index_scan(const Index *index, Pager *pager, Schema *schema, 
    IndexRangeResult *result) {
    
    return index_find_range(
        index, 
        pager, 
        schema,
        NULL, NULL, 0, true,
        NULL, NULL, 0, true,
        result
    );
}

/*
 * Insert an entry into the Index B+ Tree.
 *
 * Failure behavior:
 * Validation and duplicate-key failures do not modify Index metadata.
 *
 * B+ Tree insertion failures are propagated as Index mutation errors.
 * Atomic rollback of partially applied B+ Tree modifications is not
 * currently supported and is the responsibility of the B+ Tree layer.
 *
 * Root page changes are reflected in the in-memory Index metadata.
 * Persistent root metadata updates are deferred until system catalog
 * persistence is implemented.
 */
IndexMutationStatus index_insert_entry(Index *index, Pager *pager, Schema *schema, Row *row) {
    // Validate inputs
    if (!index || !index->key || 
        !index->key->column_index_array || 
        index->key->num_columns == 0) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!schema || !row || !row->values || row->n_columns == 0 || row->is_deleted) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    // Initialize sttructures of insertion functionality
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    BTreeIndexSpec spec = {0};
    if (!btree_index_spec_init(index, schema, &spec)) {
        return INDEX_MUTATION_ERROR;
    }

    // Initialized in btree_insert()
    BTreeInsertionResult result = {0};

    // Initialize Contents newly-inserted Cell 
    BTreeCellContents cell_contents = {0};

    BTreeStatus status = get_cell_contents_from_row(&cell_contents, &spec, row);
    
    if (status != BTREE_SUCCESS) {
        index_btree_spec_free(&spec);
        
        return status == BTREE_INVALID_ARGUMENTS
            ? INDEX_MUTATION_INVALID_ARGUMENTS
            : INDEX_MUTATION_ERROR;
    }

    // Insertion orchestration
    status = btree_insert(&btree, &cell_contents, &result, &spec);

    free(cell_contents.keys);
    cell_contents.keys = NULL;
    cell_contents.BTreePayload.row = NULL;

    switch (status) {
        case BTREE_SUCCESS:
            break;

        case BTREE_DUPLICATE_KEY:
            index_btree_spec_free(&spec);
            return INDEX_MUTATION_DUPLICATE_KEY;

        case BTREE_INVALID_ARGUMENTS:
            index_btree_spec_free(&spec);
            return INDEX_MUTATION_INVALID_ARGUMENTS;

        default:
            index_btree_spec_free(&spec);
            return INDEX_MUTATION_ERROR;
    }

    // Root may have changed during split propagation
    if (btree.root_page_num != index->root_page_num) {
        index->root_page_num = btree.root_page_num;

        // TODO(catalog): Persist the updated index root page number
        // when system-catalog metadata persistence is implemented.
    }

    index_btree_spec_free(&spec);
    return INDEX_MUTATION_SUCCESS;
}


/*
 * Delete one logical entry from the Index B+ Tree.
 *
 * For UNIQUE indexes, the IndexKey uniquely identifies the entry.
 * For NON-UNIQUE indexes, equal keys may belong to multiple rows,
 * so both the IndexKey and target Row are used to identify the
 * exact leaf entry.
 *
 * B+ Tree mutations are not transactional at this stage.
 * If deletion fails after structural modification, those changes
 * cannot currently be rolled back. Transaction/recovery support
 * will be introduced separately.
 *
 * Root page changes caused by root collapse are reflected in the
 * in-memory Index metadata. Persistent catalog updates are deferred
 * until system-catalog persistence is implemented.
 */
IndexMutationStatus index_delete_entry(Index *index, Pager *pager, Schema *schema, Row *row) {
    // Validate inputs
    if (!index || !index->key || 
        !index->key->column_index_array || 
        index->key->num_columns == 0) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!schema || !row || !row->values || row->n_columns == 0 || row->is_deleted) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    // Initialize sttructures of deletion functionality
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    BTreeIndexSpec spec = {0};
    if (!btree_index_spec_init(index, schema, &spec)) {
        return INDEX_MUTATION_ERROR;
    }

    // Initialized in btree_delete()
    BTreeDeletionResult deletion_result = {0};

    // Initialize Contents of the target Cell
    BTreeCellContents target_cell = {0};

    BTreeStatus status = get_cell_contents_from_row(&target_cell, &spec, row);
    
    if (status != BTREE_SUCCESS) {
        index_btree_spec_free(&spec);
        
        return status == BTREE_INVALID_ARGUMENTS 
            ? INDEX_MUTATION_INVALID_ARGUMENTS 
            : INDEX_MUTATION_ERROR;
    }

    // Let btree_delete() find the exact row 
    status = btree_delete(&btree, &target_cell, &deletion_result, &spec);

    IndexMutationStatus mutation_status;

    switch (status) {
        case BTREE_SUCCESS:
            mutation_status = INDEX_MUTATION_SUCCESS;
            break;

        case BTREE_NOT_FOUND:
            mutation_status = INDEX_MUTATION_NOT_FOUND;
            break;

        case BTREE_INVALID_ARGUMENTS:
            mutation_status = INDEX_MUTATION_INVALID_ARGUMENTS;
            break;

        default:
            mutation_status = INDEX_MUTATION_ERROR;
            break;
    }

    // Root may have changed because of merge/root collapse
    if (btree.root_page_num != index->root_page_num) {
        index->root_page_num = btree.root_page_num;

        /*
        * TODO(catalog): Persist the updated index root page number
        * when system-catalog metadata persistence is implemented.
        */
    }

    free(target_cell.keys);
    target_cell.keys = NULL;
    target_cell.BTreePayload.row = NULL;

    index_btree_spec_free(&spec);
    return mutation_status;
}


// Update Index entry
IndexMutationStatus index_update_entry(Index *index, Pager *pager, Schema *schema,
    Row *old_row, Row *new_row) {
    
    // Validate inputs
    if (!schema ||
        !index || 
        !index->key || 
        !index->key->column_index_array || 
        index->key->num_columns == 0) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (index->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        index->root_page_num >= pager->num_pages ||
        index->root_page_num >= MAX_PAGES) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!old_row || !old_row->values || old_row->is_deleted || old_row->n_columns == 0) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (!new_row || !new_row->values || new_row->is_deleted || new_row->n_columns == 0) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    if (old_row->n_columns != new_row->n_columns) {
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    // Initialize necessary structures
    BTree btree = {0};
    btree.pager = pager;
    btree.root_page_num = index->root_page_num;

    BTreeIndexSpec spec = {0};
    if (!btree_index_spec_init(index, schema, &spec)) {
        return INDEX_MUTATION_ERROR;
    }

    // Build Cell Contents struct from old Row
    BTreeCellContents old_cell = {0};
    BTreeStatus status = get_cell_contents_from_row(&old_cell, &spec, old_row);

    if (status != BTREE_SUCCESS) {
        index_btree_spec_free(&spec);
        
        return status == BTREE_INVALID_ARGUMENTS 
            ? INDEX_MUTATION_INVALID_ARGUMENTS 
            : INDEX_MUTATION_ERROR;
    }

    // Build Cell Contents struct from new Row
    BTreeCellContents new_cell = {0};
    status = get_cell_contents_from_row(&new_cell, &spec, new_row);

    if (status != BTREE_SUCCESS) {
        free(old_cell.keys);
        old_cell.keys = NULL;
        old_cell.BTreePayload.row = NULL;

        index_btree_spec_free(&spec);
        
        return status == BTREE_INVALID_ARGUMENTS 
            ? INDEX_MUTATION_INVALID_ARGUMENTS 
            : INDEX_MUTATION_ERROR;
    }

    // Validate keys before comparing index keys
    if (index->key->num_columns != old_cell.num_keys ||
        index->key->num_columns != new_cell.num_keys ||
        old_cell.num_keys != new_cell.num_keys) {

        free(old_cell.keys);
        free(new_cell.keys);
        old_cell.keys = NULL;
        new_cell.keys = NULL;
        old_cell.BTreePayload.row = NULL;
        new_cell.BTreePayload.row = NULL;

        index_btree_spec_free(&spec);
        return INDEX_MUTATION_INVALID_ARGUMENTS;
    }

    // Determine if the 2 cells have the same index key values:
    // Find at least one different column value
    bool same_key = true;
    for (uint32_t i = 0; i < old_cell.num_keys; i++) {
        int comp = 0;
        
        if (!value_compare(old_cell.keys[i], new_cell.keys[i], &comp)) {
            free(old_cell.keys);
            free(new_cell.keys);
            old_cell.keys = NULL;
            new_cell.keys = NULL;
            old_cell.BTreePayload.row = NULL;
            new_cell.BTreePayload.row = NULL;

            index_btree_spec_free(&spec);
            return INDEX_MUTATION_ERROR;
        }

        if (comp != 0) {
            same_key = false;
            break;
        }
    }

    IndexMutationStatus mutation_status;  

    // Checking if the old and new Rows are equal.
    // In this case we make sure the old Row actually exists, otherwise it should return NOT_FOUND
    if (row_equals(old_row, new_row)) {
        BTreeSearchKey search_key = {0};
        search_key.index = &spec;
        search_key.num_target_keys = old_cell.num_keys;
        
        search_key.target_key = values_to_serialized_key(old_cell.keys, old_cell.num_keys, &spec);
        if (!search_key.target_key) {
            free(old_cell.keys);
            free(new_cell.keys);
            old_cell.keys = NULL;
            new_cell.keys = NULL;
            old_cell.BTreePayload.row = NULL;
            new_cell.BTreePayload.row = NULL;

            index_btree_spec_free(&spec);
            return INDEX_MUTATION_ERROR;
        }

        uint32_t page_num = UINT32_MAX;
        uint16_t cell_index = UINT16_MAX;

        status = btree_locate_target_row(&btree, &search_key, old_row, &spec, &page_num, &cell_index);

        free(search_key.target_key);
        search_key.target_key = NULL;

        switch (status) {
            case BTREE_SUCCESS:
                mutation_status = INDEX_MUTATION_SUCCESS;
                break;
            
            case BTREE_NOT_FOUND:
                mutation_status = INDEX_MUTATION_NOT_FOUND;
                break;

            case BTREE_INVALID_ARGUMENTS:
                mutation_status = INDEX_MUTATION_INVALID_ARGUMENTS;
                break;
            
            default:
                mutation_status = INDEX_MUTATION_ERROR;
                break;
        }

        free(old_cell.keys);
        free(new_cell.keys);
        old_cell.keys = NULL;
        new_cell.keys = NULL;
        old_cell.BTreePayload.row = NULL;
        new_cell.BTreePayload.row = NULL;

        index_btree_spec_free(&spec);
        return mutation_status;
    }


    // If the index is Unique and the new Row has a different Index key,
    // we have to verify that the new key value doesn't already exist in the Index,
    // If it exists, we return a DUPLICATE_KEY
    if (index->is_unique && !same_key) {
        BTreeSearchKey replacement_key = {0};
        replacement_key.index = &spec;
        replacement_key.num_target_keys = new_cell.num_keys;
        
        replacement_key.target_key = values_to_serialized_key(new_cell.keys, new_cell.num_keys, &spec);
        if (!replacement_key.target_key) {
            free(old_cell.keys);
            free(new_cell.keys);
            old_cell.keys = NULL;
            new_cell.keys = NULL;
            old_cell.BTreePayload.row = NULL;
            new_cell.BTreePayload.row = NULL;

            index_btree_spec_free(&spec);
            return INDEX_MUTATION_ERROR;
        }

        BTreeSearchResult search_result = {0};
        BTreeSearchEntries entries = {0};

        status = btree_find_exact_key(&btree, &replacement_key, &search_result, &entries);
        
        free(replacement_key.target_key);
        replacement_key.target_key = NULL; 

        // New key already exists in a unique index
        if (status == BTREE_SUCCESS) {
            mutation_status = INDEX_MUTATION_DUPLICATE_KEY;
            
            free(old_cell.keys);
            free(new_cell.keys);
            old_cell.keys = NULL;
            new_cell.keys = NULL;
            old_cell.BTreePayload.row = NULL;
            new_cell.BTreePayload.row = NULL;

            btree_search_entries_free(&entries);
            index_btree_spec_free(&spec);
            return mutation_status;
        }

        // Error searching for new key
        if (status != BTREE_NOT_FOUND) {
            mutation_status = status == BTREE_INVALID_ARGUMENTS
                                    ? INDEX_MUTATION_INVALID_ARGUMENTS
                                    : INDEX_MUTATION_ERROR;
            free(old_cell.keys);
            free(new_cell.keys);
            old_cell.keys = NULL;
            new_cell.keys = NULL;
            old_cell.BTreePayload.row = NULL;
            new_cell.BTreePayload.row = NULL;

            btree_search_entries_free(&entries);
            index_btree_spec_free(&spec);
            return mutation_status;
        }

        btree_search_entries_free(&entries);
    }

    // Delete old Row
    BTreeDeletionResult deletion_result = {0};
    
    status = btree_delete(&btree, &old_cell, &deletion_result, &spec);

    switch (status) {
        case BTREE_SUCCESS:
            mutation_status = INDEX_MUTATION_SUCCESS;
            break;
        
        case BTREE_NOT_FOUND:
            mutation_status = INDEX_MUTATION_NOT_FOUND;
            break;
            
        case BTREE_INVALID_ARGUMENTS:
            mutation_status = INDEX_MUTATION_INVALID_ARGUMENTS;
            break;
        
        default:
            mutation_status = INDEX_MUTATION_ERROR;
            break;
    }

    if (status != BTREE_SUCCESS) {
        // Deletion may have changed the root before reporting failure.
        // The BTree object carries the current root.
        if (btree.root_page_num != index->root_page_num) {
            index->root_page_num = btree.root_page_num;

            /*
            * TODO(catalog): Persist the updated index root page number
            * when system-catalog metadata persistence is implemented.
            */
        }

        free(old_cell.keys);
        free(new_cell.keys);
        old_cell.keys = NULL;
        new_cell.keys = NULL;
        old_cell.BTreePayload.row = NULL;
        new_cell.BTreePayload.row = NULL;

        index_btree_spec_free(&spec);
        return mutation_status;
    }

    // Insert new Row
    BTreeInsertionResult insertion_result = {0};

    status = btree_insert(&btree, &new_cell, &insertion_result, &spec);

    /*
    * TODO(transaction):
    * If replacement insertion fails after the old entry has been deleted,
    * restore the original entry as part of transactional rollback.
    *
    * Until transaction/WAL support is implemented, an insertion failure
    * may leave the old index entry deleted.
    */

    switch (status) {
        case BTREE_SUCCESS:
            mutation_status = INDEX_MUTATION_SUCCESS;
            break;

        case BTREE_DUPLICATE_KEY:
            mutation_status = INDEX_MUTATION_DUPLICATE_KEY;
            break;

        case BTREE_INVALID_ARGUMENTS:
            mutation_status = INDEX_MUTATION_INVALID_ARGUMENTS;
            break;

        default:
            mutation_status = INDEX_MUTATION_ERROR;
            break;
    }

    if (btree.root_page_num != index->root_page_num) {
        index->root_page_num = btree.root_page_num;

        /*
        * TODO(catalog): Persist the updated index root page number
        * when system-catalog metadata persistence is implemented.
        */
    }

    free(old_cell.keys);
    free(new_cell.keys);
    old_cell.keys = NULL;
    new_cell.keys = NULL;
    old_cell.BTreePayload.row = NULL;
    new_cell.BTreePayload.row = NULL;

    index_btree_spec_free(&spec);
    return mutation_status;   
}