#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/index.h"
#include "../../include/pager.h"
#include "../../include/page.h"
#include "../../include/btree.h"

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
Index *index_create(const char *index_name, IndexType type, const IndexKey *key, Pager *pager) {
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

    // Initialize the index root with the page's binary data
    if (!btree_init_empty_leaf(index_root->page_data)) {
        printf("index_create: Root initialization failed.\n");
        goto rollback;
    }

    // Mark the page as dirty (modified)
    if (!page_mark_dirty(index_root)) {
        printf("index_create: Root page dirty marking failed.\n");
        goto rollback;
    }

    // Create index metadata structure in memory
    Index *index = index_metadata_create(index_name, type, key, root_page_num);
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
    if (!btree_traverse_reachable_pages(index, pager, visited_pages)) {
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
    if (!btree_init_empty_leaf(root_page->page_data)) {
        printf("index_truncate: Could not clear root page.\n");
        free(visited_pages);
        return false;
    }

    if (!page_mark_dirty(root_page)) {
        printf("index_truncate: Could not mark root dirty.\n");
        free(visited_pages);
        return false;
    }

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
    if (!btree_traverse_reachable_pages(index, pager, visited_pages)) {
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