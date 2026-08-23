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

    Index *index = index_create(name, type, index_key, pager, false);
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
bool search_key_init(BTreeSearchKey *search_key, BTreeIndexSpec *index_spec, Value **target_key_vals) {
    if (!search_key || !index_spec || !target_key_vals) {
        return false;
    }

    search_key->index = index_spec;
    search_key->num_target_keys = index_spec->index_key->num_columns;
    if (search_key->target_key) {
        free(search_key->target_key);
        search_key->target_key = NULL;
    }
    search_key->target_key = values_to_serialized_key(target_key_vals, index_spec->index_key->num_columns, index_spec); 
    if (!search_key->target_key) {
        return false;
    }

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

            uint32_t key_val = page_index*15 + 10*j + 5*k;
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

void index_spec_free(BTreeIndexSpec *index_spec) {
    if (!index_spec) {
        return;
    }

    if (index_spec->column_types) {
        free(index_spec->column_types);
        index_spec->column_types = NULL;
    }

    if (index_spec->schema) {
        free(index_spec->schema);
        index_spec->schema = NULL;
    }
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

bool verify_page_keys(Page *page, BTreeRangeResult *range_result, BTreeIndexSpec *index_spec, uint32_t *key_counter) {
    if (!page || !range_result || !index_spec) {
        return false;
    }

    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, page);
    btree_page_load(&btree_page);

    BTreeKeyView key_view = {0};
    BTreeStatus status = BTREE_SUCCESS;
    Value **page_keys = (Value **) calloc(index_spec->index_key->num_columns, sizeof(Value *));
    if (!page_keys) {
        return false;
    }

    int result = 0;
    for (uint32_t i = 0; i < btree_page.cell_count; i++) {
        if (*key_counter == range_result->count) {
            break;
        }

        status = get_key(&btree_page, i, &key_view, index_spec);
        if (status != BTREE_SUCCESS) {
            value_free_array(page_keys, index_spec->index_key->num_columns);
            return false;
        }

        bool equal_key = true;
        uint8_t *offset = (uint8_t *) key_view.key;
        for (uint32_t j = 0; j < index_spec->index_key->num_columns; j++) {
            page_keys[j] = value_create(index_spec->column_types[j], (void *) offset);

            offset += get_data_type_size(index_spec->column_types[j]);

            if (!value_compare(range_result->cells[*key_counter].keys[j], page_keys[j], &result)) {
                value_free_array(page_keys, index_spec->index_key->num_columns);
                return false;
            }

            if (result != 0) {
                equal_key = false;
                break;
            }
        }

        if (equal_key) {
            (*key_counter)++;
        }
    }

    value_free_array(page_keys, index_spec->index_key->num_columns);
    return true;
}

/* ---------- Unit Tests ---------- 
 *
 * (NOTE) Tests need to be ran with MAX_PAGES macro
 * equal to over 20 thousand, preferably 25000 pages.*/

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

/* Binary search (with BTREE_LOWER_BOUND) ran on an empty leaf node or 
 * a leaf node of a 2-level test B+Tree.
 *
 * Search for cases with result_index being at the beginning, in the middle,
 * at the end and an exact match of keys.  */
static int test_binary_search() {
    /* ---- SEARCH IN EMPTY ROOT ---- */
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

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == btree_page.page);
    ASSERT(search_result.result_index == 0);

    if (!pager_close(pager)) {
        return -1;
    }
    
    /* ---- SEARCH IN LEAF NODE WITH CONTENTS ---- */
    TestBTree test_btree = {0};
    pager = NULL;
    Index *index = NULL;
    uint8_t levels = 2;
    char *pathname = "build/database3.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    /* ---- SEARCH BEFORE THE FIRST KEY: ---- */
    ASSERT(index_spec_init(&index_spec, index));

    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    }

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        index_spec.column_types[i] = UNSIGNED_INTEGER;
        
        uint32_t val = i * 9;
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    search_result.page = pager->pages[test_btree.page_nums[1]]; // First leaf node 

    btree_page_attach(&btree_page, pager->pages[test_btree.page_nums[1]]);
    btree_page_load(&btree_page);

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 0);

    /* ---- SEARCH AFTER THE LAST KEY: ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {        
        uint32_t val = i + 26;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 3);

    /* ---- SEARCH FOR AN EXACT MATCH: ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {        
        uint32_t val = i*10 + 15;
        
        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);

    /* ---- SEARCH BETWEEN KEYS: ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {        
        uint32_t val = i*10 + 16;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 1);

    index_spec_free(&index_spec);
    value_free_array(target_key_vals, index->key->num_columns);
    test_btree_free(&test_btree, &index, &pager);
    free(search_key.target_key);
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
    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    };

    BTreeSearchResult search_result = {0};

    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        uint32_t val = i * 10;

        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    

    /* ---- ROOT-TO-LEAF WITH ROOT ONLY: ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        index_spec.column_types[i] = UNSIGNED_INTEGER;
        
        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        uint32_t val = i * 9;
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    BTreeStatus status = btree_root_to_leaf(test_btree.btree, &search_key, &search_result, BTREE_UPPER_BOUND);
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

    status = btree_root_to_leaf(test_btree.btree, &search_key, &search_result, BTREE_UPPER_BOUND);
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

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {        
        uint32_t val = i*10 + 91;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    status = btree_root_to_leaf(test_btree.btree, &search_key, &search_result, BTREE_UPPER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == pager->pages[test_btree.page_nums[6]]);
    ASSERT(search_result.result_index == 1);

    index_spec_free(&index_spec);
    value_free_array(target_key_vals, index->key->num_columns);
    test_btree_free(&test_btree, &index, &pager);
    free(search_key.target_key);
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
    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    }

    BTreeSplitResult split_result = {0}; 
    BTreeSearchResult search_result = {0};

    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 10;
        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        target_key_vals[i] = cell_contents.keys[i];
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    /* ---- INSERT INTO EMPTY: ---- */
    uint16_t old_free_space_offset = btree_page.free_space_offset;
    BTreeStatus status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(btree_page.cell_count == 1);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
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
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    old_free_space_offset = btree_page.free_space_offset;

    index_spec.is_unique = true;
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    index_spec.is_unique = false;
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.cell_count == 2);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
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
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    old_free_space_offset = btree_page.free_space_offset;

    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.cell_count == 3);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
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
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    old_free_space_offset = btree_page.free_space_offset;
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page.cell_count == 5);
    ASSERT(btree_page.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    status = btree_binary_search(&btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
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
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));
    status = btree_node_insert(pager, &btree_page, &cell_contents, &split_result, &index_spec);
    ASSERT(status == BTREE_DUPLICATE_KEY);
    
    index_spec_free(&index_spec);
    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    free(target_key_vals);
    index_free(index);
    pager_close(pager);
    free(search_key.target_key);
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

    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    }
    BTreeSearchKey search_key = {0};

    /* Since we are opening a database with one full page. */
    BTreeSplitResult split_result = {0};
    split_result.split = true;

    for (uint32_t i = 0; i < cell_contents.num_keys; i++) {
        uint32_t val = i * 10;

        cell_contents.keys[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
        target_key_vals[i] = cell_contents.keys[i];
        cell_contents.BTreePayload.row->values[i] = value_copy(cell_contents.keys[i]);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

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
    uint8_t *separator_offset = split_result.separator_key; 
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (separator_key[i]) {
            value_free(separator_key[i]);
        }
        separator_key[i] = value_create(UNSIGNED_INTEGER, key_view.key);
        key_view.key += get_data_type_size(UNSIGNED_INTEGER);

        if (split_result_separator_key) {
            value_free(split_result_separator_key);
        }
        split_result_separator_key = value_create(UNSIGNED_INTEGER, separator_offset);
        int result = 0;
        value_compare(split_result_separator_key, separator_key[i], &result);
        ASSERT(result == 0);

        separator_offset += get_data_type_size(UNSIGNED_INTEGER);
    }
    value_free(split_result_separator_key);
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

    index_spec_free(&index_spec);
    split_result_reset(&split_result);
    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    value_free_array(original_page_keys, index->key->num_columns);
    value_free_array(separator_key, index->key->num_columns);
    free(target_key_vals);
    index_free(index);
    pager_close(pager);
    free(search_key.target_key);
    return 0;
}

/* Test internal node splitting. */
static int test_internal_node_split() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 2;
    char *pathname = "build/database12.db";

    ASSERT(test_btree_full_init(
        &test_btree,
        &pager,
        &index,
        levels,
        pathname
    ));

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeCellContents cell_contents = {0};
    ASSERT(cell_contents_init(&cell_contents));

    BTreeSplitResult split_result = {0};

    BTreePage leaf_page = {0};

    /* Start from initial rightmost leaf. */
    BTreeStatus status = btree_page_attach_load_validate(
        pager,
        &leaf_page,
        pager->pages[test_btree.page_nums[2]],
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leaf_page.type == BTREE_LEAF_NODE);

    uint32_t old_root_page_num =
        test_btree.btree->root_page_num;

    uint16_t full_internal_cell_count = 0;
    bool internal_split = false;

    /*
     * Repeatedly fill the rightmost leaf.
     *
     * Each leaf split promotes one separator into the root.
     * Eventually the root internal node becomes full and splits.
     */
    for (uint32_t k = 0; k < 500; k++) {

        status = BTREE_SUCCESS;

        /* Fill current rightmost leaf. */
        for (uint32_t i = 0; i < 121; i++) {

            for (uint32_t j = 0;
                 j < cell_contents.num_keys;
                 j++) {

                if (cell_contents.keys[j]) {
                    value_free(cell_contents.keys[j]);
                }

                if (cell_contents.BTreePayload.row->values[j]) {
                    value_free(
                        cell_contents.BTreePayload.row->values[j]
                    );
                }

                /*
                 * Always increasing values so insertion remains
                 * on the rightmost path of the tree.
                 */
                uint32_t val =
                    100000 +
                    k * 10000 +
                    i * 10 +
                    j;

                cell_contents.keys[j] =
                    value_create(
                        UNSIGNED_INTEGER,
                        (void *) &val
                    );

                if (!cell_contents.keys[j]) {
                    return -1;
                }

                cell_contents.BTreePayload.row->values[j] =
                    value_copy(cell_contents.keys[j]);

                if (!cell_contents.BTreePayload.row->values[j]) {
                    return -1;
                }
            }

            status = btree_node_insert(
                pager,
                &leaf_page,
                &cell_contents,
                &split_result,
                &index_spec
            );
        }

        ASSERT(status == BTREE_NEEDS_SPLIT);
        ASSERT(split_result.split == true);

        /*
         * Keep root cell count before propagation.
         * If root changes below, this was the full internal
         * node that was split.
         */
        Page *root_page = pager_get_page(
            pager,
            test_btree.btree->root_page_num
        );
        if (!root_page) {
            return -1;
        }

        BTreePage root_before = {0};

        status = btree_page_attach_load_validate(
            pager,
            &root_before,
            root_page,
            &index_spec
        );
        ASSERT(status == BTREE_SUCCESS);
        ASSERT(root_before.type == BTREE_INTERNAL_NODE);

        uint32_t root_before_split =
            test_btree.btree->root_page_num;

        uint16_t root_cell_count_before =
            root_before.cell_count;

        BTreeInsertionResult insertion_res = {0};
        insertion_result_reset(&insertion_res);
        /* Propagate leaf split into parent. */
        status = btree_split_propagation(
            test_btree.btree,
            &leaf_page,
            &cell_contents,
            &split_result,
            &index_spec,
            &insertion_res
        );
        ASSERT(status == BTREE_SUCCESS);

        /*
         * Root page number changed =>
         *
         * old internal root split and a new root was created.
         */
        if (test_btree.btree->root_page_num !=
            root_before_split) {

            full_internal_cell_count =
                root_cell_count_before;

            internal_split = true;
            break;
        }

        /*
         * Root hasn't split yet.
         * Follow its rightmost pointer to continue filling
         * the newest rightmost leaf.
         */
        root_page = pager_get_page(
            pager,
            test_btree.btree->root_page_num
        );
        if (!root_page) {
            return -1;
        }

        BTreePage current_root = {0};

        status = btree_page_attach_load_validate(
            pager,
            &current_root,
            root_page,
            &index_spec
        );
        ASSERT(status == BTREE_SUCCESS);

        ASSERT(current_root.type == BTREE_INTERNAL_NODE);
        ASSERT(current_root.is_root == true);

        Page *next_leaf_page = pager_get_page(
            pager,
            current_root
                .type_specific_data
                .rightmost_child_pointer
        );
        if (!next_leaf_page) {
            return -1;
        }

        status = btree_page_attach_load_validate(
            pager,
            &leaf_page,
            next_leaf_page,
            &index_spec
        );
        ASSERT(status == BTREE_SUCCESS);
        ASSERT(leaf_page.type == BTREE_LEAF_NODE);
    }

    /* Internal node must actually have split. */
    ASSERT(internal_split == true);

    /*
     * With the current internal cell layout the root should
     * have reached its real capacity before splitting.
     */
    ASSERT(full_internal_cell_count > 0);

    /* ---------------------------------------------------------
     * VERIFY NEW ROOT
     * --------------------------------------------------------- */

    Page *new_root_page = pager_get_page(
        pager,
        test_btree.btree->root_page_num
    );
    if (!new_root_page) {
        return -1;
    }

    BTreePage new_root = {0};

    status = btree_page_attach_load_validate(
        pager,
        &new_root,
        new_root_page,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(new_root.type == BTREE_INTERNAL_NODE);
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(new_root.cell_count == 1);
    ASSERT(new_root.page->is_dirty == true);

    /*
     *                  new root
     *                 [separator]
     *                  /       \
     *          old root       right internal
     */

    uint32_t root_cell_pointer =
        get_cell_pointer(new_root.data, 0);

    uint32_t left_internal_page_num =
        get_cell_child_pointer(
            new_root.data,
            get_cell_offset(root_cell_pointer)
        );

    uint32_t right_internal_page_num =
        new_root
            .type_specific_data
            .rightmost_child_pointer;

    ASSERT(left_internal_page_num == old_root_page_num);

    ASSERT(
        right_internal_page_num >
        SYSTEM_CATALOG_PAGE_NUM
    );

    ASSERT(
        right_internal_page_num <
        pager->num_pages
    );

    ASSERT(
        left_internal_page_num !=
        right_internal_page_num
    );

    /* ---------------------------------------------------------
     * VERIFY BOTH RESULTING INTERNAL NODES
     * --------------------------------------------------------- */

    Page *left_page = pager_get_page(
        pager,
        left_internal_page_num
    );

    Page *right_page = pager_get_page(
        pager,
        right_internal_page_num
    );

    if (!left_page || !right_page) {
        return -1;
    }

    BTreePage left_internal = {0};
    BTreePage right_internal = {0};

    status = btree_page_attach_load_validate(
        pager,
        &left_internal,
        left_page,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    status = btree_page_attach_load_validate(
        pager,
        &right_internal,
        right_page,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left_internal.type == BTREE_INTERNAL_NODE);
    ASSERT(right_internal.type == BTREE_INTERNAL_NODE);

    ASSERT(left_internal.is_root == false);
    ASSERT(right_internal.is_root == false);

    ASSERT(
        left_internal.parent_pointer ==
        new_root.page->page_num
    );

    ASSERT(
        right_internal.parent_pointer ==
        new_root.page->page_num
    );

    ASSERT(left_internal.cell_count > 0);
    ASSERT(right_internal.cell_count > 0);

    /*
     * Full internal node is split in half.
     *
     * One separator gets promoted but the pending separator
     * from the triggering leaf split is also inserted.
     *
     * Therefore the combined number of separators remains
     * equal to the old full internal node's separator count.
     */
    ASSERT(
        left_internal.cell_count +
        right_internal.cell_count ==
        full_internal_cell_count
    );

    ASSERT(left_internal.page->is_dirty == true);
    ASSERT(right_internal.page->is_dirty == true);

    /* ---------------------------------------------------------
     * VERIFY BOTH INTERNAL NODES HAVE FREE SPACE
     * --------------------------------------------------------- */

    BTreeCellContents internal_cell = {0};

    internal_cell.type = BTREE_INTERNAL_NODE;
    internal_cell.num_keys =
        index_spec.index_key->num_columns;
    internal_cell.key_size =
        index_spec.key_size;
    internal_cell.cell_size =
        internal_cell.key_size +
        sizeof(uint32_t);

    status = btree_page_has_enough_space(
        &left_internal,
        &internal_cell
    );
    ASSERT(status == BTREE_SUCCESS);

    status = btree_page_has_enough_space(
        &right_internal,
        &internal_cell
    );
    ASSERT(status == BTREE_SUCCESS);

    /* ---------------------------------------------------------
     * VERIFY KEY ORDER
     * --------------------------------------------------------- */

    BTreeKeyView key_view = {0};

    status = get_key(
        &new_root,
        0,
        &key_view,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **root_key =
        serialized_key_to_values(
            key_view.key,
            index_spec.index_key->num_columns,
            &index_spec
        );
    if (!root_key) {
        return -1;
    }

    /* Last separator in left internal. */
    status = get_key(
        &left_internal,
        left_internal.cell_count - 1,
        &key_view,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **left_last_key =
        serialized_key_to_values(
            key_view.key,
            index_spec.index_key->num_columns,
            &index_spec
        );
    if (!left_last_key) {
        return -1;
    }

    int result = 0;

    status = btree_compare(
        left_last_key,
        root_key,
        index_spec.index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == -1);

    /* First separator remaining in right internal. */
    status = get_key(
        &right_internal,
        0,
        &key_view,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **right_first_key =
        serialized_key_to_values(
            key_view.key,
            index_spec.index_key->num_columns,
            &index_spec
        );
    if (!right_first_key) {
        return -1;
    }

    status = btree_compare(
        root_key,
        right_first_key,
        index_spec.index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == -1);

    /* ---------------------------------------------------------
     * VERIFY PROMOTED KEY ==
     * MINIMUM KEY OF RIGHT SUBTREE
     * --------------------------------------------------------- */

    uint32_t first_right_cell_pointer =
        get_cell_pointer(
            right_internal.data,
            0
        );

    uint32_t right_leftmost_leaf_num =
        get_cell_child_pointer(
            right_internal.data,
            get_cell_offset(first_right_cell_pointer)
        );

    Page *right_leftmost_leaf_page =
        pager_get_page(
            pager,
            right_leftmost_leaf_num
        );

    if (!right_leftmost_leaf_page) {
        return -1;
    }

    BTreePage right_leftmost_leaf = {0};

    status = btree_page_attach_load_validate(
        pager,
        &right_leftmost_leaf,
        right_leftmost_leaf_page,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(
        right_leftmost_leaf.type ==
        BTREE_LEAF_NODE
    );

    status = get_key(
        &right_leftmost_leaf,
        0,
        &key_view,
        &index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **right_subtree_min =
        serialized_key_to_values(
            key_view.key,
            index_spec.index_key->num_columns,
            &index_spec
        );
    if (!right_subtree_min) {
        return -1;
    }

    status = btree_compare(
        root_key,
        right_subtree_min,
        index_spec.index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == 0);

    /* ---------------------------------------------------------
     * VERIFY ALL CHILD POINTERS + PARENT REFERENCES
     * --------------------------------------------------------- */

    for (uint32_t i = 0;
         i < left_internal.cell_count + 1;
         i++) {

        uint32_t child_pointer = 0;

        if (i == left_internal.cell_count) {
            child_pointer =
                left_internal
                    .type_specific_data
                    .rightmost_child_pointer;
        } else {
            uint32_t cell_pointer =
                get_cell_pointer(
                    left_internal.data,
                    i
                );

            child_pointer =
                get_cell_child_pointer(
                    left_internal.data,
                    get_cell_offset(cell_pointer)
                );
        }

        Page *child_page =
            pager_get_page(
                pager,
                child_pointer
            );

        if (!child_page) {
            return -1;
        }

        BTreePage child = {0};

        status = btree_page_attach_load_validate(
            pager,
            &child,
            child_page,
            &index_spec
        );
        ASSERT(status == BTREE_SUCCESS);

        ASSERT(child.type == BTREE_LEAF_NODE);
        ASSERT(child.is_root == false);

        ASSERT(
            child.parent_pointer ==
            left_internal.page->page_num
        );
    }

    for (uint32_t i = 0;
         i < right_internal.cell_count + 1;
         i++) {

        uint32_t child_pointer = 0;

        if (i == right_internal.cell_count) {
            child_pointer =
                right_internal
                    .type_specific_data
                    .rightmost_child_pointer;
        } else {
            uint32_t cell_pointer =
                get_cell_pointer(
                    right_internal.data,
                    i
                );

            child_pointer =
                get_cell_child_pointer(
                    right_internal.data,
                    get_cell_offset(cell_pointer)
                );
        }

        Page *child_page =
            pager_get_page(
                pager,
                child_pointer
            );

        if (!child_page) {
            return -1;
        }

        BTreePage child = {0};

        status = btree_page_attach_load_validate(
            pager,
            &child,
            child_page,
            &index_spec
        );
        ASSERT(status == BTREE_SUCCESS);

        ASSERT(child.type == BTREE_LEAF_NODE);
        ASSERT(child.is_root == false);

        ASSERT(
            child.parent_pointer ==
            right_internal.page->page_num
        );
    }

    /* ---------------------------------------------------------
     * VERIFY EVERY ALLOCATED BTREE PAGE REMAINS REACHABLE
     * --------------------------------------------------------- */

    BTreePageCollection visited = {0};

    status = btree_traverse_reachable_pages(
        test_btree.btree,
        &visited
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(
        visited.count ==
        pager->num_pages - 2
    );

    /* ---------------------------------------------------------
     * CLEANUP
     *
     * NO close/reopen in the middle of this unit test.
     * test_btree_free() closes the Pager normally at the end.
     * --------------------------------------------------------- */

    value_free_array(
        root_key,
        index->key->num_columns
    );

    value_free_array(
        left_last_key,
        index->key->num_columns
    );

    value_free_array(
        right_first_key,
        index->key->num_columns
    );

    value_free_array(
        right_subtree_min,
        index->key->num_columns
    );

    split_result_reset(&split_result);

    value_free_array(
        cell_contents.keys,
        index->key->num_columns
    );

    value_free_array(
        cell_contents.BTreePayload.row->values,
        index->key->num_columns
    );

    free(cell_contents.BTreePayload.row);

    index_spec_free(&index_spec);

    test_btree_free(
        &test_btree,
        &index,
        &pager
    );

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
    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    }

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
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));
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

    uint32_t right_page_num = split_result.right_page;
    status = btree_root_split(test_btree.btree, &btree_page, &split_result, &index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(pager->pages[4]);

    BTreePage new_root_page = {0};
    btree_page_attach(&new_root_page, pager->pages[4]);
    btree_page_load(&new_root_page);

    uint32_t child_pointer = get_cell_child_pointer(new_root_page.data, get_cell_offset(get_cell_pointer(new_root_page.data, 0))); 
    ASSERT(child_pointer == btree_page.page->page_num);
    ASSERT(new_root_page.type_specific_data.rightmost_child_pointer == right_page_num);

    ASSERT(test_btree.btree->root_page_num == new_root_page.page->page_num);
    ASSERT(btree_page.is_root == false);
    ASSERT(btree_page.parent_pointer == new_root_page.page->page_num);
    ASSERT(new_root_page.is_root == true);
    ASSERT(new_root_page.parent_pointer == UINT32_MAX);

    BTreePage right_page = {0};
    btree_page_attach(&right_page, pager->pages[right_page_num]);
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

    index_spec_free(&index_spec);
    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    value_free_array(separator_key, index_spec.index_key->num_columns);
    value_free_array(new_root_keys, index_spec.index_key->num_columns);
    free(target_key_vals);
    test_btree_free(&test_btree, &index, &pager);
    free(search_key.target_key);
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

/* Test exact key lookup. */
static int test_exact_key_lookup() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 3;
    char *pathname = "build/database8_1.db";
    /* MULTIPLE LEVEL BTREE KEY LOOKUP */
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeSearchKey search_key = {0};
    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    }

    BTreeCellContents cell_contents = {0};
    BTreeSearchResult search_result = {0};

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 45;

        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    /* ---- TEST LOOKUP EXISTING KEY + KEY IN THE BEGINNING ---- */
    BTreeStatus status = btree_find_exact_key(test_btree.btree, &search_key, &search_result, &cell_contents);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.page == test_btree.btree->pager->pages[5]);
    ASSERT(search_result.result_index == 0);

    ASSERT(cell_contents.type == BTREE_LEAF_NODE);
    ASSERT(cell_contents.num_keys == 3);
    
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        int result = 0;
        ASSERT(value_compare(cell_contents.keys[i], target_key_vals[i], &result));
        ASSERT(result == 0);

        ASSERT(value_compare(cell_contents.BTreePayload.row->values[i], target_key_vals[i], &result));
        ASSERT(result == 0);
    }
    ASSERT(cell_contents.key_size == 3 * get_data_type_size(UNSIGNED_INTEGER));
    ASSERT(cell_contents.cell_size == cell_contents.key_size + sizeof(uint8_t) +
         sizeof(uint32_t) + 3*get_data_type_size(UNSIGNED_INTEGER));

    ASSERT(cell_contents.BTreePayload.row->is_deleted == false);
    ASSERT(cell_contents.BTreePayload.row->n_columns == index_spec.index_key->num_columns);
    
    /* ---- TEST LOOKUP EXISTING KEY AT THE END ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 55;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        if (cell_contents.BTreePayload.row->values[i]) {
            value_free(cell_contents.BTreePayload.row->values[i]);
        }
        if (cell_contents.keys[i]) {
            value_free(cell_contents.keys[i]);
        }
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    status = btree_find_exact_key(test_btree.btree, &search_key, &search_result, &cell_contents);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.page == test_btree.btree->pager->pages[5]);
    ASSERT(search_result.result_index == 2);

    ASSERT(cell_contents.type == BTREE_LEAF_NODE);
    ASSERT(cell_contents.num_keys == 3);

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        int result = 0;
        ASSERT(value_compare(cell_contents.keys[i], target_key_vals[i], &result));
        ASSERT(result == 0);

        ASSERT(value_compare(cell_contents.BTreePayload.row->values[i], target_key_vals[i], &result));
        ASSERT(result == 0);
    }

    /* ---- TEST LOOKUP OF MISSING KEY ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 1000;

        if (target_key_vals[i]) {
            value_free(target_key_vals[i]);
        }
        target_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        if (cell_contents.BTreePayload.row->values[i]) {
            value_free(cell_contents.BTreePayload.row->values[i]);
        }
        if (cell_contents.keys[i]) {
            value_free(cell_contents.keys[i]);
        }
    }
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));

    status = btree_find_exact_key(test_btree.btree, &search_key, &search_result, &cell_contents);
    ASSERT(status == BTREE_NOT_FOUND);
    ASSERT(search_result.found == false);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == NULL);
    ASSERT(search_result.result_index == UINT16_MAX);

    /* ---- TEST EMPTY BTREE LOOKUP ---- */
    levels = 1;
    pathname = "build/database8_2.db";
    test_btree_free(&test_btree, &index, &pager);
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));
    index_spec.index_key = index->key;

    BTreePage btree_page = {0};
    status = btree_page_attach_load_validate(pager, &btree_page, pager->pages[2], &index_spec);
    if (status != BTREE_SUCCESS) {
        return -1;
    }

    for (uint32_t i = 0; i < btree_page.cell_count; i++) {
        status = btree_remove_cell(&btree_page, 0);
        if (status != BTREE_SUCCESS) { return -1; }
    } 
    btree_page_sync(pager, &btree_page);

    status = btree_find_exact_key(test_btree.btree, &search_key, &search_result, &cell_contents);
    ASSERT(status == BTREE_NOT_FOUND);
    ASSERT(search_result.found == false);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == NULL);
    ASSERT(search_result.result_index == UINT16_MAX);

    index_spec_free(&index_spec);
    free(cell_contents.BTreePayload.row->values);
    free(cell_contents.BTreePayload.row);
    free(cell_contents.keys);
    value_free_array(target_key_vals, index_spec.index_key->num_columns);
    test_btree_free(&test_btree, &index, &pager);
    free(search_key.target_key);
    return 0;
}

/* Test range scan. */
static int test_range_query() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 3;
    char *pathname = "build/database9_1.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeSearchKey start_search_key = {0};
    Value **start_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!start_key_vals) {
        return -1;
    }

    BTreeSearchKey end_search_key = {0};
    Value **end_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!end_key_vals) {
        return -1;
    }

    BTreeRangeResult range_result = {0};

    /* ---- RANGE CONTAINED IN A SINGLE LEAF & INCLUSIVE BOUNDS ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 45;
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        val = i * 10 + 55;
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));

    BTreeStatus status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, true, &end_search_key, true, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 3);

    uint32_t key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[3]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- RANGE CONTAINED IN MULTIPLE LINKED LEAF NODES & EXCLUSIVE BOUNDS ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 45;
        if (start_key_vals[i]) {
            value_free(start_key_vals[i]);
        }
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        val = i * 10 + 70;
        if (end_key_vals[i]) {
            value_free(end_key_vals[i]);
        }
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, false, &end_search_key, false, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 4);

    key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[3]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[4]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- MIXED INCLUSIVE/EXLUSIVE BOUNDS ---- 
     * ---- UNBOUNDED LOWER RANGE ---- */
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    status = btree_find_range_keys(test_btree.btree, &index_spec, NULL, false, &end_search_key, true, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 6);

    key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[3]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[4]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- UNBOUNDED UPPER RANGE ---- */
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, true, NULL, false, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 12);

    key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[3]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[4]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[5]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[6]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- EMPTY MATCHING RANGE ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 70;
        if (start_key_vals[i]) {
            value_free(start_key_vals[i]);
        }
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        val = i * 10 + 45;
        if (end_key_vals[i]) {
            value_free(end_key_vals[i]);
        }
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, true, &end_search_key, true, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 0);

    /* ---- RANGE CONTAINING ONLY 1 KEY ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 70;
        if (start_key_vals[i]) {
            value_free(start_key_vals[i]);
        }
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        if (end_key_vals[i]) {
            value_free(end_key_vals[i]);
        }
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, true, &end_search_key, true, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 1);

    key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[4]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- START SEARCH KEY LOWER THAN FIRST LEAF'S FIRST KEY ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 35;
        if (start_key_vals[i]) {
            value_free(start_key_vals[i]);
        }
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        val = i * 10 + 70;
        if (end_key_vals[i]) {
            value_free(end_key_vals[i]);
        }
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, false, &end_search_key, false, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 5);

    key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[3]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[4]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- START SEARCH KEY BIGGER THAN LAST LEAF'S LAST KEY ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 90;
        if (start_key_vals[i]) {
            value_free(start_key_vals[i]);
        }
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        val = i * 10 + 110;
        if (end_key_vals[i]) {
            value_free(end_key_vals[i]);
        }
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, false, &end_search_key, false, &range_result);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 2);

    key_counter = 0;
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[5]], &range_result, &index_spec, &key_counter));
    ASSERT(verify_page_keys(pager->pages[test_btree.page_nums[6]], &range_result, &index_spec, &key_counter));
    ASSERT(key_counter == range_result.count);

    /* ---- INVALID SIBLING POINTERS OR CYCLIC CHAINS ---- */
    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        uint32_t val = i * 10 + 45;
        if (start_key_vals[i]) {
            value_free(start_key_vals[i]);
        }
        start_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);

        val = i * 10 + 70;
        if (end_key_vals[i]) {
            value_free(end_key_vals[i]);
        }
        end_key_vals[i] = value_create(UNSIGNED_INTEGER, (void *) &val);
    }
    ASSERT(search_key_init(&start_search_key, &index_spec, start_key_vals));
    ASSERT(search_key_init(&end_search_key, &index_spec, end_key_vals));
    
    set_leaf_next(pager->pages[test_btree.page_nums[3]]->page_data, 50);
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, false, &end_search_key, false, &range_result);
    ASSERT(status == BTREE_CORRUPT_PAGE);

    Page *page = pager_get_page(pager, test_btree.page_nums[3]);
    if (!page) {
        return -1;
    }

    set_leaf_next(pager->pages[test_btree.page_nums[3]]->page_data, MAX_PAGES);
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, false, &end_search_key, false, &range_result);
    ASSERT(status == BTREE_CORRUPT_PAGE);

    page = pager_get_page(pager, test_btree.page_nums[3]);
    if (!page) {
        return -1;
    }

    set_leaf_next(pager->pages[test_btree.page_nums[4]]->page_data, test_btree.page_nums[3]);
    status = btree_find_range_keys(test_btree.btree, &index_spec, &start_search_key, false, &end_search_key, false, &range_result);
    ASSERT(status == BTREE_CORRUPT_PAGE);

    index_spec_free(&index_spec);
    value_free_array(start_key_vals, index_spec.index_key->num_columns);
    value_free_array(end_key_vals, index_spec.index_key->num_columns);
    test_btree_free(&test_btree, &index, &pager);
    free(start_search_key.target_key);
    free(end_search_key.target_key);
    return 0;
}

/* Test prefix key search. */
static int test_find_prefix_keys() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 1;
    char *pathname = "build/database11.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    BTreeCellContents cell_contents = {0};
    ASSERT(cell_contents_init(&cell_contents));

    BTreeSplitResult split_result = {0};

    BTreeSearchKey search_key = {0};
    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) {
        return -1;
    }

    /*
     * Insert 250 keys sharing the same first key component:
     *
     * (1000, 0, 0)
     * (1000, 0, 1)
     * ...
     * (1000, 0, 4)
     * (1000, 1, 0)
     * ...
     * (1000, 49, 4)
     *
     * This guarantees:
     * - one-component prefix with many matches
     * - two-component prefix with 5 matches
     * - full-key prefix with exactly one match
     * - enough entries to cross multiple leaf pages
     * - multiple leaf splits before prefix lookup
     */
    uint32_t split_count = 0;

    for (uint32_t i = 0; i < 250; i++) {
        uint32_t vals[3] = {
            1000,
            i / 5,
            i % 5
        };

        for (uint32_t j = 0; j < cell_contents.num_keys; j++) {
            if (cell_contents.keys[j]) {
                value_free(cell_contents.keys[j]);
            }

            if (cell_contents.BTreePayload.row->values[j]) {
                value_free(cell_contents.BTreePayload.row->values[j]);
            }

            cell_contents.keys[j] = value_create(
                UNSIGNED_INTEGER,
                (void *) &vals[j]
            );
            if (!cell_contents.keys[j]) {
                return -1;
            }

            cell_contents.BTreePayload.row->values[j] =
                value_copy(cell_contents.keys[j]);

            if (!cell_contents.BTreePayload.row->values[j]) {
                return -1;
            }

            target_key_vals[j] = cell_contents.keys[j];
        }

        ASSERT(search_key_init(
            &search_key,
            &index_spec,
            target_key_vals
        ));

        BTreeSearchResult search_result = {0};

        BTreeStatus status = btree_root_to_leaf(
            test_btree.btree,
            &search_key,
            &search_result,
            BTREE_UPPER_BOUND
        );
        ASSERT(status == BTREE_SUCCESS);
        ASSERT(search_result.page);

        BTreePage leaf_page = {0};
        status = btree_page_attach_load_validate(
            pager,
            &leaf_page,
            search_result.page,
            &index_spec
        );
        if (status != BTREE_SUCCESS) {
            return -1;
        }

        ASSERT(leaf_page.type == BTREE_LEAF_NODE);

        status = btree_node_insert(
            pager,
            &leaf_page,
            &cell_contents,
            &split_result,
            &index_spec
        );

        BTreeInsertionResult insertion_res = {0};
        insertion_result_reset(&insertion_res);
        if (status == BTREE_NEEDS_SPLIT) {
            split_count++;

            status = btree_split_propagation(
                test_btree.btree,
                &leaf_page,
                &cell_contents,
                &split_result,
                &index_spec,
                &insertion_res
            );
            ASSERT(status == BTREE_SUCCESS);
        } else {
            ASSERT(status == BTREE_SUCCESS);
        }
    }

    /* Tree must have undergone multiple splits before prefix lookup. */
    ASSERT(split_count > 1);

    BTreeSearchKey prefix_key = {0};
    Value **prefix_key_vals = (Value **) calloc(
        index_spec.index_key->num_columns,
        sizeof(Value *)
    );
    if (!prefix_key_vals) {
        return -1;
    }

    BTreeRangeResult range_result = {0};

    /* ---- PREFIX CONTAINING ONE KEY COMPONENT ----
     * ---- MULTIPLE MATCHES ----
     * ---- CROSS MULTIPLE LINKED LEAF PAGES ----
     * ---- RESULT KEY ORDER ----
     * ---- ONLY MATCHING PREFIXES ---- */
    uint32_t prefix_vals[3] = {1000, 0, 0};

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        prefix_key_vals[i] = value_create(
            UNSIGNED_INTEGER,
            (void *) &prefix_vals[i]
        );
        if (!prefix_key_vals[i]) {
            return -1;
        }
    }

    ASSERT(search_key_init(
        &prefix_key,
        &index_spec,
        prefix_key_vals
    ));
    prefix_key.num_target_keys = 1;

    BTreeStatus status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 250);

    /*
     * One leaf can contain at most 123 cells with the current
     * test cell layout. Therefore 250 matching results prove
     * that the prefix lookup crossed linked leaf pages.
     */
    ASSERT(range_result.count > 123);

    for (uint32_t i = 0; i < range_result.count; i++) {
        ASSERT(range_result.cells[i].type == BTREE_LEAF_NODE);
        ASSERT(range_result.cells[i].num_keys ==
               index_spec.index_key->num_columns);

        /* Every returned key must share the requested prefix. */
        int result = 0;
        ASSERT(value_compare(
            range_result.cells[i].keys[0],
            prefix_key_vals[0],
            &result
        ));
        ASSERT(result == 0);

        /*
         * Expected complete key.
         * This also verifies there are no missing/extra results.
         */
        uint32_t expected_vals[3] = {
            1000,
            i / 5,
            i % 5
        };

        for (uint32_t j = 0;
             j < index_spec.index_key->num_columns;
             j++) {

            Value *expected = value_create(
                UNSIGNED_INTEGER,
                (void *) &expected_vals[j]
            );
            if (!expected) {
                return -1;
            }

            result = 0;
            ASSERT(value_compare(
                range_result.cells[i].keys[j],
                expected,
                &result
            ));
            ASSERT(result == 0);

            value_free(expected);
        }

        /* Results must be returned in complete-key order. */
        if (i > 0) {
            result = 0;
            status = btree_compare(
                range_result.cells[i - 1].keys,
                range_result.cells[i].keys,
                index_spec.index_key->num_columns,
                &result
            );
            ASSERT(status == BTREE_SUCCESS);
            ASSERT(result == -1);
        }
    }

    btree_range_result_free(&range_result);

    /* ---- PREFIX CONTAINING TWO KEY COMPONENTS ----
     * ---- MULTIPLE MATCHING ENTRIES ---- */
    prefix_vals[0] = 1000;
    prefix_vals[1] = 20;
    prefix_vals[2] = 0;

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (prefix_key_vals[i]) {
            value_free(prefix_key_vals[i]);
        }

        prefix_key_vals[i] = value_create(
            UNSIGNED_INTEGER,
            (void *) &prefix_vals[i]
        );
        if (!prefix_key_vals[i]) {
            return -1;
        }
    }

    ASSERT(search_key_init(
        &prefix_key,
        &index_spec,
        prefix_key_vals
    ));
    prefix_key.num_target_keys = 2;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 5);

    for (uint32_t i = 0; i < range_result.count; i++) {
        for (uint32_t j = 0; j < prefix_key.num_target_keys; j++) {
            int result = 0;

            ASSERT(value_compare(
                range_result.cells[i].keys[j],
                prefix_key_vals[j],
                &result
            ));
            ASSERT(result == 0);
        }

        uint32_t expected_third = i;
        Value *expected = value_create(
            UNSIGNED_INTEGER,
            (void *) &expected_third
        );
        if (!expected) {
            return -1;
        }

        int result = 0;
        ASSERT(value_compare(
            range_result.cells[i].keys[2],
            expected,
            &result
        ));
        ASSERT(result == 0);

        value_free(expected);

        if (i > 0) {
            result = 0;
            status = btree_compare(
                range_result.cells[i - 1].keys,
                range_result.cells[i].keys,
                index_spec.index_key->num_columns,
                &result
            );
            ASSERT(status == BTREE_SUCCESS);
            ASSERT(result == -1);
        }
    }

    btree_range_result_free(&range_result);

    /* ---- PREFIX CONTAINING ALL INDEXED KEY COMPONENTS ----
     * ---- PREFIX MATCHING EXACTLY ONE ENTRY ---- */
    prefix_vals[0] = 1000;
    prefix_vals[1] = 20;
    prefix_vals[2] = 3;

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (prefix_key_vals[i]) {
            value_free(prefix_key_vals[i]);
        }

        prefix_key_vals[i] = value_create(
            UNSIGNED_INTEGER,
            (void *) &prefix_vals[i]
        );
        if (!prefix_key_vals[i]) {
            return -1;
        }
    }

    ASSERT(search_key_init(
        &prefix_key,
        &index_spec,
        prefix_key_vals
    ));
    prefix_key.num_target_keys = index_spec.index_key->num_columns;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 1);

    for (uint32_t i = 0; i < prefix_key.num_target_keys; i++) {
        int result = 0;

        ASSERT(value_compare(
            range_result.cells[0].keys[i],
            prefix_key_vals[i],
            &result
        ));
        ASSERT(result == 0);
    }

    btree_range_result_free(&range_result);

    /* ---- PREFIX MATCHING NO ENTRIES ---- */
    prefix_vals[0] = 2000;
    prefix_vals[1] = 0;
    prefix_vals[2] = 0;

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (prefix_key_vals[i]) {
            value_free(prefix_key_vals[i]);
        }

        prefix_key_vals[i] = value_create(
            UNSIGNED_INTEGER,
            (void *) &prefix_vals[i]
        );
        if (!prefix_key_vals[i]) {
            return -1;
        }
    }

    ASSERT(search_key_init(
        &prefix_key,
        &index_spec,
        prefix_key_vals
    ));
    prefix_key.num_target_keys = 1;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 0);

    btree_range_result_free(&range_result);

    /* ---- INVALID ARGUMENTS ---- */

    prefix_vals[0] = 1000;
    prefix_vals[1] = 20;
    prefix_vals[2] = 3;

    for (uint32_t i = 0; i < index_spec.index_key->num_columns; i++) {
        if (prefix_key_vals[i]) {
            value_free(prefix_key_vals[i]);
        }

        prefix_key_vals[i] = value_create(
            UNSIGNED_INTEGER,
            (void *) &prefix_vals[i]
        );
        if (!prefix_key_vals[i]) {
            return -1;
        }
    }

    ASSERT(search_key_init(
        &prefix_key,
        &index_spec,
        prefix_key_vals
    ));

    /* prefix_key == NULL */
    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        NULL,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    /* prefix_key->target_key == NULL */
    void *valid_target_key = prefix_key.target_key;
    prefix_key.target_key = NULL;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.target_key = valid_target_key;

    /* prefix_key->index != index */
    BTreeIndexSpec different_index_spec = index_spec;
    prefix_key.index = &different_index_spec;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.index = &index_spec;

    /* prefix_key->num_target_keys == 0 */
    prefix_key.num_target_keys = 0;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    /* prefix_key->num_target_keys > index->index_key->num_columns */
    prefix_key.num_target_keys =
        index_spec.index_key->num_columns + 1;

    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.num_target_keys =
        index_spec.index_key->num_columns;

    /* result == NULL */
    status = btree_find_prefix_keys(
        test_btree.btree,
        &index_spec,
        &prefix_key,
        NULL
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    /* Invalid BTree */
    status = btree_find_prefix_keys(
        NULL,
        &index_spec,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    /* Invalid Index */
    status = btree_find_prefix_keys(
        test_btree.btree,
        NULL,
        &prefix_key,
        &range_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    btree_range_result_free(&range_result);
    split_result_reset(&split_result);

    value_free_array(
        cell_contents.keys,
        index->key->num_columns
    );
    value_free_array(
        cell_contents.BTreePayload.row->values,
        index->key->num_columns
    );
    free(cell_contents.BTreePayload.row);

    free(target_key_vals);

    value_free_array(
        prefix_key_vals,
        index->key->num_columns
    );

    free(search_key.target_key);
    free(prefix_key.target_key);

    index_spec_free(&index_spec);
    test_btree_free(&test_btree, &index, &pager);

    return 0;
}

/* Test split propagation on all levels (3).
 *
 * (Heavy stress test)
 * Third subtest in split propagation takes too
 * long and creates an excessive amount of pages (over 20 thousand).
 * For a leaf node to split, propagate to the internal
 * node above, waiting for that to split too and then
 * filling up the root node in order to split and create
 * a new root to turn the tree in to a 4-level tree. */
static int test_split_propagation() {
    TestBTree test_btree = {0};
    Pager *pager = NULL;
    Index *index = NULL;
    uint8_t levels = 1;
    char *pathname = "build/database10_1.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));

    BTreeIndexSpec index_spec = {0};
    ASSERT(index_spec_init(&index_spec, index));

    /* ---- 1-LEVEL SPLIT PROPAGATION ---- 
     *
     * Only leaf root node splits and new
     * root internal node is created. */
    BTreePage btree_page = {0};
    BTreeStatus status = btree_page_attach_load_validate(pager, &btree_page, pager->pages[test_btree.page_nums[0]], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    BTreeSearchKey search_key = {0};
    Value **target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) { return -1; }

    BTreeCellContents cell_contents = {0};
    ASSERT(cell_contents_init(&cell_contents));

    BTreeSplitResult split_result = {0};

    /* Insert until leaf root node is full. */
    for (uint32_t i = 0; i < 121; i++) {
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
    ASSERT(search_key_init(&search_key, &index_spec, target_key_vals));
    ASSERT(status == BTREE_NEEDS_SPLIT);
    ASSERT(split_result.split == true);

    BTreeInsertionResult insertion_res = {0};
    insertion_result_reset(&insertion_res);
    status = btree_split_propagation(test_btree.btree, &btree_page, &cell_contents, &split_result, &index_spec, &insertion_res);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(pager->num_pages == 5); // 0, 1 (system pages), 2 (leaf_left), 3(leaf_right), 4 (new root)
    ASSERT(test_btree.btree->root_page_num == pager->pages[4]->page_num);

    btree_page_load(&btree_page);
    ASSERT(btree_page.cell_count == 61);
    ASSERT(btree_page.is_root == false);
    ASSERT(btree_page.type_specific_data.siblings.previous_leaf_pointer == UINT32_MAX);
    ASSERT(btree_page.page->is_dirty == true);

    BTreePage right_page = {0}, new_root = {0};
    status = btree_page_attach_load_validate(pager, &right_page, pager->pages[3], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    ASSERT(right_page.cell_count == 63);
    ASSERT(right_page.is_root == false);
    ASSERT(right_page.type_specific_data.siblings.previous_leaf_pointer == btree_page.page->page_num);
    ASSERT(right_page.type_specific_data.siblings.next_leaf_pointer == UINT32_MAX);
    ASSERT(btree_page.type_specific_data.siblings.next_leaf_pointer == right_page.page->page_num);
    ASSERT(right_page.page->is_dirty == true);

    status = btree_page_attach_load_validate(pager, &new_root, pager->pages[4], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    ASSERT(new_root.is_root == true);
    ASSERT(new_root.cell_count == 1);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(btree_page.parent_pointer == new_root.page->page_num);
    ASSERT(right_page.parent_pointer == new_root.page->page_num);
    ASSERT(new_root.type_specific_data.rightmost_child_pointer == right_page.page->page_num);
    ASSERT(new_root.page->is_dirty == true);

    BTreeKeyView key_view = {0};
    status = get_key(&new_root, 0, &key_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    Value **new_root_cell = serialized_key_to_values(key_view.key, index_spec.index_key->num_columns, &index_spec);
    if (!new_root_cell) { return -1; }

    status = get_key(&right_page, 0, &key_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    Value **right_page_cell = serialized_key_to_values(key_view.key, index_spec.index_key->num_columns, &index_spec);
    if (!right_page_cell) { return -1; }
    
    int result = 0;
    status = btree_compare(new_root_cell, right_page_cell, index->key->num_columns, &result);
    ASSERT(result == 0);

    index_spec_free(&index_spec);
    split_result_reset(&split_result);
    value_free_array(new_root_cell, index->key->num_columns);
    value_free_array(right_page_cell, index->key->num_columns);
    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    free(target_key_vals);
    search_key.target_key = NULL;
    free(search_key.target_key);
    test_btree_free(&test_btree, &index, &pager);

    /* ---- 2-LEVEL SPLIT PROPAGATION ---- 
     *
     * Leaf node splits, separator key moves up
     * to already existing root that also splits
     * and creates another new root. */
    levels = 2;
    pathname = "build/database10_2.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));
    ASSERT(index_spec_init(&index_spec, index));

    status = btree_page_attach_load_validate(pager, &btree_page, pager->pages[test_btree.page_nums[2]], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) { return -1; }

    ASSERT(cell_contents_init(&cell_contents));

    uint32_t old_root_page_num = test_btree.btree->root_page_num;
    /* Insert until leaf root node is full. */
    for (uint32_t k = 0; k < 500; k++) { // Amount of splits needed for full root
        for (uint32_t i = 0; i < 121; i++) {
            for (uint32_t j = 0; j < cell_contents.num_keys; j++) {
                uint32_t val = k * 1000 + i*10 + j;
                
               
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

        if (status != BTREE_NEEDS_SPLIT) { return -1; }

        status = btree_split_propagation(test_btree.btree, &btree_page, &cell_contents, &split_result, &index_spec, &insertion_res);
        if (status != BTREE_SUCCESS) {
            return -1;
        }

        if (old_root_page_num != test_btree.btree->root_page_num) {
            break;
        }

        status = btree_page_attach_load_validate(pager, &new_root, pager->pages[test_btree.btree->root_page_num], &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        status = btree_page_attach_load_validate(pager, &btree_page, pager->pages[new_root.type_specific_data.rightmost_child_pointer], &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }
    }

    ASSERT(old_root_page_num != test_btree.btree->root_page_num);
    
    status = btree_page_attach_load_validate(pager, &new_root, pager->pages[test_btree.btree->root_page_num], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(new_root.cell_count == 1);
    
    BTreePage old_root = {0};
    status = btree_page_attach_load_validate(pager, &old_root, pager->pages[old_root_page_num], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    ASSERT(old_root.is_root == false);
    ASSERT(old_root.cell_count == 102);
    ASSERT(old_root.parent_pointer == test_btree.btree->root_page_num);

    status = btree_page_attach_load_validate(pager, &right_page, pager->pages[new_root.type_specific_data.rightmost_child_pointer], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    ASSERT(right_page.is_root == false);
    ASSERT(right_page.cell_count == 102);
    ASSERT(right_page.parent_pointer == test_btree.btree->root_page_num);

    uint32_t new_root_cell_pointer = get_cell_pointer(new_root.data, 0);
    uint32_t new_root_child_pointer = get_cell_child_pointer(new_root.data, get_cell_offset(new_root_cell_pointer));
    ASSERT(new_root_child_pointer == old_root.page->page_num);
    ASSERT(new_root.type_specific_data.rightmost_child_pointer == right_page.page->page_num);

    for (uint32_t i = 0; i < old_root.cell_count+1; i++) {
        BTreePage leaf_node = {0};
        uint32_t child_pointer = 0;
        if (i == old_root.cell_count) {
            child_pointer = old_root.type_specific_data.rightmost_child_pointer;
        } else {    
            uint32_t cell_pointer = get_cell_pointer(old_root.data, i);
            child_pointer = get_cell_child_pointer(old_root.data, get_cell_offset(cell_pointer));

        }
        Page *page = pager_get_page(pager, child_pointer);
        if (!page) { return -1; }

        status = btree_page_attach_load_validate(pager, &leaf_node, page, &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        ASSERT(leaf_node.type == BTREE_LEAF_NODE);
        ASSERT(leaf_node.is_root == false);
        ASSERT(leaf_node.parent_pointer == old_root.page->page_num);
    }

    for (uint32_t i = 0; i < right_page.cell_count; i++) {
        BTreePage leaf_node = {0};
        uint32_t child_pointer = 0;
        if (i == right_page.cell_count) {
            child_pointer = right_page.type_specific_data.rightmost_child_pointer;
        } else {    
            uint32_t cell_pointer = get_cell_pointer(right_page.data, i);
            child_pointer = get_cell_child_pointer(right_page.data, get_cell_offset(cell_pointer));

        }

        Page *page = pager_get_page(pager, child_pointer);
        if (!page) { return -1; }

        status = btree_page_attach_load_validate(pager, &leaf_node, page, &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        ASSERT(leaf_node.type == BTREE_LEAF_NODE);
        ASSERT(leaf_node.is_root == false);
        ASSERT(leaf_node.parent_pointer == right_page.page->page_num);
    }

    status = get_key(&new_root, 0, &key_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    new_root_cell = serialized_key_to_values(key_view.key, index_spec.index_key->num_columns, &index_spec);
    if (!new_root_cell) { return -1; }

    Page *right_internal_page = pager_get_page(pager, new_root.type_specific_data.rightmost_child_pointer);
    if (!right_internal_page) { return -1; }

    BTreePage right_internal = {0};
    status = btree_page_attach_load_validate(pager, &right_internal, right_internal_page, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    BTreeCellView cell_view = {0};
    status = get_cell(&right_internal, 0, &cell_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    right_page_cell = serialized_key_to_values(cell_view.key.key, index_spec.index_key->num_columns, &index_spec);
    if (!right_page_cell) { return -1; }

    status = get_key(&old_root, old_root.cell_count-1, &key_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    Value **left_page_last_cell = serialized_key_to_values(key_view.key, index->key->num_columns, &index_spec);
    if (!left_page_last_cell) { return -1; }
    
    result = 0;
    status = btree_compare(left_page_last_cell, new_root_cell, index->key->num_columns, &result);
    ASSERT(result == -1);

    status = btree_compare(new_root_cell, right_page_cell, index->key->num_columns, &result);
    ASSERT(result == -1);

    uint32_t right_subtree_leftmost = 0;
    memcpy(&right_subtree_leftmost, cell_view.payload, sizeof(uint32_t));

    Page *right_subtree_leftmost_page = pager_get_page(pager, right_subtree_leftmost);
    if (!right_subtree_leftmost_page) { return -1; }

    BTreePage right_subtree_leftmost_btree_page = {0};
    status = btree_page_attach_load_validate(pager, &right_subtree_leftmost_btree_page, right_subtree_leftmost_page, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    status = get_key(&right_subtree_leftmost_btree_page, 0, &key_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    Value **right_subbtree_leftmost_key = serialized_key_to_values(key_view.key, index_spec.index_key->num_columns, &index_spec);
    if (!right_subbtree_leftmost_key) { return -1; }

    status = btree_compare(new_root_cell, right_subbtree_leftmost_key, index->key->num_columns, &result);
    ASSERT(result == 0);

    BTreePageCollection visited = {0};

    status = btree_traverse_reachable_pages(test_btree.btree, &visited);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(visited.count == pager->num_pages - 2);

    uint32_t leftmost_cell_pointer = get_cell_pointer(old_root.data, 0);
    uint32_t leftmost_child_pointer = get_cell_child_pointer(old_root.data, get_cell_offset(leftmost_cell_pointer));

    Page *leftmost_leaf_node_page = pager_get_page(pager, leftmost_child_pointer);
    if (!leftmost_leaf_node_page) { return -1; }

    BTreePage leftmost_leaf_node_btree_page = {0};
    status = btree_page_attach_load_validate(pager, &leftmost_leaf_node_btree_page, leftmost_leaf_node_page, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    uint32_t counter = 1, prev_page_num = leftmost_leaf_node_btree_page.page->page_num;
    uint32_t next_leaf = leftmost_leaf_node_btree_page.type_specific_data.siblings.next_leaf_pointer;
    ASSERT(leftmost_leaf_node_btree_page.type_specific_data.siblings.previous_leaf_pointer == UINT32_MAX);
    while (next_leaf != UINT32_MAX) {        
        Page *page = pager_get_page(pager, next_leaf);
        if (!page) { return -1; }

        BTreePage leaf_page = {0};
        status = btree_page_attach_load_validate(pager, &leaf_page, page, &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }
        ASSERT(leaf_page.type == BTREE_LEAF_NODE);

        ASSERT(leaf_page.type_specific_data.siblings.previous_leaf_pointer == prev_page_num);
        next_leaf = leaf_page.type_specific_data.siblings.next_leaf_pointer;

        prev_page_num = leaf_page.page->page_num;
        counter++;
    }
    ASSERT(counter == pager->num_pages-5);

    index_spec_free(&index_spec);
    split_result_reset(&split_result);
    value_free_array(new_root_cell, index->key->num_columns);
    value_free_array(right_page_cell, index->key->num_columns);
    value_free_array(cell_contents.keys, index->key->num_columns);
    value_free_array(cell_contents.BTreePayload.row->values, index->key->num_columns);
    free(target_key_vals);
    search_key.target_key = NULL;
    free(search_key.target_key);
    test_btree_free(&test_btree, &index, &pager);

     /* ---- 3-LEVEL SPLIT PROPAGATION ---- 
     *
     * Leaf node splits, separator key moves up
     * to internal node that splits and internal
     * node's separator key moves up to root
     * until it splits up too. */
    levels = 3;
    pathname = "build/database10_3.db";
    ASSERT(test_btree_full_init(&test_btree, &pager, &index, levels, pathname));
    ASSERT(index_spec_init(&index_spec, index));

    status = btree_page_attach_load_validate(pager, &btree_page, pager->pages[test_btree.page_nums[6]], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    target_key_vals = (Value **) calloc(index_spec.index_key->num_columns, sizeof(Value *));
    if (!target_key_vals) { return -1; }

    ASSERT(cell_contents_init(&cell_contents));

    old_root_page_num = test_btree.btree->root_page_num;
    /* Insert until leaf root node is full. */
    for (uint32_t k = 0; k < 22000; k++) { // Amount of splits needed for full root
        for (uint32_t i = 0; i < 121; i++) {
            for (uint32_t j = 0; j < cell_contents.num_keys; j++) {
                uint32_t val = k * 10000 + i*10 + j;
                
               
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

        if (status != BTREE_NEEDS_SPLIT) { return -1; }

        status = btree_split_propagation(test_btree.btree, &btree_page, &cell_contents, &split_result, &index_spec, &insertion_res);
        if (status != BTREE_SUCCESS) {
            return -1;
        }

        if (old_root_page_num != test_btree.btree->root_page_num) {
            break;
        }

        status = btree_page_attach_load_validate(pager, &new_root, pager->pages[test_btree.btree->root_page_num], &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        BTreePage right_internal = {0};
        status = btree_page_attach_load_validate(pager, &right_internal, pager->pages[new_root.type_specific_data.rightmost_child_pointer], &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        status = btree_page_attach_load_validate(pager, &btree_page, pager->pages[right_internal.type_specific_data.rightmost_child_pointer], &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }
    }

    ASSERT(old_root_page_num != test_btree.btree->root_page_num);
    
    status = btree_page_attach_load_validate(pager, &new_root, pager->pages[test_btree.btree->root_page_num], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(new_root.cell_count == 1);
    
    status = btree_page_attach_load_validate(pager, &old_root, pager->pages[old_root_page_num], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    ASSERT(old_root.is_root == false);
    ASSERT(old_root.parent_pointer == test_btree.btree->root_page_num);

    status = btree_page_attach_load_validate(pager, &right_page, pager->pages[new_root.type_specific_data.rightmost_child_pointer], &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    ASSERT(right_page.is_root == false);
    ASSERT(right_page.parent_pointer == test_btree.btree->root_page_num);

    new_root_cell_pointer = get_cell_pointer(new_root.data, 0);
    new_root_child_pointer = get_cell_child_pointer(new_root.data, get_cell_offset(new_root_cell_pointer));
    ASSERT(new_root_child_pointer == old_root.page->page_num);
    ASSERT(new_root.type_specific_data.rightmost_child_pointer == right_page.page->page_num);

    Page *first_leaf_node = NULL;
    for (uint32_t i = 0; i < old_root.cell_count+1; i++) {
        BTreePage internal_node = {0};
        uint32_t child_pointer = 0;
        if (i == old_root.cell_count) {
            child_pointer = old_root.type_specific_data.rightmost_child_pointer;
        } else {    
            uint32_t cell_pointer = get_cell_pointer(old_root.data, i);
            child_pointer = get_cell_child_pointer(old_root.data, get_cell_offset(cell_pointer));

        }
        Page *page = pager_get_page(pager, child_pointer);
        if (!page) { return -1; }

        status = btree_page_attach_load_validate(pager, &internal_node, page, &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        ASSERT(internal_node.type == BTREE_INTERNAL_NODE);
        ASSERT(internal_node.is_root == false);
        ASSERT(internal_node.parent_pointer == old_root.page->page_num);

        for (uint32_t j = 0; j < internal_node.cell_count+1; j++) {
            BTreePage leaf_node = {0};
            child_pointer = 0;
            if (j == internal_node.cell_count) {
                child_pointer = internal_node.type_specific_data.rightmost_child_pointer;
            } else {    
                uint32_t cell_pointer = get_cell_pointer(internal_node.data, j);
                child_pointer = get_cell_child_pointer(internal_node.data, get_cell_offset(cell_pointer));
            }

            page = pager_get_page(pager, child_pointer);
            if (!page) { return -1; }

            status = btree_page_attach_load_validate(pager, &leaf_node, page, &index_spec);
            if (status != BTREE_SUCCESS) { return -1; }

            if (i == old_root.cell_count && j == internal_node.cell_count) {
                status = get_key(&leaf_node, leaf_node.cell_count-1, &key_view, &index_spec);
                if (status != BTREE_SUCCESS) { return -1; }

                left_page_last_cell = serialized_key_to_values(key_view.key, index->key->num_columns, &index_spec);
                if (!left_page_last_cell) { return -1; }
            }

            ASSERT(leaf_node.type == BTREE_LEAF_NODE);
            ASSERT(leaf_node.is_root == false);
            ASSERT(leaf_node.parent_pointer == internal_node.page->page_num);

            if (i == 0 && j == 0) {
                first_leaf_node = page;
            }
        }
    }

    for (uint32_t i = 0; i < right_page.cell_count+1; i++) {
        BTreePage internal_node = {0};
        uint32_t child_pointer = 0;
        if (i == right_page.cell_count) {
            child_pointer = right_page.type_specific_data.rightmost_child_pointer;
        } else {    
            uint32_t cell_pointer = get_cell_pointer(right_page.data, i);
            child_pointer = get_cell_child_pointer(right_page.data, get_cell_offset(cell_pointer));

        }
        Page *page = pager_get_page(pager, child_pointer);
        if (!page) { return -1; }

        status = btree_page_attach_load_validate(pager, &internal_node, page, &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }

        ASSERT(internal_node.type == BTREE_INTERNAL_NODE);
        ASSERT(internal_node.is_root == false);
        ASSERT(internal_node.parent_pointer == right_page.page->page_num);

        for (uint32_t j = 0; j < internal_node.cell_count+1; j++) {
            BTreePage leaf_node = {0};
            child_pointer = 0;
            if (j == internal_node.cell_count) {
                child_pointer = internal_node.type_specific_data.rightmost_child_pointer;
            } else {    
                uint32_t cell_pointer = get_cell_pointer(internal_node.data, j);
                child_pointer = get_cell_child_pointer(internal_node.data, get_cell_offset(cell_pointer));
            }

            page = pager_get_page(pager, child_pointer);
            if (!page) { return -1; }

            status = btree_page_attach_load_validate(pager, &leaf_node, page, &index_spec);
            if (status != BTREE_SUCCESS) { return -1; }

            if (i == 0 && j == 0) {
                status = get_key(&leaf_node, j, &key_view, &index_spec);
                if (status != BTREE_SUCCESS) { return -1; }

                right_page_cell = serialized_key_to_values(key_view.key, index->key->num_columns, &index_spec);
                if (!right_page_cell) { return -1; }
            }

            ASSERT(leaf_node.type == BTREE_LEAF_NODE);
            ASSERT(leaf_node.is_root == false);
            ASSERT(leaf_node.parent_pointer == internal_node.page->page_num);
        }
    }

    status = get_key(&new_root, 0, &key_view, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    new_root_cell = serialized_key_to_values(key_view.key, index_spec.index_key->num_columns, &index_spec);
    if (!new_root_cell) { return -1; }

    right_internal_page = pager_get_page(pager, new_root.type_specific_data.rightmost_child_pointer);
    if (!right_internal_page) { return -1; }

    status = btree_page_attach_load_validate(pager, &right_internal, right_internal_page, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }


    result = 0;
    status = btree_compare(left_page_last_cell, new_root_cell, index->key->num_columns, &result);
    ASSERT(result == -1);

    status = btree_compare(new_root_cell, right_page_cell, index->key->num_columns, &result);
    ASSERT(result == 0);

    // right_subtree_leftmost = 0;
    // memcpy(&right_subtree_leftmost, cell_view.payload, sizeof(uint32_t));

    // right_subtree_leftmost_page = pager_get_page(pager, right_subtree_leftmost);
    // if (!right_subtree_leftmost_page) { return -1; }

    status = btree_traverse_reachable_pages(test_btree.btree, &visited);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(visited.count == pager->num_pages - 2);

    BTreePage btree_first_leaf_node = {0};
    status = btree_page_attach_load_validate(pager, &btree_first_leaf_node, first_leaf_node, &index_spec);
    if (status != BTREE_SUCCESS) { return -1; }
    uint32_t leaf_count = 0;

    for (uint32_t i = 0; i < visited.count; i++) {
        Page *page = pager_get_page(pager, visited.page_numbers[i]);
        if (!page) { return -1; }

        BTreePage page_btree = {0};
        status = btree_page_attach_load_validate(
            pager,
            &page_btree,
            page,
            &index_spec
        );
        if (status != BTREE_SUCCESS) { return -1; }

        if (page_btree.type == BTREE_LEAF_NODE) {
            leaf_count++;
        }
    }

    counter = 1;
    prev_page_num = btree_first_leaf_node.page->page_num;
    next_leaf = btree_first_leaf_node.type_specific_data.siblings.next_leaf_pointer;
    ASSERT(btree_first_leaf_node.type_specific_data.siblings.previous_leaf_pointer == UINT32_MAX);
    while (next_leaf != UINT32_MAX) {
        ASSERT(counter < leaf_count);
        
        Page *page = pager_get_page(pager, next_leaf);
        if (!page) { return -1; }

        BTreePage leaf_page = {0};
        status = btree_page_attach_load_validate(pager, &leaf_page, page, &index_spec);
        if (status != BTREE_SUCCESS) { return -1; }
        ASSERT(leaf_page.type == BTREE_LEAF_NODE);

        ASSERT(leaf_page.type_specific_data.siblings.previous_leaf_pointer == prev_page_num);
        next_leaf = leaf_page.type_specific_data.siblings.next_leaf_pointer;

        prev_page_num = leaf_page.page->page_num;
        counter++;
    }
    ASSERT(counter == leaf_count);
    

    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

/* (NOTE) MAX_PAGES >= 25000 (Won't run correctly otherwise)*/
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
    result = unlink("build/database8_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database8_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database9_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database11.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database12.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    result = test_empty_leaf_init();
    generate_output(result, 0, "test_empty_leaf_init");
    result = test_init_internal();
    generate_output(result, 1, "test_init_internal");
    result = test_binary_search();
    generate_output(result, 2, "test_binary_search");
    result = test_root_to_leaf();
    generate_output(result, 3, "test_root_to_leaf");
    result = test_node_insert();
    generate_output(result, 4, "test_node_insert");
    result = test_leaf_node_split();
    generate_output(result, 5, "test_leaf_node_split");
    result = test_internal_node_split();
    generate_output(result, 6, "test_internal_node_split");
    result = test_root_node_split();
    generate_output(result, 7, "test_root_node_split");
    result = test_reachable_page_traversal();
    generate_output(result, 8, "test_reachable_page_traversal");
    result = test_exact_key_lookup();
    generate_output(result, 9, "test_exact_key_lookup");
    result = test_range_query();
    generate_output(result, 10, "test_range_query");
    result = test_find_prefix_keys();
    generate_output(result, 11, "test_find_prefix_keys");
    result = test_split_propagation();
    generate_output(result, 12, "test_split_propagation");
    

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
    result = unlink("build/database8_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database8_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database9_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database11.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database12.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    return 0;
}