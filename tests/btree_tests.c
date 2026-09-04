#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "../include/btree.h"
#include "../src/btree/btree_utils.h"
#include "../include/page.h"
#include "../include/index.h"
#include "../include/row.h"
#include "../src/data_types/data_types_utils.h"
#include "../include/serialize.h"
#include "../include/schema.h"
#include "../include/constraints.h"

#define ASSERT(condition) { \
    if (!(condition)) { \
        return 1; \
    } \
}

typedef struct btree_test_context {
    BTree *btree;
    Pager *pager;
    Index *index;
    BTreeIndexSpec *index_spec;
    Schema *schema;
} BTreeTestContext;

static void free_cell_contents(BTreeCellContents *cell_contents) {
    if (!cell_contents) {
        return;
    }

    if (cell_contents->keys) {
        value_free_array(cell_contents->keys, cell_contents->num_keys);
        cell_contents->keys = NULL;
    }

    if (cell_contents->type == BTREE_LEAF_NODE
        && cell_contents->BTreePayload.row) {
        row_free(cell_contents->BTreePayload.row);
        cell_contents->BTreePayload.row = NULL;
    }

    cell_contents->num_keys = 0;
    cell_contents->key_size = 0;
    cell_contents->cell_size = 0;
}

static void btree_test_context_free(BTreeTestContext *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->pager) {
        pager_close(ctx->pager);
        ctx->pager = NULL;
    }

    if (ctx->index) {
        index_free(ctx->index);
        ctx->index = NULL;
    }

    if (ctx->schema) {
        schema_free(ctx->schema);
        ctx->schema = NULL;
    }

    if (ctx->index_spec) {
        index_btree_spec_free(ctx->index_spec);
        free(ctx->index_spec);
        ctx->index_spec = NULL;
    }

    if (ctx->btree) {
        free(ctx->btree);
        ctx->btree = NULL;
    }

}

static bool timestamp_to_unix(timestamp_t *timestamp, uint64_t *result) {
    if (!timestamp || !result) {
        return false;
    }

    struct tm tm_value = {0};
    tm_value.tm_year = (int) timestamp->year - 1900;
    tm_value.tm_mon = (int) timestamp->month - 1;
    tm_value.tm_mday = timestamp->day;
    tm_value.tm_hour = timestamp->hour;
    tm_value.tm_min  = timestamp->minute;
    tm_value.tm_sec  = timestamp->second;

    __time64_t unix_time = _mkgmtime64(&tm_value);
    if (unix_time == (__time64_t)-1) {
        return false;
    }

    *result = (uint64_t) unix_time;
    return true;
}

static bool btree_test_context_init(BTreeTestContext *ctx, const char *pathname) {
    if (!ctx || !pathname) {
        return false;
    }
    *ctx = (BTreeTestContext){0};

    /* --- PAGER --- */
    ctx->pager = pager_open(pathname);
    if (!ctx->pager) {
        goto cleanup;
    }

    /* --- INDEX --- */
    uint32_t amount_of_key_columns = 3;
    uint32_t key_columns[] = {5, 1, 4};
    bool is_unique = true;

    IndexKey *index_key = index_key_create(key_columns, amount_of_key_columns);
    if (!index_key) {
        goto cleanup;
    }

    ctx->index = index_create("Index", SECONDARY_INDEX, index_key, ctx->pager, is_unique);
    if (!ctx->index) {
        index_key_free(index_key);
        goto cleanup;
    }
    index_key_free(index_key);

    /* --- SCHEMA --- */

    uint32_t amount_columns = 6;
    Column **columns = calloc(amount_columns, sizeof(Column *));
    if (!columns) {
        goto cleanup;
    }

    Constraint **constraints = calloc(amount_columns, sizeof(Constraint *));
    if (!constraints) {
        goto cleanup;
    }
    
    char column_name[64] = {0};
    DataType column_data_types[] = {UNSIGNED_INTEGER, VARCHAR, VARCHAR, NUMERIC, TIMESTAMP, BOOL};
    uint32_t type_parameters[] = {0, 64, 128, 0, 0, 0};
    for (uint32_t i = 0; i < amount_columns; i++) {
        snprintf(column_name, sizeof(column_name), "%s%u", "col", i);

        columns[i] = column_alloc(column_name, column_data_types[i], type_parameters[i], 0, 0);
        if (!columns[i]) {
            goto cleanup;
        }
    }

    uint32_t primary_columns[] = {0};
    constraints[0] = constraint_create_primary_key("PRIMARY_KEY_CONSTRAINT_0", primary_columns, 1);
    if (!constraints[0]) { goto cleanup; }

    constraints[1] = constraint_create_not_null("NOT_NULL_CONSTRAINT_1_00", 1);
    if (!constraints[1]) { goto cleanup; }
    constraints[2] = constraint_create_not_null("NOT_NULL_CONSTRAINT_1_01", 2);
    if (!constraints[2]) { goto cleanup; }
    constraints[3] = constraint_create_not_null("NOT_NULL_CONSTRAINT_1_02", 4);
    if (!constraints[3]) { goto cleanup; }
    constraints[4] = constraint_create_not_null("NOT_NULL_CONSTRAINT_1_03", 5);
    if (!constraints[4]) { goto cleanup; }

    uint32_t unique_columns[] = {2};
    constraints[5] = constraint_create_unique("UNIQUE_COLUMNS_2", unique_columns, 1);
    if (!constraints[5]) { goto cleanup; }


    ctx->schema = schema_create(columns, constraints, amount_columns, amount_columns);
    if (!ctx->schema) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < 6; i++) {
        free(columns[i]);
        constraint_free(constraints[i]);
    }
    free(columns);
    free(constraints);
    /* --- INDEX SPEC --- */

    ctx->index_spec = (BTreeIndexSpec *) calloc(1, sizeof(BTreeIndexSpec));
    if (!ctx->index_spec) {
        goto cleanup;
    }

    if (!btree_index_spec_init(ctx->index, ctx->schema, ctx->index_spec)) {
        goto cleanup;
    }

    /* --- BTREE --- */

    ctx->btree = (BTree *) calloc(1, sizeof(BTree));
    if (!ctx->btree) {
        goto cleanup;
    }

    ctx->btree->pager = ctx->pager;
    ctx->btree->root_page_num = ctx->index->root_page_num;

    return true;
    
    cleanup:

    btree_test_context_free(ctx);
    return false;
}

static bool create_cell_contents(BTreeCellContents *cell_contents, BTreeIndexSpec *index_spec, uint32_t user_id) {
    if (!cell_contents || !index_spec) {
        return false;
    }

    cell_contents->type = BTREE_LEAF_NODE;
    cell_contents->num_keys = index_spec->index_key->num_columns;

    cell_contents->BTreePayload.row = (Row *) calloc(1, sizeof(Row));
    if (!cell_contents->BTreePayload.row) {
        goto cleanup;
    }
    cell_contents->BTreePayload.row->is_deleted = false;
    cell_contents->BTreePayload.row->n_columns = index_spec->schema->num_columns;

    cell_contents->BTreePayload.row->values = (Value **) calloc(index_spec->schema->num_columns, sizeof(Value *));
    if (!cell_contents->BTreePayload.row->values) {
        goto cleanup;
    }    

    // id: UNSIGNED INTEGER | PRIMARY_KEY
    cell_contents->BTreePayload.row->values[0] = value_create(index_spec->schema->columns[0]->type, &user_id);
    if (!cell_contents->BTreePayload.row->values[0]) {
        goto cleanup;
    }

    // username: VARCHAR | NOT_NULL
    char buffer[128] = {0};
    snprintf(buffer, 128, "%s%u", "user", (user_id % 5));
    varchar_n_t username_varchar = {0};
    username_varchar.max_n = 64;
    username_varchar.string = buffer;

    cell_contents->BTreePayload.row->values[1] = value_create(index_spec->schema->columns[1]->type, &username_varchar);
    if (!cell_contents->BTreePayload.row->values[1]) {
        goto cleanup;
    }

    // email: VARCHAR | UNIQUE | NOT_NULL
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, 128, "%s%u@email.com", "user", user_id);
    varchar_n_t email_varchar = {0};
    email_varchar.max_n = 128;
    email_varchar.string = buffer;

    cell_contents->BTreePayload.row->values[2] = value_create(index_spec->schema->columns[2]->type, &email_varchar);
    if (!cell_contents->BTreePayload.row->values[2]) {
        goto cleanup;
    }

    // salary: NUMERIC
    numeric_t numeric = {0};
    numeric.scale = 2;
    numeric.val = user_id*120000 + user_id*7500; 

    cell_contents->BTreePayload.row->values[3] = value_create(index_spec->schema->columns[3]->type, &numeric);
    if (!cell_contents->BTreePayload.row->values[3]) {
        goto cleanup;
    }

    // created_at: TIMESTAMP | NOT_NULL
    timestamp_t timestamp = {0};
    timestamp.year = 2026 - (user_id % 10);
    timestamp.month  = 1 + (user_id % 12);
    timestamp.day    = 1 + ((user_id * 3) % 28);
    timestamp.hour   = (user_id * 5) % 24;
    timestamp.minute = (user_id * 2) % 60;
    timestamp.second = (user_id * 7) % 60;

    uint64_t unix_time = 0;
    if (!timestamp_to_unix(&timestamp, &unix_time)) {
        goto cleanup;
    }

    cell_contents->BTreePayload.row->values[4] = value_create(index_spec->schema->columns[4]->type, &unix_time);
    if (!cell_contents->BTreePayload.row->values[4]) {
        goto cleanup;
    }

    // active: BOOL
    bool val = (user_id % 2);

    cell_contents->BTreePayload.row->values[5] = value_create(index_spec->schema->columns[5]->type, &val);
    if (!cell_contents->BTreePayload.row->values[5]) {
        goto cleanup;
    }

    cell_contents->keys = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));
    if (!cell_contents->keys) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < cell_contents->num_keys; i++) {
        cell_contents->keys[i] = value_copy(cell_contents->BTreePayload.row->values[index_spec->index_key->column_index_array[i]]);
        if (!cell_contents->keys[i]) {
            goto cleanup;
        }
    }

    cell_contents->key_size = index_spec->key_size;

    uint32_t bitmap_size = (index_spec->schema->num_columns + 7) / 8;
    cell_contents->cell_size = cell_contents->key_size + bitmap_size
                                + sizeof(uint8_t) + sizeof(uint32_t);
    for (uint32_t i = 0; i < index_spec->schema->num_columns; i++) {
        cell_contents->cell_size += get_serialized_column_size(index_spec, i);
    }
    
    return true;

    cleanup:

    free_cell_contents(cell_contents);
    return false;
}

static bool search_key_init(BTreeSearchKey *search_key, BTreeIndexSpec *index_spec,
    Value **target_key_vals, uint32_t num_target_keys) {
    if (!search_key || !index_spec || !target_key_vals
        || !num_target_keys
        || num_target_keys > index_spec->index_key->num_columns) {
        return false;
    }

    if (search_key->target_key) {
        free(search_key->target_key);
        search_key->target_key = NULL;
    }

    search_key->index = index_spec;
    search_key->num_target_keys = num_target_keys;
    search_key->target_key = values_to_serialized_key(target_key_vals, num_target_keys, index_spec);
    if (!search_key->target_key) {
        return false;
    }

    return true;
}

static bool create_index_key_values(Value **key_values, BTreeIndexSpec *index_spec,
    bool active, const char *username, uint64_t created_at) {
    if (!key_values || !index_spec || !username
        || index_spec->index_key->num_columns != 3) {
        return false;
    }

    varchar_n_t username_varchar = {0};
    username_varchar.max_n = get_key_column(index_spec, 1)->type_parameter;
    username_varchar.string = (char *) username;

    key_values[0] = value_create(get_key_column(index_spec, 0)->type, &active);
    if (!key_values[0]) {
        goto cleanup;
    }

    key_values[1] = value_create(get_key_column(index_spec, 1)->type, &username_varchar);
    if (!key_values[1]) {
        goto cleanup;
    }

    key_values[2] = value_create(get_key_column(index_spec, 2)->type, &created_at);
    if (!key_values[2]) {
        goto cleanup;
    }

    return true;

    cleanup:

    for (uint32_t i = 0; i < index_spec->index_key->num_columns; i++) {
        if (key_values[i]) {
            value_free(key_values[i]);
            key_values[i] = NULL;
        }
    }
    return false;
}

static bool set_cell_index_values(BTreeCellContents *cell_contents, BTreeIndexSpec *index_spec,
    bool active, const char *username, uint64_t created_at) {
    if (!cell_contents || !cell_contents->keys
        || !cell_contents->BTreePayload.row
        || !cell_contents->BTreePayload.row->values
        || !index_spec || !username
        || cell_contents->num_keys != index_spec->index_key->num_columns) {
        return false;
    }

    Value **new_keys = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));
    if (!new_keys) {
        return false;
    }

    Value **new_row_values = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));
    if (!new_row_values) {
        free(new_keys);
        return false;
    }

    if (!create_index_key_values(new_keys, index_spec, active, username, created_at)) {
        free(new_keys);
        free(new_row_values);
        return false;
    }

    for (uint32_t i = 0; i < cell_contents->num_keys; i++) {
        new_row_values[i] = value_copy(new_keys[i]);
        if (!new_row_values[i]) {
            value_free_array(new_keys, cell_contents->num_keys);
            value_free_array(new_row_values, cell_contents->num_keys);
            return false;
        }
    }

    for (uint32_t i = 0; i < cell_contents->num_keys; i++) {
        uint32_t column_index = index_spec->index_key->column_index_array[i];

        value_free(cell_contents->keys[i]);
        cell_contents->keys[i] = new_keys[i];
        new_keys[i] = NULL;

        value_free(cell_contents->BTreePayload.row->values[column_index]);
        cell_contents->BTreePayload.row->values[column_index] = new_row_values[i];
        new_row_values[i] = NULL;
    }

    free(new_keys);
    free(new_row_values);
    return true;
}

static bool create_ordered_cell_contents(BTreeCellContents *cell_contents, BTreeIndexSpec *index_spec,
    uint32_t user_id, uint32_t key_order) {
    if (!create_cell_contents(cell_contents, index_spec, user_id)) {
        return false;
    }

    char username[64] = {0};
    snprintf(username, sizeof(username), "user%08u", key_order);

    bool active = false;
    uint64_t created_at = 1700000000ULL + key_order;

    if (!set_cell_index_values(cell_contents, index_spec, active, username, created_at)) {
        free_cell_contents(cell_contents);
        return false;
    }

    return true;
}

static bool ordered_search_key_init(BTreeSearchKey *search_key, BTreeIndexSpec *index_spec,
    uint32_t key_order) {
    if (!search_key || !index_spec) {
        return false;
    }

    Value **key_values = (Value **) calloc(index_spec->index_key->num_columns, sizeof(Value *));
    if (!key_values) {
        return false;
    }

    char username[64] = {0};
    snprintf(username, sizeof(username), "user%08u", key_order);

    bool active = false;
    uint64_t created_at = 1700000000ULL + key_order;

    if (!create_index_key_values(key_values, index_spec, active, username, created_at)) {
        free(key_values);
        return false;
    }

    bool result = search_key_init(search_key, index_spec, key_values, index_spec->index_key->num_columns);
    value_free_array(key_values, index_spec->index_key->num_columns);

    return result;
}

static bool search_key_from_user_id(BTreeSearchKey *search_key, BTreeTestContext *ctx, uint32_t user_id) {
    if (!search_key || !ctx) {
        return false;
    }

    BTreeCellContents cell_contents = {0};
    if (!create_cell_contents(&cell_contents, ctx->index_spec, user_id)) {
        return false;
    }

    bool result = search_key_init(search_key, ctx->index_spec, cell_contents.keys, cell_contents.num_keys);
    free_cell_contents(&cell_contents);

    return result;
}

static bool insert_ordered_cell(BTreeTestContext *ctx, uint32_t user_id, uint32_t key_order,
    BTreeInsertionResult *insertion_res) {
    if (!ctx || !insertion_res) {
        return false;
    }

    BTreeCellContents cell_contents = {0};
    if (!create_ordered_cell_contents(&cell_contents, ctx->index_spec, user_id, key_order)) {
        return false;
    }

    BTreeStatus status = btree_insert(ctx->btree, &cell_contents, insertion_res, ctx->index_spec);
    free_cell_contents(&cell_contents);

    return status == BTREE_SUCCESS;
}

static bool insert_cell_into_page(BTreeTestContext *ctx, uint32_t page_num, uint32_t user_id) {
    if (!ctx || page_num >= MAX_PAGES) {
        return false;
    }

    BTreeCellContents cell_to_be_inserted = {0};
    if (!create_cell_contents(&cell_to_be_inserted, ctx->index_spec, user_id)) {
        return false;
    }

    Page *page = pager_get_page(ctx->pager, page_num);
    if (!page) {
        free_cell_contents(&cell_to_be_inserted);
        return false;
    }

    BTreePage btree_page = {0};
    BTreeStatus status = btree_page_attach_load_validate(ctx->pager, &btree_page, page, ctx->index_spec);
    if (status != BTREE_SUCCESS) {
        free_cell_contents(&cell_to_be_inserted);
        return false;
    }

    BTreeSearchResult search_result = {0};
    search_result.result_index = btree_page.cell_count;

    status = insert_cell(ctx->pager, &btree_page, &cell_to_be_inserted, search_result.result_index, ctx->index_spec);
    if (status != BTREE_SUCCESS) {
        free_cell_contents(&cell_to_be_inserted);
        return false;
    }

    if (!btree_page_sync(ctx->pager, &btree_page)) {
        free_cell_contents(&cell_to_be_inserted);
        return false;
    }

    free_cell_contents(&cell_to_be_inserted);
    return true;
}

static bool get_btree_height(BTreeTestContext *ctx, uint32_t *height) {
    if (!ctx || !ctx->btree || !ctx->pager || !height) {
        return false;
    }

    *height = 0;

    Page *page = pager_get_page(ctx->pager, ctx->btree->root_page_num);
    if (!page) {
        return false;
    }

    while (true) {
        BTreePage btree_page = {0};
        BTreeStatus status = btree_page_attach_load_validate(ctx->pager, &btree_page, page, ctx->index_spec);
        if (status != BTREE_SUCCESS) {
            return false;
        }

        (*height)++;

        if (btree_page.type == BTREE_LEAF_NODE) {
            return true;
        }

        if (*height >= MAX_PAGES) {
            return false;
        }

        page = pager_get_page(ctx->pager, btree_page.type_specific_data.rightmost_child_pointer);
        if (!page) {
            return false;
        }
    }
}

static bool create_empty_leaf_child(BTreeTestContext *ctx, uint32_t parent_page_num, uint32_t *page_num) {
    if (!ctx || !page_num) {
        return false;
    }

    if (!pager_allocate_page(ctx->pager, page_num)) {
        return false;
    }

    Page *page = pager_get_page(ctx->pager, *page_num);
    if (!page) {
        return false;
    }

    BTreePage leaf = {0};
    btree_page_attach(&leaf, page);

    BTreeStatus status = btree_page_init_empty_leaf(&leaf);
    if (status != BTREE_SUCCESS) {
        return false;
    }

    leaf.is_root = false;
    leaf.parent_pointer = parent_page_num;

    if (!btree_page_sync(ctx->pager, &leaf)) {
        return false;
    }

    return true;
}

static bool create_internal_cell_contents(BTreeCellContents *cell_contents, BTreeIndexSpec *index_spec,
    uint32_t key_order, uint32_t child_page_num) {
    if (!cell_contents || !index_spec
        || child_page_num <= SYSTEM_CATALOG_PAGE_NUM
        || child_page_num >= MAX_PAGES) {
        return false;
    }

    cell_contents->type = BTREE_INTERNAL_NODE;
    cell_contents->num_keys = index_spec->index_key->num_columns;
    cell_contents->key_size = index_spec->key_size;
    cell_contents->cell_size = cell_contents->key_size + sizeof(uint32_t);
    cell_contents->BTreePayload.child_pointer = child_page_num;

    cell_contents->keys = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));
    if (!cell_contents->keys) {
        return false;
    }

    char username[64] = {0};
    snprintf(username, sizeof(username), "user%08u", key_order);

    bool active = false;
    uint64_t created_at = 1700000000ULL + key_order;

    if (!create_index_key_values(cell_contents->keys, index_spec, active, username, created_at)) {
        free(cell_contents->keys);
        cell_contents->keys = NULL;
        return false;
    }

    return true;
}


static bool initialize_test_internal_page(BTreeTestContext *ctx, uint32_t page_num,
    uint32_t parent_page_num, bool is_root, uint32_t rightmost_child_page_num) {
    if (!ctx || page_num >= MAX_PAGES
        || rightmost_child_page_num <= SYSTEM_CATALOG_PAGE_NUM
        || rightmost_child_page_num >= ctx->pager->num_pages) {
        return false;
    }

    Page *page = pager_get_page(ctx->pager, page_num);
    if (!page) {
        return false;
    }

    if (!page_clear(ctx->pager, page)) {
        return false;
    }

    BTreePage internal = {0};
    btree_page_attach(&internal, page);

    BTreeStatus status = btree_page_init_internal(&internal, rightmost_child_page_num);
    if (status != BTREE_SUCCESS) {
        return false;
    }

    internal.is_root = is_root;
    internal.parent_pointer = is_root ? UINT32_MAX : parent_page_num;

    if (!btree_page_sync(ctx->pager, &internal)) {
        return false;
    }

    return true;
}

static bool append_test_internal_cell(BTreeTestContext *ctx, BTreePage *internal,
    uint32_t key_order, uint32_t left_child_page_num, uint32_t right_child_page_num) {
    if (!ctx || !internal || !internal->page || !internal->data
        || internal->type != BTREE_INTERNAL_NODE
        || left_child_page_num <= SYSTEM_CATALOG_PAGE_NUM
        || right_child_page_num <= SYSTEM_CATALOG_PAGE_NUM
        || left_child_page_num >= ctx->pager->num_pages
        || right_child_page_num >= ctx->pager->num_pages) {
        return false;
    }

    BTreeCellContents cell_contents = {0};
    if (!create_internal_cell_contents(
            &cell_contents,
            ctx->index_spec,
            key_order,
            left_child_page_num)) {
        return false;
    }

    internal->type_specific_data.rightmost_child_pointer = right_child_page_num;

    BTreeSearchResult search_result = {0};
    search_result.result_index = internal->cell_count;

    BTreeStatus status = insert_cell(
        ctx->pager,
        internal,
        &cell_contents,
        search_result.result_index,
        ctx->index_spec
    );

    free_cell_contents(&cell_contents);

    if (status != BTREE_SUCCESS) {
        return false;
    }

    if (!btree_page_sync(ctx->pager, internal)) {
        return false;
    }

    return true;
}

static bool append_test_leaf_cell(BTreeTestContext *ctx, BTreePage *leaf,
    uint32_t user_id, uint32_t key_order) {
    if (!ctx || !leaf || !leaf->page || !leaf->data
        || leaf->type != BTREE_LEAF_NODE) {
        return false;
    }

    BTreeCellContents cell_contents = {0};
    if (!create_ordered_cell_contents(&cell_contents, ctx->index_spec, user_id, key_order)) {
        return false;
    }

    BTreeSearchResult search_result = {0};
    search_result.result_index = leaf->cell_count;

    BTreeStatus status = insert_cell(ctx->pager, leaf, &cell_contents, search_result.result_index, ctx->index_spec);
    free_cell_contents(&cell_contents);

    if (status != BTREE_SUCCESS) {
        return false;
    }

    if (!btree_page_sync(ctx->pager, leaf)) {
        return false;
    }

    return true;
}

static bool page_cell_matches_key_order(BTreePage *btree_page, BTreeIndexSpec *index_spec,
    uint32_t cell_index, uint32_t key_order) {
    if (!btree_page || !index_spec || cell_index >= btree_page->cell_count) {
        return false;
    }

    BTreeCellContents cell_contents = {0};
    BTreeStatus status = get_cell_contents(btree_page, cell_index, &cell_contents, index_spec);
    if (status != BTREE_SUCCESS) {
        return false;
    }

    Value **expected_keys = (Value **) calloc(index_spec->index_key->num_columns, sizeof(Value *));
    if (!expected_keys) {
        free_cell_contents(&cell_contents);
        return false;
    }

    char username[64] = {0};
    snprintf(username, sizeof(username), "user%08u", key_order);

    bool active = false;
    uint64_t created_at = 1700000000ULL + key_order;

    if (!create_index_key_values(expected_keys, index_spec, active, username, created_at)) {
        free(expected_keys);
        free_cell_contents(&cell_contents);
        return false;
    }

    int result = 0;
    status = btree_compare(cell_contents.keys, expected_keys, index_spec->index_key->num_columns, &result);

    value_free_array(expected_keys, index_spec->index_key->num_columns);
    free_cell_contents(&cell_contents);

    return status == BTREE_SUCCESS && result == 0;
}

static bool get_test_free_list_head(BTreeTestContext *ctx, uint32_t *free_list_head) {
    if (!ctx || !free_list_head) {
        return false;
    }

    Page *zero = pager_get_page(ctx->pager, 0);
    if (!zero) {
        return false;
    }

    memcpy(free_list_head, zero->page_data + FREE_LIST_HEAD_OFFSET, sizeof(uint32_t));
    return true;
}

static bool build_two_level_test_tree(BTreeTestContext *ctx, uint32_t separator_key_order,
    uint32_t *left_leaf_page_num, uint32_t *right_leaf_page_num) {
    if (!ctx || !left_leaf_page_num || !right_leaf_page_num) {
        return false;
    }

    uint32_t root_page_num = ctx->btree->root_page_num;

    if (!create_empty_leaf_child(ctx, root_page_num, left_leaf_page_num)) {
        return false;
    }

    if (!create_empty_leaf_child(ctx, root_page_num, right_leaf_page_num)) {
        return false;
    }

    if (!initialize_test_internal_page(
            ctx,
            root_page_num,
            UINT32_MAX,
            true,
            *right_leaf_page_num)) {
        return false;
    }

    Page *root_page = pager_get_page(ctx->pager, root_page_num);
    if (!root_page) {
        return false;
    }

    BTreePage root = {0};
    BTreeStatus status = btree_page_attach_load_validate(
        ctx->pager,
        &root,
        root_page,
        ctx->index_spec
    );
    if (status != BTREE_SUCCESS) {
        return false;
    }

    if (!append_test_internal_cell(
            ctx,
            &root,
            separator_key_order,
            *left_leaf_page_num,
            *right_leaf_page_num)) {
        return false;
    }

    return true;
}

static bool expand_test_tree_to_three_levels(BTreeTestContext *ctx,
    uint32_t left_internal_page_num, uint32_t right_internal_page_num,
    uint32_t left_separator_key_order, uint32_t right_separator_key_order,
    uint32_t *rightmost_leaf_page_num) {
    if (!ctx || !rightmost_leaf_page_num) {
        return false;
    }

    uint32_t left_left_leaf = 0;
    uint32_t left_right_leaf = 0;
    uint32_t right_left_leaf = 0;
    uint32_t right_right_leaf = 0;

    if (!create_empty_leaf_child(ctx, left_internal_page_num, &left_left_leaf)
        || !create_empty_leaf_child(ctx, left_internal_page_num, &left_right_leaf)
        || !create_empty_leaf_child(ctx, right_internal_page_num, &right_left_leaf)
        || !create_empty_leaf_child(ctx, right_internal_page_num, &right_right_leaf)) {
        return false;
    }

    if (!initialize_test_internal_page(
            ctx,
            left_internal_page_num,
            ctx->btree->root_page_num,
            false,
            left_right_leaf)) {
        return false;
    }

    if (!initialize_test_internal_page(
            ctx,
            right_internal_page_num,
            ctx->btree->root_page_num,
            false,
            right_right_leaf)) {
        return false;
    }

    Page *left_page = pager_get_page(ctx->pager, left_internal_page_num);
    Page *right_page = pager_get_page(ctx->pager, right_internal_page_num);
    if (!left_page || !right_page) {
        return false;
    }

    BTreePage left_internal = {0};
    BTreePage right_internal = {0};

    BTreeStatus status = btree_page_attach_load_validate(
        ctx->pager,
        &left_internal,
        left_page,
        ctx->index_spec
    );
    if (status != BTREE_SUCCESS) {
        return false;
    }

    status = btree_page_attach_load_validate(
        ctx->pager,
        &right_internal,
        right_page,
        ctx->index_spec
    );
    if (status != BTREE_SUCCESS) {
        return false;
    }

    if (!append_test_internal_cell(
            ctx,
            &left_internal,
            left_separator_key_order,
            left_left_leaf,
            left_right_leaf)) {
        return false;
    }

    if (!append_test_internal_cell(
            ctx,
            &right_internal,
            right_separator_key_order,
            right_left_leaf,
            right_right_leaf)) {
        return false;
    }

    *rightmost_leaf_page_num = right_right_leaf;
    return true;
}

static bool build_full_internal_root(BTreeTestContext *ctx,
    uint32_t *rightmost_leaf_page_num, uint32_t *next_separator_key_order) {
    if (!ctx || !rightmost_leaf_page_num || !next_separator_key_order) {
        return false;
    }

    uint32_t root_page_num = ctx->btree->root_page_num;
    uint32_t first_leaf_page_num = 0;

    if (!create_empty_leaf_child(ctx, root_page_num, &first_leaf_page_num)) {
        return false;
    }

    if (!initialize_test_internal_page(
            ctx,
            root_page_num,
            UINT32_MAX,
            true,
            first_leaf_page_num)) {
        return false;
    }

    Page *root_page = pager_get_page(ctx->pager, root_page_num);
    if (!root_page) {
        return false;
    }

    BTreePage root = {0};
    BTreeStatus status = btree_page_attach_load_validate(
        ctx->pager,
        &root,
        root_page,
        ctx->index_spec
    );
    if (status != BTREE_SUCCESS) {
        return false;
    }

    uint16_t internal_cell_size = ctx->index_spec->key_size + sizeof(uint32_t);
    uint32_t key_order = 0;

    while (true) {
        status = btree_page_has_enough_space(&root, internal_cell_size);

        if (status == BTREE_NEEDS_SPLIT) {
            break;
        }

        if (status != BTREE_SUCCESS) {
            return false;
        }

        uint32_t old_rightmost_page_num =
            root.type_specific_data.rightmost_child_pointer;

        uint32_t new_rightmost_page_num = 0;
        if (!create_empty_leaf_child(
                ctx,
                root_page_num,
                &new_rightmost_page_num)) {
            return false;
        }

        if (!append_test_internal_cell(
                ctx,
                &root,
                key_order,
                old_rightmost_page_num,
                new_rightmost_page_num)) {
            return false;
        }

        key_order++;
    }

    if (root.cell_count == 0) {
        return false;
    }

    *rightmost_leaf_page_num =
        root.type_specific_data.rightmost_child_pointer;

    *next_separator_key_order = key_order;
    return true;
}

static bool verify_ordered_entries(BTreeSearchEntries *entries, BTreeIndexSpec *index_spec,
    uint32_t first_key_order, uint32_t expected_count) {
    if (!entries || !index_spec || entries->count != expected_count) {
        return false;
    }

    for (uint32_t i = 0; i < entries->count; i++) {
        Value **expected_keys = (Value **) calloc(index_spec->index_key->num_columns, sizeof(Value *));
        if (!expected_keys) {
            return false;
        }

        char username[64] = {0};
        snprintf(username, sizeof(username), "user%08u", first_key_order + i);

        bool active = false;
        uint64_t created_at = 1700000000ULL + first_key_order + i;

        if (!create_index_key_values(expected_keys, index_spec, active, username, created_at)) {
            free(expected_keys);
            return false;
        }

        int result = 0;
        BTreeStatus status = btree_compare(
            entries->entries[i].cell.keys,
            expected_keys,
            index_spec->index_key->num_columns,
            &result
        );

        value_free_array(expected_keys, index_spec->index_key->num_columns);

        if (status != BTREE_SUCCESS || result != 0) {
            return false;
        }

        if (i > 0) {
            result = 0;
            status = btree_compare(
                entries->entries[i-1].cell.keys,
                entries->entries[i].cell.keys,
                index_spec->index_key->num_columns,
                &result
            );

            if (status != BTREE_SUCCESS || result != -1) {
                return false;
            }
        }
    }

    return true;
}

static int test_empty_leaf_init() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database1.db"));

    BTreePage leaf = {0};
    Page *root_leaf_page = pager_get_page(ctx.btree->pager, ctx.btree->root_page_num);
    if (!root_leaf_page) { return -1; }

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &leaf, root_leaf_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(leaf.type == BTREE_LEAF_NODE);
    ASSERT(leaf.is_root == true);
    ASSERT(leaf.parent_pointer == UINT32_MAX);
    ASSERT(leaf.cell_count == 0);
    ASSERT(leaf.free_space_offset == PAGE_SIZE);
    ASSERT(leaf.type_specific_data.siblings.previous_leaf_pointer == UINT32_MAX);
    ASSERT(leaf.type_specific_data.siblings.next_leaf_pointer == UINT32_MAX);

    if (!btree_page_sync(ctx.pager, &leaf)) { return -1; }

    btree_test_context_free(&ctx);
    return 0;
}

static int test_empty_internal_init() {
    Pager *pager = pager_open("build/database2.db");
    if (!pager) { return -1; }

    uint32_t page_num = 0;
    if (!pager_allocate_page(pager, &page_num)) { return -1; }

    Page *page = pager_get_page(pager, page_num);
    if (!page) { return -1; }

    BTreePage internal = {0};

    btree_page_attach(&internal, page);
    BTreeStatus status = btree_page_init_internal(&internal, UINT32_MAX);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(internal.type == BTREE_INTERNAL_NODE);
    ASSERT(internal.is_root == 1);
    ASSERT(internal.parent_pointer == UINT32_MAX);
    ASSERT(internal.cell_count == 0);
    ASSERT(internal.free_space_offset == PAGE_SIZE);
    ASSERT(internal.type_specific_data.rightmost_child_pointer == UINT32_MAX);

    if (!btree_page_sync(pager, &internal)) { return -1; }
    if (!pager_close(pager)) { return -1; }

    return 0;
}

static int test_btree_binary_search() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database3.db"));

    Page *root_leaf_page = pager_get_page(ctx.btree->pager, ctx.btree->root_page_num);
    if (!root_leaf_page) { return -1; }

    BTreePage leaf = {0};
    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &leaf, root_leaf_page, ctx.index_spec);
    if (status != BTREE_SUCCESS) { return -1; }

    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};

    /* --- EMPTY LEAF ROOT --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 0));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 0);
    ASSERT(search_result.page == root_leaf_page);

    /* --- INSERT SORTED CELLS DIRECTLY ONTO LEAF --- */
    uint32_t user_ids[] = {2, 4, 1};
    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(insert_cell_into_page(&ctx, ctx.btree->root_page_num, user_ids[i]));
    }

    btree_page_load(&leaf);
    ASSERT(leaf.cell_count == 3);

    /* --- SEARCH BEFORE FIRST KEY --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 0));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 0);
    ASSERT(search_result.page == root_leaf_page);

    /* --- SEARCH FIRST EXACT KEY --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 2));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);
    ASSERT(search_result.page == root_leaf_page);

    /* --- SEARCH BETWEEN FIRST AND SECOND KEY --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 8));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 1);
    ASSERT(search_result.page == root_leaf_page);

    /* --- SEARCH MIDDLE EXACT KEY --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 4));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 1);
    ASSERT(search_result.page == root_leaf_page);

    /* --- SEARCH LAST EXACT KEY --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 1));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 2);
    ASSERT(search_result.page == root_leaf_page);

    /* --- SEARCH AFTER LAST KEY --- */
    ASSERT(search_key_from_user_id(&search_key, &ctx, 3));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.result_index == 3);
    ASSERT(search_result.page == root_leaf_page);

    free(search_key.target_key);
    search_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_root_to_leaf() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database4.db"));

    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};

    /* --- ROOT-ONLY TREE --- */
    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 0));

    BTreeStatus status = btree_root_to_leaf(
        ctx.btree,
        &search_key,
        &search_result,
        BTREE_UPPER_BOUND
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page);
    ASSERT(search_result.page->page_num == ctx.btree->root_page_num);
    ASSERT(search_result.result_index == 0);

    /* --- TWO-LEVEL TREE --- */
    uint32_t left_leaf_page_num = 0;
    uint32_t right_leaf_page_num = 0;

    ASSERT(build_two_level_test_tree(
        &ctx,
        50,
        &left_leaf_page_num,
        &right_leaf_page_num
    ));

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 10));

    status = btree_root_to_leaf(
        ctx.btree,
        &search_key,
        &search_result,
        BTREE_UPPER_BOUND
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page);
    ASSERT(search_result.page->page_num == left_leaf_page_num);
    ASSERT(search_result.result_index == 0);

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 70));

    status = btree_root_to_leaf(
        ctx.btree,
        &search_key,
        &search_result,
        BTREE_UPPER_BOUND
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page);
    ASSERT(search_result.page->page_num == right_leaf_page_num);
    ASSERT(search_result.result_index == 0);

    /* --- THREE-LEVEL TREE & RIGHTMOST PATH --- */
    uint32_t rightmost_leaf_page_num = 0;

    ASSERT(expand_test_tree_to_three_levels(
        &ctx,
        left_leaf_page_num,
        right_leaf_page_num,
        25,
        75,
        &rightmost_leaf_page_num
    ));

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 90));

    status = btree_root_to_leaf(
        ctx.btree,
        &search_key,
        &search_result,
        BTREE_UPPER_BOUND
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(search_result.found == true);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page);
    ASSERT(search_result.page->page_num == rightmost_leaf_page_num);
    ASSERT(search_result.result_index == 0);

    Page *parent_page = pager_get_page(
        ctx.pager,
        right_leaf_page_num
    );
    if (!parent_page) { return -1; }

    BTreePage parent = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &parent,
        parent_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(parent.type == BTREE_INTERNAL_NODE);
    ASSERT(parent.is_root == false);
    ASSERT(parent.parent_pointer == ctx.btree->root_page_num);

    free(search_key.target_key);
    search_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_node_insert() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database5.db"));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    if (!root_page) { return -1; }

    BTreePage leaf = {0};
    BTreeStatus status = btree_page_attach_load_validate(
        ctx.pager,
        &leaf,
        root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    BTreeSplitResult split_result = {0};
    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};
    BTreeCellContents cell_contents = {0};

    /* --- INSERT INTO EMPTY PAGE --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 20, 20));

    uint16_t old_free_space_offset = leaf.free_space_offset;

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(leaf.cell_count == 1);
    ASSERT(leaf.free_space_offset == old_free_space_offset - cell_contents.cell_size);
    ASSERT(split_result.split == false);

    free_cell_contents(&cell_contents);

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 20));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);

    /* --- INSERT AT THE BEGINNING --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 10, 10));

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leaf.cell_count == 2);

    free_cell_contents(&cell_contents);

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 10));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 0);

    /* --- INSERT IN THE MIDDLE --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 15, 15));

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leaf.cell_count == 3);

    free_cell_contents(&cell_contents);

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 15));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 1);

    /* --- REJECT UNIQUE DUPLICATE KEY --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 115, 15));

    ctx.index_spec->is_unique = true;

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_DUPLICATE_KEY);
    ASSERT(leaf.cell_count == 3);

    free_cell_contents(&cell_contents);

    /* --- ACCEPT NON-UNIQUE DUPLICATE KEY --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 215, 15));

    ctx.index_spec->is_unique = false;

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leaf.cell_count == 4);

    free_cell_contents(&cell_contents);

    /* --- INSERT AT THE END --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 30, 30));

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leaf.cell_count == 5);

    free_cell_contents(&cell_contents);

    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 30));

    status = btree_binary_search(&leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == true);
    ASSERT(search_result.result_index == 4);

    /* --- INSERT UNTIL PAGE GETS FULL --- */
    ctx.index_spec->is_unique = true;

    uint32_t key_order = 40;
    for (; key_order < 1000; key_order++) {
        ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, key_order, key_order));

        status = btree_node_insert(
            ctx.pager,
            &leaf,
            &cell_contents,
            &split_result,
            ctx.index_spec
        );

        free_cell_contents(&cell_contents);

        if (status == BTREE_NEEDS_SPLIT) {
            break;
        }

        ASSERT(status == BTREE_SUCCESS);
    }

    ASSERT(status == BTREE_NEEDS_SPLIT);
    ASSERT(split_result.split == true);
    ASSERT(split_result.left_page == leaf.page->page_num);

    /* --- DUPLICATE CHECK STILL HAPPENS BEFORE FULL-PAGE CHECK --- */
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 320, 20));

    status = btree_node_insert(
        ctx.pager,
        &leaf,
        &cell_contents,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_DUPLICATE_KEY);

    free_cell_contents(&cell_contents);
    split_result_reset(&split_result);

    free(search_key.target_key);
    search_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_leaf_node_split() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database6.db"));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    if (!root_page) { return -1; }

    BTreePage leaf = {0};
    BTreeStatus status = btree_page_attach_load_validate(
        ctx.pager,
        &leaf,
        root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    BTreeSplitResult split_result = {0};
    BTreeCellContents cell_contents = {0};

    /* --- FILL LEAF UNTIL NEXT CELL REQUIRES SPLIT --- */
    uint32_t key_order = 0;

    for (; key_order < 1000; key_order++) {
        ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, key_order, key_order));

        status = btree_node_insert(
            ctx.pager,
            &leaf,
            &cell_contents,
            &split_result,
            ctx.index_spec
        );

        free_cell_contents(&cell_contents);

        if (status == BTREE_NEEDS_SPLIT) {
            break;
        }

        ASSERT(status == BTREE_SUCCESS);
    }

    ASSERT(status == BTREE_NEEDS_SPLIT);
    ASSERT(split_result.split == true);

    uint16_t old_cell_count = leaf.cell_count;
    uint32_t old_parent_pointer = leaf.parent_pointer;
    uint32_t old_previous_pointer = leaf.type_specific_data.siblings.previous_leaf_pointer;
    uint32_t old_next_pointer = leaf.type_specific_data.siblings.next_leaf_pointer;

    /* --- SPLIT LEAF --- */
    status = btree_leaf_node_split(
        ctx.pager,
        &leaf,
        ctx.index_spec,
        &split_result
    );
    ASSERT(status == BTREE_SUCCESS);

    Page *right_page = pager_get_page(ctx.pager, split_result.right_page);
    if (!right_page) { return -1; }

    BTreePage right_leaf = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &right_leaf,
        right_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(leaf.type == BTREE_LEAF_NODE);
    ASSERT(right_leaf.type == BTREE_LEAF_NODE);

    ASSERT(leaf.cell_count > 0);
    ASSERT(right_leaf.cell_count > 0);
    ASSERT(leaf.cell_count + right_leaf.cell_count == old_cell_count);

    ASSERT(leaf.parent_pointer == old_parent_pointer);
    ASSERT(right_leaf.parent_pointer == old_parent_pointer);

    ASSERT(leaf.type_specific_data.siblings.previous_leaf_pointer == old_previous_pointer);
    ASSERT(leaf.type_specific_data.siblings.next_leaf_pointer == right_leaf.page->page_num);
    ASSERT(right_leaf.type_specific_data.siblings.previous_leaf_pointer == leaf.page->page_num);
    ASSERT(right_leaf.type_specific_data.siblings.next_leaf_pointer == old_next_pointer);

    ASSERT(split_result.left_page == leaf.page->page_num);
    ASSERT(split_result.right_page == right_leaf.page->page_num);
    ASSERT(split_result.separator_key);
    ASSERT(split_result.separator_size == ctx.index_spec->key_size);

    /* --- SEPARATOR KEY == FIRST KEY OF RIGHT LEAF --- */
    Value **separator_key = serialized_key_to_values(
        split_result.separator_key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!separator_key) { return -1; }

    BTreeKeyView key_view = {0};
    status = get_key(
        &right_leaf,
        0,
        &key_view,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **right_first_key = serialized_key_to_values(
        key_view.key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!right_first_key) { return -1; }

    int result = 0;
    status = btree_compare(
        separator_key,
        right_first_key,
        ctx.index_spec->index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == 0);

    /* --- LAST LEFT KEY < SEPARATOR KEY --- */
    status = get_key(
        &leaf,
        leaf.cell_count-1,
        &key_view,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **left_last_key = serialized_key_to_values(
        key_view.key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!left_last_key) { return -1; }

    result = 0;
    status = btree_compare(
        left_last_key,
        separator_key,
        ctx.index_spec->index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == -1);

    ASSERT(leaf.page->is_dirty == true);
    ASSERT(right_leaf.page->is_dirty == true);

    value_free_array(separator_key, ctx.index_spec->index_key->num_columns);
    value_free_array(right_first_key, ctx.index_spec->index_key->num_columns);
    value_free_array(left_last_key, ctx.index_spec->index_key->num_columns);

    split_result_reset(&split_result);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_internal_node_split() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database7.db"));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    if (!root_page) { return -1; }

    if (!page_clear(ctx.pager, root_page)) { return -1; }

    /* --- CREATE FIRST CHILD & INITIALIZE ROOT AS INTERNAL --- */
    uint32_t first_child_page_num = 0;
    ASSERT(create_empty_leaf_child(
        &ctx,
        ctx.btree->root_page_num,
        &first_child_page_num
    ));

    BTreePage internal = {0};
    btree_page_attach(&internal, root_page);

    BTreeStatus status = btree_page_init_internal(
        &internal,
        first_child_page_num
    );
    ASSERT(status == BTREE_SUCCESS);

    if (!btree_page_sync(ctx.pager, &internal)) { return -1; }

    /* --- FILL INTERNAL NODE TO ITS REAL CAPACITY --- */
    uint16_t internal_cell_size = ctx.index_spec->key_size + sizeof(uint32_t);
    uint32_t key_order = 0;

    while (btree_page_has_enough_space(&internal, internal_cell_size) == BTREE_SUCCESS) {
        uint32_t child_page_num = 0;
        ASSERT(create_empty_leaf_child(
            &ctx,
            internal.page->page_num,
            &child_page_num
        ));

        BTreeCellContents internal_cell = {0};
        ASSERT(create_internal_cell_contents(
            &internal_cell,
            ctx.index_spec,
            key_order,
            child_page_num
        ));

        BTreeSplitResult insert_split_result = {0};

        status = btree_node_insert(
            ctx.pager,
            &internal,
            &internal_cell,
            &insert_split_result,
            ctx.index_spec
        );
        ASSERT(status == BTREE_SUCCESS);
        ASSERT(insert_split_result.split == false);

        free_cell_contents(&internal_cell);
        key_order++;
    }

    ASSERT(internal.cell_count > 1);
    ASSERT(
        btree_page_has_enough_space(&internal, internal_cell_size)
        == BTREE_NEEDS_SPLIT
    );

    uint16_t old_cell_count = internal.cell_count;

    /* --- SPLIT INTERNAL NODE --- */
    BTreeSplitResult split_result = {0};

    status = btree_internal_node_split(
        ctx.pager,
        &internal,
        ctx.index_spec,
        &split_result
    );
    ASSERT(status == BTREE_SUCCESS);

    Page *right_page = pager_get_page(ctx.pager, split_result.right_page);
    if (!right_page) { return -1; }

    BTreePage right_internal = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &right_internal,
        right_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(internal.type == BTREE_INTERNAL_NODE);
    ASSERT(right_internal.type == BTREE_INTERNAL_NODE);

    ASSERT(internal.cell_count > 0);
    ASSERT(right_internal.cell_count > 0);
    ASSERT(internal.cell_count + right_internal.cell_count + 1 == old_cell_count);

    ASSERT(split_result.left_page == internal.page->page_num);
    ASSERT(split_result.right_page == right_internal.page->page_num);
    ASSERT(split_result.separator_key);
    ASSERT(split_result.separator_size == ctx.index_spec->key_size);

    /* --- LEFT LAST KEY < PROMOTED KEY < RIGHT FIRST KEY --- */
    Value **separator_key = serialized_key_to_values(
        split_result.separator_key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!separator_key) { return -1; }

    BTreeKeyView key_view = {0};

    status = get_key(
        &internal,
        internal.cell_count-1,
        &key_view,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **left_last_key = serialized_key_to_values(
        key_view.key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!left_last_key) { return -1; }

    status = get_key(
        &right_internal,
        0,
        &key_view,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **right_first_key = serialized_key_to_values(
        key_view.key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!right_first_key) { return -1; }

    int result = 0;
    status = btree_compare(
        left_last_key,
        separator_key,
        ctx.index_spec->index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == -1);

    result = 0;
    status = btree_compare(
        separator_key,
        right_first_key,
        ctx.index_spec->index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == -1);

    /* --- VERIFY LEFT CHILD PARENT POINTERS --- */
    for (uint32_t i = 0; i < internal.cell_count + 1; i++) {
        uint32_t child_page_num = 0;

        if (i == internal.cell_count) {
            child_page_num = internal.type_specific_data.rightmost_child_pointer;
        } else {
            uint32_t cell_pointer = get_cell_pointer(internal.data, i);
            child_page_num = get_cell_child_pointer(
                internal.data,
                get_cell_offset(cell_pointer)
            );
        }

        Page *child_page = pager_get_page(ctx.pager, child_page_num);
        if (!child_page) { return -1; }

        BTreePage child = {0};
        status = btree_page_attach_load_validate(
            ctx.pager,
            &child,
            child_page,
            ctx.index_spec
        );
        ASSERT(status == BTREE_SUCCESS);

        ASSERT(child.type == BTREE_LEAF_NODE);
        ASSERT(child.is_root == false);
        ASSERT(child.parent_pointer == internal.page->page_num);
    }

    /* --- VERIFY RIGHT CHILD PARENT POINTERS --- */
    for (uint32_t i = 0; i < right_internal.cell_count + 1; i++) {
        uint32_t child_page_num = 0;

        if (i == right_internal.cell_count) {
            child_page_num = right_internal.type_specific_data.rightmost_child_pointer;
        } else {
            uint32_t cell_pointer = get_cell_pointer(right_internal.data, i);
            child_page_num = get_cell_child_pointer(
                right_internal.data,
                get_cell_offset(cell_pointer)
            );
        }

        Page *child_page = pager_get_page(ctx.pager, child_page_num);
        if (!child_page) { return -1; }

        BTreePage child = {0};
        status = btree_page_attach_load_validate(
            ctx.pager,
            &child,
            child_page,
            ctx.index_spec
        );
        ASSERT(status == BTREE_SUCCESS);

        ASSERT(child.type == BTREE_LEAF_NODE);
        ASSERT(child.is_root == false);
        ASSERT(child.parent_pointer == right_internal.page->page_num);
    }

    ASSERT(internal.page->is_dirty == true);
    ASSERT(right_internal.page->is_dirty == true);

    value_free_array(separator_key, ctx.index_spec->index_key->num_columns);
    value_free_array(left_last_key, ctx.index_spec->index_key->num_columns);
    value_free_array(right_first_key, ctx.index_spec->index_key->num_columns);

    split_result_reset(&split_result);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_root_node_split() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database8.db"));

    uint32_t old_root_page_num = ctx.btree->root_page_num;

    Page *root_page = pager_get_page(ctx.pager, old_root_page_num);
    if (!root_page) { return -1; }

    BTreePage old_root = {0};
    BTreeStatus status = btree_page_attach_load_validate(
        ctx.pager,
        &old_root,
        root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    BTreeCellContents cell_contents = {0};
    BTreeSplitResult split_result = {0};

    /* --- FILL ROOT LEAF UNTIL SPLIT IS NEEDED --- */
    uint32_t key_order = 0;

    for (; key_order < 1000; key_order++) {
        ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, key_order, key_order));

        status = btree_node_insert(
            ctx.pager,
            &old_root,
            &cell_contents,
            &split_result,
            ctx.index_spec
        );

        free_cell_contents(&cell_contents);

        if (status == BTREE_NEEDS_SPLIT) {
            break;
        }

        ASSERT(status == BTREE_SUCCESS);
    }

    ASSERT(status == BTREE_NEEDS_SPLIT);
    ASSERT(split_result.split == true);

    /* --- SPLIT OLD ROOT LEAF --- */
    status = btree_leaf_node_split(
        ctx.pager,
        &old_root,
        ctx.index_spec,
        &split_result
    );
    ASSERT(status == BTREE_SUCCESS);

    uint32_t right_page_num = split_result.right_page;

    Value **separator_key = serialized_key_to_values(
        split_result.separator_key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!separator_key) { return -1; }

    /* --- CREATE NEW ROOT --- */
    status = btree_root_split(
        ctx.btree,
        &old_root,
        &split_result,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(ctx.btree->root_page_num != old_root_page_num);

    Page *new_root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    if (!new_root_page) { return -1; }

    BTreePage new_root = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &new_root,
        new_root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(new_root.type == BTREE_INTERNAL_NODE);
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(new_root.cell_count == 1);

    uint32_t root_cell_pointer = get_cell_pointer(new_root.data, 0);
    uint32_t left_child_page_num = get_cell_child_pointer(
        new_root.data,
        get_cell_offset(root_cell_pointer)
    );

    ASSERT(left_child_page_num == old_root_page_num);
    ASSERT(new_root.type_specific_data.rightmost_child_pointer == right_page_num);

    /* --- VERIFY CHILD METADATA --- */
    Page *left_page = pager_get_page(ctx.pager, old_root_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!left_page || !right_page) { return -1; }

    BTreePage left_child = {0};
    BTreePage right_child = {0};

    status = btree_page_attach_load_validate(
        ctx.pager,
        &left_child,
        left_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    status = btree_page_attach_load_validate(
        ctx.pager,
        &right_child,
        right_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left_child.type == BTREE_LEAF_NODE);
    ASSERT(right_child.type == BTREE_LEAF_NODE);

    ASSERT(left_child.is_root == false);
    ASSERT(right_child.is_root == false);

    ASSERT(left_child.parent_pointer == new_root.page->page_num);
    ASSERT(right_child.parent_pointer == new_root.page->page_num);

    /* --- NEW ROOT KEY == SPLIT SEPARATOR KEY --- */
    BTreeKeyView key_view = {0};
    status = get_key(
        &new_root,
        0,
        &key_view,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    Value **new_root_key = serialized_key_to_values(
        key_view.key,
        ctx.index_spec->index_key->num_columns,
        ctx.index_spec
    );
    if (!new_root_key) { return -1; }

    int result = 0;
    status = btree_compare(
        new_root_key,
        separator_key,
        ctx.index_spec->index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == 0);

    ASSERT(new_root.page->is_dirty == true);
    ASSERT(left_child.page->is_dirty == true);
    ASSERT(right_child.page->is_dirty == true);

    value_free_array(separator_key, ctx.index_spec->index_key->num_columns);
    value_free_array(new_root_key, ctx.index_spec->index_key->num_columns);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_reachable_page_traversal() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database9.db"));

    BTreePageCollection visited_pages = {0};

    /* --- ONE-LEVEL TREE --- */
    BTreeStatus status = btree_traverse_reachable_pages(
        ctx.btree,
        &visited_pages
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(visited_pages.count == 1);
    ASSERT(visited_pages.page_numbers[0] == ctx.btree->root_page_num);

    /* --- TWO-LEVEL TREE --- */
    uint32_t left_leaf_page_num = 0;
    uint32_t right_leaf_page_num = 0;

    ASSERT(build_two_level_test_tree(
        &ctx,
        50,
        &left_leaf_page_num,
        &right_leaf_page_num
    ));

    status = btree_traverse_reachable_pages(
        ctx.btree,
        &visited_pages
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(visited_pages.count == 3);
    ASSERT(visited_pages.page_numbers[0] == ctx.btree->root_page_num);
    ASSERT(visited_pages.page_numbers[1] == left_leaf_page_num);
    ASSERT(visited_pages.page_numbers[2] == right_leaf_page_num);

    /* --- THREE-LEVEL TREE --- */
    uint32_t rightmost_leaf_page_num = 0;

    ASSERT(expand_test_tree_to_three_levels(
        &ctx,
        left_leaf_page_num,
        right_leaf_page_num,
        25,
        75,
        &rightmost_leaf_page_num
    ));

    status = btree_traverse_reachable_pages(
        ctx.btree,
        &visited_pages
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(visited_pages.count == 7);

    for (uint32_t i = SYSTEM_CATALOG_PAGE_NUM + 1;
         i < ctx.pager->num_pages;
         i++) {
        ASSERT(btree_collection_contains(&visited_pages, i));
    }

    ASSERT(visited_pages.page_numbers[0] == ctx.btree->root_page_num);
    ASSERT(btree_collection_contains(
        &visited_pages,
        rightmost_leaf_page_num
    ));

    btree_test_context_free(&ctx);
    return 0;
}

static int test_exact_key_lookup() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database10_1.db"));

    /* --- BUILD MULTI-LEAF TREE --- */
    for (uint32_t i = 0; i < 40; i++) {
        BTreeInsertionResult insertion_res = {0};
        ASSERT(insert_ordered_cell(&ctx, i, i, &insertion_res));
    }

    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};

    /* --- LOOKUP EXISTING KEYS --- */
    uint32_t exact_key_orders[] = {0, 20, 39};

    for (uint32_t i = 0; i < 3; i++) {
        uint32_t key_order = exact_key_orders[i];

        ASSERT(ordered_search_key_init(
            &search_key,
            ctx.index_spec,
            key_order
        ));

        BTreeSearchEntries entries = {0};

        BTreeStatus status = btree_find_exact_key(
            ctx.btree,
            &search_key,
            &search_result,
            &entries
        );
        ASSERT(status == BTREE_SUCCESS);

        ASSERT(search_result.found == true);
        ASSERT(search_result.exact_match == true);
        ASSERT(search_result.page);
        ASSERT(search_result.result_index != UINT16_MAX);

        ASSERT(entries.count == 1);
        ASSERT(entries.entries);
        ASSERT(entries.entries[0].page_num == search_result.page->page_num);
        ASSERT(entries.entries[0].cell_index == search_result.result_index);

        ASSERT(verify_ordered_entries(
            &entries,
            ctx.index_spec,
            key_order,
            1
        ));

        BTreeCellContents expected_cell = {0};
        ASSERT(create_ordered_cell_contents(
            &expected_cell,
            ctx.index_spec,
            key_order,
            key_order
        ));

        ASSERT(row_equals(
            entries.entries[0].cell.BTreePayload.row,
            expected_cell.BTreePayload.row
        ));

        free_cell_contents(&expected_cell);
        btree_search_entries_free(&entries);
    }

    /* --- LOOKUP MISSING KEY --- */
    ASSERT(ordered_search_key_init(
        &search_key,
        ctx.index_spec,
        1000
    ));

    BTreeSearchEntries entries = {0};

    BTreeStatus status = btree_find_exact_key(
        ctx.btree,
        &search_key,
        &search_result,
        &entries
    );
    ASSERT(status == BTREE_NOT_FOUND);

    ASSERT(search_result.found == false);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == NULL);
    ASSERT(search_result.result_index == UINT16_MAX);

    ASSERT(entries.entries == NULL);
    ASSERT(entries.count == 0);
    ASSERT(entries.capacity == 0);

    free(search_key.target_key);
    search_key.target_key = NULL;

    btree_test_context_free(&ctx);

    /* --- EMPTY BTREE LOOKUP --- */
    ASSERT(btree_test_context_init(&ctx, "build/database10_2.db"));

    ASSERT(ordered_search_key_init(
        &search_key,
        ctx.index_spec,
        0
    ));

    entries = (BTreeSearchEntries){0};

    status = btree_find_exact_key(
        ctx.btree,
        &search_key,
        &search_result,
        &entries
    );
    ASSERT(status == BTREE_NOT_FOUND);

    ASSERT(search_result.found == false);
    ASSERT(search_result.exact_match == false);
    ASSERT(search_result.page == NULL);
    ASSERT(search_result.result_index == UINT16_MAX);

    ASSERT(entries.entries == NULL);
    ASSERT(entries.count == 0);
    ASSERT(entries.capacity == 0);

    free(search_key.target_key);
    search_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_range_query() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database11.db"));

    /* --- BUILD ORDERED MULTI-LEAF TREE --- */
    for (uint32_t i = 0; i < 40; i++) {
        BTreeInsertionResult insertion_res = {0};
        ASSERT(insert_ordered_cell(&ctx, i, i, &insertion_res));
    }

    BTreeSearchKey start_search_key = {0};
    BTreeSearchKey end_search_key = {0};
    BTreeSearchEntries range_result = {0};

    /* --- INCLUSIVE BOUNDS --- */
    ASSERT(ordered_search_key_init(
        &start_search_key,
        ctx.index_spec,
        10
    ));
    ASSERT(ordered_search_key_init(
        &end_search_key,
        ctx.index_spec,
        20
    ));

    BTreeStatus status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        true,
        &end_search_key,
        true,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        10,
        11
    ));

    btree_search_entries_free(&range_result);

    /* --- EXCLUSIVE BOUNDS --- */
    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        false,
        &end_search_key,
        false,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        11,
        9
    ));

    btree_search_entries_free(&range_result);

    /* --- UNBOUNDED LOWER RANGE --- */
    ASSERT(ordered_search_key_init(
        &end_search_key,
        ctx.index_spec,
        5
    ));

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        NULL,
        false,
        &end_search_key,
        true,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        0,
        6
    ));

    btree_search_entries_free(&range_result);

    /* --- UNBOUNDED UPPER RANGE --- */
    ASSERT(ordered_search_key_init(
        &start_search_key,
        ctx.index_spec,
        35
    ));

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        true,
        NULL,
        false,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        35,
        5
    ));

    btree_search_entries_free(&range_result);

    /* --- EMPTY MATCHING RANGE --- */
    ASSERT(ordered_search_key_init(
        &start_search_key,
        ctx.index_spec,
        30
    ));
    ASSERT(ordered_search_key_init(
        &end_search_key,
        ctx.index_spec,
        20
    ));

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        true,
        &end_search_key,
        true,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 0);

    btree_search_entries_free(&range_result);

    /* --- RANGE CONTAINING ONE KEY --- */
    ASSERT(ordered_search_key_init(
        &start_search_key,
        ctx.index_spec,
        17
    ));
    ASSERT(ordered_search_key_init(
        &end_search_key,
        ctx.index_spec,
        17
    ));

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        true,
        &end_search_key,
        true,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        17,
        1
    ));

    btree_search_entries_free(&range_result);

    /* --- START KEY LOWER THAN FIRST TREE KEY --- */
    Value **outside_key_vals = (Value **) calloc(
        ctx.index_spec->index_key->num_columns,
        sizeof(Value *)
    );
    if (!outside_key_vals) { return -1; }

    bool outside_active = false;
    char outside_username[64] = "aaa";
    uint64_t outside_created_at = 0;

    ASSERT(create_index_key_values(
        outside_key_vals,
        ctx.index_spec,
        outside_active,
        outside_username,
        outside_created_at
    ));

    ASSERT(search_key_init(
        &start_search_key,
        ctx.index_spec,
        outside_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    value_free_array(
        outside_key_vals,
        ctx.index_spec->index_key->num_columns
    );

    ASSERT(ordered_search_key_init(
        &end_search_key,
        ctx.index_spec,
        5
    ));

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        false,
        &end_search_key,
        false,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        0,
        5
    ));

    btree_search_entries_free(&range_result);

    /* --- START KEY BIGGER THAN LAST TREE KEY --- */
    outside_key_vals = (Value **) calloc(
        ctx.index_spec->index_key->num_columns,
        sizeof(Value *)
    );
    if (!outside_key_vals) { return -1; }

    outside_active = true;
    strcpy(outside_username, "zzz");
    outside_created_at = UINT64_MAX;

    ASSERT(create_index_key_values(
        outside_key_vals,
        ctx.index_spec,
        outside_active,
        outside_username,
        outside_created_at
    ));

    ASSERT(search_key_init(
        &start_search_key,
        ctx.index_spec,
        outside_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    ASSERT(search_key_init(
        &end_search_key,
        ctx.index_spec,
        outside_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    value_free_array(
        outside_key_vals,
        ctx.index_spec->index_key->num_columns
    );

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        &start_search_key,
        true,
        &end_search_key,
        true,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(range_result.count == 0);

    btree_search_entries_free(&range_result);

    /* --- FULL UNBOUNDED RANGE ACROSS LINKED LEAVES --- */
    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        NULL,
        false,
        NULL,
        false,
        &range_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(verify_ordered_entries(
        &range_result,
        ctx.index_spec,
        0,
        40
    ));

    ASSERT(range_result.entries[0].page_num
        != range_result.entries[range_result.count-1].page_num);

    btree_search_entries_free(&range_result);

    /* --- INVALID NEXT SIBLING POINTER --- */
    Page *leftmost_page = NULL;
    status = btree_find_leftmost_page(
        ctx.btree,
        ctx.index_spec,
        &leftmost_page
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leftmost_page);

    BTreePage leftmost_leaf = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &leftmost_leaf,
        leftmost_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(leftmost_leaf.type == BTREE_LEAF_NODE);
    ASSERT(leftmost_leaf.type_specific_data.siblings.next_leaf_pointer != UINT32_MAX);

    uint32_t old_next_page_num = leftmost_leaf.type_specific_data.siblings.next_leaf_pointer;

    set_leaf_next(leftmost_leaf.data, MAX_PAGES);

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        NULL,
        false,
        NULL,
        false,
        &range_result
    );
    ASSERT(status == BTREE_CORRUPT_PAGE);

    set_leaf_next(leftmost_leaf.data, old_next_page_num);

    /* --- CYCLIC LEAF CHAIN --- */
    Page *second_leaf_page = pager_get_page(
        ctx.pager,
        old_next_page_num
    );
    if (!second_leaf_page) { return -1; }

    BTreePage second_leaf = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &second_leaf,
        second_leaf_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    uint32_t old_second_next = second_leaf.type_specific_data.siblings.next_leaf_pointer;

    set_leaf_next(
        second_leaf.data,
        leftmost_leaf.page->page_num
    );

    status = btree_find_range_keys(
        ctx.btree,
        ctx.index_spec,
        NULL,
        false,
        NULL,
        false,
        &range_result
    );
    ASSERT(status == BTREE_CORRUPT_PAGE);

    set_leaf_next(second_leaf.data, old_second_next);

    free(start_search_key.target_key);
    start_search_key.target_key = NULL;

    free(end_search_key.target_key);
    end_search_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_find_prefix_keys() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database12.db"));

    /* --- INSERT ORDERED KEYS WITH SHARED PREFIXES --- */
    for (uint32_t i = 0; i < 60; i++) {
        BTreeCellContents cell_contents = {0};
        ASSERT(create_cell_contents(
            &cell_contents,
            ctx.index_spec,
            i
        ));

        char username[64] = {0};
        snprintf(
            username,
            sizeof(username),
            "group%02u",
            i / 5
        );

        bool active = false;
        uint64_t created_at = 1700000000ULL + (i % 5);

        ASSERT(set_cell_index_values(
            &cell_contents,
            ctx.index_spec,
            active,
            username,
            created_at
        ));

        BTreeInsertionResult insertion_res = {0};
        BTreeStatus status = btree_insert(
            ctx.btree,
            &cell_contents,
            &insertion_res,
            ctx.index_spec
        );
        ASSERT(status == BTREE_SUCCESS);

        free_cell_contents(&cell_contents);
    }

    BTreeSearchKey prefix_key = {0};
    BTreeSearchEntries prefix_result = {0};

    Value **prefix_key_vals = (Value **) calloc(
        ctx.index_spec->index_key->num_columns,
        sizeof(Value *)
    );
    if (!prefix_key_vals) { return -1; }

    /* --- ONE-COMPONENT PREFIX: ALL FALSE KEYS --- */
    bool active = false;
    char username[64] = "group00";
    uint64_t created_at = 1700000000ULL;

    ASSERT(create_index_key_values(
        prefix_key_vals,
        ctx.index_spec,
        active,
        username,
        created_at
    ));

    ASSERT(search_key_init(
        &prefix_key,
        ctx.index_spec,
        prefix_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    prefix_key.num_target_keys = 1;

    BTreeStatus status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(prefix_result.count == 60);

    ASSERT(prefix_result.entries[0].page_num
        != prefix_result.entries[prefix_result.count-1].page_num);

    for (uint32_t i = 0; i < prefix_result.count; i++) {
        int result = 0;

        ASSERT(value_compare(
            prefix_result.entries[i].cell.keys[0],
            prefix_key_vals[0],
            &result
        ));
        ASSERT(result == 0);

        if (i > 0) {
            result = 0;

            status = btree_compare(
                prefix_result.entries[i-1].cell.keys,
                prefix_result.entries[i].cell.keys,
                ctx.index_spec->index_key->num_columns,
                &result
            );
            ASSERT(status == BTREE_SUCCESS);
            ASSERT(result == -1);
        }
    }

    btree_search_entries_free(&prefix_result);

    /* --- TWO-COMPONENT PREFIX: FIVE MATCHES --- */
    for (uint32_t i = 0; i < ctx.index_spec->index_key->num_columns; i++) {
        value_free(prefix_key_vals[i]);
        prefix_key_vals[i] = NULL;
    }

    strcpy(username, "group05");
    created_at = 1700000000ULL;

    ASSERT(create_index_key_values(
        prefix_key_vals,
        ctx.index_spec,
        active,
        username,
        created_at
    ));

    ASSERT(search_key_init(
        &prefix_key,
        ctx.index_spec,
        prefix_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    prefix_key.num_target_keys = 2;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(prefix_result.count == 5);

    for (uint32_t i = 0; i < prefix_result.count; i++) {
        int result = 0;

        ASSERT(value_compare(
            prefix_result.entries[i].cell.keys[0],
            prefix_key_vals[0],
            &result
        ));
        ASSERT(result == 0);

        result = 0;
        ASSERT(value_compare(
            prefix_result.entries[i].cell.keys[1],
            prefix_key_vals[1],
            &result
        ));
        ASSERT(result == 0);

        uint64_t expected_timestamp = 1700000000ULL + i;
        Value *expected = value_create(
            get_key_column(ctx.index_spec, 2)->type,
            &expected_timestamp
        );
        if (!expected) { return -1; }

        result = 0;
        ASSERT(value_compare(
            prefix_result.entries[i].cell.keys[2],
            expected,
            &result
        ));
        ASSERT(result == 0);

        value_free(expected);
    }

    btree_search_entries_free(&prefix_result);

    /* --- FULL KEY PREFIX: ONE EXACT MATCH --- */
    for (uint32_t i = 0; i < ctx.index_spec->index_key->num_columns; i++) {
        value_free(prefix_key_vals[i]);
        prefix_key_vals[i] = NULL;
    }

    strcpy(username, "group05");
    created_at = 1700000002ULL;

    ASSERT(create_index_key_values(
        prefix_key_vals,
        ctx.index_spec,
        active,
        username,
        created_at
    ));

    ASSERT(search_key_init(
        &prefix_key,
        ctx.index_spec,
        prefix_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    prefix_key.num_target_keys = ctx.index_spec->index_key->num_columns;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(prefix_result.count == 1);

    int result = 0;
    status = btree_compare(
        prefix_result.entries[0].cell.keys,
        prefix_key_vals,
        ctx.index_spec->index_key->num_columns,
        &result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(result == 0);

    btree_search_entries_free(&prefix_result);

    /* --- PREFIX MATCHING NO ENTRIES --- */
    for (uint32_t i = 0; i < ctx.index_spec->index_key->num_columns; i++) {
        value_free(prefix_key_vals[i]);
        prefix_key_vals[i] = NULL;
    }

    active = true;
    strcpy(username, "group00");
    created_at = 1700000000ULL;

    ASSERT(create_index_key_values(
        prefix_key_vals,
        ctx.index_spec,
        active,
        username,
        created_at
    ));

    ASSERT(search_key_init(
        &prefix_key,
        ctx.index_spec,
        prefix_key_vals,
        ctx.index_spec->index_key->num_columns
    ));

    prefix_key.num_target_keys = 1;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(prefix_result.count == 0);

    btree_search_entries_free(&prefix_result);

    /* --- INVALID ARGUMENTS --- */
    prefix_key.num_target_keys = 1;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        NULL,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    void *valid_target_key = prefix_key.target_key;
    prefix_key.target_key = NULL;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.target_key = valid_target_key;

    BTreeIndexSpec different_index_spec = *ctx.index_spec;
    prefix_key.index = &different_index_spec;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.index = ctx.index_spec;
    prefix_key.num_target_keys = 0;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.num_target_keys = ctx.index_spec->index_key->num_columns + 1;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    prefix_key.num_target_keys = ctx.index_spec->index_key->num_columns;

    status = btree_find_prefix_keys(
        ctx.btree,
        ctx.index_spec,
        &prefix_key,
        NULL
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    status = btree_find_prefix_keys(
        NULL,
        ctx.index_spec,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    status = btree_find_prefix_keys(
        ctx.btree,
        NULL,
        &prefix_key,
        &prefix_result
    );
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    for (uint32_t i = 0; i < ctx.index_spec->index_key->num_columns; i++) {
        value_free(prefix_key_vals[i]);
        prefix_key_vals[i] = NULL;
    }
    free(prefix_key_vals);

    free(prefix_key.target_key);
    prefix_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_split_propagation() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database13_1.db"));

    /* --- ONE-LEVEL SPLIT PROPAGATION --- */
    Page *root_page = pager_get_page(
        ctx.pager,
        ctx.btree->root_page_num
    );
    if (!root_page) { return -1; }

    BTreePage leaf = {0};
    BTreeStatus status = btree_page_attach_load_validate(
        ctx.pager,
        &leaf,
        root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(leaf.type == BTREE_LEAF_NODE);
    ASSERT(leaf.is_root == true);

    BTreeSplitResult split_result = {0};
    BTreeCellContents pending_cell = {0};

    uint32_t key_order = 0;

    while (true) {
        ASSERT(create_ordered_cell_contents(
            &pending_cell,
            ctx.index_spec,
            key_order,
            key_order
        ));

        status = btree_node_insert(
            ctx.pager,
            &leaf,
            &pending_cell,
            &split_result,
            ctx.index_spec
        );

        if (status == BTREE_NEEDS_SPLIT) {
            break;
        }

        ASSERT(status == BTREE_SUCCESS);

        free_cell_contents(&pending_cell);
        key_order++;
    }

    ASSERT(split_result.split == true);

    uint32_t old_root_page_num = ctx.btree->root_page_num;

    BTreeInsertionResult insertion_res = {0};

    status = btree_split_propagation(
        ctx.btree,
        &leaf,
        &pending_cell,
        &split_result,
        ctx.index_spec,
        &insertion_res
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(insertion_res.split_levels == 1);
    ASSERT(insertion_res.insertion_page_num != UINT32_MAX);
    ASSERT(ctx.btree->root_page_num != old_root_page_num);

    uint32_t height = 0;
    ASSERT(get_btree_height(&ctx, &height));
    ASSERT(height == 2);

    Page *new_root_page = pager_get_page(
        ctx.pager,
        ctx.btree->root_page_num
    );
    if (!new_root_page) { return -1; }

    BTreePage new_root = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &new_root,
        new_root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(new_root.type == BTREE_INTERNAL_NODE);
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(new_root.cell_count == 1);

    free_cell_contents(&pending_cell);
    split_result_reset(&split_result);

    btree_test_context_free(&ctx);

    /* --- TWO-LEVEL SPLIT PROPAGATION ---
     *
     * Build a real full internal root directly instead of inserting
     * hundreds of rows through root-to-leaf traversal.
     *
     * Then fill its rightmost leaf, split it and propagate the
     * separator through the already full internal root. */
    ASSERT(btree_test_context_init(&ctx, "build/database13_2.db"));

    uint32_t rightmost_leaf_page_num = 0;
    uint32_t next_separator_key_order = 0;

    ASSERT(build_full_internal_root(
        &ctx,
        &rightmost_leaf_page_num,
        &next_separator_key_order
    ));

    Page *full_root_page = pager_get_page(
        ctx.pager,
        ctx.btree->root_page_num
    );
    if (!full_root_page) { return -1; }

    BTreePage full_root = {0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &full_root,
        full_root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(full_root.type == BTREE_INTERNAL_NODE);
    ASSERT(full_root.is_root == true);

    uint16_t internal_cell_size =
        ctx.index_spec->key_size + sizeof(uint32_t);

    status = btree_page_has_enough_space(
        &full_root,
        internal_cell_size
    );
    ASSERT(status == BTREE_NEEDS_SPLIT);

    old_root_page_num = ctx.btree->root_page_num;

    Page *rightmost_leaf_page = pager_get_page(
        ctx.pager,
        rightmost_leaf_page_num
    );
    if (!rightmost_leaf_page) { return -1; }

    leaf = (BTreePage){0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &leaf,
        rightmost_leaf_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(leaf.type == BTREE_LEAF_NODE);
    ASSERT(leaf.is_root == false);
    ASSERT(leaf.parent_pointer == old_root_page_num);

    pending_cell = (BTreeCellContents){0};
    split_result = (BTreeSplitResult){0};

    key_order = 1000 + next_separator_key_order;

    while (true) {
        ASSERT(create_ordered_cell_contents(
            &pending_cell,
            ctx.index_spec,
            key_order,
            key_order
        ));

        status = btree_node_insert(
            ctx.pager,
            &leaf,
            &pending_cell,
            &split_result,
            ctx.index_spec
        );

        if (status == BTREE_NEEDS_SPLIT) {
            break;
        }

        ASSERT(status == BTREE_SUCCESS);

        free_cell_contents(&pending_cell);
        key_order++;
    }

    ASSERT(split_result.split == true);

    insertion_res = (BTreeInsertionResult){0};

    status = btree_split_propagation(
        ctx.btree,
        &leaf,
        &pending_cell,
        &split_result,
        ctx.index_spec,
        &insertion_res
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(insertion_res.split_levels == 2);
    ASSERT(insertion_res.insertion_page_num != UINT32_MAX);
    ASSERT(ctx.btree->root_page_num != old_root_page_num);

    ASSERT(get_btree_height(&ctx, &height));
    ASSERT(height == 3);

    new_root_page = pager_get_page(
        ctx.pager,
        ctx.btree->root_page_num
    );
    if (!new_root_page) { return -1; }

    new_root = (BTreePage){0};
    status = btree_page_attach_load_validate(
        ctx.pager,
        &new_root,
        new_root_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(new_root.type == BTREE_INTERNAL_NODE);
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);
    ASSERT(new_root.cell_count == 1);

    uint32_t root_cell_pointer = get_cell_pointer(
        new_root.data,
        0
    );

    uint32_t left_internal_page_num = get_cell_child_pointer(
        new_root.data,
        get_cell_offset(root_cell_pointer)
    );

    uint32_t right_internal_page_num =
        new_root.type_specific_data.rightmost_child_pointer;

    ASSERT(left_internal_page_num == old_root_page_num);
    ASSERT(right_internal_page_num != old_root_page_num);

    Page *left_internal_page = pager_get_page(
        ctx.pager,
        left_internal_page_num
    );

    Page *right_internal_page = pager_get_page(
        ctx.pager,
        right_internal_page_num
    );

    if (!left_internal_page || !right_internal_page) { return -1; }

    BTreePage left_internal = {0};
    BTreePage right_internal = {0};

    status = btree_page_attach_load_validate(
        ctx.pager,
        &left_internal,
        left_internal_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    status = btree_page_attach_load_validate(
        ctx.pager,
        &right_internal,
        right_internal_page,
        ctx.index_spec
    );
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left_internal.type == BTREE_INTERNAL_NODE);
    ASSERT(right_internal.type == BTREE_INTERNAL_NODE);

    ASSERT(left_internal.is_root == false);
    ASSERT(right_internal.is_root == false);

    ASSERT(left_internal.parent_pointer == new_root.page->page_num);
    ASSERT(right_internal.parent_pointer == new_root.page->page_num);

    ASSERT(left_internal.cell_count > 0);
    ASSERT(right_internal.cell_count > 0);

    BTreePageCollection visited_pages = {0};

    status = btree_traverse_reachable_pages(
        ctx.btree,
        &visited_pages
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(visited_pages.count == ctx.pager->num_pages - 2);

    /* Pending cell must still be searchable after both split levels. */
    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};

    ASSERT(ordered_search_key_init(
        &search_key,
        ctx.index_spec,
        key_order
    ));

    status = btree_root_to_leaf(
        ctx.btree,
        &search_key,
        &search_result,
        BTREE_UPPER_BOUND
    );
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == true);

    free_cell_contents(&pending_cell);
    split_result_reset(&split_result);

    free(search_key.target_key);
    search_key.target_key = NULL;

    btree_test_context_free(&ctx);
    return 0;
}

static int test_node_delete() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database14.db"));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    if (!root_page) { return -1; }

    BTreePage leaf = {0};
    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &leaf, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &leaf, i, i));
    }

    BTreeCellContents target_cell = {0};
    ASSERT(create_ordered_cell_contents(&target_cell, ctx.index_spec, 0, 0));

    BTreeDeletionResult deletion_result = {0};
    BTreePage target_leaf = {0};

    status = btree_node_delete(ctx.btree, &target_cell, &deletion_result, ctx.index_spec, &target_leaf);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(deletion_result.deleted == true);
    ASSERT(deletion_result.underflow == false);
    ASSERT(deletion_result.page_num == ctx.btree->root_page_num);
    ASSERT(deletion_result.first_key_changed == true);
    ASSERT(target_leaf.page == root_page);
    ASSERT(target_leaf.cell_count == 3);
    ASSERT(page_cell_matches_key_order(&target_leaf, ctx.index_spec, 0, 1));

    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};
    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 0));

    status = btree_binary_search(&target_leaf, &search_key, &search_result, BTREE_LOWER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == false);

    free_cell_contents(&deletion_result.first_cell);
    free_cell_contents(&target_cell);
    free(search_key.target_key);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_leaf_borrow() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database15_1.db"));

    /* --- BORROW FROM LEFT --- */
    uint32_t left_page_num = 0;
    uint32_t right_page_num = 0;
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left = {0};
    BTreePage right = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    uint32_t left_keys[] = {10, 20, 30};
    uint32_t right_keys[] = {100, 110};

    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &left, left_keys[i], left_keys[i]));
    }
    for (uint32_t i = 0; i < 2; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &right, right_keys[i], right_keys[i]));
    }

    status = btree_leaf_borrow(ctx.pager, 0, &right, &root, &left,
        BTREE_BORROW_FROM_LEFT, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left.cell_count == 2);
    ASSERT(right.cell_count == 3);
    ASSERT(root.cell_count == 1);

    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 0, 10));
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 1, 20));
    ASSERT(page_cell_matches_key_order(&right, ctx.index_spec, 0, 30));
    ASSERT(page_cell_matches_key_order(&right, ctx.index_spec, 1, 100));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 30));

    btree_test_context_free(&ctx);

    /* --- BORROW FROM RIGHT --- */
    ASSERT(btree_test_context_init(&ctx, "build/database15_2.db"));
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    left_page = pager_get_page(ctx.pager, left_page_num);
    right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    root = (BTreePage){0};
    left = (BTreePage){0};
    right = (BTreePage){0};

    status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    uint32_t left_keys_2[] = {10, 20};
    uint32_t right_keys_2[] = {100, 110, 120};

    for (uint32_t i = 0; i < 2; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &left, left_keys_2[i], left_keys_2[i]));
    }
    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &right, right_keys_2[i], right_keys_2[i]));
    }

    status = btree_leaf_borrow(ctx.pager, 0, &left, &root, &right,
        BTREE_BORROW_FROM_RIGHT, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left.cell_count == 3);
    ASSERT(right.cell_count == 2);
    ASSERT(root.cell_count == 1);

    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 2, 100));
    ASSERT(page_cell_matches_key_order(&right, ctx.index_spec, 0, 110));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 110));

    btree_test_context_free(&ctx);
    return 0;
}

static int test_internal_borrow() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database16_1.db"));

    /* --- BORROW FROM LEFT --- */
    uint32_t left_internal_page_num = 0;
    uint32_t right_internal_page_num = 0;
    uint32_t rightmost_leaf_page_num = 0;

    ASSERT(build_two_level_test_tree(&ctx, 50, &left_internal_page_num, &right_internal_page_num));
    ASSERT(expand_test_tree_to_three_levels(&ctx, left_internal_page_num, right_internal_page_num,
        20, 70, &rightmost_leaf_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_internal_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_internal_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left_internal = {0};
    BTreePage right_internal = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left_internal, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right_internal, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    uint32_t old_left_rightmost = left_internal.type_specific_data.rightmost_child_pointer;
    uint32_t new_left_rightmost = 0;
    ASSERT(create_empty_leaf_child(&ctx, left_internal_page_num, &new_left_rightmost));
    ASSERT(append_test_internal_cell(&ctx, &left_internal, 30, old_left_rightmost, new_left_rightmost));

    status = btree_internal_borrow(ctx.pager, 0, &right_internal, &root, &left_internal,
        BTREE_BORROW_FROM_LEFT, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left_internal.cell_count == 1);
    ASSERT(right_internal.cell_count == 2);
    ASSERT(root.cell_count == 1);

    ASSERT(page_cell_matches_key_order(&left_internal, ctx.index_spec, 0, 20));
    ASSERT(page_cell_matches_key_order(&right_internal, ctx.index_spec, 0, 50));
    ASSERT(page_cell_matches_key_order(&right_internal, ctx.index_spec, 1, 70));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 30));

    btree_test_context_free(&ctx);

    /* --- BORROW FROM RIGHT --- */
    ASSERT(btree_test_context_init(&ctx, "build/database16_2.db"));
    ASSERT(build_two_level_test_tree(&ctx, 50, &left_internal_page_num, &right_internal_page_num));
    ASSERT(expand_test_tree_to_three_levels(&ctx, left_internal_page_num, right_internal_page_num,
        20, 70, &rightmost_leaf_page_num));

    root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    left_page = pager_get_page(ctx.pager, left_internal_page_num);
    right_page = pager_get_page(ctx.pager, right_internal_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    root = (BTreePage){0};
    left_internal = (BTreePage){0};
    right_internal = (BTreePage){0};

    status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left_internal, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right_internal, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    uint32_t old_right_rightmost = right_internal.type_specific_data.rightmost_child_pointer;
    uint32_t new_right_rightmost = 0;
    ASSERT(create_empty_leaf_child(&ctx, right_internal_page_num, &new_right_rightmost));
    ASSERT(append_test_internal_cell(&ctx, &right_internal, 80, old_right_rightmost, new_right_rightmost));

    status = btree_internal_borrow(ctx.pager, 0, &left_internal, &root, &right_internal,
        BTREE_BORROW_FROM_RIGHT, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left_internal.cell_count == 2);
    ASSERT(right_internal.cell_count == 1);
    ASSERT(root.cell_count == 1);

    ASSERT(page_cell_matches_key_order(&left_internal, ctx.index_spec, 0, 20));
    ASSERT(page_cell_matches_key_order(&left_internal, ctx.index_spec, 1, 50));
    ASSERT(page_cell_matches_key_order(&right_internal, ctx.index_spec, 0, 80));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 70));

    btree_test_context_free(&ctx);
    return 0;
}

static int test_node_borrow() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database17.db"));

    uint32_t left_page_num = 0;
    uint32_t right_page_num = 0;
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left = {0};
    BTreePage right = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(append_test_leaf_cell(&ctx, &left, 10, 10));
    ASSERT(append_test_leaf_cell(&ctx, &right, 100, 100));
    ASSERT(append_test_leaf_cell(&ctx, &right, 110, 110));

    status = btree_node_borrow(ctx.pager, 0, &left, &root, &right,
        BTREE_BORROW_FROM_RIGHT, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left.cell_count == 2);
    ASSERT(right.cell_count == 1);
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 1, 100));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 110));

    status = btree_node_borrow(ctx.pager, 0, &left, &root, &right,
        (BTreeBorrowDirection) 100, ctx.index_spec);
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_leaf_merge() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database18.db"));

    uint32_t left_page_num = 0;
    uint32_t right_page_num = 0;
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left = {0};
    BTreePage right = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(append_test_leaf_cell(&ctx, &left, 10, 10));
    ASSERT(append_test_leaf_cell(&ctx, &left, 20, 20));
    ASSERT(append_test_leaf_cell(&ctx, &right, 100, 100));
    ASSERT(append_test_leaf_cell(&ctx, &right, 110, 110));

    status = connect_sibling_leaf_nodes(ctx.pager, &left, &right);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(btree_page_sync(ctx.pager, &left));
    ASSERT(btree_page_sync(ctx.pager, &right));

    status = btree_leaf_merge(ctx.pager, &left, &right, &root, 0, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left.cell_count == 4);
    ASSERT(root.cell_count == 0);
    ASSERT(left.type_specific_data.siblings.next_leaf_pointer == UINT32_MAX);

    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 0, 10));
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 1, 20));
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 2, 100));
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 3, 110));

    uint32_t free_list_head = 0;
    ASSERT(get_test_free_list_head(&ctx, &free_list_head));
    ASSERT(free_list_head == right_page_num);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_internal_merge() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database19.db"));

    uint32_t left_internal_page_num = 0;
    uint32_t right_internal_page_num = 0;
    uint32_t rightmost_leaf_page_num = 0;

    ASSERT(build_two_level_test_tree(&ctx, 50, &left_internal_page_num, &right_internal_page_num));
    ASSERT(expand_test_tree_to_three_levels(&ctx, left_internal_page_num, right_internal_page_num,
        20, 70, &rightmost_leaf_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_internal_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_internal_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left_internal = {0};
    BTreePage right_internal = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left_internal, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right_internal, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    uint32_t old_rightmost = right_internal.type_specific_data.rightmost_child_pointer;

    status = btree_internal_merge(ctx.pager, &left_internal, &right_internal, &root, 0, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left_internal.cell_count == 3);
    ASSERT(root.cell_count == 0);
    ASSERT(left_internal.type_specific_data.rightmost_child_pointer == old_rightmost);

    ASSERT(page_cell_matches_key_order(&left_internal, ctx.index_spec, 0, 20));
    ASSERT(page_cell_matches_key_order(&left_internal, ctx.index_spec, 1, 50));
    ASSERT(page_cell_matches_key_order(&left_internal, ctx.index_spec, 2, 70));

    for (uint32_t i = 0; i < left_internal.cell_count; i++) {
        uint32_t cell_pointer = get_cell_pointer(left_internal.data, i);
        uint32_t child_page_num = get_cell_child_pointer(left_internal.data, get_cell_offset(cell_pointer));
        Page *child_page = pager_get_page(ctx.pager, child_page_num);
        if (!child_page) { return -1; }
        ASSERT(get_parent_pointer(child_page->page_data) == left_internal_page_num);
    }

    Page *rightmost_child = pager_get_page(ctx.pager, left_internal.type_specific_data.rightmost_child_pointer);
    if (!rightmost_child) { return -1; }
    ASSERT(get_parent_pointer(rightmost_child->page_data) == left_internal_page_num);

    uint32_t free_list_head = 0;
    ASSERT(get_test_free_list_head(&ctx, &free_list_head));
    ASSERT(free_list_head == right_internal_page_num);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_node_merge() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database20.db"));

    uint32_t left_page_num = 0;
    uint32_t right_page_num = 0;
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left = {0};
    BTreePage right = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(append_test_leaf_cell(&ctx, &left, 10, 10));
    ASSERT(append_test_leaf_cell(&ctx, &right, 100, 100));

    BTreeMergeResult merge_result = {0};
    status = btree_node_merge(ctx.pager, &merge_result, ctx.index_spec);
    ASSERT(status == BTREE_INVALID_ARGUMENTS);

    merge_result.needs_merge = true;
    merge_result.underflowing_page_num = right_page_num;
    merge_result.sibling_page_num = left_page_num;
    merge_result.parent_page_num = ctx.btree->root_page_num;
    merge_result.parent_underflowing_cell_index = 1;
    merge_result.parent_sibling_cell_index = 0;

    status = btree_node_merge(ctx.pager, &merge_result, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    btree_page_load(&root);
    btree_page_load(&left);

    ASSERT(left.cell_count == 2);
    ASSERT(root.cell_count == 0);
    ASSERT(root.type_specific_data.rightmost_child_pointer == left_page_num);
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 0, 10));
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 1, 100));

    uint32_t free_list_head = 0;
    ASSERT(get_test_free_list_head(&ctx, &free_list_head));
    ASSERT(free_list_head == right_page_num);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_root_collapse() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database21.db"));

    uint32_t old_root_page_num = ctx.btree->root_page_num;
    uint32_t child_page_num = 0;
    ASSERT(create_empty_leaf_child(&ctx, old_root_page_num, &child_page_num));
    ASSERT(initialize_test_internal_page(&ctx, old_root_page_num, UINT32_MAX, true, child_page_num));

    Page *old_root_page = pager_get_page(ctx.pager, old_root_page_num);
    if (!old_root_page) { return -1; }

    BTreePage old_root = {0};
    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &old_root, old_root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(old_root.type == BTREE_INTERNAL_NODE);
    ASSERT(old_root.cell_count == 0);
    ASSERT(old_root.is_root == true);

    status = btree_root_collapse(ctx.btree, &old_root, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(ctx.btree->root_page_num == child_page_num);

    Page *new_root_page = pager_get_page(ctx.pager, child_page_num);
    if (!new_root_page) { return -1; }

    BTreePage new_root = {0};
    status = btree_page_attach_load_validate(ctx.pager, &new_root, new_root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(new_root.type == BTREE_LEAF_NODE);
    ASSERT(new_root.is_root == true);
    ASSERT(new_root.parent_pointer == UINT32_MAX);

    uint32_t free_list_head = 0;
    ASSERT(get_test_free_list_head(&ctx, &free_list_head));
    ASSERT(free_list_head == old_root_page_num);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_node_redistribution() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database22_1.db"));

    /* --- REDISTRIBUTE FROM RIGHT --- */
    uint32_t left_page_num = 0;
    uint32_t right_page_num = 0;
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left = {0};
    BTreePage right = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &left, 10 + i, 10 + i));
    }
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &right, 100 + i, 100 + i));
    }

    status = btree_check_underflow(&left, UINT32_MAX);
    ASSERT(status == BTREE_NODE_UNDERFLOW);
    status = btree_node_can_lend(&right, 0, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    BTreeMergeResult merge_result = {0};
    status = btree_node_redistribution(ctx.pager, &left, ctx.index_spec, &merge_result);
    ASSERT(status == BTREE_SUCCESS);

    btree_page_load(&root);
    btree_page_load(&left);
    btree_page_load(&right);

    ASSERT(merge_result.needs_merge == false);
    ASSERT(left.cell_count == 4);
    ASSERT(right.cell_count == 4);
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 3, 100));
    ASSERT(page_cell_matches_key_order(&right, ctx.index_spec, 0, 101));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 101));

    btree_test_context_free(&ctx);

    /* --- NEEDS MERGE --- */
    ASSERT(btree_test_context_init(&ctx, "build/database22_2.db"));
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    left_page = pager_get_page(ctx.pager, left_page_num);
    right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    root = (BTreePage){0};
    left = (BTreePage){0};
    right = (BTreePage){0};

    status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    for (uint32_t i = 0; i < 3; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &left, 10 + i, 10 + i));
    }
    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &right, 100 + i, 100 + i));
    }

    status = btree_check_underflow(&left, UINT32_MAX);
    ASSERT(status == BTREE_NODE_UNDERFLOW);
    status = btree_node_can_lend(&right, 0, ctx.index_spec);
    ASSERT(status == BTREE_NODE_UNDERFLOW);

    merge_result = (BTreeMergeResult){0};
    status = btree_node_redistribution(ctx.pager, &left, ctx.index_spec, &merge_result);
    ASSERT(status == BTREE_NEEDS_MERGE);

    ASSERT(merge_result.needs_merge == true);
    ASSERT(merge_result.underflowing_page_num == left_page_num);
    ASSERT(merge_result.sibling_page_num == right_page_num);
    ASSERT(merge_result.parent_page_num == ctx.btree->root_page_num);
    ASSERT(merge_result.parent_underflowing_cell_index == 0);
    ASSERT(merge_result.parent_sibling_cell_index == 1);

    btree_test_context_free(&ctx);
    return 0;
}

static int test_btree_insert() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database23.db"));

    uint32_t old_root_page_num = ctx.btree->root_page_num;

    BTreeCellContents cell_contents = {0};
    ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, 0, 0));

    BTreeInsertionResult insertion_result = {0};
    BTreeStatus status = btree_insert(ctx.btree, &cell_contents, &insertion_result, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(insertion_result.inserted == true);
    ASSERT(insertion_result.splitted == false);
    ASSERT(insertion_result.insertion_page_num == old_root_page_num);
    ASSERT(insertion_result.split_levels == 0);

    status = btree_insert(ctx.btree, &cell_contents, &insertion_result, ctx.index_spec);
    ASSERT(status == BTREE_DUPLICATE_KEY);
    ASSERT(insertion_result.inserted == false);

    free_cell_contents(&cell_contents);

    uint32_t key_order = 1;
    while (true) {
        ASSERT(create_ordered_cell_contents(&cell_contents, ctx.index_spec, key_order, key_order));

        status = btree_insert(ctx.btree, &cell_contents, &insertion_result, ctx.index_spec);
        ASSERT(status == BTREE_SUCCESS);

        bool splitted = insertion_result.splitted;
        free_cell_contents(&cell_contents);

        if (splitted) {
            break;
        }

        key_order++;
        ASSERT(key_order < 100);
    }

    ASSERT(insertion_result.inserted == true);
    ASSERT(insertion_result.splitted == true);
    ASSERT(insertion_result.split_levels == 1);
    ASSERT(ctx.btree->root_page_num != old_root_page_num);

    uint32_t height = 0;
    ASSERT(get_btree_height(&ctx, &height));
    ASSERT(height == 2);

    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};
    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, key_order));

    status = btree_root_to_leaf(ctx.btree, &search_key, &search_result, BTREE_UPPER_BOUND);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(search_result.exact_match == true);

    free(search_key.target_key);
    btree_test_context_free(&ctx);
    return 0;
}

static int test_btree_delete() {
    BTreeTestContext ctx = {0};
    ASSERT(btree_test_context_init(&ctx, "build/database24.db"));

    uint32_t left_page_num = 0;
    uint32_t right_page_num = 0;
    ASSERT(build_two_level_test_tree(&ctx, 100, &left_page_num, &right_page_num));

    Page *root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    Page *left_page = pager_get_page(ctx.pager, left_page_num);
    Page *right_page = pager_get_page(ctx.pager, right_page_num);
    if (!root_page || !left_page || !right_page) { return -1; }

    BTreePage root = {0};
    BTreePage left = {0};
    BTreePage right = {0};

    BTreeStatus status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &left, 10 + i, 10 + i));
    }
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(append_test_leaf_cell(&ctx, &right, 100 + i, 100 + i));
    }

    BTreeCellContents target_cell = {0};
    ASSERT(create_ordered_cell_contents(&target_cell, ctx.index_spec, 10, 10));

    BTreeDeletionResult deletion_result = {0};
    status = btree_delete(ctx.btree, &target_cell, &deletion_result, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    ASSERT(deletion_result.deleted == true);

    left_page = pager_get_page(ctx.pager, left_page_num);
    right_page = pager_get_page(ctx.pager, right_page_num);
    root_page = pager_get_page(ctx.pager, ctx.btree->root_page_num);
    if (!left_page || !right_page || !root_page) { return -1; }

    left = (BTreePage){0};
    right = (BTreePage){0};
    root = (BTreePage){0};

    status = btree_page_attach_load_validate(ctx.pager, &left, left_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &right, right_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);
    status = btree_page_attach_load_validate(ctx.pager, &root, root_page, ctx.index_spec);
    ASSERT(status == BTREE_SUCCESS);

    ASSERT(left.cell_count == 4);
    ASSERT(right.cell_count == 4);
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 0, 11));
    ASSERT(page_cell_matches_key_order(&left, ctx.index_spec, 3, 100));
    ASSERT(page_cell_matches_key_order(&right, ctx.index_spec, 0, 101));
    ASSERT(page_cell_matches_key_order(&root, ctx.index_spec, 0, 101));

    BTreeSearchKey search_key = {0};
    BTreeSearchResult search_result = {0};
    BTreeSearchEntries entries = {0};
    ASSERT(ordered_search_key_init(&search_key, ctx.index_spec, 10));

    status = btree_find_exact_key(ctx.btree, &search_key, &search_result, &entries);
    ASSERT(status == BTREE_NOT_FOUND);

    btree_search_entries_free(&entries);
    free(search_key.target_key);
    free_cell_contents(&target_cell);

    if (deletion_result.first_key_changed) {
        free_cell_contents(&deletion_result.first_cell);
    }

    btree_test_context_free(&ctx);
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
    result = unlink("build/database4.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database5.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database6.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database7.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database8.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database9.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database11.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database12.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database13_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database13_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database14.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database15_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database15_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database16_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database16_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database17.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database18.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database19.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database20.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database21.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database22_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database22_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database23.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database24.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    result = test_empty_leaf_init();
    generate_output(result, 0, "test_empty_leaf_init");
    result = test_empty_internal_init();
    generate_output(result, 1, "test_empty_internal_init");
    result = test_btree_binary_search();
    generate_output(result, 2, "test_btree_binary_search");
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
    result = test_node_delete();
    generate_output(result, 13, "test_node_delete");
    result = test_leaf_borrow();
    generate_output(result, 14, "test_leaf_borrow");
    result = test_internal_borrow();
    generate_output(result, 15, "test_internal_borrow");
    result = test_node_borrow();
    generate_output(result, 16, "test_node_borrow");
    result = test_leaf_merge();
    generate_output(result, 17, "test_leaf_merge");
    result = test_internal_merge();
    generate_output(result, 18, "test_internal_merge");
    result = test_node_merge();
    generate_output(result, 19, "test_node_merge");
    result = test_root_collapse();
    generate_output(result, 20, "test_root_collapse");
    result = test_node_redistribution();
    generate_output(result, 21, "test_node_redistribution");
    result = test_btree_insert();
    generate_output(result, 22, "test_btree_insert");
    result = test_btree_delete();
    generate_output(result, 23, "test_btree_delete");

    result = unlink("build/database1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database4.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database5.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database6.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database7.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database8.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database9.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database10_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database11.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database12.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database13_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database13_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database14.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database15_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database15_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database16_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database16_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database17.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database18.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database19.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database20.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database21.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database22_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database22_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database23.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database24.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    return 0;
}
