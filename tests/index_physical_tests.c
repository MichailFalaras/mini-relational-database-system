#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/index.h"
#include "../include/pager.h"
#include "../include/page.h"
#include "../include/btree.h"
#include "../src/btree/btree_utils.h"
#include "../src/index/index_utils.h"
#include "../include/schema.h"
#include "../include/row.h"
#include "../include/data_types.h"

#define ASSERT(condition) do { \
    if (!(condition)) { \
        goto cleanup; \
    } \
} while (0)

/* ---------- Helpers/Shared Setup for unit tests ---------- */

// Pager Helpers
typedef struct test_pager {
    char path[256];
    Pager *pager;
} TestPager;


static bool create_test_pager(TestPager *test_pager) {
    if (!test_pager) {
        return NULL;
    }

    memset(test_pager, 0, sizeof(TestPager));

    // Create temporary database file
    char path_template[] = "./build/minidb_index_test_XXXXXX";

    int fd = mkstemp(path_template);
    if (fd == -1) {
        printf("create_test_pager: Error creating temporary file.\n");
        return false;
    }

    // Close it, since pager_open() opens it to initialize the database
    if (close(fd) == -1) {
        printf("create_test_pager: Closing file descriptor failed.\n");
        unlink(path_template);
        return false;
    }

    if (strlen(path_template) >= sizeof(test_pager->path)) {
        unlink(path_template);
        return false;
    }

    strcpy(test_pager->path, path_template);

    test_pager->pager = pager_open(test_pager->path);
    if (!test_pager->pager) {
        unlink(path_template);
        test_pager->path[0] = '\0';
        return false;
    }

    return true;
}

static void destroy_test_pager(TestPager *test_pager) {
    if (!test_pager) {
        return;
    }

    if (test_pager->pager) {
        pager_close(test_pager->pager);
        test_pager->pager = NULL;
    }

    if (test_pager->path[0] != '\0') {
        unlink(test_pager->path);
        test_pager->path[0] = '\0';
    }
}

// Index Helpers
static IndexKey *create_test_index_key_from(const uint32_t *columns, uint32_t count) {
    return index_key_create(columns, count);
}

static Index *create_test_index_with(Pager *pager, const char *name, IndexType type, 
    const uint32_t *columns, uint32_t column_count, bool is_unique) {
    
    IndexKey *key = index_key_create(columns, column_count);
    if (!key) {
        return NULL;
    }

    Index *index = index_create(name, type, key, pager, is_unique);
    
    index_key_free(key);

    return index;
}


// Schema & Row Helpers
static Schema *create_test_schema() {
    Column first_column = {0};
    Column second_column = {0};
    Column payload_column = {0};

    strcpy(first_column.name, "first_column");
    strcpy(second_column.name, "second_column");
    strcpy(payload_column.name, "payload_column");

    first_column.type = INTEGER;
    second_column.type = INTEGER;
    payload_column.type = INTEGER;

    Column *columns[] = {&first_column, &second_column, &payload_column};

    return schema_create(columns, NULL, 3, 0);
}

static Schema *create_large_key_test_schema() {
    Column key_column = {0};

    strcpy(key_column.name, "key_column");
    key_column.type = TEXT;

    Column *columns[] = {&key_column};

    return schema_create(columns, NULL, 1, 0);
}

static Row *create_large_key_test_row(const char *key) {
    if (!key) {
        return NULL;
    }

    Row *row = (Row *) calloc(1, sizeof(Row));
    if (!row) {
        return NULL;
    }

    row->n_columns = 1;
    row->values = (Value **) calloc(1, sizeof(Value *));
    if (!row->values) {
        row_free(row);
        return NULL;
    }

    row->values[0] = value_create(TEXT, key);
    if (!row->values[0]) {
        row_free(row);
        return NULL;
    }

    return row;
}

static bool large_key_row_matches(const Row *row, const char *key) {
    if (!row || !row->values || row->n_columns != 1 || !key) {
        return false;
    }

    return row->values[0] &&
           !row->values[0]->null_val &&
           row->values[0]->type == TEXT &&
           row->values[0]->value.text_val &&
           strcmp(row->values[0]->value.text_val, key) == 0;
}

static Row *create_test_row(int32_t first, int32_t second, int32_t payload) {
    Row *row = (Row *) calloc(1, sizeof(Row));
    if (!row) {
        return NULL;
    }

    row->n_columns = 3;
    row->values = (Value **) calloc(row->n_columns, sizeof(Value *));
    if (!row->values) {
        row_free(row);
        return NULL;
    }

    row->values[0] = value_create(INTEGER, &first);
    row->values[1] = value_create(INTEGER, &second);
    row->values[2] = value_create(INTEGER, &payload);

    if (!row->values[0] || !row->values[1] || !row->values[2]) {
        row_free(row);
        return NULL;
    }

    return row;
}

static bool row_matches(const Row *row, int32_t first, int32_t second, int32_t payload) {
    if (!row || !row->values || row->n_columns != 3) {
        return false;
    }

    for (uint32_t i = 0; i < row->n_columns; i++) {
        if (!row->values[i] || row->values[i]->null_val || row->values[i]->type != INTEGER) {
            return false;
        }
    }

    return row->values[0]->value.int32_val == first &&
           row->values[1]->value.int32_val == second &&
           row->values[2]->value.int32_val == payload;
}

static bool result_contains_row(const IndexRangeResult *result, int32_t first, int32_t second, int32_t payload) {
    if (!result || !result->entries) {
        return false;
    }

    for (uint32_t i = 0; i < result->count; i++) {
        if (row_matches(result->entries[i].row, first, second, payload)) {
            return true;
        }
    }

    return false;
}

// B+ Tree Helpers

// A simple deterministic B+ Tree with a root node and 2 children nodes
typedef struct test_btree {
    uint32_t root_page_num;
    uint32_t child_page_nums[2];
    uint32_t child_count;
} TestBTree;

static bool build_test_btree(Index *index, Pager *pager, TestBTree *test_btree) {
    if (!index || !pager || !test_btree) {
        return false;
    }
    
    memset(test_btree, 0, sizeof(TestBTree));

    test_btree->root_page_num = index->root_page_num;
    test_btree->child_count = 2;

    // Allocate pages & page numbers for child nodes
    for (uint32_t i = 0; i < test_btree->child_count; i++) {
        if (!pager_allocate_page(pager, &test_btree->child_page_nums[i])) {
            return false;
        }
    }

    // Retrieve the B+ Tree's pages
    Page *root = pager_get_page(pager, test_btree->root_page_num);
    Page *left_child = pager_get_page(pager, test_btree->child_page_nums[0]);
    Page *right_child = pager_get_page(pager, test_btree->child_page_nums[1]);

    if (!root || !left_child || !right_child) {
        return false;
    }

    // Clear the original root and both new pages before assigning their B+ tree layouts.
    if (!page_clear(pager, root) || !page_clear(pager, left_child) || !page_clear(pager, right_child)) {
        return false;
    }

    BTreePage root_node = {0};
    BTreePage left_child_node = {0};
    BTreePage right_child_node = {0};

    btree_page_attach(&root_node, root);
    btree_page_attach(&left_child_node, left_child);
    btree_page_attach(&right_child_node, right_child);

    // Initialize leaf nodes
    if (btree_page_init_empty_leaf(&left_child_node) != BTREE_SUCCESS || 
        btree_page_init_empty_leaf(&right_child_node) != BTREE_SUCCESS) {
        return false;
    }

    // Setting leaf node connections
    left_child_node.is_root = 0;
    left_child_node.parent_pointer = test_btree->root_page_num;
    left_child_node.type_specific_data.siblings.previous_leaf_pointer = UINT32_MAX;
    left_child_node.type_specific_data.siblings.next_leaf_pointer = test_btree->child_page_nums[1];

    right_child_node.is_root = 0;
    right_child_node.parent_pointer = test_btree->root_page_num;
    right_child_node.type_specific_data.siblings.previous_leaf_pointer = test_btree->child_page_nums[0];
    right_child_node.type_specific_data.siblings.next_leaf_pointer = UINT32_MAX;

    // Synchronize leaf headers and sibling metadata
    btree_page_sync(pager, &left_child_node);
    btree_page_sync(pager, &right_child_node);

    /* Initialize root metadata. */
    if (btree_page_init_internal(&root_node, test_btree->child_page_nums[1]) != BTREE_SUCCESS) {
        return false;
    }

    uint16_t cell_size = sizeof(uint32_t);
    uint16_t cell_offset = (uint16_t)(PAGE_SIZE - cell_size);

    root_node.cell_count = 1;
    root_node.free_space_offset = cell_offset;

    /*
    * Must happen before set_cell_pointer(), because that setter reads
    * node_type from page_data to determine the header size.
    */
    btree_page_sync(pager, &root_node);

    set_cell_pointer(root_node.data, 0, make_cell_pointer(cell_offset, cell_size));

    set_cell_child_pointer(root_node.data, cell_offset, test_btree->child_page_nums[0]);

    if (!page_mark_dirty(root) || !page_touch(pager, root)) {
        return false;
    }

    return true;
}

static bool is_empty_btree_root(Page *page) {
    if (!page) {
        return false;
    }

    return get_node_type(page->page_data) == BTREE_LEAF_NODE &&
           get_root_status(page->page_data) == 1 &&
           get_parent_pointer(page->page_data) == UINT32_MAX &&
           get_cell_count(page->page_data) == 0 &&
           get_free_space_offset(page->page_data) == PAGE_SIZE &&
           get_leaf_previous(page->page_data) == UINT32_MAX &&
           get_leaf_next(page->page_data) == UINT32_MAX;
}

static bool contains_page_num(const uint32_t *page_nums, uint32_t count, uint32_t target) { 
    if (!page_nums) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (page_nums[i] == target) {
            return true;
        }
    }

    return false;
}


/* ---------- index_create unit tests ---------- */

static int test_index_create_success() {
    int result = 1;
    TestPager test_pager = {0};
    IndexKey *key = NULL;
    Index *index = NULL;

    uint32_t columns[] = {0};
    uint32_t expected_root_page_num = 0;

    ASSERT(create_test_pager(&test_pager));

    expected_root_page_num = test_pager.pager->num_pages;

    key = create_test_index_key_from(columns, 1);
    ASSERT(key);

    index = index_create("test_index", PRIMARY_INDEX, key, test_pager.pager, true);
    ASSERT(index);

    ASSERT(strcmp(index->name, "test_index") == 0);

    ASSERT(index->type == PRIMARY_INDEX);
    ASSERT(index->is_unique);

    ASSERT(index->key &&
           index->key->column_index_array &&
           index->key->num_columns == 1 &&
           index->key->column_index_array[0] == columns[0]);

    ASSERT(index->root_page_num == expected_root_page_num);

    ASSERT(index->root_page_num > SYSTEM_CATALOG_PAGE_NUM &&
           index->root_page_num < test_pager.pager->num_pages);

    result = 0;

cleanup:
    if (index) { index_free(index); }
    if (key) { index_key_free(key); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_create_initialize_empty_root() {
    int result = 1;
    TestPager test_pager = {0};
    IndexKey *key = NULL;
    Index *index = NULL;

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    key = create_test_index_key_from(columns, 1);
    ASSERT(key);

    index = index_create("test_index", PRIMARY_INDEX, key, test_pager.pager, true);
    ASSERT(index);

    Page *root = pager_get_page(test_pager.pager, index->root_page_num);
    ASSERT(root);

    ASSERT(is_empty_btree_root(root));

    result = 0;

cleanup:
    if (index) { index_free(index); }
    if (key) { index_key_free(key); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_create_invalid_arguments() {
    int result = 1;
    TestPager test_pager = {0};
    IndexKey *key = NULL;

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    key = create_test_index_key_from(columns, 1);
    ASSERT(key);

    // Testing various error cases
    // If any error cases somehow turns up to be successfully, the test fails
    ASSERT(!(index_create(NULL, PRIMARY_INDEX, key, test_pager.pager, true)));

    ASSERT(!(index_create("", PRIMARY_INDEX, key, test_pager.pager, true)));

    ASSERT(!(index_create("test_index", (IndexType) 99, key, test_pager.pager, false)));

    ASSERT(!(index_create("test_index", PRIMARY_INDEX, NULL, test_pager.pager, true)));

    ASSERT(!(index_create("test_index", PRIMARY_INDEX, key, NULL, true)));

    result = 0;

cleanup:
    if (key) { index_key_free(key); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_create_reuses_released_pages() {
    int result = 1;
    TestPager test_pager = {0};
    IndexKey *key = NULL;
    Index *index = NULL;

    uint32_t columns[] = {0};
    uint32_t released_page_num = 0;

    ASSERT(create_test_pager(&test_pager));


    key = create_test_index_key_from(columns, 1);
    ASSERT(key);

    // Freeing the allocated page 0, from the creation of the pager
    ASSERT(pager_allocate_page(test_pager.pager, &released_page_num));

    ASSERT(pager_release_page(test_pager.pager, released_page_num));

    index = index_create("test_index", PRIMARY_INDEX, key, test_pager.pager, true);
    ASSERT(index);

    // Creating the index should use the released page 0
    ASSERT(index->root_page_num == released_page_num);

    result = 0;
    
cleanup:
    if (index) { index_free(index); }
    if (key) { index_key_free(key); }
    destroy_test_pager(&test_pager);

    return result;
}


/* ---------- index_truncate unit tests ---------- */

static int test_index_truncate_empty_tree() {
    int result = 1;
    TestPager test_pager = {0};
    Index *index = NULL;

    uint32_t columns[] = {0};
    uint32_t original_root_page_num = 0;

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    original_root_page_num = index->root_page_num;

    ASSERT(index_truncate(index, test_pager.pager));

    ASSERT(index->root_page_num == original_root_page_num);

    Page *root = pager_get_page(test_pager.pager, original_root_page_num);

    ASSERT(is_empty_btree_root(root));

    result = 0;

cleanup:
    if (index) { index_free(index); }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_index_truncate_multi_page_tree() {
    int result = 1;
    TestPager test_pager = {0};
    IndexKey *key = NULL;
    Index *index = NULL;
    TestBTree test_btree = {0};

    uint32_t columns[] = {0};
    uint32_t original_root_page_num = 0;
    uint32_t reallocated_page_nums[2] = {0};

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    original_root_page_num = index->root_page_num;

    ASSERT(build_test_btree(index, test_pager.pager, &test_btree));

    // Confirm test B+ tree was constructed correctly
    ASSERT(test_btree.root_page_num == original_root_page_num);

    ASSERT(test_btree.child_count == 2);

    ASSERT(index_truncate(index, test_pager.pager));

    // Verify root still exists
    ASSERT(index->root_page_num == original_root_page_num);

    Page *root = pager_get_page(test_pager.pager, index->root_page_num);
    
    // Verify root is empty
    ASSERT(root && is_empty_btree_root(root));

    for (uint32_t i = 0; i < 2; i++) {
        ASSERT(pager_allocate_page(test_pager.pager, &reallocated_page_nums[i]));
    }

    ASSERT(contains_page_num(reallocated_page_nums, 2, test_btree.child_page_nums[0]));

    ASSERT(contains_page_num(reallocated_page_nums, 2, test_btree.child_page_nums[1]));

    ASSERT(!(contains_page_num(reallocated_page_nums, 2, original_root_page_num)));

    result = 0;

cleanup:
    if (index) { index_free(index); }
    if (key) { index_key_free(key); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_truncate_preserves_metadata() {
    int result = 1;
    TestPager test_pager = {0};
    Index *index = NULL;
    TestBTree test_btree = {0};
    
    uint32_t columns[] = {1, 3};
    uint32_t original_root_page_num = 0;

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, false);
    ASSERT(index);

    original_root_page_num = index->root_page_num;

    ASSERT(build_test_btree(index, test_pager.pager, &test_btree));

    ASSERT(index_truncate(index, test_pager.pager));

    ASSERT(strcmp(index->name, "test_index") == 0 &&
           index->type == SECONDARY_INDEX &&
           index->root_page_num == original_root_page_num &&
           index->key &&
           index->key->column_index_array &&
           index->key->num_columns == 2 &&
           index->key->column_index_array[0] == columns[0] &&
           index->key->column_index_array[1] == columns[1]);

    result = 0;

cleanup:
    if (index) { index_free(index); }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_index_truncate_invalid_arguments() {
    int result = 1;
    TestPager test_pager = {0};
    Index *index = NULL; 
    
    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 1, false);
    ASSERT(index);

    // If error cases somehow succeed, the test fails
    ASSERT(!(index_truncate(NULL, test_pager.pager)));

    ASSERT(!(index_truncate(index, NULL)));

    result = 0;

cleanup:
    if (index) { index_free(index); }

    destroy_test_pager(&test_pager);
    return result;
}


/* ---------- index_drop unit tests ---------- */

static int test_index_drop_single_page_tree() {
    int result = 1;
    TestPager test_pager = {0};
    Index *index = NULL;

    uint32_t columns[] = {0};
    uint32_t root_page_num = 0;
    uint32_t reallocated_page_num = 0;

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    root_page_num = index->root_page_num;

    ASSERT(index_drop(index, test_pager.pager));

    // index_drop() also frees Index metadata
    index = NULL;

    // Testing if the root page that was previously freed can be reallocated
    ASSERT(pager_allocate_page(test_pager.pager, &reallocated_page_num));

    ASSERT(reallocated_page_num == root_page_num);

    result = 0;

cleanup:
    if (index) { index_free(index); }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_index_drop_multi_page_tree() {
    int result = 1;
    TestPager test_pager = {0};
    Index *index = NULL;
    TestBTree test_btree = {0};

    uint32_t columns[] = {0};
    uint32_t reallocated_page_nums[3] = {0};

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    ASSERT(build_test_btree(index, test_pager.pager, &test_btree));

    ASSERT(index_drop(index, test_pager.pager));

    index = NULL;

    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(pager_allocate_page(test_pager.pager, &reallocated_page_nums[i]));
    }

    ASSERT(contains_page_num(reallocated_page_nums, 3, test_btree.root_page_num) &&
           contains_page_num(reallocated_page_nums, 3, test_btree.child_page_nums[0]) &&
           contains_page_num(reallocated_page_nums, 3, test_btree.child_page_nums[1]));
    
    result = 0;

cleanup:
    if (index) { index_free(index); }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_index_drop_invalid_arguments(void) {
    int result = 1;
    TestPager test_pager = {0};
    Index *index = NULL;

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    // If any error case somehow succeeds, the test fails
    ASSERT(!(index_drop(NULL, test_pager.pager)));

    ASSERT(!(index_drop(index, NULL)));

    result = 0;

cleanup:
    if (index) {
        index_free(index);
    }

    destroy_test_pager(&test_pager);
    return result;
}



/* ---------- index_find_exact unit tests ---------- */

static int test_index_find_exact_full_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *first_row = NULL;
    Row *second_row = NULL;
    IndexRangeResult range_result = {0};
    Value *first_key = NULL;
    Value *second_key = NULL;

    uint32_t columns[] = {0, 1};
    Value *key_values[2] = {0};
    int32_t first = 2;
    int32_t second = 20;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    first_row = create_test_row(1, 10, 110);
    second_row = create_test_row(2, 20, 220);
    ASSERT(first_row && second_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, first_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, second_row) == INDEX_MUTATION_SUCCESS);

    first_key = value_create(INTEGER, &first);
    second_key = value_create(INTEGER, &second);
    ASSERT(first_key && second_key);

    key_values[0] = first_key;
    key_values[1] = second_key;

    ASSERT(index_find_exact(index, test_pager.pager, schema,
        key_values, columns, 2, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 2, 20, 220));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_key) { value_free(first_key); }
    if (second_key) { value_free(second_key); }
    if (first_row) { row_free(first_row); }
    if (second_row) { row_free(second_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_find_exact_missing_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *row = NULL;
    IndexRangeResult range_result = {0};
    Value *first_key = NULL;
    Value *second_key = NULL;

    uint32_t columns[] = {0, 1};
    Value *key_values[2] = {0};
    int32_t first = 9;
    int32_t second = 90;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    row = create_test_row(1, 10, 110);
    ASSERT(row);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_SUCCESS);

    first_key = value_create(INTEGER, &first);
    second_key = value_create(INTEGER, &second);
    ASSERT(first_key && second_key);

    key_values[0] = first_key;
    key_values[1] = second_key;

    ASSERT(index_find_exact(index, test_pager.pager, schema,
        key_values, columns, 2, &range_result) == INDEX_LOOKUP_NOT_FOUND);

    ASSERT(range_result.entries == NULL && range_result.count == 0 && range_result.capacity == 0);

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_key) { value_free(first_key); }
    if (second_key) { value_free(second_key); }
    if (row) { row_free(row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}


/* ---------- index_find_prefix unit tests ---------- */

static int test_index_find_prefix_composite_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[3] = {0};
    IndexRangeResult range_result = {0};
    Value *prefix_key = NULL;

    uint32_t columns[] = {0, 1};
    uint32_t prefix_columns[] = {0};
    Value *prefix_values[1] = {0};
    int32_t first = 1;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    rows[0] = create_test_row(2, 10, 210);
    rows[1] = create_test_row(1, 20, 120);
    rows[2] = create_test_row(1, 10, 110);
    ASSERT(rows[0] && rows[1] && rows[2]);

    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
    }

    prefix_key = value_create(INTEGER, &first);
    ASSERT(prefix_key);
    prefix_values[0] = prefix_key;

    ASSERT(index_find_prefix(index, test_pager.pager, schema,
        prefix_values, prefix_columns, 1, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 2);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 110));
    ASSERT(row_matches(range_result.entries[1].row, 1, 20, 120));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (prefix_key) { value_free(prefix_key); }
    for (uint32_t i = 0; i < 3; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}


/* ---------- index_find_range unit tests ---------- */

static int test_index_find_range_full_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[5] = {0};
    IndexRangeResult range_result = {0};
    Value *start_first = NULL;
    Value *start_second = NULL;
    Value *end_first = NULL;
    Value *end_second = NULL;

    uint32_t columns[] = {0, 1};
    Value *start_values[2] = {0};
    Value *end_values[2] = {0};
    int32_t start_first_value = 1;
    int32_t start_second_value = 20;
    int32_t end_first_value = 2;
    int32_t end_second_value = 20;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    rows[0] = create_test_row(2, 20, 220);
    rows[1] = create_test_row(1, 20, 120);
    rows[2] = create_test_row(3, 10, 310);
    rows[3] = create_test_row(1, 10, 110);
    rows[4] = create_test_row(2, 10, 210);

    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(rows[i]);
        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
    }

    start_first = value_create(INTEGER, &start_first_value);
    start_second = value_create(INTEGER, &start_second_value);
    end_first = value_create(INTEGER, &end_first_value);
    end_second = value_create(INTEGER, &end_second_value);
    ASSERT(start_first && start_second && end_first && end_second);

    start_values[0] = start_first;
    start_values[1] = start_second;
    end_values[0] = end_first;
    end_values[1] = end_second;

    ASSERT(index_find_range(index, test_pager.pager, schema,
        start_values, columns, 2, true,
        end_values, columns, 2, true, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 3);
    ASSERT(row_matches(range_result.entries[0].row, 1, 20, 120));
    ASSERT(row_matches(range_result.entries[1].row, 2, 10, 210));
    ASSERT(row_matches(range_result.entries[2].row, 2, 20, 220));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (start_first) { value_free(start_first); }
    if (start_second) { value_free(start_second); }
    if (end_first) { value_free(end_first); }
    if (end_second) { value_free(end_second); }
    for (uint32_t i = 0; i < 5; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_find_range_composite_prefix() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[5] = {0};
    IndexRangeResult range_result = {0};
    Value *start_key = NULL;
    Value *end_key = NULL;

    uint32_t columns[] = {0, 1};
    uint32_t prefix_columns[] = {0};
    Value *start_values[1] = {0};
    Value *end_values[1] = {0};
    int32_t start_value = 1;
    int32_t end_value = 2;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    rows[0] = create_test_row(2, 20, 220);
    rows[1] = create_test_row(1, 20, 120);
    rows[2] = create_test_row(3, 10, 310);
    rows[3] = create_test_row(1, 10, 110);
    rows[4] = create_test_row(2, 10, 210);

    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(rows[i]);
        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
    }

    start_key = value_create(INTEGER, &start_value);
    end_key = value_create(INTEGER, &end_value);
    ASSERT(start_key && end_key);

    start_values[0] = start_key;
    end_values[0] = end_key;

    ASSERT(index_find_range(index, test_pager.pager, schema,
        start_values, prefix_columns, 1, true,
        end_values, prefix_columns, 1, true, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 4);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 110));
    ASSERT(row_matches(range_result.entries[1].row, 1, 20, 120));
    ASSERT(row_matches(range_result.entries[2].row, 2, 10, 210));
    ASSERT(row_matches(range_result.entries[3].row, 2, 20, 220));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (start_key) { value_free(start_key); }
    if (end_key) { value_free(end_key); }
    for (uint32_t i = 0; i < 5; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_find_range_boundaries() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[5] = {0};
    IndexRangeResult range_result = {0};
    Value *start_first = NULL;
    Value *start_second = NULL;
    Value *end_first = NULL;
    Value *end_second = NULL;

    uint32_t columns[] = {0, 1};
    Value *start_values[2] = {0};
    Value *end_values[2] = {0};
    int32_t start_first_value = 1;
    int32_t start_second_value = 20;
    int32_t end_first_value = 2;
    int32_t end_second_value = 20;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    rows[0] = create_test_row(2, 20, 220);
    rows[1] = create_test_row(1, 20, 120);
    rows[2] = create_test_row(3, 10, 310);
    rows[3] = create_test_row(1, 10, 110);
    rows[4] = create_test_row(2, 10, 210);

    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(rows[i]);
        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
    }

    start_first = value_create(INTEGER, &start_first_value);
    start_second = value_create(INTEGER, &start_second_value);
    end_first = value_create(INTEGER, &end_first_value);
    end_second = value_create(INTEGER, &end_second_value);
    ASSERT(start_first && start_second && end_first && end_second);

    start_values[0] = start_first;
    start_values[1] = start_second;
    end_values[0] = end_first;
    end_values[1] = end_second;

    ASSERT(index_find_range(index, test_pager.pager, schema,
        start_values, columns, 2, false,
        end_values, columns, 2, false, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 2, 10, 210));

    index_range_result_free(&range_result);

    ASSERT(index_find_range(index, test_pager.pager, schema,
        start_values, columns, 2, true,
        end_values, columns, 2, false, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 2);
    ASSERT(row_matches(range_result.entries[0].row, 1, 20, 120));
    ASSERT(row_matches(range_result.entries[1].row, 2, 10, 210));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (start_first) { value_free(start_first); }
    if (start_second) { value_free(start_second); }
    if (end_first) { value_free(end_first); }
    if (end_second) { value_free(end_second); }
    for (uint32_t i = 0; i < 5; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}


/* ---------- index_scan unit tests ---------- */

static int test_index_scan_key_order() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[5] = {0};
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0, 1};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    rows[0] = create_test_row(2, 20, 220);
    rows[1] = create_test_row(1, 20, 120);
    rows[2] = create_test_row(3, 10, 310);
    rows[3] = create_test_row(1, 10, 110);
    rows[4] = create_test_row(2, 10, 210);

    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(rows[i]);
        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
    }

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);

    ASSERT(range_result.count == 5);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 110));
    ASSERT(row_matches(range_result.entries[1].row, 1, 20, 120));
    ASSERT(row_matches(range_result.entries[2].row, 2, 10, 210));
    ASSERT(row_matches(range_result.entries[3].row, 2, 20, 220));
    ASSERT(row_matches(range_result.entries[4].row, 3, 10, 310));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    for (uint32_t i = 0; i < 5; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_lookup_invalid_key_definitions() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    IndexRangeResult range_result = {0};
    Value *first_key = NULL;
    Value *second_key = NULL;

    uint32_t columns[] = {0, 1};
    uint32_t wrong_columns[] = {1, 0};
    uint32_t wrong_prefix[] = {1};
    Value *key_values[2] = {0};
    Value *prefix_values[1] = {0};
    int32_t first = 1;
    int32_t second = 10;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, true);
    ASSERT(index);

    first_key = value_create(INTEGER, &first);
    second_key = value_create(INTEGER, &second);
    ASSERT(first_key && second_key);

    key_values[0] = first_key;
    key_values[1] = second_key;
    prefix_values[0] = first_key;

    // Exact lookup requires the full Index key in the correct column order
    ASSERT(index_find_exact(index, test_pager.pager, schema,
        key_values, columns, 1, &range_result) == INDEX_LOOKUP_INVALID_ARGUMENTS);

    ASSERT(index_find_exact(index, test_pager.pager, schema,
        key_values, wrong_columns, 2, &range_result) == INDEX_LOOKUP_INVALID_ARGUMENTS);

    // Prefix lookup must begin from the first Index key column
    ASSERT(index_find_prefix(index, test_pager.pager, schema,
        prefix_values, wrong_prefix, 1, &range_result) == INDEX_LOOKUP_INVALID_ARGUMENTS);

    // Both range bounds must use prefixes of the same length
    ASSERT(index_find_range(index, test_pager.pager, schema,
        prefix_values, columns, 1, true,
        key_values, columns, 2, true, &range_result) == INDEX_LOOKUP_INVALID_ARGUMENTS);

    // A partially-defined range bound is invalid
    ASSERT(index_find_range(index, test_pager.pager, schema,
        prefix_values, NULL, 1, true,
        NULL, NULL, 0, true, &range_result) == INDEX_LOOKUP_INVALID_ARGUMENTS);

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_key) { value_free(first_key); }
    if (second_key) { value_free(second_key); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}


/* ---------- index_insert_entry unit tests ---------- */

static int test_index_insert_entry_success() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    row = create_test_row(1, 10, 100);
    ASSERT(row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (row) { row_free(row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_insert_duplicate_unique_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *first_row = NULL;
    Row *duplicate_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    first_row = create_test_row(1, 10, 100);
    duplicate_row = create_test_row(1, 20, 200);
    ASSERT(first_row && duplicate_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, first_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, duplicate_row) == INDEX_MUTATION_DUPLICATE_KEY);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_row) { row_free(first_row); }
    if (duplicate_row) { row_free(duplicate_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_insert_duplicate_unique_secondary_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *first_row = NULL;
    Row *duplicate_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 1, true);
    ASSERT(index);

    first_row = create_test_row(1, 10, 100);
    duplicate_row = create_test_row(1, 20, 200);
    ASSERT(first_row && duplicate_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, first_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, duplicate_row) == INDEX_MUTATION_DUPLICATE_KEY);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_row) { row_free(first_row); }
    if (duplicate_row) { row_free(duplicate_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_insert_duplicate_secondary_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *first_row = NULL;
    Row *second_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 1, false);
    ASSERT(index);

    first_row = create_test_row(1, 10, 100);
    second_row = create_test_row(1, 20, 200);
    ASSERT(first_row && second_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, first_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, second_row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 2);
    ASSERT(result_contains_row(&range_result, 1, 10, 100));
    ASSERT(result_contains_row(&range_result, 1, 20, 200));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_row) { row_free(first_row); }
    if (second_row) { row_free(second_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_root_metadata_after_root_split() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};
    uint32_t original_root_page_num = 0;
    uint32_t inserted_count = 0;

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    original_root_page_num = index->root_page_num;

    // Insert only until the original leaf root splits and Index metadata changes
    for (int32_t i = 0; i < 512 && index->root_page_num == original_root_page_num; i++) {
        row = create_test_row(i, i + 1000, i + 2000);
        ASSERT(row);

        ASSERT(index_insert_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_SUCCESS);
        inserted_count++;

        row_free(row);
        row = NULL;
    }

    ASSERT(index->root_page_num != original_root_page_num);

    Page *new_root = pager_get_page(test_pager.pager, index->root_page_num);
    ASSERT(new_root);

    // Verify the updated Index root points to the B+ Tree's actual new root
    ASSERT(get_root_status(new_root->page_data) == 1);
    ASSERT(get_node_type(new_root->page_data) == BTREE_INTERNAL_NODE);

    // Verify the split did not lose existing entries
    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == inserted_count);

    for (uint32_t i = 0; i < range_result.count; i++) {
        ASSERT(row_matches(range_result.entries[i].row, i, i + 1000, i + 2000));
    }

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (row) { row_free(row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_insert_across_internal_root_split() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[64] = {0};
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};
    uint32_t original_root_page_num = 0;
    uint32_t first_split_root_page_num = UINT32_MAX;
    uint32_t inserted_count = 0;
    char key[32] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_large_key_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    original_root_page_num = index->root_page_num;

    // Large keys keep the test small while still forcing leaf and internal/root splits
    for (uint32_t i = 0; i < 64 && index->root_page_num == original_root_page_num; i++) {
        snprintf(key, sizeof(key), "key_%08u", i);
        rows[i] = create_large_key_test_row(key);
        ASSERT(rows[i]);

        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
        inserted_count++;
    }

    ASSERT(index->root_page_num != original_root_page_num);
    first_split_root_page_num = index->root_page_num;

    for (uint32_t i = inserted_count; i < 64 && index->root_page_num == first_split_root_page_num; i++) {
        snprintf(key, sizeof(key), "key_%08u", i);
        rows[i] = create_large_key_test_row(key);
        ASSERT(rows[i]);

        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
        inserted_count++;
    }

    // A second root change means the internal root itself split
    ASSERT(index->root_page_num != first_split_root_page_num);

    Page *new_root = pager_get_page(test_pager.pager, index->root_page_num);
    ASSERT(new_root);
    ASSERT(get_root_status(new_root->page_data) == 1);
    ASSERT(get_node_type(new_root->page_data) == BTREE_INTERNAL_NODE);

    // Verify all entries survived both leaf and internal split propagation
    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == inserted_count);

    for (uint32_t i = 0; i < range_result.count; i++) {
        snprintf(key, sizeof(key), "key_%08u", i);
        ASSERT(large_key_row_matches(range_result.entries[i].row, key));
    }

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    for (uint32_t i = 0; i < 64; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

/* ---------- index_delete_entry unit tests ---------- */

static int test_index_delete_entry_success() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    row = create_test_row(1, 10, 100);
    ASSERT(row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_delete_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 0);

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (row) { row_free(row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_delete_duplicate_secondary_entry() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *first_row = NULL;
    Row *second_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 1, false);
    ASSERT(index);

    first_row = create_test_row(1, 10, 100);
    second_row = create_test_row(1, 20, 200);
    ASSERT(first_row && second_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, first_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, second_row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_delete_entry(index, test_pager.pager, schema, first_row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 20, 200));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (first_row) { row_free(first_row); }
    if (second_row) { row_free(second_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_delete_entry_not_found() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *existing_row = NULL;
    Row *missing_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    existing_row = create_test_row(1, 10, 100);
    missing_row = create_test_row(2, 20, 200);
    ASSERT(existing_row && missing_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, existing_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_delete_entry(index, test_pager.pager, schema, missing_row) == INDEX_MUTATION_NOT_FOUND);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (existing_row) { row_free(existing_row); }
    if (missing_row) { row_free(missing_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

static int test_index_root_metadata_after_root_collapse() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *rows[64] = {0};
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};
    uint32_t original_root_page_num = 0;
    uint32_t split_root_page_num = 0;
    uint32_t inserted_count = 0;
    uint32_t deleted_count = 0;
    char key[32] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_large_key_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    original_root_page_num = index->root_page_num;

    // First create a two-level tree so deletion can collapse its internal root
    for (uint32_t i = 0; i < 64 && index->root_page_num == original_root_page_num; i++) {
        snprintf(key, sizeof(key), "key_%08u", i);
        rows[i] = create_large_key_test_row(key);
        ASSERT(rows[i]);

        ASSERT(index_insert_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
        inserted_count++;
    }

    ASSERT(index->root_page_num != original_root_page_num);
    split_root_page_num = index->root_page_num;

    // Delete until the internal root collapses and Index metadata follows the promoted child
    for (uint32_t i = 0; i < inserted_count && index->root_page_num == split_root_page_num; i++) {
        ASSERT(index_delete_entry(index, test_pager.pager, schema, rows[i]) == INDEX_MUTATION_SUCCESS);
        deleted_count++;
    }

    ASSERT(index->root_page_num != split_root_page_num);

    Page *new_root = pager_get_page(test_pager.pager, index->root_page_num);
    ASSERT(new_root);
    ASSERT(get_root_status(new_root->page_data) == 1);
    ASSERT(get_parent_pointer(new_root->page_data) == UINT32_MAX);
    ASSERT(get_node_type(new_root->page_data) == BTREE_LEAF_NODE);

    // Verify remaining entries are still reachable through the updated root metadata
    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == inserted_count - deleted_count);

    for (uint32_t i = 0; i < range_result.count; i++) {
        snprintf(key, sizeof(key), "key_%08u", i + deleted_count);
        ASSERT(large_key_row_matches(range_result.entries[i].row, key));
    }

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    for (uint32_t i = 0; i < 64; i++) {
        if (rows[i]) { row_free(rows[i]); }
    }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

/* ---------- index_update_entry unit tests ---------- */

static int test_index_update_entry_same_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *old_row = NULL;
    Row *new_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    old_row = create_test_row(1, 10, 100);
    new_row = create_test_row(1, 20, 200);
    ASSERT(old_row && new_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, old_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_update_entry(index, test_pager.pager, schema, old_row, new_row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 20, 200));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (old_row) { row_free(old_row); }
    if (new_row) { row_free(new_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_update_entry_changed_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *old_row = NULL;
    Row *new_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    old_row = create_test_row(1, 10, 100);
    new_row = create_test_row(2, 10, 100);
    ASSERT(old_row && new_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, old_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_update_entry(index, test_pager.pager, schema, old_row, new_row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 2, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (old_row) { row_free(old_row); }
    if (new_row) { row_free(new_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_update_entry_equal_row() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    row = create_test_row(1, 10, 100);
    ASSERT(row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_SUCCESS);

    // Equal old/new Rows should only verify that the existing entry is present
    ASSERT(index_update_entry(index, test_pager.pager, schema, row, row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (row) { row_free(row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_update_duplicate_unique_key() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *old_row = NULL;
    Row *existing_row = NULL;
    Row *new_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    old_row = create_test_row(1, 10, 100);
    existing_row = create_test_row(2, 20, 200);
    new_row = create_test_row(2, 30, 300);
    ASSERT(old_row && existing_row && new_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, old_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_insert_entry(index, test_pager.pager, schema, existing_row) == INDEX_MUTATION_SUCCESS);

    ASSERT(index_update_entry(index, test_pager.pager, schema,
        old_row, new_row) == INDEX_MUTATION_DUPLICATE_KEY);

    // Duplicate rejection must happen before the old entry is deleted
    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 2);
    ASSERT(result_contains_row(&range_result, 1, 10, 100));
    ASSERT(result_contains_row(&range_result, 2, 20, 200));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (old_row) { row_free(old_row); }
    if (existing_row) { row_free(existing_row); }
    if (new_row) { row_free(new_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_update_entry_not_found() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *existing_row = NULL;
    Row *old_row = NULL;
    Row *new_row = NULL;
    IndexRangeResult range_result = {0};

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    existing_row = create_test_row(1, 10, 100);
    old_row = create_test_row(2, 20, 200);
    new_row = create_test_row(3, 30, 300);
    ASSERT(existing_row && old_row && new_row);

    ASSERT(index_insert_entry(index, test_pager.pager, schema, existing_row) == INDEX_MUTATION_SUCCESS);
    ASSERT(index_update_entry(index, test_pager.pager, schema, old_row, new_row) == INDEX_MUTATION_NOT_FOUND);

    ASSERT(index_scan(index, test_pager.pager, schema, &range_result) == INDEX_LOOKUP_SUCCESS);
    ASSERT(range_result.count == 1);
    ASSERT(row_matches(range_result.entries[0].row, 1, 10, 100));

    result = 0;

cleanup:
    index_range_result_free(&range_result);
    if (existing_row) { row_free(existing_row); }
    if (old_row) { row_free(old_row); }
    if (new_row) { row_free(new_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}
static int test_index_mutation_invalid_arguments() {
    int result = 1;
    TestPager test_pager = {0};
    Schema *schema = NULL;
    Index *index = NULL;
    Row *row = NULL;
    Row *short_row = NULL;

    uint32_t columns[] = {0};

    ASSERT(create_test_pager(&test_pager));

    schema = create_test_schema();
    ASSERT(schema);

    index = create_test_index_with(test_pager.pager, "test_index", PRIMARY_INDEX, columns, 1, true);
    ASSERT(index);

    row = create_test_row(1, 10, 100);
    ASSERT(row);

    short_row = (Row *) calloc(1, sizeof(Row));
    ASSERT(short_row);
    short_row->n_columns = 1;
    short_row->values = (Value **) calloc(1, sizeof(Value *));
    ASSERT(short_row->values);
    short_row->values[0] = value_create(INTEGER, &(int32_t){1});
    ASSERT(short_row->values[0]);

    ASSERT(index_insert_entry(NULL, test_pager.pager, schema, row) == INDEX_MUTATION_INVALID_ARGUMENTS);
    ASSERT(index_delete_entry(index, NULL, schema, row) == INDEX_MUTATION_INVALID_ARGUMENTS);
    ASSERT(index_update_entry(index, test_pager.pager, schema,
        row, short_row) == INDEX_MUTATION_INVALID_ARGUMENTS);

    row->is_deleted = true;
    ASSERT(index_insert_entry(index, test_pager.pager, schema, row) == INDEX_MUTATION_INVALID_ARGUMENTS);

    result = 0;

cleanup:
    if (row) { row_free(row); }
    if (short_row) { row_free(short_row); }
    if (index) { index_free(index); }
    if (schema) { schema_free(schema); }
    destroy_test_pager(&test_pager);

    return result;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}


int main(int argc, char *argv[]) {
    int result;

    /* ---------- index_create unit tests ---------- */
    result = test_index_create_success();
    generate_output(result, 0, "test_index_create_success");
    result = test_index_create_initialize_empty_root();
    generate_output(result, 1, "test_index_create_initialize_empty_root");
    result = test_index_create_invalid_arguments();
    generate_output(result, 2, "test_index_create_invalid_arguments");
    result = test_index_create_reuses_released_pages();
    generate_output(result, 3, "test_index_create_reuses_released_pages");

    /* ---------- index_trunate unit tests ---------- */
    result = test_index_truncate_empty_tree();
    generate_output(result, 4, "test_index_truncate_empty_tree");
    result = test_index_truncate_multi_page_tree();
    generate_output(result, 5, "test_index_truncate_multi_page_tree");
    result = test_index_truncate_preserves_metadata();
    generate_output(result, 6, "test_index_truncate_preserves_metadata");
    result = test_index_truncate_invalid_arguments();
    generate_output(result, 7, "test_index_truncate_invalid_arguments");

    /* ---------- index_drop unit tests ---------- */
    result = test_index_drop_single_page_tree();
    generate_output(result, 8, "test_index_drop_single_page_tree");
    result = test_index_drop_multi_page_tree();
    generate_output(result, 9, "test_index_drop_multi_page_tree");
    result = test_index_drop_invalid_arguments();
    generate_output(result, 10, "test_index_drop_invalid_arguments");


    /* ---------- index_find_exact unit tests ---------- */
    result = test_index_find_exact_full_key();
    generate_output(result, 11, "test_index_find_exact_full_key");
    result = test_index_find_exact_missing_key();
    generate_output(result, 12, "test_index_find_exact_missing_key");

    /* ---------- index_find_prefix unit tests ---------- */
    result = test_index_find_prefix_composite_key();
    generate_output(result, 13, "test_index_find_prefix_composite_key");

    /* ---------- index_find_range unit tests ---------- */
    result = test_index_find_range_full_key();
    generate_output(result, 14, "test_index_find_range_full_key");
    result = test_index_find_range_composite_prefix();
    generate_output(result, 15, "test_index_find_range_composite_prefix");
    result = test_index_find_range_boundaries();
    generate_output(result, 16, "test_index_find_range_boundaries");

    /* ---------- index_scan unit tests ---------- */
    result = test_index_scan_key_order();
    generate_output(result, 17, "test_index_scan_key_order");
    result = test_index_lookup_invalid_key_definitions();
    generate_output(result, 18, "test_index_lookup_invalid_key_definitions");

    /* ---------- index_insert_entry unit tests ---------- */
    result = test_index_insert_entry_success();
    generate_output(result, 19, "test_index_insert_entry_success");
    result = test_index_insert_duplicate_unique_key();
    generate_output(result, 20, "test_index_insert_duplicate_unique_key");
    result = test_index_insert_duplicate_unique_secondary_key();
    generate_output(result, 21, "test_index_insert_duplicate_unique_secondary_key");
    result = test_index_insert_duplicate_secondary_key();
    generate_output(result, 22, "test_index_insert_duplicate_secondary_key");
    result = test_index_root_metadata_after_root_split();
    generate_output(result, 23, "test_index_root_metadata_after_root_split");
    result = test_index_insert_across_internal_root_split();
    generate_output(result, 24, "test_index_insert_across_internal_root_split");

    /* ---------- index_delete_entry unit tests ---------- */
    result = test_index_delete_entry_success();
    generate_output(result, 25, "test_index_delete_entry_success");
    result = test_index_delete_duplicate_secondary_entry();
    generate_output(result, 26, "test_index_delete_duplicate_secondary_entry");
    result = test_index_delete_entry_not_found();
    generate_output(result, 27, "test_index_delete_entry_not_found");
    result = test_index_root_metadata_after_root_collapse();
    generate_output(result, 28, "test_index_root_metadata_after_root_collapse");

    /* ---------- index_update_entry unit tests ---------- */
    result = test_index_update_entry_same_key();
    generate_output(result, 29, "test_index_update_entry_same_key");
    result = test_index_update_entry_changed_key();
    generate_output(result, 30, "test_index_update_entry_changed_key");
    result = test_index_update_entry_equal_row();
    generate_output(result, 31, "test_index_update_entry_equal_row");
    result = test_index_update_duplicate_unique_key();
    generate_output(result, 32, "test_index_update_duplicate_unique_key");
    result = test_index_update_entry_not_found();
    generate_output(result, 33, "test_index_update_entry_not_found");
    result = test_index_mutation_invalid_arguments();
    generate_output(result, 34, "test_index_mutation_invalid_arguments");


    return 0;
}