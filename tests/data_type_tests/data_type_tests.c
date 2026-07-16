#include <stdio.h>
#include <string.h>
#include "../../include/data_types.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return 1;  \
    }

/* ---------- value_alloc unit tests ---------- */

// Tests correctly assigned data type
static int test_value_alloc_sets_type() {
    Value *value = value_alloc(INTEGER);

    ASSERT(value != NULL);
    ASSERT(value->type == INTEGER);

    value_free(value);
    return 0;
}   

// Tests if union is allocated with 0
static int test_value_alloc_0_init_union() {
    Value *value = value_alloc(TEXT);
    ASSERT(value != NULL);

    Value zero_value = {0};
    ASSERT(memcmp(&value->value, &zero_value.value, sizeof(value->value)) == 0);

    value_free(value);
    return 0;
}

// Tests allocation of all data types
static int test_value_alloc_all_data_types() {
    const DataType types[] = {INTEGER, UNSIGNED_INTEGER, NUMERIC, FLOAT, DOUBLE, 
                        CHAR, VARCHAR, TEXT, DATE, TIMESTAMP, BLOB, BOOL, JSONB, NULL_TYPE};

    int num_types = sizeof(types) / sizeof(DataType);

    for (int i=0; i<num_types; i++) {
        Value *value = value_alloc(types[i]);

        ASSERT(value != NULL);
        ASSERT(value->type == types[i]);

        value_free(value);
    }

    return 0;
}

/* ---------- value_create unit tests ---------- */

// Successful value creation of all data types

static int test_value_create_integer() {
    int32_t input = -42;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == INTEGER);
    ASSERT(value->value.int32_val == -42);

    value_free(value);
    return 0;
}

static int test_value_create_unsigned_integer() {
    uint32_t input = 42;
    Value *value = value_create(UNSIGNED_INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == UNSIGNED_INTEGER);
    ASSERT(value->value.uint32_val == 42);

    value_free(value);
    return 0;
}

static int test_value_create_numeric() {
    numeric_t input = { .sign = 1, .val = 1532, .scale = 2};
    Value *value = value_create(NUMERIC, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == NUMERIC);
    ASSERT(value->value.numeric_val.sign == 1);
    ASSERT(value->value.numeric_val.val == 1532);
    ASSERT(value->value.numeric_val.scale == 2);

    value_free(value);
    return 0;
}

static int test_value_create_float() {
    float input = 12.3f;
    Value *value = value_create(FLOAT, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == FLOAT);
    ASSERT(value->value.float_val == input);

    value_free(value);
    return 0;
}

static int test_value_create_double() {
    double input = 2.4563123;
    Value *value = value_create(DOUBLE, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == DOUBLE);
    ASSERT(value->value.double_val == input);

    value_free(value);
    return 0;
}

static int test_value_create_char() {
    char source[] = "abc";
    char_n_t input = { .n = 5, .string = source};

    Value *value = value_create(CHAR, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == CHAR);
    ASSERT(value->value.char_val.n == 5);
    ASSERT(strcmp(value->value.char_val.string, source) == 0);
    ASSERT(value->value.char_val.string != source);

    value_free(value);
    return 0;
}

static int test_value_create_varchar() {
    char source[] = "hello";
    varchar_n_t input = { .max_n = 20, .string = source};

    Value *value = value_create(VARCHAR, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == VARCHAR);
    ASSERT(value->value.varchar_val.max_n == 20);
    ASSERT(strcmp(value->value.varchar_val.string, source) == 0);
    ASSERT(value->value.varchar_val.string != source);

    value_free(value);
    return 0;
}

static int test_value_create_text() {
    char source[] = "Database text";
    Value *value = value_create(TEXT, source);

    ASSERT(value != NULL);
    ASSERT(value->type == TEXT);
    ASSERT(strcmp(value->value.text_val, source) == 0);
    ASSERT(value->value.text_val != source);

    value_free(value);
    return 0;
}

static int test_value_create_date() {
    uint64_t input = 1752537600ULL;
    Value *value = value_create(DATE, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == DATE);
    ASSERT(value->value.date_val == input);

    value_free(value);
    return 0;
}

static int test_value_create_timestamp() {
    uint64_t input = 1752537600ULL;
    Value *value = value_create(TIMESTAMP, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == TIMESTAMP);
    ASSERT(value->value.timestamp_val == input);

    value_free(value);
    return 0;
}

static int test_value_create_blob() {
    uint8_t source[] = {0x01, 0x02, 0x03};

    blob_t input = { .size = sizeof(source), .buffer = source};
    Value *value = value_create(BLOB, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == BLOB);
    ASSERT(value->value.blob_val.size == sizeof(source));
    ASSERT(memcmp(value->value.blob_val.buffer, source, sizeof(source)) == 0);
    ASSERT(value->value.blob_val.buffer != source);

    value_free(value);
    return 0;
}

static int test_value_create_empty_blob() {
    blob_t input = { .size = 0, .buffer = NULL};
    Value *value = value_create(BLOB, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == BLOB);
    ASSERT(value->value.blob_val.size == 0);
    ASSERT(value->value.blob_val.buffer == NULL);

    value_free(value);
    return 0;
}

static int test_value_create_bool() {
    bool input = true;
    Value *value = value_create(BOOL, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == BOOL);
    ASSERT(value->value.bool_val == true);

    value_free(value);
    return 0;
}

static int test_value_create_jsonb() {
    uint8_t source[] = {'{', '"', 'a', '"', ':', '1', '}'};

    jsonb_t input = { .size = sizeof(source), .buffer = source};
    Value *value = value_create(JSONB, &input);

    ASSERT(value != NULL);
    ASSERT(value->type == JSONB);
    ASSERT(value->value.blob_val.size == sizeof(source));
    ASSERT(memcmp(value->value.blob_val.buffer, source, sizeof(source)) == 0);
    ASSERT(value->value.blob_val.buffer != source);

    value_free(value);
    return 0;
}

static int test_value_create_null() {
    Value *value = value_create(NULL_TYPE, NULL);

    ASSERT(value != NULL);
    ASSERT(value->type == NULL_TYPE);
    ASSERT(value->value.null_val == true);

    value_free(value);
    return 0;
}

// Failure cases
// NULL input 
static int test_value_create_null_input() {
    Value *value = value_create(INTEGER, NULL);

    ASSERT(value == NULL);
    return 0;
}

// NULL input string for CHAR(n)
static int test_value_create_null_char_string() {
    char_n_t input = {.n = 5, .string = NULL};
    Value *value = value_create(CHAR, &input);

    ASSERT(value == NULL);
    return 0;
}

// Oversized CHAR(n) string
static int test_value_create_oversized_char_string() {
    char_n_t input = {.n = 5, .string = "acdefghi"};
    Value *value = value_create(CHAR, &input);

    ASSERT(value == NULL);
    return 0;
}

// NULL input string for VARCHAR(n)
static int test_value_create_null_varchar_string() {
    varchar_n_t input = {.max_n = 5, .string = NULL};
    Value *value = value_create(VARCHAR, &input);

    ASSERT(value == NULL);
    return 0;
}

// Oversized VARCHAR(n) string
static int test_value_create_oversized_varchar_string() {
    varchar_n_t input = {.max_n = 5, .string = "acdefghi"};
    Value *value = value_create(VARCHAR, &input);

    ASSERT(value == NULL);
    return 0;
}

// NULL BLOB buffer
static int test_value_create_null_blob_buffer() {
    blob_t input = {.size = 5, .buffer = NULL};
    Value *value = value_create(BLOB, &input);

    ASSERT(value == NULL);
    return 0;
}

// NULL JSONB buffer
static int test_value_create_null_jsonb_buffer() {
    jsonb_t input = {.size = 5, .buffer = NULL};
    Value *value = value_create(JSONB, &input);

    ASSERT(value == NULL);
    return 0;
}

// Unsupported data type
static int test_value_create_unsupported_type() {
    int32_t input = 10;
    Value *value = value_create((DataType) 9999, &input);

    ASSERT(value == NULL);
    return 0;
}

/* ---------- value_copy unit tests ---------- */

// Successful creation of value copy for all data types

static int test_value_copy_integer() {
    int32_t input = -42;
    Value *value = value_create(INTEGER, &input);
    ASSERT(value != NULL);
    
    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == INTEGER);
    ASSERT(copy->value.int32_val == -42);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_unsigned_integer() {
    uint32_t input = 42;
    Value *value = value_create(UNSIGNED_INTEGER, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == UNSIGNED_INTEGER);
    ASSERT(copy->value.uint32_val == 42);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_numeric() {
    numeric_t input = { .sign = 1, .val = 1532, .scale = 2};
    Value *value = value_create(NUMERIC, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == NUMERIC);
    ASSERT(copy->value.numeric_val.sign == 1);
    ASSERT(copy->value.numeric_val.val == 1532);
    ASSERT(copy->value.numeric_val.scale == 2);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_float() {
    float input = 12.3f;
    Value *value = value_create(FLOAT, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == FLOAT);
    ASSERT(copy->value.float_val == input);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_double() {
    double input = 2.4563123;
    Value *value = value_create(DOUBLE, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);
    
    ASSERT(copy != NULL);
    ASSERT(copy->type == DOUBLE);
    ASSERT(copy->value.double_val == input);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_char() {
    char source[] = "abc";
    char_n_t input = { .n = 5, .string = source};

    Value *value = value_create(CHAR, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == CHAR);
    ASSERT(copy->value.char_val.n == 5);
    ASSERT(strcmp(copy->value.char_val.string, source) == 0);
    ASSERT(copy->value.char_val.string != source);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_varchar() {
    char source[] = "hello";
    varchar_n_t input = { .max_n = 20, .string = source };

    Value *value = value_create(VARCHAR, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == VARCHAR);
    ASSERT(copy->value.varchar_val.max_n == 20);
    ASSERT(strcmp(copy->value.varchar_val.string, source) == 0);
    ASSERT(copy->value.varchar_val.string != source);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_text() {
    char source[] = "Database text";
    Value *value = value_create(TEXT, source);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == TEXT);
    ASSERT(strcmp(copy->value.text_val, source) == 0);
    ASSERT(copy->value.text_val != source);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_date() {
    uint64_t input = 1752537600ULL;
    Value *value = value_create(DATE, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == DATE);
    ASSERT(copy->value.date_val == input);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_timestamp() {
    uint64_t input = 1752537600ULL;
    Value *value = value_create(TIMESTAMP, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == TIMESTAMP);
    ASSERT(copy->value.timestamp_val == input);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_blob() {
    uint8_t source[] = {0x01, 0x02, 0x03};

    blob_t input = { .size = sizeof(source), .buffer = source};
    Value *value = value_create(BLOB, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == BLOB);
    ASSERT(copy->value.blob_val.size == sizeof(source));
    ASSERT(memcmp(copy->value.blob_val.buffer, source, sizeof(source)) == 0);
    ASSERT(copy->value.blob_val.buffer != source);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_empty_blob() {
    blob_t input = { .size = 0, .buffer = NULL};
    Value *value = value_create(BLOB, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == BLOB);
    ASSERT(copy->value.blob_val.size == 0);
    ASSERT(copy->value.blob_val.buffer == NULL);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_bool() {
    bool input = true;
    Value *value = value_create(BOOL, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == BOOL);
    ASSERT(copy->value.bool_val == true);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_jsonb() {
    uint8_t source[] = {'{', '"', 'a', '"', ':', '1', '}'};

    jsonb_t input = { .size = sizeof(source), .buffer = source};
    Value *value = value_create(JSONB, &input);
    ASSERT(value != NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(copy->type == JSONB);
    ASSERT(copy->value.blob_val.size == sizeof(source));
    ASSERT(memcmp(copy->value.blob_val.buffer, source, sizeof(source)) == 0);
    ASSERT(copy->value.blob_val.buffer != source);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

static int test_value_copy_null() {
    Value *value = value_create(NULL_TYPE, NULL);

    Value *copy = value_copy(value);

    ASSERT(copy != NULL);
    ASSERT(value != NULL);
    ASSERT(value->type == NULL_TYPE);
    ASSERT(value->value.null_val == true);
    ASSERT(copy != value);

    value_free(value);
    value_free(copy);
    return 0;
}

// Failure cases
// NULL input value
static int test_value_copy_null_value() {
    Value *copy = value_copy(NULL);

    ASSERT(copy == NULL);
    return 0;
}

// Unsupported data type
static int test_value_copy_unsupported_data_type() {
    Value value = { .type = (DataType) 9999, .value = { 42135834L } };
    
    Value *copy = value_copy(&value);

    ASSERT(copy == NULL);
    return 0;
}

/* ---------- value_assign unit tests ---------- */

// Successful cases
// Scaler assignment
static int test_value_assign_integer() {
    int32_t src_input = -42;
    int32_t dest_input = 10;

    Value *src = value_create(INTEGER, &src_input);
    Value *dest = value_create(INTEGER, &dest_input);
    ASSERT(src != NULL);
    ASSERT(dest != NULL);

    bool assigned = value_assign(dest, src);

    ASSERT(assigned == true);
    ASSERT(dest->type == INTEGER);
    ASSERT(dest->value.int32_val == src_input);
    ASSERT(dest != src);

    ASSERT(src->value.int32_val == src_input);

    value_free(src);
    value_free(dest);
    return 0;
}

// Assignment changes the destination type
static int test_value_assign_changes_type() {
    double src_input = 12.5;
    int32_t dest_input = 10;

    Value *src = value_create(DOUBLE, &src_input);
    Value *dest = value_create(INTEGER, &dest_input);
    ASSERT(src != NULL);
    ASSERT(dest != NULL);

    bool assigned = value_assign(dest, src);

    ASSERT(assigned == true);
    ASSERT(dest->type == DOUBLE);
    ASSERT(dest->value.double_val == src_input);
    ASSERT(dest != src);

    ASSERT(src->value.double_val == src_input);

    value_free(src);
    value_free(dest);
    return 0;
}

// String assignment creates a separate copy
static int test_value_assign_text_deep_copy() {
    Value *src = value_create(TEXT, "New source text");
    Value *dest = value_create(TEXT, "Old destination text");
    ASSERT(src != NULL);
    ASSERT(dest != NULL);

    bool assigned = value_assign(dest, src);

    ASSERT(assigned == true);
    ASSERT(dest->type == TEXT);
    ASSERT(strcmp(dest->value.text_val, "New source text") == 0);
    ASSERT(dest->value.text_val != src->value.text_val);

    // Source TEXT string modification shouldn't affect the destination TEXT string
    src->value.text_val[0] = 'X';
    ASSERT(strcmp(dest->value.text_val, "New source text") == 0);

    value_free(src);
    value_free(dest);
    return 0;
}

// BLOB assignment creates a separate copy
static int test_value_assign_blob_deep_copy() {
    uint8_t src_buf[] = {1, 2, 3, 4};
    uint8_t dest_buf[] = {9, 8};

    blob_t src_blob = {.size = sizeof(src_buf), .buffer = src_buf };
    blob_t dest_blob = {.size = sizeof(dest_buf), .buffer = dest_buf };

    Value *src = value_create(BLOB, &src_blob);
    Value *dest = value_create(BLOB, &dest_blob);
    ASSERT(src != NULL);
    ASSERT(dest != NULL);

    bool assigned = value_assign(dest, src);

    ASSERT(assigned == true);
    ASSERT(dest->type == BLOB);
    ASSERT(dest->value.blob_val.size == sizeof(src_buf));
    ASSERT(memcmp(dest->value.blob_val.buffer, src_buf, sizeof(src_buf)) == 0);
    ASSERT(dest->value.blob_val.buffer != src->value.blob_val.buffer);

    // Source BLOB buffer modification shouldn't affect the destination BLOB buffer
    src->value.blob_val.buffer[0] = 99;
    ASSERT(dest->value.blob_val.buffer[0] == 1);

    value_free(src);
    value_free(dest);

    return 0;
}

// Self-assignment
static int test_value_assign_self_assignment() {
    Value *value = value_create(TEXT, "Hello");
    ASSERT(value != NULL);

    char *original_string = value->value.text_val;

    bool assigned = value_assign(value, value);

    ASSERT(assigned == true);
    ASSERT(value->type == TEXT);
    ASSERT(strcmp(value->value.text_val, "Hello") == 0);

    // The function returns before copying or freeing anything.
    ASSERT(value->value.text_val == original_string);

    value_free(value);

    return 0;
}

// Failure cases
// NULL source
static int test_value_assign_null_source() {
    int32_t input = 10;
    Value *dest = value_create(INTEGER, &input);
    ASSERT(dest != NULL);

    bool assigned = value_assign(dest, NULL);

    ASSERT(assigned == false);

    // Destination should remain unchanged.
    ASSERT(dest->type == INTEGER);
    ASSERT(dest->value.int32_val == input);

    value_free(dest);
    return 0;
}

// NULL destination
static int test_value_assign_null_destination() {
    int32_t input = 10;
    Value *src = value_create(INTEGER, &input);
    ASSERT(src != NULL);

    bool assigned = value_assign(NULL, src);

    ASSERT(assigned == false);

    value_free(src);
    return 0;
}

// Invalid source type
static int test_value_assign_invalid_source_type() {
    int32_t dest_input = 25;
    Value *dest = value_create(INTEGER, &dest_input);
    ASSERT(dest != NULL);

    Value invalid_src = { .type = (DataType)999 };

    bool assigned = value_assign(dest, &invalid_src);

    ASSERT(assigned == false);

    // Destination must remain unchanged because the temporary
    // copy failed before value_free_internal(dest) was called.
    ASSERT(dest->type == INTEGER);
    ASSERT(dest->value.int32_val == dest_input);

    value_free(dest);
    return 0;
}

/* ---------- value_types_compatible unit tests ---------- */

// Test all valid numerical data type pairs
static int test_value_types_compatible_numerical() {
    ASSERT(value_types_compatible(INTEGER, INTEGER));
    ASSERT(value_types_compatible(INTEGER, UNSIGNED_INTEGER));
    ASSERT(value_types_compatible(INTEGER, NUMERIC));
    ASSERT(value_types_compatible(INTEGER, FLOAT));
    ASSERT(value_types_compatible(INTEGER, DOUBLE));

    ASSERT(value_types_compatible(UNSIGNED_INTEGER, UNSIGNED_INTEGER));
    ASSERT(value_types_compatible(UNSIGNED_INTEGER, NUMERIC));
    ASSERT(value_types_compatible(UNSIGNED_INTEGER, FLOAT));
    ASSERT(value_types_compatible(UNSIGNED_INTEGER, DOUBLE));
    
    ASSERT(value_types_compatible(NUMERIC, NUMERIC));
    ASSERT(value_types_compatible(NUMERIC, FLOAT));
    ASSERT(value_types_compatible(NUMERIC, DOUBLE));

    ASSERT(value_types_compatible(FLOAT, DOUBLE));

    return 0;
}

// Test all valid text data type pairs
static int test_value_types_compatible_text() {
    ASSERT(value_types_compatible(CHAR, CHAR));
    ASSERT(value_types_compatible(CHAR, VARCHAR));
    ASSERT(value_types_compatible(CHAR, TEXT));

    ASSERT(value_types_compatible(VARCHAR, VARCHAR));
    ASSERT(value_types_compatible(VARCHAR, TEXT));

    ASSERT(value_types_compatible(TEXT, TEXT));

    return 0;
}

// Test all valid temporal data type pairs
static int test_value_types_compatible_temporal() {
    ASSERT(value_types_compatible(DATE, DATE));
    ASSERT(value_types_compatible(DATE, TIMESTAMP));
    ASSERT(value_types_compatible(TIMESTAMP, DATE));
    ASSERT(value_types_compatible(TIMESTAMP, TIMESTAMP));

    return 0;
}

// Test all invalid temporal data type pairs
static int test_value_types_compatible_rejects_invalid() {
    ASSERT(!value_types_compatible(INTEGER, CHAR));
    ASSERT(!value_types_compatible(INTEGER, VARCHAR));
    ASSERT(!value_types_compatible(INTEGER, TEXT));
    ASSERT(!value_types_compatible(INTEGER, DATE));
    ASSERT(!value_types_compatible(INTEGER, TIMESTAMP));
    ASSERT(!value_types_compatible(INTEGER, BLOB));
    ASSERT(!value_types_compatible(INTEGER, JSONB));
    ASSERT(!value_types_compatible(INTEGER, BOOL));

    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, CHAR));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, VARCHAR));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, TEXT));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, DATE));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, TIMESTAMP));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, BLOB));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, JSONB));
    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, BOOL));

    ASSERT(!value_types_compatible(NUMERIC, CHAR));
    ASSERT(!value_types_compatible(NUMERIC, VARCHAR));
    ASSERT(!value_types_compatible(NUMERIC, TEXT));
    ASSERT(!value_types_compatible(NUMERIC, DATE));
    ASSERT(!value_types_compatible(NUMERIC, TIMESTAMP));
    ASSERT(!value_types_compatible(NUMERIC, BLOB));
    ASSERT(!value_types_compatible(NUMERIC, JSONB));
    ASSERT(!value_types_compatible(NUMERIC, BOOL));

    ASSERT(!value_types_compatible(FLOAT, CHAR));
    ASSERT(!value_types_compatible(FLOAT, VARCHAR));
    ASSERT(!value_types_compatible(FLOAT, TEXT));
    ASSERT(!value_types_compatible(FLOAT, DATE));
    ASSERT(!value_types_compatible(FLOAT, TIMESTAMP));
    ASSERT(!value_types_compatible(FLOAT, BLOB));
    ASSERT(!value_types_compatible(FLOAT, JSONB));
    ASSERT(!value_types_compatible(FLOAT, BOOL));

    ASSERT(!value_types_compatible(DOUBLE, CHAR));
    ASSERT(!value_types_compatible(DOUBLE, VARCHAR));
    ASSERT(!value_types_compatible(DOUBLE, TEXT));
    ASSERT(!value_types_compatible(DOUBLE, DATE));
    ASSERT(!value_types_compatible(DOUBLE, TIMESTAMP));
    ASSERT(!value_types_compatible(DOUBLE, BLOB));
    ASSERT(!value_types_compatible(DOUBLE, JSONB));
    ASSERT(!value_types_compatible(DOUBLE, BOOL));

    ASSERT(!value_types_compatible(DATE, CHAR));
    ASSERT(!value_types_compatible(DATE, VARCHAR));
    ASSERT(!value_types_compatible(DATE, TEXT));
    ASSERT(!value_types_compatible(DATE, BLOB));
    ASSERT(!value_types_compatible(DATE, JSONB));
    ASSERT(!value_types_compatible(DATE, BOOL));

    ASSERT(!value_types_compatible(TIMESTAMP, CHAR));
    ASSERT(!value_types_compatible(TIMESTAMP, VARCHAR));
    ASSERT(!value_types_compatible(TIMESTAMP, TEXT));
    ASSERT(!value_types_compatible(TIMESTAMP, BLOB));
    ASSERT(!value_types_compatible(TIMESTAMP, JSONB));
    ASSERT(!value_types_compatible(TIMESTAMP, BOOL));

    ASSERT(!value_types_compatible(CHAR, BLOB));
    ASSERT(!value_types_compatible(CHAR, JSONB));
    ASSERT(!value_types_compatible(CHAR, BOOL));
    ASSERT(!value_types_compatible(VARCHAR, BLOB));
    ASSERT(!value_types_compatible(VARCHAR, JSONB));
    ASSERT(!value_types_compatible(VARCHAR, BOOL));
    ASSERT(!value_types_compatible(TEXT, BLOB));
    ASSERT(!value_types_compatible(TEXT, JSONB));
    ASSERT(!value_types_compatible(TEXT, BOOL));

    return 0;
}

static int test_value_types_compatible_rejects_null_type() {
    ASSERT(!value_types_compatible(INTEGER, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, INTEGER));

    ASSERT(!value_types_compatible(UNSIGNED_INTEGER, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, UNSIGNED_INTEGER));

    ASSERT(!value_types_compatible(NUMERIC, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, NUMERIC));

    ASSERT(!value_types_compatible(FLOAT, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, FLOAT));

    ASSERT(!value_types_compatible(DOUBLE, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, DOUBLE));

    ASSERT(!value_types_compatible(CHAR, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, CHAR));

    ASSERT(!value_types_compatible(VARCHAR, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, VARCHAR));

    ASSERT(!value_types_compatible(TEXT, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, TEXT));

    ASSERT(!value_types_compatible(DATE, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, DATE));

    ASSERT(!value_types_compatible(TIMESTAMP, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, TIMESTAMP));

    ASSERT(!value_types_compatible(BLOB, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, BLOB));

    ASSERT(!value_types_compatible(JSONB, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, JSONB));

    ASSERT(!value_types_compatible(BOOL, NULL_TYPE));
    ASSERT(!value_types_compatible(NULL_TYPE, BOOL));

    ASSERT(!value_types_compatible(NULL_TYPE, NULL_TYPE));

    return 0;
}

/* ---------- value_can_assign unit tests ---------- */

// Testing assignment cases for only data types allowing conversions besides the same data type
static int test_value_can_assign_integer() {
    int32_t input = -42;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(UNSIGNED_INTEGER, value));
    ASSERT(value_can_assign(NUMERIC, value));
    ASSERT(value_can_assign(FLOAT, value));
    ASSERT(value_can_assign(DOUBLE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_unsigned_integer() {
    uint32_t input = 42;
    Value *value = value_create(UNSIGNED_INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value_can_assign(UNSIGNED_INTEGER, value));
    ASSERT(value_can_assign(NUMERIC, value));
    ASSERT(value_can_assign(FLOAT, value));
    ASSERT(value_can_assign(DOUBLE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_numeric() {
    numeric_t input = { .sign = 1, .val = 12345, .scale = 2};
    Value *value = value_create(NUMERIC, &input);

    ASSERT(value != NULL);
    ASSERT(!value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(UNSIGNED_INTEGER, value));
    ASSERT(value_can_assign(NUMERIC, value));
    ASSERT(!value_can_assign(FLOAT, value));
    ASSERT(!value_can_assign(DOUBLE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_float() {
    float input = 3.5f;
    Value *value = value_create(FLOAT, &input);

    ASSERT(value != NULL);
    ASSERT(!value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(NUMERIC, value));
    ASSERT(value_can_assign(FLOAT, value));
    ASSERT(value_can_assign(DOUBLE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_double() {
    double input = 12.5625;
    Value *value = value_create(DOUBLE, &input);

    ASSERT(value != NULL);
    ASSERT(!value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(NUMERIC, value));
    ASSERT(!value_can_assign(FLOAT, value));
    ASSERT(value_can_assign(DOUBLE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_char() {
    char_n_t input = { .n = 5, .string = "abc" };
    Value *value = value_create(CHAR, &input);
    
    ASSERT(value != NULL);
    ASSERT(value_can_assign(CHAR, value));
    ASSERT(value_can_assign(VARCHAR, value));
    ASSERT(value_can_assign(TEXT, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_varchar() {
    varchar_n_t input = { .max_n = 5, .string = "abcd" };
    Value *value = value_create(VARCHAR, &input);
    
    ASSERT(value != NULL);
    ASSERT(value_can_assign(CHAR, value));
    ASSERT(value_can_assign(VARCHAR, value));
    ASSERT(value_can_assign(TEXT, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_text() {
    char input[] = "abcde";
    Value *value = value_create(TEXT, &input);
    
    ASSERT(value != NULL);
    ASSERT(!value_can_assign(CHAR, value));
    ASSERT(!value_can_assign(VARCHAR, value));
    ASSERT(value_can_assign(TEXT, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_date() {
    uint64_t input = 42135834LL;
    Value *value = value_create(DATE, &input);
    
    ASSERT(value != NULL);
    ASSERT(value_can_assign(TIMESTAMP, value));
    ASSERT(value_can_assign(DATE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_timestamp() {
    uint64_t input = 42135834LL;
    Value *value = value_create(TIMESTAMP, &input);
    
    ASSERT(value != NULL);
    ASSERT(value_can_assign(TIMESTAMP, value));
    ASSERT(!value_can_assign(DATE, value));

    value_free(value);
    return 0;
}

static int test_value_can_assign_null_source() {
    Value *value = value_create(NULL_TYPE, NULL);

    ASSERT(value != NULL);
    ASSERT(value_can_assign(INTEGER, value));
    ASSERT(value_can_assign(TEXT, value));
    ASSERT(value_can_assign(BOOL, value));

    value_free(value);
    return 0;
}

// Failure cases
static int test_value_can_assign_rejects_unrelated_type()
{
    bool input = true;
    Value *value = value_create(BOOL, &input);

    ASSERT(value != NULL);
    ASSERT(!value_can_assign(INTEGER, value));
    ASSERT(!value_can_assign(TEXT, value));

    value_free(value);
    return 0;
}

/* ---------- value_convert_data_type unit tests ---------- */

// Testing successful cases according to value_can_assign() function
static int test_convert_integer_to_unsigned() {
    int32_t input = 42;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value_convert_data_type(UNSIGNED_INTEGER, value));
    ASSERT(value->type == UNSIGNED_INTEGER);
    ASSERT(value->value.uint32_val == 42U);

    value_free(value);
    return 0;
}

static int test_value_convert_integer_to_numeric() {
    int32_t input = -42;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value_convert_data_type(NUMERIC, value));

    ASSERT(value->type == NUMERIC);
    ASSERT(value->value.numeric_val.sign == -1);
    ASSERT(value->value.numeric_val.val == 42);
    ASSERT(value->value.numeric_val.scale == 0);

    value_free(value);
    return 0;
}

static int test_value_convert_integer_to_float() {
    int32_t input = 42;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value_convert_data_type(FLOAT, value));
    ASSERT(value->type == FLOAT);
    ASSERT(value->value.float_val == 42.0f);

    value_free(value);
    return 0;
}

static int test_value_convert_float_to_double() {
    float input = 4.5f;
    Value *value = value_create(FLOAT, &input);

    ASSERT(value != NULL);
    ASSERT(value_convert_data_type(DOUBLE, value));
    ASSERT(value->type == DOUBLE);
    ASSERT(value->value.double_val == (double)input);

    value_free(value);
    return 0;
}

static int test_value_convert_char_to_varchar() {
    char_n_t input = { .n = 10, .string = "hello" };

    Value *value = value_create(CHAR, &input);
    ASSERT(value != NULL);

    char *old_pointer = value->value.char_val.string;

    ASSERT(value_convert_data_type(VARCHAR, value));
    ASSERT(value->type == VARCHAR);
    ASSERT(value->value.varchar_val.max_n == 10);
    ASSERT(strcmp(value->value.varchar_val.string, "hello") == 0);
    ASSERT(value->value.varchar_val.string != old_pointer);

    value_free(value);
    return 0;
}

static int test_value_convert_varchar_to_text() {
    varchar_n_t input = { .max_n = 20, .string = "database" };

    Value *value = value_create(VARCHAR, &input);
    ASSERT(value != NULL);

    ASSERT(value_convert_data_type(TEXT, value));
    ASSERT(value->type == TEXT);
    ASSERT(strcmp(value->value.text_val, "database") == 0);

    value_free(value);
    return 0;
}

static int test_value_convert_date_to_timestamp() {
    uint64_t epoch = 1752537600ULL;
    Value *value = value_create(DATE, &epoch);

    ASSERT(value != NULL);
    ASSERT(value_convert_data_type(TIMESTAMP, value));

    ASSERT(value->type == TIMESTAMP);
    ASSERT(value->value.timestamp_val == epoch);

    value_free(value);
    return 0;
}

static int test_value_convert_identical_type() {
    int32_t input = 10;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(value_convert_data_type(INTEGER, value));
    ASSERT(value->type == INTEGER);
    ASSERT(value->value.int32_val == input);

    value_free(value);
    return 0;
}

// Failure case
static int test_value_convert_negative_integer_to_unsigned_fails() {
    int32_t input = -42;
    Value *value = value_create(INTEGER, &input);

    ASSERT(value != NULL);
    ASSERT(!value_convert_data_type(UNSIGNED_INTEGER, value));

    // Original value must remain unchanged.
    ASSERT(value->type == INTEGER);
    ASSERT(value->value.int32_val == input);

    value_free(value);
    return 0;
}

static int test_value_convert_null_value() {
    ASSERT(!value_convert_data_type(INTEGER, NULL));

    return 0;
}

static int test_value_convert_unsupported_conversion() {
    bool input = true;
    Value *value = value_create(BOOL, &input);

    ASSERT(value != NULL);
    ASSERT(!value_convert_data_type(INTEGER, value));

    ASSERT(value->type == BOOL);
    ASSERT(value->value.bool_val == true);

    value_free(value);
    return 0;
}

/* ---------- value_compare unit tests ---------- */

// Success cases
static int test_value_compare_integers() {

    // left < right
    int32_t left_input = 10;
    int32_t right_input = 20;

    Value *left = value_create(INTEGER, &left_input);
    Value *right = value_create(INTEGER, &right_input);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result = 99;

    ASSERT(value_compare(left, right, &result));
    ASSERT(result == -1);

    // left == right
    left->value.int32_val = 20;
    right->value.int32_val = 20;

    ASSERT(value_compare(left, right, &result));
    ASSERT(result == 0);

    // left > right
    left->value.int32_val = 30;
    right->value.int32_val = 20;

    ASSERT(value_compare(left, right, &result));
    ASSERT(result == 1);

    value_free(left);
    value_free(right);
    return 0;
}

static int test_value_compare_integer_double() {
    int32_t left_input = 10;
    double right_input = 10.5;

    Value *left = value_create(INTEGER, &left_input);
    Value *right = value_create(DOUBLE, &right_input);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;
    ASSERT(value_compare(left, right, &result));
    ASSERT(result == -1);

    // Operands remain unchanged because internal copies get converted to a common data type
    ASSERT(left->type == INTEGER);
    ASSERT(right->type == DOUBLE);

    value_free(left);
    value_free(right);
    return 0;
}

static int test_value_compare_numeric_different_scales() {
    numeric_t left_input = { .sign = 1, .val = 123, .scale = 1 };
    numeric_t right_input = { .sign = 1, .val = 1230, .scale = 2 };

    Value *left = value_create(NUMERIC, &left_input);
    Value *right = value_create(NUMERIC, &right_input);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;
    ASSERT(value_compare(left, right, &result));
    ASSERT(result == 0);

    value_free(left);
    value_free(right);
    return 0;
}

static int test_value_compare_char_and_varchar() {
    char_n_t left_input = { .n = 10, .string = "apple" };
    varchar_n_t right_input = { .max_n = 20, .string = "banana" };

    Value *left = value_create(CHAR, &left_input);
    Value *right = value_create(VARCHAR, &right_input);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;
    ASSERT(value_compare(left, right, &result));
    ASSERT(result == -1);

    value_free(left);
    value_free(right);
    return 0;
}

static int test_value_compare_date_timestamp() {
    uint64_t date_epoch = 1000;
    uint64_t timestamp_epoch = 2000;

    Value *left = value_create(DATE, &date_epoch);
    Value *right = value_create(TIMESTAMP, &timestamp_epoch);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;

    ASSERT(value_compare(left, right, &result));
    ASSERT(result == -1);

    value_free(left);
    value_free(right);
    return 0;
}

static int test_value_compare_bool() {
    bool left_input = false;
    bool right_input = true;

    Value *left = value_create(BOOL, &left_input);
    Value *right = value_create(BOOL, &right_input);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;

    ASSERT(value_compare(left, right, &result));
    ASSERT(result == -1);

    value_free(left);
    value_free(right);
    return 0;
}

static int test_value_compare_blob() {
    uint8_t left_buffer[] = {1, 2, 3};
    uint8_t right_buffer[] = {1, 2, 4};

    blob_t left_blob = { .size = sizeof(left_buffer), .buffer = left_buffer };
    blob_t right_blob = { .size = sizeof(right_buffer), .buffer = right_buffer };

    Value *left = value_create(BLOB, &left_blob);
    Value *right = value_create(BLOB, &right_blob);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;

    ASSERT(value_compare(left, right, &result));
    ASSERT(result == -1);

    value_free(left);
    value_free(right);
    return 0;
}

// Failure cases
static int test_value_compare_incompatible_types() {
    int32_t integer = 10;
    bool boolean = true;

    Value *left = value_create(INTEGER, &integer);
    Value *right = value_create(BOOL, &boolean);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result = 123;

    ASSERT(!value_compare(left, right, &result));

    // A failed comparison should ideally leave result unchanged.
    ASSERT(result == 123);

    value_free(left);
    value_free(right);

    return 0;
}

static int test_value_compare_null_type_fails() {
    int32_t input = 10;

    Value *left = value_create(NULL_TYPE, NULL);
    Value *right = value_create(INTEGER, &input);

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    int result;

    ASSERT(!value_compare(left, right, &result));

    value_free(left);
    value_free(right);

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
    printf("> STARTING TESTS\n");

    /* ---------- value_alloc unit tests ---------- */
    result = test_value_alloc_sets_type();
    generate_output(result, 0, "test_value_alloc_sets_type");
    result = test_value_alloc_0_init_union();
    generate_output(result, 1, "test_value_alloc_0_init_union");
    result = test_value_alloc_all_data_types();
    generate_output(result, 2, "test_value_alloc_all_data_types");


    /* ---------- value_create unit tests ---------- */
    result = test_value_create_integer();
    generate_output(result, 3, "test_value_create_integer");
    result = test_value_create_unsigned_integer();
    generate_output(result, 4, "test_value_create_unsigned_integer");
    result = test_value_create_numeric();
    generate_output(result, 5, "test_value_create_numeric");
    result = test_value_create_float();
    generate_output(result, 6, "test_value_create_float");
    result = test_value_create_double();
    generate_output(result, 7, "test_value_create_double");
    result = test_value_create_char();
    generate_output(result, 8, "test_value_create_char");
    result = test_value_create_varchar();
    generate_output(result, 9, "test_value_create_varchar");
    result = test_value_create_text();
    generate_output(result, 10, "test_value_create_text");
    result = test_value_create_date();
    generate_output(result, 11, "test_value_create_date");
    result = test_value_create_timestamp();
    generate_output(result, 12, "test_value_create_timestamp");
    result = test_value_create_blob();
    generate_output(result, 13, "test_value_create_blob");
    result = test_value_create_empty_blob();
    generate_output(result, 14, "test_value_create_empty_blob");
    result = test_value_create_bool();
    generate_output(result, 15, "test_value_create_bool");
    result = test_value_create_jsonb();
    generate_output(result, 16, "test_value_create_jsonb");
    result = test_value_create_null();
    generate_output(result, 17, "test_value_create_null");

    result = test_value_create_null_input();
    generate_output(result, 18, "test_value_create_null_input");
    result = test_value_create_null_char_string();
    generate_output(result, 19, "test_value_create_null_char_string");
    result = test_value_create_oversized_char_string();
    generate_output(result, 20, "test_value_create_oversized_char_string");
    result = test_value_create_null_varchar_string();
    generate_output(result, 21, "test_value_create_null_varchar_string");
    result = test_value_create_oversized_varchar_string();
    generate_output(result, 22, "test_value_create_oversized_varchar_string");
    result = test_value_create_null_blob_buffer();
    generate_output(result, 23, "test_value_create_null_blob_buffer");
    result = test_value_create_null_jsonb_buffer();
    generate_output(result, 24, "test_value_create_null_jsonb_buffer");
    result = test_value_create_unsupported_type();
    generate_output(result, 25, "test_value_create_unsupported_type");


    /* ---------- value_copy unit tests ---------- */
    result = test_value_copy_integer();
    generate_output(result, 26, "test_value_copy_integer");
    result = test_value_copy_unsigned_integer();
    generate_output(result, 27, "test_value_copy_unsigned_integer");
    result = test_value_copy_numeric();
    generate_output(result, 28, "test_value_copy_numeric");
    result = test_value_copy_float();
    generate_output(result, 29, "test_value_copy_float");
    result = test_value_copy_double();
    generate_output(result, 30, "test_value_copy_double");
    result = test_value_copy_char();
    generate_output(result, 31, "test_value_copy_char");
    result = test_value_copy_varchar();
    generate_output(result, 32, "test_value_copy_varchar");
    result = test_value_copy_text();
    generate_output(result, 33, "test_value_copy_text");
    result = test_value_copy_date();
    generate_output(result, 34, "test_value_copy_date");
    result = test_value_copy_timestamp();
    generate_output(result, 35, "test_value_copy_timestamp");
    result = test_value_copy_blob();
    generate_output(result, 36, "test_value_copy_blob");
    result = test_value_copy_empty_blob();
    generate_output(result, 37, "test_value_copy_empty_blob");
    result = test_value_copy_bool();
    generate_output(result, 38, "test_value_copy_bool");
    result = test_value_copy_jsonb();
    generate_output(result, 39, "test_value_copy_jsonb");
    result = test_value_copy_null();
    generate_output(result, 40, "test_value_copy_null");

    result = test_value_copy_null_value();
    generate_output(result, 41, "test_value_copy_null_value");
    result = test_value_copy_unsupported_data_type();
    generate_output(result, 42, "test_value_copy_unsupported_data_type");


    /* ---------- value_assign unit tests ---------- */
    result = test_value_assign_integer();
    generate_output(result, 43, "test_value_assign_integer");
    result = test_value_assign_changes_type();
    generate_output(result, 44, "test_value_assign_changes_type");
    result = test_value_assign_text_deep_copy();
    generate_output(result, 45, "test_value_assign_text_deep_copy");
    result = test_value_assign_blob_deep_copy();
    generate_output(result, 45, "test_value_assign_blob_deep_copy");
    result = test_value_assign_self_assignment();
    generate_output(result, 46, "test_value_assign_self_assignment");
    result = test_value_assign_null_source();
    generate_output(result, 47, "test_value_assign_null_source");
    result = test_value_assign_null_destination();
    generate_output(result, 48, "test_value_assign_null_destination");
    result = test_value_assign_invalid_source_type();
    generate_output(result, 49, "test_value_assign_invalid_source_type");

    /* ---------- value_types_compatible unit tests ---------- */
    result = test_value_types_compatible_numerical();
    generate_output(result, 50, "test_value_types_compatible_numerical");
    result = test_value_types_compatible_text();
    generate_output(result, 51, "test_value_types_compatible_numerical");
    result = test_value_types_compatible_temporal();
    generate_output(result, 52, "test_value_types_compatible_temporal");
    result = test_value_types_compatible_rejects_invalid();
    generate_output(result, 53, "test_value_types_compatible_rejects_invalid");
    result = test_value_types_compatible_rejects_null_type();
    generate_output(result, 54, "test_value_types_compatible_rejects_null_type");

    /* ---------- value_can_assign unit tests ---------- */
    result = test_value_can_assign_integer();
    generate_output(result, 55, "test_value_can_assign_integer");
    result = test_value_can_assign_unsigned_integer();
    generate_output(result, 56, "test_value_can_assign_unsigned_integer");
    result = test_value_can_assign_numeric();
    generate_output(result, 58, "test_value_can_assign_numeric");
    result = test_value_can_assign_float();
    generate_output(result, 59, "test_value_can_assign_float");
    result = test_value_can_assign_double();
    generate_output(result, 60, "test_value_can_assign_double");
    result = test_value_can_assign_char();
    generate_output(result, 61, "test_value_can_assign_char");
    result = test_value_can_assign_varchar();
    generate_output(result, 62, "test_value_can_assign_varchar");
    result = test_value_can_assign_text();
    generate_output(result, 63, "test_value_can_assign_text");
    result = test_value_can_assign_date();
    generate_output(result, 64, "test_value_can_assign_date");
    result = test_value_can_assign_timestamp();
    generate_output(result, 65, "test_value_can_assign_timestamp");
    result = test_value_can_assign_null_source();
    generate_output(result, 66, "test_value_can_assign_null_source");
    result = test_value_can_assign_rejects_unrelated_type();
    generate_output(result, 67, "test_value_can_assign_rejects_unrelated_type");

    /* ---------- value_convert_data_type unit tests ---------- */
    result = test_convert_integer_to_unsigned();
    generate_output(result, 68, "test_convert_integer_to_unsigned");
    result = test_value_convert_integer_to_numeric();
    generate_output(result, 69, "test_value_convert_integer_to_numeric");
    result = test_value_convert_integer_to_float();
    generate_output(result, 70, "test_value_convert_integer_to_float");
    result = test_value_convert_float_to_double();
    generate_output(result, 71, "test_value_convert_float_to_double");
    result = test_value_convert_char_to_varchar();
    generate_output(result, 72, "test_value_convert_char_to_varchar");
    result = test_value_convert_varchar_to_text();
    generate_output(result, 73, "test_value_convert_varchar_to_text");
    result = test_value_convert_date_to_timestamp();
    generate_output(result, 74, "test_value_convert_date_to_timestamp");
    result = test_value_convert_identical_type();
    generate_output(result, 75, "test_value_convert_identical_type");
    result = test_value_convert_negative_integer_to_unsigned_fails();
    generate_output(result, 76, "test_value_convert_negative_integer_to_unsigned_fails");
    result = test_value_convert_null_value();
    generate_output(result, 77, "test_value_convert_null_value");
    result = test_value_convert_unsupported_conversion();
    generate_output(result, 78, "test_value_convert_unsupported_conversion");

    /* ---------- value_compare unit tests ---------- */
    result = test_value_compare_integers();
    generate_output(result, 79, "test_value_compare_integers");
    result = test_value_compare_integer_double();
    generate_output(result, 80, "test_value_compare_integer_double");
    result = test_value_compare_numeric_different_scales();
    generate_output(result, 81, "test_value_compare_numeric_different_scales");
    result = test_value_compare_char_and_varchar();
    generate_output(result, 82, "test_value_compare_char_and_varchar");
    result = test_value_compare_date_timestamp();
    generate_output(result, 83, "test_value_compare_date_timestamp");
    result = test_value_compare_bool();
    generate_output(result, 84, "test_value_compare_bool");
    result = test_value_compare_blob();
    generate_output(result, 85, "test_value_compare_blob");
    result = test_value_compare_incompatible_types();
    generate_output(result, 86, "test_value_compare_incompatible_types");
    result = test_value_compare_null_type_fails();
    generate_output(result, 87, "test_value_compare_null_type_fails");
    result = test_value_compare_incompatible_types();
    generate_output(result, 88, "test_value_compare_incompatible_types");


    printf("> TESTS RAN SUCCESSFULLY\n");
    return 0;
}

