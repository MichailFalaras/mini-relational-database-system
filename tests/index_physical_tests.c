#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/index.h"
#include "../include/pager.h"
#include "../include/page.h"
#include "../include/btree.h"
#include "../src/btree/btree_utils.h"

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


/* ---------- index_trunate unit tests ---------- */

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

    index = create_test_index_with(test_pager.pager, "test_index", SECONDARY_INDEX, columns, 2, false);
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

    return 0;
}