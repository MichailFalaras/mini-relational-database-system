#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/btree.h"
#include "../src/btree/btree_utils.h"
#include "../include/page.h"
#include "../include/index.h"
#include "../include/row.h"
#include "../src/data_types/data_types_utils.h"
#include "../include/serialize.h"
#include "../include/schema.h"

#define ASSERT(condition) { \
    if (!(condition)) { \
        return 1; \
    } \
}

/* ---------- Initialization Helpers ---------- */

// Temporary Test BTree Struct
typedef struct test_btree {
    BTree *btree;
    uint8_t levels;
    uint32_t page_count;
    uint32_t *page_nums;
} TestBTree;

// Pager initialization
bool pager_open_and_create_new_page(const char *pathname, Pager **pager, Page **page) {
    *pager = pager_open(pathname);
    if (!(*pager)) {
        return false;
    }

    uint32_t page_num = 0;    
    if (!pager_allocate_page(*pager, &page_num)) {
        return false;
    }

    *page = pager_get_page(*pager, page_num);
    if (!(*page)) {
        return false;
    }

    return true;
}

bool pager_open_and_get_page_in_cache(const char *pathname, Pager **pager, Page **page, uint32_t page_num) {
    *pager = pager_open(pathname);
    if (!(*pager)) {
        return false;
    }

    *page = pager_get_page(*pager, page_num);
    if (!(*page)) {
        return false;
    }

    return true;
}

// Index initialization
Index *index_init(Pager *pager, const char *name, IndexType type,
    const uint32_t *key_columns, uint32_t amount_of_key_columns) {
    if (!name || !key_columns) {
        return NULL;
    }

    IndexKey *index_key = index_key_create(key_columns, amount_of_key_columns);
    if (!index_key) {
        return NULL;
    }

    Index *index = index_create(name, type, index_key, pager);
    index_key_free(index_key);

    return index;
}

bool index_spec_init(BTreeIndexSpec *index_spec, Index *index) {
    index_spec->index_key = index->key;
    index_spec->is_unique = false;
    index_spec->schema = (Schema *) calloc(1, sizeof(Schema));
    if (!index_spec->schema) {
        return false;
    }

    index_spec->schema->num_columns = 3;
    index_spec->key_size = index_spec->index_key->num_columns * get_data_type_size(UNSIGNED_INTEGER);
    index_spec->schema->columns = (Column **) calloc(index_spec->schema->num_columns, sizeof(Column *));
    if (!index_spec->schema->columns) {
        return false;
    }

    index_spec->column_types = (DataType *) calloc(index->key->num_columns, sizeof(DataType));
    if (!index_spec->column_types) {
        return false;
    }

    for (uint32_t i = 0; i < index_spec->schema->num_columns; i++) {
        index_spec->schema->columns[i] = (Column *) calloc(1, sizeof(Column));
        if (!index_spec->schema->columns[i]) {
            return false;
        }

        index_spec->column_types[i] = UNSIGNED_INTEGER;
        strcpy(index_spec->schema->columns[i]->name, "Col");
        index_spec->schema->columns[i]->non_null_rows = 123;
        index_spec->schema->columns[i]->null_rows = 0;
        index_spec->schema->columns[i]->type = UNSIGNED_INTEGER;
    }

    return true;
}

// Cell Contents initialization
bool cell_contents_init(BTreeCellContents *cell_contents) {
    cell_contents->BTreePayload.row = (Row *) calloc(1, sizeof(Row));
    if (!cell_contents->BTreePayload.row) {
        return false;
    }
    
    cell_contents->type = BTREE_LEAF_NODE;
    cell_contents->num_keys = 3;
    cell_contents->keys = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));
    if (!cell_contents->keys) {
        return false;
    }

    cell_contents->key_size = 3 * get_data_type_size(UNSIGNED_INTEGER);
    cell_contents->BTreePayload.row->is_deleted = false;
    cell_contents->BTreePayload.row->n_columns = 3;
    cell_contents->cell_size = cell_contents->key_size + sizeof(uint8_t) + sizeof(uint32_t) + 3*get_data_type_size(UNSIGNED_INTEGER); 

    cell_contents->BTreePayload.row->values = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));
    if (!cell_contents->BTreePayload.row->values) {
        return false;
    }

    return true;
}

// Search Key initialization
bool search_key_init(BTreeSearchKey *search_key, BTreeIndexSpec *index_spec, Value ***target_key_vals) {
    
    *target_key_vals = (Value **) calloc(index_spec->index_key->num_columns, sizeof(Value *));
    if (!(*target_key_vals)) {
        return false;
    }

    search_key->index = index_spec;
    search_key->num_target_keys = index_spec->index_key->num_columns;
    search_key->target_key = (void *) *target_key_vals; 

    return true;
}

// Test BTree initialization helpers
void connect_sibling_nodes(TestBTree *test_btree) {

    uint32_t leaf_node_beg = (1 << (test_btree->levels-1)) - 1;

    BTreePage btree_page = {0};
    for (uint32_t i = leaf_node_beg; i < test_btree->page_count; i++) {
        btree_page_attach(&btree_page, test_btree->btree->pager->pages[test_btree->page_nums[i]]);
        btree_page_load(&btree_page);

        if (i == leaf_node_beg) {
            btree_page.type_specific_data.siblings.previous_leaf_pointer = UINT32_MAX;
        } else {
            btree_page.type_specific_data.siblings.previous_leaf_pointer = test_btree->page_nums[i-1];
        }
        
        if (i == test_btree->page_count-1) {
            btree_page.type_specific_data.siblings.next_leaf_pointer = UINT32_MAX;
        } else {
            btree_page.type_specific_data.siblings.next_leaf_pointer = test_btree->page_nums[i+1];
        }

        btree_page_sync(test_btree->btree->pager, &btree_page);
    }

}

bool populate_leaf_nodes(TestBTree *test_btree, uint8_t *page_data, uint32_t page_index,  BTreeIndexSpec *index_spec) {
    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, test_btree->btree->pager->pages[test_btree->page_nums[page_index]]);
    btree_page_load(&btree_page);

    BTreeCellContents cell_contents = {0};
    cell_contents.type = BTREE_LEAF_NODE;
    cell_contents.num_keys = 3;
    cell_contents.keys = (Value **) calloc(cell_contents.num_keys, sizeof(Value *));
    if (!cell_contents.keys) {
        return false;
    }

    cell_contents.BTreePayload.row = (Row *) calloc(1, sizeof(Row));
    if (!cell_contents.BTreePayload.row) {
        return false;
    }
    cell_contents.BTreePayload.row->is_deleted = false;
    cell_contents.BTreePayload.row->n_columns = 3;
    cell_contents.BTreePayload.row->values = (Value **) calloc(cell_contents.BTreePayload.row->n_columns, sizeof(Value *)); 
    if (!cell_contents.BTreePayload.row->values) {
        return false;
    }

    cell_contents.cell_size = index_spec->key_size + sizeof(uint8_t) + sizeof(uint32_t) +
                cell_contents.BTreePayload.row->n_columns * get_data_type_size(UNSIGNED_INTEGER);

    /* 3 cells created for each page. */
    for (uint32_t k = 0; k < 3; k++) {
        for (uint32_t j = 0; j < cell_contents.num_keys; j++) {
            /* If it already exists, free it.
                * Shouldn't exist the first time entering the loop. */
            if (cell_contents.keys[j]) {
                value_free(cell_contents.keys[j]);
            }

            if (cell_contents.BTreePayload.row->values[j]) {
                value_free(cell_contents.BTreePayload.row->values[j]);
            }

            uint32_t key_val = page_index*10 + 10*j + 5*k;
            cell_contents.keys[j] = value_create(UNSIGNED_INTEGER, &key_val);
            cell_contents.BTreePayload.row->values[j] = value_copy(cell_contents.keys[j]);
        }

        btree_page.cell_count++;
        btree_page.free_space_offset -= cell_contents.cell_size;

        set_cell_pointer(page_data, k, make_cell_pointer(btree_page.free_space_offset, cell_contents.cell_size));
        serialize_cell_contents(btree_page.data + btree_page.free_space_offset, &btree_page, &cell_contents);
    }

    value_free_array(cell_contents.keys, cell_contents.num_keys);
    row_free(cell_contents.BTreePayload.row);
    btree_page_sync(test_btree->btree->pager, &btree_page);

    return true;
}

bool populate_internal_nodes(TestBTree *test_btree, uint8_t *page_data, uint32_t page_index, BTreeIndexSpec *index_spec) {
    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, test_btree->btree->pager->pages[test_btree->page_nums[page_index]]);
    btree_page_load(&btree_page);

    BTreeCellContents cell_contents = {0};
    cell_contents.type = BTREE_INTERNAL_NODE;
    cell_contents.num_keys = 3;
    
    if (2*page_index + 1 >= test_btree->page_count || 2*page_index + 2 >= test_btree->page_count) {
        return false;
    }
    cell_contents.BTreePayload.child_pointer = test_btree->page_nums[2*page_index + 1];
    cell_contents.cell_size = cell_contents.num_keys*get_data_type_size(UNSIGNED_INTEGER) + sizeof(uint32_t);

    btree_page.free_space_offset -= cell_contents.cell_size;
    btree_page.cell_count++;
    btree_page.type_specific_data.rightmost_child_pointer = test_btree->page_nums[2*page_index + 2];
    BTreePage right_child = {0};
    btree_page_attach(&right_child, test_btree->btree->pager->pages[test_btree->page_nums[2*page_index + 2]]);
    btree_page_load(&right_child);

    BTreeKeyView key_view = {0};
    cell_contents.keys = (Value **) calloc(cell_contents.num_keys, sizeof(Value *));
    if (!cell_contents.keys) {
        return false;
    }

    get_key(&right_child, 0, &key_view, index_spec);
    uint8_t *offset = (uint8_t *) key_view.key;
    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        cell_contents.keys[i] = deserialize_value_data(UNSIGNED_INTEGER, (void *)offset);
        offset += get_data_type_size(UNSIGNED_INTEGER);
    }
    
    set_cell_pointer(page_data, 0, make_cell_pointer(btree_page.free_space_offset, cell_contents.cell_size));
    serialize_cell_contents(btree_page.data + btree_page.free_space_offset, &btree_page, &cell_contents);

    value_free_array(cell_contents.keys, cell_contents.num_keys);
    btree_page_sync(test_btree->btree->pager, &btree_page);

    return true;
}

bool give_keys_and_connect(TestBTree *test_btree, Index *index, uint8_t levels) {
    
    BTreeIndexSpec index_spec = {0};
    index_spec.key_size = 3*get_data_type_size(UNSIGNED_INTEGER);

    for (int32_t i = test_btree->page_count-1; i >= 0; i--) {
        uint8_t *page_data = test_btree->btree->pager->pages[test_btree->page_nums[i]]->page_data; 
        if (get_node_type(page_data) == BTREE_LEAF_NODE) {
            if (!populate_leaf_nodes(test_btree, page_data, i,  &index_spec)) {
                return false;
            }
        } else {
            if (!populate_internal_nodes(test_btree, page_data, i, &index_spec)) {
                return false;
            }
        }
    }
    
    if (test_btree->page_count != 1) {
        connect_sibling_nodes(test_btree);
    }

    return true;
}

bool build_test_btree(TestBTree *test_btree, Index *index, uint8_t levels) {
    if (!test_btree || !index || levels == 0) {
        return false;
    }

    uint32_t page_count = (1 << levels) - 1;

    test_btree->btree->root_page_num = index->root_page_num;
    test_btree->levels = levels;
    test_btree->page_count = page_count;
    test_btree->page_nums = calloc(page_count, sizeof(uint32_t));
    if (!test_btree->page_nums) {
        return false;
    }

    /* Allocate every page except the root (already exists). */
    for (uint32_t i = 1; i < page_count; i++) {
        if (!pager_allocate_page(test_btree->btree->pager,
                                 &test_btree->page_nums[i])) {
            return false;
        }
    }

    /* Root page number already comes from the index. */
    test_btree->page_nums[0] = index->root_page_num;

    /* Initialize pages. */
    for (uint32_t i = 0; i < page_count; i++) {

        Page *page = pager_get_page(test_btree->btree->pager,
                                    test_btree->page_nums[i]);
        if (!page) {
            return false;
        }

        if (!page_clear(test_btree->btree->pager, page)) {
            return false;
        }

        BTreePage btree_page = {0};
        btree_page_attach(&btree_page, page);

        /* Last level -> leaves. */
        bool leaf = (i >= ((1u << (levels - 1)) - 1));

        BTreeStatus status;
        if (leaf) {
            status = btree_page_init_empty_leaf(&btree_page);
        } else {
            status = btree_page_init_internal(&btree_page, UINT32_MAX);
        }

        if (status != BTREE_SUCCESS) {
            return false;
        }

        /* Root metadata. */
        if (i == 0) {
            btree_page.is_root = 1;
            btree_page.parent_pointer = UINT32_MAX;
        } else {
            btree_page.is_root = 0;

            uint32_t parent_index = (i - 1) / 2;
            btree_page.parent_pointer = test_btree->page_nums[parent_index];
        }

        btree_page_sync(test_btree->btree->pager, &btree_page);
    }

    if (!give_keys_and_connect(test_btree, index, levels)) {
        return false;
    }

    return true;
}

void test_btree_free(TestBTree *test_btree, Index **index, Pager **pager) {
    free(test_btree->page_nums);
    test_btree->page_nums = NULL;

    free(test_btree->btree);
    test_btree->btree = NULL;

    index_free(*index);
    *index = NULL;

    pager_close(*pager);
    *pager = NULL;
}

/* Initializes Pager, Index, TestBtree and creates a B+Tree.
 * B+Tree contains as many nodes as supported in level given.
 *
 * » All B+Tree Keys are Composite Keys containing 3 UNSIGNED_INTEGERs (columns).
 * » B+Tree Internal Nodes contain only one key each and rightmost child pointer
 * for traversing. (Keys usually promoted upwards but NOT REMOVED from original internal node).
 * » B+Tree Leaf Nodes contain 3 cell pointers and 3 cells containing THE SAME VALUES for
 * keys and row->values. */
bool test_btree_full_init(TestBTree *test_btree, Pager **pager, Index **index, uint8_t levels, char *pathname) {
    // only if it didn't already exist
    if (!(*pager)) {
        *pager = pager_open(pathname);
        if (!(*pager)) {
            return false;
        }
    }
    
    if (!(*index)) {
        uint32_t amount_of_key_columns = 3;
        uint32_t *key_columns = (uint32_t *) malloc(3*sizeof(uint32_t));
        key_columns[0] = 0;
        key_columns[1] = 1;
        key_columns[2] = 2;
        
        *index = index_init(*pager, "TestIndex", SECONDARY_INDEX, key_columns, amount_of_key_columns);
        if (!(*index)) {
            return false;
        }
    }
    
    if (!(test_btree->btree)) {
        test_btree->btree = (BTree *) calloc(1, sizeof(BTree));
        test_btree->btree->pager = *pager;
    }
    
    if (!build_test_btree(test_btree, *index, levels)) {
        return false;
    }

    return true;
}

/* ---------- Unit Tests ---------- */

/* Initialization of BTreePage always requires more than just btree_page_init_empty_leaf.
 * btree_page_init_empty_leaf only initializes the header metadata. Anything about the page
 * stored in a btree_page requires further checking. */
static int test_empty_leaf_init() {
    BTreePage btree_page = {0};
    Pager *pager; Page *page;
    ASSERT(pager_open_and_create_new_page("build/database1.db", &pager, &page));
    
    btree_page_attach(&btree_page, page);
    ASSERT(btree_page.page == page);
    ASSERT(btree_page.data == page->page_data);

    BTreeStatus status = btree_page_init_empty_leaf(&btree_page);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.type == BTREE_LEAF_NODE);
    ASSERT(btree_page.is_root == 1);
    ASSERT(btree_page.parent_pointer == UINT32_MAX);
    ASSERT(btree_page.cell_count == 0);
    ASSERT(btree_page.free_space_offset == PAGE_SIZE);
    ASSERT(btree_page.type_specific_data.siblings.previous_leaf_pointer == UINT32_MAX);
    ASSERT(btree_page.type_specific_data.siblings.next_leaf_pointer == UINT32_MAX);

    btree_page_sync(pager, &btree_page);
    ASSERT(btree_page.page->is_dirty == true);

    bool res = pager_close(pager);
    ASSERT(res == true);
    
    return 0;
}

/* So does btree_page_init_internal, only initializes header metadata and rightmost pointer. */
static int test_init_internal() {
    BTreePage btree_page = {0};
    Pager *pager; Page *page;
    ASSERT(pager_open_and_create_new_page("build/database2.db", &pager, &page));

    btree_page_attach(&btree_page, page);
    ASSERT(btree_page.page == page);
    ASSERT(btree_page.data == page->page_data);

    BTreeStatus status = btree_page_init_internal(&btree_page, UINT32_MAX);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.type == BTREE_INTERNAL_NODE);
    ASSERT(btree_page.is_root == 1);
    ASSERT(btree_page.parent_pointer == UINT32_MAX);
    ASSERT(btree_page.cell_count == 0);
    ASSERT(btree_page.free_space_offset == PAGE_SIZE);
    ASSERT(btree_page.type_specific_data.rightmost_child_pointer == UINT32_MAX);

    btree_page_sync(pager, &btree_page);
    ASSERT(btree_page.page->is_dirty == true);

    bool res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Empty Root Lower Bound search. */
static int test_lower_bound_search_empty_root() {
    BTreePage btree_page = {0};
    Pager *pager; Page *page;
    // Already existing page empty root leaf node created in TEST[00]
    ASSERT(pager_open_and_get_page_in_cache("build/database1.db", &pager, &page, 2));

    // Don't actually need to initialize these for this test
    BTreeSearchKey search_key = {0}; 
    BTreeSearchResult search_result = {0};
    BTreeIndexSpec index_spec = {0};

    BTreeStatus status = btree_page_attach_load_validate(pager, &btree_page, page, &index_spec);
    ASSERT(status == BTREE_SUCCESS);

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == btree_page.page);
    ASSERT(search_result.result_index == 0);

    bool res = pager_close(pager);
    ASSERT(res == true);
    return 0;
}

/* Lower Bound Search ran on a leaf node of a 2-level test B+Tree.
 * Search for cases with result_index being at the beginning, in the middle,
 * at the end and an exact match of keys.  */
static int test_lower_bound_search() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 2;
    char *pathname = "build/database3.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    /* ---- SEARCH BEFORE THE FIRST KEY: ---- */
    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeSearchKey search_key = {0};
    Value **target_key_vals = NULL;
    ASSERT(search_key_init(&search_key, &index_spec, &target_key_vals));

    for (uint32_t i = 0; i < search_key.num_target_keys; i++) {
        index_spec.column_types[i] = UNSIGNED_INTEGER;
        
        uint32_t val = i * 9;
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    BTreeSearchResult search_result = {0};
    search_result.page = pager->pages[test_btree.page_nums[1]]; // First leaf node 

    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, pager->pages[test_btree.page_nums[1]]);
    btree_page_load(&btree_page);

    BTreeStatus status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 0);

    /* ---- SEARCH AFTER THE LAST KEY: ---- */
    for (uint32_t i = 0; i < search_key.num_target_keys; i++) {        
        uint32_t val = i + 21;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 3);

    /* ---- SEARCH FOR AN EXACT MATCH: ---- */
    for (uint32_t i = 0; i < search_key.num_target_keys; i++) {        
        uint32_t val = i*10 + 10;
        
        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);

    /* ---- SEARCH BETWEEN KEYS: ---- */
    for (uint32_t i = 0; i < search_key.num_target_keys; i++) {        
        uint32_t val = i*10 + 11;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 1);

    value_free_array(target_key_vals, index->key->num_columns);
    test_btree_free(&test_btree, &index, &pager);
    return 0;
}

/* Test multi-level root-to-leaf traversal. */
static int test_root_to_leaf() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 1;
    char *pathname = "build/database4_1.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeSearchKey search_key = {0};
    Value **target_key_vals = NULL;
    ASSERT(search_key_init(&search_key, &index_spec, &target_key_vals));

    BTreeSearchResult search_result = {0};

    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        uint32_t val = i * 10;

        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    /* ---- ROOT-TO-LEAF WITH ROOT ONLY: ---- */
    for (uint32_t i = 0; i < search_key.num_target_keys; i++) {
        index_spec.column_types[i] = UNSIGNED_INTEGER;
        
        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        uint32_t val = i * 9;
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    BTreeStatus status = btree_root_to_leaf(test_btree.btree, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == pager->pages[test_btree.page_nums[0]]);
    ASSERT(search_result.result_index == 0);

    /* ---- TWO-LEVEL TREE TRAVERSAL: ---- */
    test_btree_free(&test_btree, &index, &pager);
    pathname = "build/database4_2.db";
    levels = 2;
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));
    index_spec.index_key = index->key;

    status = btree_root_to_leaf(test_btree.btree, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == pager->pages[test_btree.page_nums[1]]);
    ASSERT(search_result.result_index == 0);

    /* ---- THREE LEVEL TREE & TRAVERSE THROUGH RIGHTMOST CHILD: ----*/
    test_btree_free(&test_btree, &index, &pager);
    pathname = "build/database4_3.db";
    levels = 3;
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));
    index_spec.index_key = index->key;

    for (uint32_t i = 0; i < search_key.num_target_keys; i++) {        
        uint32_t val = i*10 + 61;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }

    status = btree_root_to_leaf(test_btree.btree, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == pager->pages[test_btree.page_nums[6]]);
    ASSERT(search_result.result_index == 1);

    value_free_array(target_key_vals, index->key->num_columns);
    test_btree_free(&test_btree, &index, &pager);
    return 0;
}

/* Test leaf node insertion. 
 * Cases:
 * • Insert key into empty page
 * • Insert a UNIQUE, NOT DUPLICATE key in the beginning
 * • Insert key in the middle
 * • Insert UNIQUE, DUPLICATE key & cell
 * • Insert accepted NON-UNIQUE, DUPLICATE key & cell
 * • Insert key at the end
 * • Insert until page gets full
 * • Insert UNIQUE, DUPLICATE key after the page gets full*/
static int test_node_insert() {
    Pager *pager = NULL;
    Index *index = NULL;
    Page *page = NULL;
    ASSERT(pager_open_and_create_new_page("build/database5.db", &pager, &page));
    
    uint32_t amount_of_key_columns = 3;
    uint32_t *key_columns = (uint32_t *) malloc(3*sizeof(uint32_t));
    key_columns[0] = 0;
    key_columns[1] = 1;
    key_columns[2] = 2;
    index = index_init(pager, "index", SECONDARY_INDEX, key_columns, amount_of_key_columns);

    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, page);
    btree_page_init_empty_leaf(&btree_page);
    btree_page_sync(pager, &btree_page);

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeCellContents cell_contents = {0};
    ASSERT(cell_contents_init(&cell_contents));
    
    BTreeSearchKey search_key = {0};
    Value **target_key_vals = NULL;
    ASSERT(search_key_init(&search_key, &index_spec, &target_key_vals));

    BTreeSplitResult split_result = {0}; 
    BTreeSearchResult search_result = {0};

    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 10;
        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        target_key_vals[i] = cell_contents.keys[i];
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
    }
    
    /* ---- INSERT INTO EMPTY: ---- */
    uint16_t old_free_space_offset = btree_page.free_space_offset;
    BTreeStatus status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(btree_page.cell_count == 1);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);

    /* ---- INSERT UNIQUE (NOT DUPLICATE) IN BEGINNING: ---- */
    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 8;

        if (cell_contents.keys[i]) {
            value_free(cell_contents.keys[i]);
        }
        
        if (cell_contents.BTreePayload.row->values[i]) {
            value_free(cell_contents.BTreePayload.row->values[i]);
        }
        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        target_key_vals[i] = cell_contents.keys[i];
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
    }

    old_free_space_offset = btree_page.free_space_offset;

    index_spec.is_unique = true;
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    index_spec.is_unique = false;
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.cell_count == 2);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);

    /* ---- INSERT INTO MIDDLE: ---- */
    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 9;

        if (cell_contents.keys[i]) {
            value_free(cell_contents.keys[i]);
        } 
        if (cell_contents.BTreePayload.row->values[i]) {
            value_free(cell_contents.BTreePayload.row->values[i]);
        }
        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
        target_key_vals[i] = cell_contents.keys[i];
    }

    old_free_space_offset = btree_page.free_space_offset;

    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.cell_count == 3);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 1);

    /* ---- INSERT ALREADY INSERTED KEY FOR DUPLICATE CHECK: ---- */
    index_spec.is_unique = true;
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_DUPLICATE_KEY);
    index_spec.is_unique = false;

    /* ---- INSERT DUPLICATE KEY WITHOUT is_unique FLAG: ---- */
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);

    /* ---- INSERT AT THE END: ---- */
    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 15;
        
        if (cell_contents.keys[i]) {
            value_free(cell_contents.keys[i]);
        }
        if (cell_contents.BTreePayload.row->values[i]) {
            value_free(cell_contents.BTreePayload.row->values[i]);
        }
        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
        target_key_vals[i] = cell_contents.keys[i];
    }

    old_free_space_offset = btree_page.free_space_offset;
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.cell_count == 5);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_lower_bound_search(&btree_page, &search_key, &search_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 4);

    /* ---- INSERT UNTIL FULL: (upper limit was found to be 123) ---- */
    for (uint32_t i = 0; i < 123; i++) {
        for (uint32_t j = 0; j < cell_contents.num_keys; j++) {
            uint32_t val = j * 20 + i * 5;
   
            if (cell_contents.keys[j]) {
                value_free(cell_contents.keys[j]);
            } 
            if (cell_contents.BTreePayload.row->values[j]) {
                value_free(cell_contents.BTreePayload.row->values[j]);
            }
            cell_contents.keys[j] = value_create(UNSIGNED_INTEGER, (void *) &val);
            cell_contents.BTreePayload.row->values[j] = value_copy(cell_contents.keys[j]);

        }

        status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    }
    ASSERT(status == BTREE_NEEDS_SPLIT);
    ASSERT(split_result.split == true);


    /* ---- DUPLICATE KEY INSERTION AFTER FULL LEAF NODE ---- */
    index_spec.is_unique = true;
    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 10;

        if (cell_contents.keys[i]) {
            value_free(cell_contents.keys[i]);
        } 
        if (cell_contents.BTreePayload.row->values[i]) {
            value_free(cell_contents.BTreePayload.row->values[i]);
        }
        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        target_key_vals[i] = cell_contents.keys[i];
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);

    }
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_DUPLICATE_KEY);
    
    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    free(target_key_vals);
    index_free(index);
    pager_close(pager);
    return 0;
}

/* Test if leaf node splitted correctly. */
static int test_leaf_node_split() {
    Pager *pager = NULL;
    Index *index = NULL;
    Page *page = NULL;
    ASSERT(pager_open_and_get_page_in_cache("build/database5.db", &pager, &page, 2));

    uint32_t amount_of_key_columns = 3;
    uint32_t *key_columns = (uint32_t *) malloc(3*sizeof(uint32_t));
    key_columns[0] = 0;
    key_columns[1] = 1;
    key_columns[2] = 2;
    index = index_init(pager, "index", SECONDARY_INDEX, key_columns, amount_of_key_columns);

    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, page);
    btree_page_load(&btree_page);

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeCellContents cell_contents = {0};
    ASSERT(cell_contents_init(&cell_contents));    

    Value **target_key_vals = NULL;
    BTreeSearchKey search_key = {0};
    ASSERT(search_key_init(&search_key, &index_spec, &target_key_vals));

    /* Since we are opening a database with one full page. */
    BTreeSplitResult split_result = {0};
    split_result.split = true;

    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 10;

        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        target_key_vals[i] = cell_contents.keys[i];
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
    }
    index_spec.key_size = cell_contents.key_size;
    
    /* Check metadata. */
    uint16_t cell_count = btree_page.cell_count;
    BTreeStatus status = btree_leaf_node_split(pager, &btree_page, &index_spec, &split_result);
    ASSERT(status == BTREE_SUCCESS);

    BTreePage right_page = {0};
    status = btree_page_attach_load_validate(pager, &right_page, pager->pages[5], &index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(split_result.right_page == pager->pages[5]->page_num);
    ASSERT(btree_page.cell_count == (cell_count / 2));
    ASSERT(right_page.cell_count ==  cell_count - (cell_count / 2));
    ASSERT(btree_page.free_space_offset == PAGE_SIZE - (btree_page.cell_count * 29));
    ASSERT(right_page.free_space_offset == PAGE_SIZE - (right_page.cell_count * 29));
    ASSERT(btree_page.parent_pointer == right_page.parent_pointer);    
    ASSERT(btree_page.type_specific_data.siblings.next_leaf_pointer == 5);
    ASSERT(btree_page.type_specific_data.siblings.previous_leaf_pointer == UINT32_MAX);
    ASSERT(right_page.type_specific_data.siblings.next_leaf_pointer == UINT32_MAX);
    ASSERT(right_page.type_specific_data.siblings.previous_leaf_pointer == 2);

    BTreeKeyView key_view = {0};
    status = get_key(&right_page, 0, &key_view, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    BTreeCellView cell_view = {0};

    /* Check if correct key was moved as separator key to the node above. */
    Value **separator_key = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    ASSERT(separator_key);
    Value *split_result_separator_key = NULL;    
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (separator_key[i]) {
            value_free(separator_key[i]);
        }
        separator_key[i] = value_create(UNSIGNED_INTEGER, key_view.key);
        key_view.key += get_data_type_size(UNSIGNED_INTEGER);

        if (split_result_separator_key) {
            value_free(split_result_separator_key);
        }
        split_result_separator_key = value_create(UNSIGNED_INTEGER, split_result.separator_key);
        int result = 0;
        value_compare(split_result_separator_key, separator_key[i], &result);
        ASSERT(result == 0);

        split_result.separator_key += get_data_type_size(UNSIGNED_INTEGER);
    }
    ASSERT(split_result.separator_size == key_view.key_size);
    ASSERT(split_result.right_page == right_page.page->page_num);
    
    /* Check if original page's keys are all smaller than the first key of the right child. */
    Value **original_page_keys = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    ASSERT(original_page_keys);
    for (uint32_t i = 0; i < btree_page.cell_count; i++) {
        status = get_key(&btree_page, i, &key_view, &index_spec);
        status = get_cell(&btree_page, i, &cell_view, &index_spec);
        ASSERT(status == BTREE_SUCCESS);
        
        for (uint32_t j = 0; j < index_spec.index_key->num_columns; j++) {
            if (original_page_keys[j]) {
                value_free(original_page_keys[j]);
            }
            original_page_keys[j] = value_create(UNSIGNED_INTEGER, key_view.key);
            key_view.key += get_data_type_size(UNSIGNED_INTEGER);

            int result = 0;
            value_compare(original_page_keys[j], separator_key[j], &result);
            ASSERT(result == -1);
        }
    }

    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    value_free_array(original_page_keys, index->key->num_columns);
    value_free_array(separator_key, index->key->num_columns);
    free(target_key_vals);
    index_free(index);
    pager_close(pager);
    return 0;
}

/* Test root node split handling. */
static int test_root_node_split() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 1;
    char *pathname = "build/database6.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, pager->pages[2]);
    btree_page_init_empty_leaf(&btree_page);
    btree_page_sync(pager, &btree_page);

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeSearchKey search_key = {0};
    Value **target_key_vals = NULL;
    ASSERT(search_key_init(&search_key, &index_spec, &target_key_vals));

    BTreeCellContents cell_contents = {0};
    ASSERT(cell_contents_init(&cell_contents));

    BTreeSplitResult split_result = {0};

    BTreeStatus status = BTREE_SUCCESS;
    /* ---- INSERT UNTIL FULL: (upper limit was found to be 131) ---- */
    for (uint32_t i = 0; i < 124; i++) {
        for (uint32_t j = 0; j < cell_contents.num_keys; j++) {
            uint32_t val = j * 20 + i * 5;
   
            if (cell_contents.keys[j]) {
                value_free(cell_contents.keys[j]);
            } 
            if (cell_contents.BTreePayload.row->values[j]) {
                value_free(cell_contents.BTreePayload.row->values[j]);
            }
            cell_contents.keys[j] = value_create(UNSIGNED_INTEGER, (void *) &val);
            cell_contents.BTreePayload.row->values[j] = value_copy(cell_contents.keys[j]);
            target_key_vals[j] = cell_contents.keys[j];
            
        }

        status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);

    }
    ASSERT(status == BTREE_NEEDS_SPLIT);
    ASSERT(split_result.split == true);

    status = btree_leaf_node_split(pager, &btree_page, &index_spec, &split_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(split_result.right_page == pager->pages[3]->page_num);

    ASSERT(pager->pages[4] == NULL); // New root page doesn't exist yet.
    Value **separator_key = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    ASSERT(separator_key);

    uint8_t *separator_offset = (uint8_t *) split_result.separator_key;
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (separator_key[i]) {
            value_free(separator_key[i]);
        }
        separator_key[i] = value_create(index_spec.column_types[i], separator_offset);

        separator_offset += get_data_type_size(index_spec.column_types[i]);
    }

    status = btree_root_split(test_btree.btree, &btree_page, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(pager->pages[4]);

    BTreePage new_root_page = {0};
    btree_page_attach(&new_root_page, pager->pages[4]);
    btree_page_load(&new_root_page);

    uint32_t child_pointer = get_cell_child_pointer(new_root_page.data, get_cell_offset(get_cell_pointer(new_root_page.data, 0))); 
    ASSERT(child_pointer == btree_page.page->page_num);
    ASSERT(new_root_page.type_specific_data.rightmost_child_pointer == split_result.right_page);

    ASSERT(test_btree.btree->root_page_num == new_root_page.page->page_num);
    ASSERT(btree_page.is_root == false);
    ASSERT(btree_page.parent_pointer == new_root_page.page->page_num);
    ASSERT(new_root_page.is_root == true);
    ASSERT(new_root_page.parent_pointer == UINT32_MAX);

    BTreePage right_page = {0};
    btree_page_attach(&right_page, pager->pages[split_result.right_page]);
    btree_page_load(&right_page);
    ASSERT(right_page.is_root == false);
    ASSERT(right_page.parent_pointer == new_root_page.page->page_num);

    BTreeKeyView key_view = {0};
    status = get_key(&new_root_page, 0, &key_view, &index_spec);
    ASSERT(status == BTREE_SUCCESS);

    Value **new_root_keys = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    ASSERT(new_root_keys);

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (new_root_keys[i]) {
            value_free(new_root_keys[i]);
        }
        new_root_keys[i] = value_create(index_spec.column_types[i], key_view.key);

        int result = 0;
        ASSERT(value_compare(new_root_keys[i], separator_key[i], &result));
        ASSERT(result == 0);
        key_view.key += get_data_type_size(index_spec.column_types[i]);
    }

    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    value_free_array(separator_key, index_spec.index_key->num_columns);
    value_free_array(new_root_keys, index_spec.index_key->num_columns);
    free(target_key_vals);
    test_btree_free(&test_btree, &index, &pager);
    return 0;
}

/* Test if all pages were reached in a multi-level B+Tree. */
static int test_reachable_page_traversal() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 1;
    char *pathname = "build/database7_1.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreePageCollection visited_pages = {0};

    /* ---- 1 LEVEL TREE: ---- */
    BTreeStatus status = btree_traverse_reachable_pages(test_btree.btree, &visited_pages);
    ASSERT(status == BTREE_SUCCESS);
    
    for (uint32_t i = 2; i < pager->num_pages; i++) {
        ASSERT(btree_collection_contains(&visited_pages, i));
    }

    ASSERT(visited_pages.page_numbers[0] == 2);
    /* ---- 2 LEVEL TREE: ---- */ 
    test_btree_free(&test_btree, &index, &pager);
    visited_pages.count = 0;
    levels = 2;
    pathname = "build/database7_2.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    status = btree_traverse_reachable_pages(test_btree.btree, &visited_pages);
    ASSERT(status == BTREE_SUCCESS);
    
    for (uint32_t i = 2; i < pager->num_pages; i++) {
        ASSERT(btree_collection_contains(&visited_pages, i));
    }

    ASSERT(visited_pages.page_numbers[0] == 2);
    ASSERT(visited_pages.page_numbers[1] == 3);
    ASSERT(visited_pages.page_numbers[2] == 4);
    /* ---- 3 LEVEL TREE: ---- */
    test_btree_free(&test_btree, &index, &pager);
    visited_pages.count = 0;
    levels = 3;
    pathname = "build/database7_3.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    status = btree_traverse_reachable_pages(test_btree.btree, &visited_pages);
    ASSERT(status == BTREE_SUCCESS);
    
    for (uint32_t i = 2; i < pager->num_pages; i++) {
        ASSERT(btree_collection_contains(&visited_pages, i));
    }

    ASSERT(visited_pages.page_numbers[0] == 2);
    ASSERT(visited_pages.page_numbers[1] == 3);
    ASSERT(visited_pages.page_numbers[2] == 5);
    ASSERT(visited_pages.page_numbers[3] == 6);
    ASSERT(visited_pages.page_numbers[4] == 4);
    ASSERT(visited_pages.page_numbers[5] == 7);
    ASSERT(visited_pages.page_numbers[6] == 8);

    test_btree_free(&test_btree, &index, &pager);
    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;

    result = unlink("build/database1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database4_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database4_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database4_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database5.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database6.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database7_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database7_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database7_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    result = test_empty_leaf_init();
    generate_output(result, 0, "test_empty_leaf_init");
    result = test_init_internal();
    generate_output(result, 1, "test_init_internal");
    result = test_lower_bound_search_empty_root();
    generate_output(result, 2, "test_lower_bound_search_empty_root");
    result = test_lower_bound_search();
    generate_output(result, 3, "test_lower_bound_search");
    result = test_root_to_leaf();
    generate_output(result, 4, "test_root_to_leaf");
    result = test_node_insert();
    generate_output(result, 5, "test_node_insert");
    result = test_leaf_node_split();
    generate_output(result, 6, "test_leaf_node_split");
    result = test_root_node_split();
    generate_output(result, 7, "test_root_node_split");
    result = test_reachable_page_traversal();
    generate_output(result, 8, "test_reachable_page_traversal");
    return 0;
}