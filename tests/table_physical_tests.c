#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/table.h"
#include "../include/schema.h"
#include "../include/index.h"
#include "../include/constraints.h"
#include "../include/data_types.h"
#include "../include/database.h"
#include "../include/pager.h"
#include "../include/page.h"
#include "../include/btree.h"
#include "../src/btree/btree_utils.h"

#define ASSERT(condition) do { \
    if (!(condition)) { \
        goto cleanup; \
    } \
} while (0)

/* ---------- Column/Schema/Table/Database Creation Helpers ---------- */

static Schema *create_test_schema_with_constraints() {
    Column id = {.name = "id", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};
    Column email = {.name = "email", .type = TEXT, .non_null_rows = 0, .null_rows = 0};
    Column age = {.name = "age", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};

    Column *columns[] = {&id, &email, &age};

    uint32_t primary_columns[] = {0};
    uint32_t unique_columns[] = {1};

    Constraint *primary_key = constraint_create_primary_key("pk_users", primary_columns, 1);
    Constraint *unique_email = constraint_create_unique("uq_users_email", unique_columns, 1);

    if (!primary_key || !unique_email) {
        constraint_free(primary_key);
        constraint_free(unique_email);
        return NULL;
    }

    Constraint *constraints[] = { primary_key, unique_email };

    Schema *schema = schema_create(columns, constraints, 3, 2);

    constraint_free(primary_key);
    constraint_free(unique_email);

    return schema;
}

static Schema *create_test_schema_without_constraints() {
    Column id = {.name = "id", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};
    Column name = {.name = "name", .type = TEXT, .non_null_rows = 0, .null_rows = 0};

    Column *columns[] = {&id, &name};

    return schema_create(columns, NULL, 2, 0);
}

static Table *create_test_table() {
    Schema *input_schema = create_test_schema_with_constraints();

    if (!input_schema) {
        return NULL;
    }

    Table *table = table_metadata_create("users", input_schema);

    schema_free(input_schema);
    return table;
}

static Table *create_test_physical_table(Pager *pager) {
    Schema *schema = create_test_schema_with_constraints();
    if (!schema) {
        return NULL;
    }

    Table *table = table_create("users", schema, pager);

    schema_free(schema);
    return table;
}


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


/* ---------- table_create unit tests ---------- */

static int test_table_create_success() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("test_table", input_schema, test_pager.pager);

    ASSERT(table != NULL);

    ASSERT(!table->is_deleted);
    ASSERT(table->is_materialized);
    ASSERT(strcmp(table->name, "test_table") == 0);
    ASSERT(table->row_count == 0);

    // Validate schema deep copy
    ASSERT(table->table_schema != input_schema);
    ASSERT(table->table_schema->columns != input_schema->columns);
    ASSERT(table->table_schema->constraints != input_schema->constraints);
    
    ASSERT(table->table_schema->num_columns == 3);
    ASSERT(table->table_schema->num_constraints == 2);

    // Input schema mutation does not affect table's deep-copied schema
    strcpy(input_schema->columns[0]->name, "changed_id");
    ASSERT(strcmp(table->table_schema->columns[0]->name, "id") == 0);

    // Validate the primary key of 1 column
    ASSERT(table->primary_index != NULL);
    ASSERT(strcmp(table->primary_index->name, "pk_users") == 0);
    ASSERT(table->primary_index->type == PRIMARY_INDEX);
    ASSERT(table->primary_index->root_page_num != INVALID_ROOT_PAGE);

    // Validate the 1 secondary index of 1 column
    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes[0] != NULL);

    ASSERT(strcmp(table->secondary_indexes[0]->name, "uq_users_email") == 0);
    ASSERT(table->secondary_indexes[0]->root_page_num != INVALID_ROOT_PAGE);

    // Validate empty B+ Tree roots of primary and secondary indexes
    Page *primary_root_page = pager_get_page(test_pager.pager, table->primary_index->root_page_num);
    ASSERT(primary_root_page != NULL);
    ASSERT(is_empty_btree_root(primary_root_page));

    Page *secondary_root_page = pager_get_page(test_pager.pager, table->secondary_indexes[0]->root_page_num);
    ASSERT(secondary_root_page != NULL);
    ASSERT(is_empty_btree_root(secondary_root_page));

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_create_success_without_constraints() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_without_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("test_table", input_schema, test_pager.pager);

    ASSERT(table != NULL);

    ASSERT(!table->is_deleted);
    ASSERT(table->is_materialized);
    ASSERT(strcmp(table->name, "test_table") == 0);
    ASSERT(table->row_count == 0);

    
    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }
    
    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_create_empty_roots() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;
    Page *primary_root = NULL;
    Page *secondary_root = NULL;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("test_table", input_schema, test_pager.pager);

    // Validate table metadata, primary, and secondary indexes
    ASSERT(table != NULL);
    ASSERT(table->is_materialized);

    ASSERT(table->primary_index != NULL);
    ASSERT(table->primary_index->root_page_num != INVALID_ROOT_PAGE);

    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes[0] != NULL);
    ASSERT(table->secondary_indexes[0]->root_page_num != INVALID_ROOT_PAGE);

    // Validate the root pages of primary and the 1 secondary index are not the same
    ASSERT(table->primary_index->root_page_num != table->secondary_indexes[0]->root_page_num);

    // Validate the root pages are empty
    primary_root = pager_get_page(test_pager.pager, table->primary_index->root_page_num);
    ASSERT(primary_root != NULL);
    ASSERT(is_empty_btree_root(primary_root));

    secondary_root = pager_get_page(test_pager.pager, table->secondary_indexes[0]->root_page_num);
    ASSERT(secondary_root != NULL);
    ASSERT(is_empty_btree_root(secondary_root));
    
    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_create_invalid_arguments() {
    int result = 1;
    
    TestPager test_pager = {0};
    Schema *input_schema = NULL;

    ASSERT(create_test_pager(&test_pager));

    ASSERT(table_create(NULL, input_schema, test_pager.pager) == NULL);
    ASSERT(table_create("", input_schema, test_pager.pager) == NULL);

    char oversized_name[65];
    memset(oversized_name, 'a', sizeof(oversized_name) - 1);
    oversized_name[sizeof(oversized_name) - 1] = '\0';

    ASSERT(table_create(oversized_name, input_schema, test_pager.pager) == NULL);
    ASSERT(table_create("test_table", NULL, test_pager.pager) == NULL);
    ASSERT(table_create("test_table", input_schema, NULL) == NULL);

    Pager invalid_pager = {0};

    ASSERT(table_create("test_table", input_schema, &invalid_pager) == NULL);

    result = 0;

cleanup:
    if (input_schema) { 
        schema_free(input_schema); 
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_create_add_secondary_index() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;
    IndexKey *key = NULL;
    Index *created_index = NULL;
    Page *root_page = NULL;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("test_table", input_schema, test_pager.pager);
    ASSERT(table != NULL);

    // Initial index totals
    ASSERT(table->total_secondary_indexes == 1);

    // Create secondary index with the 3rd column of the schema as its key
    uint32_t age_column[] = {2}; 
    key = index_key_create(age_column, 1);
    ASSERT(key != NULL);

    // Add it to the table
    ASSERT(table_create_index(table, "idx_users_age", SECONDARY_INDEX, key, test_pager.pager, false));

    // Retrieve the created index and verify its metadata
    ASSERT(table->total_secondary_indexes == 2);

    created_index = table_find_index(table, "idx_users_age");

    ASSERT(created_index != NULL);
    ASSERT(created_index == table->secondary_indexes[1]);
    ASSERT(created_index->type == SECONDARY_INDEX);

    ASSERT(created_index->key != NULL);
    ASSERT(created_index->key->num_columns == 1);
    ASSERT(created_index->key->column_index_array[0] == 2);

    // Verify the existence and emptiness of the new index root
    ASSERT(created_index->root_page_num != INVALID_ROOT_PAGE);

    root_page = pager_get_page(test_pager.pager, created_index->root_page_num);

    ASSERT(root_page != NULL);
    ASSERT(is_empty_btree_root(root_page));

    result = 0;

cleanup:
    if (key) {
        index_key_free(key);
    }

    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}

/* ---------- table_materialize unit tests ---------- */

static int test_table_materialize_success() {
    int result = 1;

    TestPager test_pager = {0};
    Table *table = NULL;
    
    ASSERT(create_test_pager(&test_pager));

    table = create_test_table();
    ASSERT(table != NULL);

    ASSERT(table_materialize(table, test_pager.pager));

    // Validate index metadata exist in the table structure
    // especially a valid physical page number
    ASSERT(table->is_materialized);

    ASSERT(table->primary_index != NULL);
    ASSERT(strcmp(table->primary_index->name, "pk_users") == 0);
    ASSERT(table->primary_index->root_page_num != INVALID_ROOT_PAGE);
    ASSERT(table->primary_index->key != NULL);
    ASSERT(table->primary_index->key->column_index_array != NULL);
    ASSERT(table->primary_index->key->column_index_array[0] == 0);
    ASSERT(table->primary_index->key->num_columns == 1);

    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes[0] != NULL);
    ASSERT(strcmp(table->secondary_indexes[0]->name, "uq_users_email") == 0);
    ASSERT(table->secondary_indexes[0]->root_page_num != INVALID_ROOT_PAGE);
    ASSERT(table->secondary_indexes[0]->key->column_index_array != NULL);
    ASSERT(table->secondary_indexes[0]->key->column_index_array[0] == 1);
    ASSERT(table->secondary_indexes[0]->key->num_columns == 1);

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_table_materialize_without_constraints() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;

    create_test_pager(&test_pager);

    input_schema = create_test_schema_without_constraints();
    ASSERT(input_schema != NULL);

    uint32_t original_num_pages = test_pager.pager->num_pages;

    table = table_metadata_create("test_table", input_schema);
    ASSERT(table != NULL);

    ASSERT(table_materialize(table, test_pager.pager));

    ASSERT(table->is_materialized);
    ASSERT(table->primary_index == NULL);

    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->total_secondary_indexes == 0);
    ASSERT(table->secondary_indexes[0] == NULL);

    // Make sure no pages were added to a physical table that had no indexes defined
    ASSERT(test_pager.pager->num_pages == original_num_pages);

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_table_materialize_empty_roots() {
    int result = 1;

    TestPager test_pager = {0};
    Table *table = NULL;
    Page *primary_root = NULL;
    Page *secondary_root = NULL;
    
    ASSERT(create_test_pager(&test_pager));

    table = create_test_table();
    ASSERT(table != NULL);

    ASSERT(table_materialize(table, test_pager.pager));

    // Validate that the primary index root and the secondary index root are empty pages
    ASSERT(table->is_materialized);

    primary_root = pager_get_page(test_pager.pager, table->primary_index->root_page_num);
    
    ASSERT(primary_root != NULL);
    ASSERT(is_empty_btree_root(primary_root));

    secondary_root = pager_get_page(test_pager.pager, table->secondary_indexes[0]->root_page_num);

    ASSERT(secondary_root != NULL);
    ASSERT(is_empty_btree_root(secondary_root));

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;   
}

static int test_table_materialize_rejects_second_call() {
    int result = 1;

    TestPager test_pager = {0};
    Table *table = NULL;
    
    ASSERT(create_test_pager(&test_pager));

    table = create_test_table();
    ASSERT(table != NULL);

    ASSERT(table_materialize(table, test_pager.pager));
    ASSERT(table->is_materialized);

    // Validate that an additional materialization function call fails
    ASSERT(!table_materialize(table, test_pager.pager));

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        // In case table_drop() fails, since it doesn't support rollback
        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;  
}

static int test_table_materialize_preserves_metadata() {
    int result = 1;
    TestPager test_pager = {0};
    Table *table = NULL;

    Index *original_primary = NULL;
    Index *original_secondary = NULL;
    Schema *original_schema = NULL;

    char original_table_name[sizeof(((Table *) 0)->name)] = {0};
    char original_primary_name[sizeof(((Index *) 0)->name)] = {0};
    char original_secondary_name[sizeof(((Index *) 0)->name)] = {0};

    ASSERT(create_test_pager(&test_pager));

    table = create_test_table();
    ASSERT(table != NULL);

    original_primary = table->primary_index;
    original_secondary = table->secondary_indexes[0];
    original_schema = table->table_schema;

    strcpy(original_table_name, table->name);
    strcpy(original_primary_name, table->primary_index->name);
    strcpy(original_secondary_name, table->secondary_indexes[0]->name);

    ASSERT(table_materialize(table, test_pager.pager));

    /* Original logical objects remain attached. */
    ASSERT(table->primary_index == original_primary);
    ASSERT(table->secondary_indexes[0] == original_secondary);
    ASSERT(table->table_schema == original_schema);

    ASSERT(strcmp(table->name, original_table_name) == 0);
    ASSERT(strcmp(table->primary_index->name, original_primary_name) == 0);
    ASSERT(strcmp(table->secondary_indexes[0]->name, original_secondary_name) == 0);

    ASSERT(table->primary_index->type == PRIMARY_INDEX);
    ASSERT(table->secondary_indexes[0]->type == SECONDARY_INDEX);

    ASSERT(table->primary_index->key->num_columns == 1);
    ASSERT(table->primary_index->key->column_index_array[0] == 0);

    ASSERT(table->secondary_indexes[0]->key->num_columns == 1);
    ASSERT(table->secondary_indexes[0]->key->column_index_array[0] == 1);

    ASSERT(table->row_count == 0);
    ASSERT(table->table_schema->num_columns == 3);
    ASSERT(table->table_schema->num_constraints == 2);

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_table_materialize_invalid_arguments() {
    int result = 1;
    TestPager test_pager = {0};
    Table *table = NULL;

    ASSERT(create_test_pager(&test_pager));

    table = create_test_table();
    ASSERT(table != NULL);

    // Invlaid argument cases
    ASSERT(!table_materialize(NULL, test_pager.pager));
    ASSERT(!table_materialize(table, NULL));

    Pager invalid_pager = {0};
    ASSERT(!table_materialize(table, &invalid_pager));

    // Corrupt schema pointer temporarily
    Schema *saved_schema = table->table_schema;
    table->table_schema = NULL;

    ASSERT(!table_materialize(table, test_pager.pager));

    table->table_schema = saved_schema;

    // Corrupt secondary-index array temporarily
    Index **saved_secondary_indexes = table->secondary_indexes;
    table->secondary_indexes = NULL;

    ASSERT(!table_materialize(table, test_pager.pager));

    table->secondary_indexes = saved_secondary_indexes;

    ASSERT(!table->is_materialized);
    ASSERT(table->primary_index->root_page_num == INVALID_ROOT_PAGE);
    ASSERT(table->secondary_indexes[0]->root_page_num == INVALID_ROOT_PAGE);

    result = 0;

cleanup:
    if (table) {
        table_free(table);
    }

    destroy_test_pager(&test_pager);
    return result;
}

/* ---------- table_drop unit tests ---------- */

static int test_table_drop_success() {
    int result = 1;
    TestPager test_pager = {0};
    Table *table = NULL;

    uint32_t primary_root = INVALID_ROOT_PAGE;
    uint32_t secondary_root = INVALID_ROOT_PAGE;

    ASSERT(create_test_pager(&test_pager));

    table = create_test_physical_table(test_pager.pager);
    ASSERT(table != NULL);

    primary_root = table->primary_index->root_page_num;
    secondary_root = table->secondary_indexes[0]->root_page_num;

    ASSERT(primary_root != INVALID_ROOT_PAGE);
    ASSERT(secondary_root != INVALID_ROOT_PAGE);

    ASSERT(table_drop(table, test_pager.pager));

    // table_drop() freed the Table
    table = NULL;

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_drop_reuses_root_pages() {
    int result = 1;
    TestPager test_pager = {0};
    Table *table = NULL;

    uint32_t dropped_roots[2] = {0};
    uint32_t reallocated_pages[2] = {0};

    ASSERT(create_test_pager(&test_pager));

    table = create_test_physical_table(test_pager.pager);
    ASSERT(table != NULL);

    dropped_roots[0] = table->primary_index->root_page_num;
    dropped_roots[1] = table->secondary_indexes[0]->root_page_num;

    ASSERT(table_drop(table, test_pager.pager));
    table = NULL;

    // Pages are reused as they're released after physical page drop
    ASSERT(pager_allocate_page(test_pager.pager, &reallocated_pages[0]));

    ASSERT(pager_allocate_page(test_pager.pager, &reallocated_pages[1]));

    ASSERT(contains_page_num(reallocated_pages, 2, dropped_roots[0]));

    ASSERT(contains_page_num(reallocated_pages, 2, dropped_roots[1]));

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_table_drop_rejects_unmaterialized_table() {
    int result = 1;
    TestPager test_pager = {0};
    Table *table = NULL;

    ASSERT(create_test_pager(&test_pager));

    table = create_test_table();
    ASSERT(table != NULL);

    ASSERT(!table->is_materialized);
    ASSERT(table->primary_index->root_page_num == INVALID_ROOT_PAGE);
    ASSERT(table->secondary_indexes[0]->root_page_num == INVALID_ROOT_PAGE);

    ASSERT(!table_drop(table, test_pager.pager));

    // Rejection must preserve all logical metadata.
    ASSERT(!table->is_materialized);
    ASSERT(table->primary_index != NULL);
    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes[0] != NULL);
    ASSERT(table->table_schema != NULL);

    result = 0;

cleanup:
    if (table) {
        table_free(table);
    }

    destroy_test_pager(&test_pager);
    return result;
}

static int test_table_drop_invalid_arguments() {
    int result = 1;
    TestPager test_pager = {0};
    Table *table = NULL;

    ASSERT(create_test_pager(&test_pager));

    table = create_test_physical_table(test_pager.pager);
    ASSERT(table != NULL);

    ASSERT(!table_drop(NULL, test_pager.pager));
    ASSERT(!table_drop(table, NULL));

    Pager invalid_pager = {0};
    ASSERT(!table_drop(table, &invalid_pager));

    // Table should be intact after failed drop operation
    ASSERT(table->is_materialized);
    ASSERT(table->primary_index != NULL);
    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes[0] != NULL);

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_drop_rmv_secondary_index() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;
    IndexKey *key = NULL;

    uint32_t age_column[] = {2};

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    // Firstly, create the test table with the to-be-removed secondary index
    table = table_create("users", input_schema, test_pager.pager);

    ASSERT(table != NULL);
    ASSERT(table->total_secondary_indexes == 1);

    key = index_key_create(age_column, 1);
    ASSERT(key != NULL);

    ASSERT(table_create_index(table, "idx_users_age", SECONDARY_INDEX, key, test_pager.pager, false));

    ASSERT(table->total_secondary_indexes == 2);
    ASSERT(strcmp(table->secondary_indexes[0]->name, "uq_users_email") == 0);
    ASSERT(strcmp(table->secondary_indexes[1]->name, "idx_users_age") == 0);

    // Drop the 1st secondary index that was created with the initial standard schema
    ASSERT(table_drop_index(table, "uq_users_email", test_pager.pager));

    // Verify it doesn't exist in the secondary index array
    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table_find_index(table, "uq_users_email") == NULL);

    ASSERT(table->secondary_indexes[0] != NULL);
    
    // Verify that the 2nd secondary index has been shifted in the first position
    ASSERT(strcmp(table->secondary_indexes[0]->name, "idx_users_age") == 0);
    ASSERT(table->secondary_indexes[1] == NULL);
    
    result = 0;

cleanup:
    if (key) {
        index_key_free(key);        
    }

    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_drop_rmv_secondary_index_reuse_root_page() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;
    IndexKey *key = NULL;

    Index *age_index = NULL;

    uint32_t age_column[] = {2};
    uint32_t dropped_root_page = 0;
    uint32_t reallocated_page = 0;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("users", input_schema, test_pager.pager);

    ASSERT(table != NULL);

    // Add secondary inex
    key = index_key_create(age_column, 1);
    ASSERT(key != NULL);

    ASSERT(table_create_index(table, "idx_users_age", SECONDARY_INDEX, key, test_pager.pager, false));

    age_index = table_find_index(table, "idx_users_age");

    ASSERT(age_index != NULL);

    dropped_root_page = age_index->root_page_num;

    // Then drop it
    ASSERT(table_drop_index(table, "idx_users_age", test_pager.pager));

    ASSERT(table_find_index(table, "idx_users_age") == NULL);

    // After that, allocate a new page and make sure the dropped index's 
    // released root page was reallocated
    ASSERT(pager_allocate_page(test_pager.pager, &reallocated_page));

    ASSERT(reallocated_page == dropped_root_page);

    result = 0;

cleanup:
    if (key) {
        index_key_free(key);
    }

    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {

            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_drop_releases_all_remaining_pages() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;
    IndexKey *key = NULL;

    uint32_t age_column[] = {2};

    uint32_t dropped_roots[3] = {0};
    uint32_t reallocated_pages[3] = {0};

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("users", input_schema, test_pager.pager);
    ASSERT(table != NULL);

    key = index_key_create(age_column, 1);
    ASSERT(key != NULL);

    ASSERT(table_create_index(table, "idx_users_age", SECONDARY_INDEX, key, test_pager.pager, false));

    ASSERT(table->total_secondary_indexes == 2);

    // Track the root page numbers
    dropped_roots[0] = table->primary_index->root_page_num;
    dropped_roots[1] = table->secondary_indexes[0]->root_page_num;
    dropped_roots[2] = table->secondary_indexes[1]->root_page_num;

    ASSERT(table_drop(table, test_pager.pager));

    table = NULL;

    // Reallocate released pages,
    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(pager_allocate_page(test_pager.pager, &reallocated_pages[i]));
    }

    // And verify they're the dropped root numbers
    ASSERT(contains_page_num(reallocated_pages, 3, dropped_roots[0]));
    ASSERT(contains_page_num(reallocated_pages, 3, dropped_roots[1]));
    ASSERT(contains_page_num(reallocated_pages, 3, dropped_roots[2]));

    result = 0;

cleanup:
    if (key) {
        index_key_free(key);
    }

    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {

            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


/* ---------- table_truncate unit tests ---------- */

static int test_table_truncate_success() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;
    
    TestBTree primary_tree = {0};
    TestBTree secondary_tree = {0};

    uint32_t original_primary_root = 0;
    uint32_t original_secondary_root = 0;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("users", input_schema, test_pager.pager);
    ASSERT(table != NULL);

    original_primary_root = table->primary_index->root_page_num;
    original_secondary_root = table->secondary_indexes[0]->root_page_num;

    // Create secondary indexes
    ASSERT(build_test_btree(table->primary_index, test_pager.pager, &primary_tree));
    ASSERT(build_test_btree(table->secondary_indexes[0], test_pager.pager, &secondary_tree));

    table->row_count = 25;

    // Truncate the table
    ASSERT(table_truncate(table, test_pager.pager));

    ASSERT(table->row_count == 0);

    // Validate that the root page numbers are unchanged
    ASSERT(table->primary_index->root_page_num == original_primary_root);
    ASSERT(table->secondary_indexes[0]->root_page_num == original_secondary_root);
    
    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {

            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_truncate_preserves_indexes() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;

    TestBTree primary_tree = {0};
    TestBTree secondary_tree = {0};

    Index *original_primary = NULL;
    Index *original_secondary = NULL;

    uint32_t primary_root = 0;
    uint32_t secondary_root = 0;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("users", input_schema, test_pager.pager);
    ASSERT(table != NULL);

    original_primary = table->primary_index;
    original_secondary = table->secondary_indexes[0];

    primary_root = original_primary->root_page_num;
    secondary_root = original_secondary->root_page_num;

    // Create secondary indexes
    ASSERT(build_test_btree(original_primary, test_pager.pager, &primary_tree));
    ASSERT(build_test_btree(original_secondary, test_pager.pager, &secondary_tree));

    // Truncate table
    ASSERT(table_truncate(table, test_pager.pager));


    // Verify indexes and their metadata still exist
    ASSERT(table->primary_index == original_primary);
    ASSERT(table->secondary_indexes[0] == original_secondary);

    ASSERT(table->total_secondary_indexes == 1);

    ASSERT(strcmp(table->primary_index->name, "pk_users") == 0);
    ASSERT(strcmp(table->secondary_indexes[0]->name, "uq_users_email") == 0);

    ASSERT(table->primary_index->root_page_num == primary_root);
    ASSERT(table->secondary_indexes[0]->root_page_num == secondary_root);

    // Verify that the root pages still exist and are empty B+ Tree nodes
    Page *primary_page = pager_get_page(test_pager.pager, primary_root);
    Page *secondary_page = pager_get_page(test_pager.pager, secondary_root);

    ASSERT(primary_page != NULL);
    ASSERT(secondary_page != NULL);

    ASSERT(is_empty_btree_root(primary_page));
    ASSERT(is_empty_btree_root(secondary_page));

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {

            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}


static int test_table_truncate_resets_row_count() {
    int result = 1;

    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("users", input_schema, test_pager.pager);
    ASSERT(table != NULL);

    table->row_count = 42;

    // Testing only if the table count resets to 0
    ASSERT(table_truncate(table, test_pager.pager));

    ASSERT(table->row_count == 0);

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized &&
            table_drop(table, test_pager.pager)) {

            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

    destroy_test_pager(&test_pager);
    return result;
}

/* ---------- pager persistence integration test ---------- */

static int test_table_persistence_after_reopen() {
    int result = 1;
    
    TestPager test_pager = {0};
    Schema *input_schema = NULL;
    Table *table = NULL;

    uint32_t primary_root_num = 0;
    uint32_t secondary_root_num = 0;

    ASSERT(create_test_pager(&test_pager));

    input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    table = table_create("users", input_schema, test_pager.pager);
    ASSERT(table != NULL);

    primary_root_num = table->primary_index->root_page_num;
    secondary_root_num = table->secondary_indexes[0]->root_page_num;

    // Close the pager and reopen it
    ASSERT(pager_close(test_pager.pager));
    test_pager.pager = NULL;

    test_pager.pager = pager_open(test_pager.path);
    ASSERT(test_pager.pager != NULL);

    // Load the root pages to the Pager and verify they're empty
    Page *primary_root = pager_get_page(test_pager.pager, primary_root_num);
    ASSERT(primary_root != NULL);
    ASSERT(is_empty_btree_root(primary_root));

    Page *secondary_root = pager_get_page(test_pager.pager, secondary_root_num);
    ASSERT(secondary_root != NULL);
    ASSERT(is_empty_btree_root(secondary_root));

    result = 0;

cleanup:
    if (table) {
        if (table->is_materialized && table_drop(table, test_pager.pager)) {
            table = NULL;
        }

        if (table) {
            table_free(table);
        }
    }

    if (input_schema) {
        schema_free(input_schema);
    }

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

    /* ---------- table_create unit tests ---------- */
    result = test_table_create_success();
    generate_output(result, 0, "test_table_create_success");
    result = test_table_create_success_without_constraints();
    generate_output(result, 1, "test_table_create_without_constraints");
    result = test_table_create_empty_roots();
    generate_output(result, 2, "test_table_create_empty_roots");
    result = test_table_create_invalid_arguments();
    generate_output(result, 3, "test_table_create_invalid_arguments");
    result = test_table_create_add_secondary_index();
    generate_output(result, 4, "test_table_create_add_secondary_index");

    /* ---------- table_materialize unit tests ---------- */
    result = test_table_materialize_success();
    generate_output(result, 5, "test_table_materialize_success");
    result = test_table_materialize_without_constraints();
    generate_output(result, 6, "test_table_materialize_without_constraints");
    result = test_table_materialize_empty_roots();
    generate_output(result, 7, "test_table_materialize_empty_roots");
    result = test_table_materialize_rejects_second_call();
    generate_output(result, 8, "test_table_materialize_rejects_second_call");
    result = test_table_materialize_preserves_metadata();
    generate_output(result, 9, "test_table_materialize_preserves_metadata");
    result = test_table_materialize_invalid_arguments();
    generate_output(result, 10, "test_table_materialize_invalid_arguments");

    /* ---------- table_drop unit tests ---------- */
    result = test_table_drop_success();
    generate_output(result, 11, "test_table_drop_success");
    result = test_table_drop_reuses_root_pages();
    generate_output(result, 12, "test_table_drop_reuses_root_pages");
    result = test_table_drop_rejects_unmaterialized_table();
    generate_output(result, 13, "test_table_drop_rejects_unmaterialized_table");
    result = test_table_drop_invalid_arguments();
    generate_output(result, 14, "test_table_drop_invalid_arguments");
    result = test_table_drop_rmv_secondary_index();
    generate_output(result, 15, "test_table_drop_rmv_secondary_index");
    result = test_table_drop_rmv_secondary_index_reuse_root_page();
    generate_output(result, 16, "test_table_drop_rmv_secondary_index_reuse_root_page");
    result = test_table_drop_releases_all_remaining_pages();
    generate_output(result, 17, "test_table_drop_releases_all_remaining_pages");
    
    /* ---------- table_truncate unit tests ---------- */
    result = test_table_truncate_success();
    generate_output(result, 18, "test_table_truncate_success");
    result = test_table_truncate_preserves_indexes();
    generate_output(result, 19, "test_table_truncate_preserves_indexes");
    result = test_table_truncate_resets_row_count();
    generate_output(result, 20, "test_table_truncate_resets_row_count");

    /* ---------- pager persistence integration test ---------- */
    result = test_table_persistence_after_reopen();
    generate_output(result, 21, "test_table_persistence_after_reopen");

    return 0;
}