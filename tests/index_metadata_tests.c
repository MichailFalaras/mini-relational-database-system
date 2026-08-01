#include <stdio.h>
#include <string.h>
#include "../include/index.h"

#define ASSERT(condition) { \
    if (!(condition)) { \
        return 1; \
    } \
}


/* ---------- index_key_create unit tests ---------- */

static int test_index_key_create() {
    uint32_t column_indexes[] = {2, 3, 5};
    const uint32_t num_columns = 3;

    IndexKey *index_key = index_key_create(column_indexes, num_columns);
    ASSERT(index_key != NULL);

    ASSERT(index_key->num_columns == num_columns);
    ASSERT(index_key->column_index_array != column_indexes);
    ASSERT(memcmp(index_key->column_index_array, column_indexes, sizeof(column_indexes)) == 0);

    /* Verify that the column array was deep-copied. */
    column_indexes[0] = 99;
    ASSERT(index_key->column_index_array[0] == 2);

    index_key_free(index_key);
    return 0;
}

static int test_index_key_create_null_columns() {
    ASSERT(!index_key_create(NULL, 3));
    return 0;
}

static int test_index_key_create_zero_columns() {
    uint32_t column_indexes[] = {2, 3, 5};

    ASSERT(!index_key_create(column_indexes, 0));
    return 0;
}

/* ---------- index_metadata_create unit tests ---------- */

static int test_index_metadata_create() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *input_key = index_key_create(column_indexes, 3);
    ASSERT(input_key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX, input_key, 2);
    ASSERT(index != NULL);

    ASSERT(strcmp(index->name, "New Index") == 0);
    ASSERT(index->type == PRIMARY_INDEX);
    ASSERT(index->root_page_num == 2);

    ASSERT(index->key != NULL);
    ASSERT(index->key != input_key);
    ASSERT(index->key->column_index_array != input_key->column_index_array);
    ASSERT(index->key->num_columns == input_key->num_columns);
    ASSERT(memcmp(index->key->column_index_array, input_key->column_index_array,input_key->num_columns * sizeof(uint32_t)) == 0);

    /* Verify that the nested array was deep-copied. */
    input_key->column_index_array[0] = 99;
    ASSERT(index->key->column_index_array[0] == 2);

    index_free(index);
    index_key_free(input_key);
    return 0;
}

static int test_index_metadata_create_null_name() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    ASSERT(!index_metadata_create(NULL, PRIMARY_INDEX, key, 2));

    index_key_free(key);
    return 0;
}

static int test_index_metadata_create_empty_name() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    ASSERT(!index_metadata_create("", PRIMARY_INDEX, key, 2));

    index_key_free(key);
    return 0;
}

static int test_index_metadata_create_invalid_index_type() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    ASSERT(!index_metadata_create("New Index", (IndexType)9999, key, 2));

    index_key_free(key);
    return 0;
}

static int test_index_metadata_create_null_index_key() {
    ASSERT(!index_metadata_create("New Index", SECONDARY_INDEX, NULL, 2));

    return 0;
}

static int test_index_metadata_create_null_column_array() {
    IndexKey invalid_key = {.column_index_array = NULL, .num_columns = 3 };

    ASSERT(!index_metadata_create("New Index", SECONDARY_INDEX, &invalid_key, 2));

    return 0;
}

static int test_index_metadata_create_zero_key_columns() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey invalid_key = {.column_index_array = column_indexes, .num_columns = 0};

    ASSERT(!index_metadata_create("New Index", SECONDARY_INDEX, &invalid_key, 2));

    return 0;
}

static int test_index_metadata_create_invalid_pages() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    // Page 0: superblock.
    ASSERT(!index_metadata_create("New Index", PRIMARY_INDEX, key, SUPERBLOCK_PAGE_NUM));

    // Page 1: system catalog.
    ASSERT(!index_metadata_create("New Index", SECONDARY_INDEX, key, SYSTEM_CATALOG_PAGE_NUM));

    index_key_free(key);
    return 0;
}

static int test_index_metadata_create_name_exceeds_limit() {
    char index_name[sizeof(((Index *)0)->name) + 1];

    memset(index_name, 'A', sizeof(index_name) - 1);
    index_name[sizeof(index_name) - 1] = '\0';

    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    ASSERT(!index_metadata_create(index_name, SECONDARY_INDEX, key, 2));

    index_key_free(key);
    return 0;
}

static int test_index_metadata_create_max_valid_name() {
    char index_name[sizeof(((Index *)0)->name)];

    memset(index_name, 'A', sizeof(index_name) - 1);
    index_name[sizeof(index_name) - 1] = '\0';

    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create(index_name, SECONDARY_INDEX, key, 2);
    ASSERT(index != NULL);
    ASSERT(strcmp(index->name, index_name) == 0);

    index_free(index);
    index_key_free(key);
    return 0;
}

/* ---------- index_key_has_column unit tests ---------- */

static int test_index_key_has_column() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX,key, 2);
    ASSERT(index != NULL);

    ASSERT(index_key_has_column(index, 2));
    ASSERT(index_key_has_column(index, 3));
    ASSERT(index_key_has_column(index, 5));

    ASSERT(!index_key_has_column(index, 4));
    ASSERT(!index_key_has_column(index, 6));

    index_free(index);
    index_key_free(key);
    return 0;
}

static int test_index_key_has_column_null_index() {
    ASSERT(!index_key_has_column(NULL, 2));
    return 0;
}

static int test_index_key_has_column_null_index_key() {
    Index index = {0};

    ASSERT(!index_key_has_column(&index, 2));
    return 0;
}

static int test_index_key_has_column_null_column_array() {
    IndexKey key = { .column_index_array = NULL, .num_columns = 3 };
    Index index = { .key = &key };

    ASSERT(!index_key_has_column(&index, 2));
    return 0;
}

static int test_index_key_has_column_zero_key_columns() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey key = { .column_index_array = column_indexes, .num_columns = 0 };
    Index index = { .key = &key };

    ASSERT(!index_key_has_column(&index, 2));
    return 0;
}

/* ---------- index_key_matches_key unit tests ---------- */

static int test_index_key_matches_key() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX, key, 2);
    ASSERT(index != NULL);

    uint32_t exact_match[] = {2, 3, 5};
    uint32_t different_value[] = {2, 3, 6};
    uint32_t wrong_order[] = {3, 2, 5};
    uint32_t too_few[] = {2, 3};
    uint32_t too_many[] = {2, 3, 5, 7};

    ASSERT(index_key_matches_key(index, exact_match, 3));
    ASSERT(!index_key_matches_key(index, different_value, 3));
    ASSERT(!index_key_matches_key(index, wrong_order, 3));
    ASSERT(!index_key_matches_key(index, too_few, 2));
    ASSERT(!index_key_matches_key(index, too_many, 4));

    index_free(index);
    index_key_free(key);
    return 0;
}

static int test_index_key_matches_key_null_index() {
    uint32_t column_ids[] = {2, 3, 5};

    ASSERT(!index_key_matches_key(NULL, column_ids, 3));
    return 0;
}

static int test_index_key_matches_key_null_column_ids() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX, key, 2);
    ASSERT(index != NULL);

    ASSERT(!index_key_matches_key(index, NULL, 3));

    index_free(index);
    index_key_free(key);
    return 0;
}

static int test_index_key_matches_key_zero_num_columns() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX, key, 2);
    ASSERT(index != NULL);

    uint32_t column_ids[] = {2, 3, 5};

    ASSERT(!index_key_matches_key(index, column_ids, 0));

    index_free(index);
    index_key_free(key);
    return 0;
}

/* ---------- index_key_matches_prefix unit tests ---------- */

static int test_index_key_matches_prefix() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX,key, 2);
    ASSERT(index != NULL);

    uint32_t prefix_one[] = {2};
    uint32_t prefix_two[] = {2, 3};
    uint32_t full_key[] = {2, 3, 5};

    uint32_t non_leading[] = {3, 5};
    uint32_t wrong_order[] = {3, 2};
    uint32_t wrong_value[] = {2, 5};
    uint32_t too_many[] = {2, 3, 5, 7};

    ASSERT(index_key_matches_prefix(index, prefix_one, 1));
    ASSERT(index_key_matches_prefix(index, prefix_two, 2));
    ASSERT(index_key_matches_prefix(index, full_key, 3));

    ASSERT(!index_key_matches_prefix(index, non_leading, 2));
    ASSERT(!index_key_matches_prefix(index, wrong_order, 2));
    ASSERT(!index_key_matches_prefix(index, wrong_value, 2));
    ASSERT(!index_key_matches_prefix(index, too_many, 4));

    index_free(index);
    index_key_free(key);
    return 0;
}

static int test_index_key_matches_prefix_null_index() {
    uint32_t column_ids[] = {2, 3};

    ASSERT(!index_key_matches_prefix(NULL, column_ids, 2));
    return 0;
}

static int test_index_key_matches_prefix_null_column_ids() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX, key, 2);
    ASSERT(index != NULL);

    ASSERT(!index_key_matches_prefix(index, NULL, 2));

    index_free(index);
    index_key_free(key);
    return 0;
}

static int test_index_key_matches_prefix_zero_num_columns() {
    uint32_t column_indexes[] = {2, 3, 5};
    IndexKey *key = index_key_create(column_indexes, 3);
    ASSERT(key != NULL);

    Index *index = index_metadata_create("New Index", PRIMARY_INDEX, key, 2);
    ASSERT(index != NULL);

    uint32_t column_ids[] = {2, 3, 5};

    ASSERT(!index_key_matches_prefix(index, column_ids, 0));

    index_free(index);
    index_key_free(key);
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

    /* ---------- index_key_create unit tests ---------- */
    result = test_index_key_create();
    generate_output(result, 0, "test_index_key_create");
    result = test_index_key_create_null_columns();
    generate_output(result, 1, "test_index_key_create_null_columns");
    result = test_index_key_create_zero_columns();
    generate_output(result, 2, "test_index_key_create_zero_columns");

    /* ---------- index_metadata_create unit tests ---------- */
    result = test_index_metadata_create();
    generate_output(result, 3, "test_index_metadata_create");
    result = test_index_metadata_create_null_name();
    generate_output(result, 4, "test_index_metadata_create_null_name");
    result = test_index_metadata_create_empty_name();
    generate_output(result, 5, "test_index_metadata_create_empty_name");
    result = test_index_metadata_create_invalid_index_type();
    generate_output(result, 6, "test_index_metadata_create_invalid_index_type");
    result = test_index_metadata_create_null_index_key();
    generate_output(result, 7, "test_index_metadata_create_null_index_key");
    result = test_index_metadata_create_null_column_array();
    generate_output(result, 8, "test_index_metadata_create_null_column_array");
    result = test_index_metadata_create_zero_key_columns();
    generate_output(result, 9, "test_index_metadata_create_zero_key_columns");
    result = test_index_metadata_create_invalid_pages();
    generate_output(result, 10, "test_index_metadata_create_invalid_pages");
    result = test_index_metadata_create_name_exceeds_limit();
    generate_output(result, 11, "test_index_metadata_create_name_exceeds_limit");
    result = test_index_metadata_create_max_valid_name();
    generate_output(result, 12, "test_index_metadata_create_max_valid_name");

    /* ---------- index_key_has_column unit tests ---------- */
    result = test_index_key_has_column();
    generate_output(result, 13, "test_index_key_has_column");
    result = test_index_key_has_column_null_index();
    generate_output(result, 14, "test_index_key_has_column_null_index");
    result = test_index_key_has_column_null_index_key();
    generate_output(result, 15, "test_index_key_has_column_null_index_key");
    result = test_index_key_has_column_null_column_array();
    generate_output(result, 16, "test_index_key_has_column_null_column_array");
    result = test_index_key_has_column_zero_key_columns();
    generate_output(result, 17, "test_index_key_has_column_zero_key_columns");
    
    /* ---------- index_key_matches_key unit tests ---------- */
    result = test_index_key_matches_key();
    generate_output(result, 18, "test_index_key_matches_key");
    result = test_index_key_matches_key_null_index();
    generate_output(result, 19, "test_index_key_matches_key_null_index");
    result = test_index_key_matches_key_null_column_ids();
    generate_output(result, 20, "test_index_key_matches_key_null_column_ids");
    result = test_index_key_matches_key_zero_num_columns();
    generate_output(result, 21, "test_index_key_matches_key_zero_num_columns");

    /* ---------- index_key_matches_prefix unit tests ---------- */
    result = test_index_key_matches_prefix();
    generate_output(result, 22, "test_index_key_matches_prefix");
    result = test_index_key_matches_prefix_null_index();
    generate_output(result, 23, "test_index_key_matches_prefix_null_index");
    result = test_index_key_matches_prefix_null_column_ids();
    generate_output(result, 24, "test_index_key_matches_prefix_null_column_ids");
    result = test_index_key_matches_prefix_zero_num_columns();
    generate_output(result, 22, "test_index_key_matches_prefix_zero_num_columns");
    

    return 0;
}