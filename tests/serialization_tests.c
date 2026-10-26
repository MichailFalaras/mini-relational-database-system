#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../include/serialize.h"
#include "../include/encoding.h"
#include "../include/data_types.h"
#include "../include/schema.h"
#include "../include/constraints.h"
#include "../include/expressions.h"
#include "../include/page.h"
#include "../include/pager.h"
#include "../include/index.h"
#include "../include/table.h"
#include "../include/catalog.h"
#include "../include/btree.h"
#include "../include/row.h"
#include "../src/data_types/data_types_utils.h"

#define ASSERT(condition) { \
    if (!(condition)) { \
        fprintf(stderr, "Assertion failed in %s at line %d: %s\n", __func__, __LINE__, #condition); \
        return 1; \
    } \
}

/* ---------- Serialized Values ---------- */

static int test_integer_value(void) {
    uint8_t buffer[16] = {0};
    Column column = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    Value value = {.type = INTEGER, .null_val = false};
    value.value.int32_val = -12345678;

    ASSERT(serialize_value_data(&value, &column, buffer));
    ASSERT(load_i32_le(buffer) == -12345678);

    Value *decoded = deserialize_value_data(&column, false, buffer);
    ASSERT(decoded != NULL);
    ASSERT(decoded->type == INTEGER);
    ASSERT(decoded->value.int32_val == value.value.int32_val);
    value_free(decoded);
    return 0;
}

static int test_unsigned_value(void) {
    uint8_t buffer[16] = {0};
    Column column = {.type = UNSIGNED_INTEGER, .serialized_size = sizeof(uint32_t)};
    Value value = {.type = UNSIGNED_INTEGER};
    value.value.uint32_val = UINT32_C(0xAABBCCDD);

    ASSERT(serialize_value_data(&value, &column, buffer));
    const uint8_t expected[] = {0xDD, 0xCC, 0xBB, 0xAA};
    ASSERT(memcmp(buffer, expected, sizeof(expected)) == 0);

    Value *decoded = deserialize_value_data(&column, false, buffer);
    ASSERT(decoded != NULL);
    ASSERT(decoded->value.uint32_val == value.value.uint32_val);
    value_free(decoded);
    return 0;
}

static int test_numeric_value(void) {
    uint8_t buffer[16] = {0};
    Column column = {.type = NUMERIC, .serialized_size = sizeof(int64_t) + sizeof(uint32_t)};
    Value value = {.type = NUMERIC};
    value.value.numeric_val.val = INT64_C(-123456789012);
    value.value.numeric_val.scale = 4;

    ASSERT(serialize_value_data(&value, &column, buffer));
    ASSERT(load_i64_le(buffer) == value.value.numeric_val.val);
    ASSERT(load_u32_le(buffer + sizeof(int64_t)) == 4);

    Value *decoded = deserialize_value_data(&column, false, buffer);
    ASSERT(decoded != NULL);
    ASSERT(decoded->value.numeric_val.val == value.value.numeric_val.val);
    ASSERT(decoded->value.numeric_val.scale == value.value.numeric_val.scale);
    value_free(decoded);
    return 0;
}

static int test_floating_values(void) {
    uint8_t buffer[16] = {0};
    Column float_column = {.type = FLOAT, .serialized_size = sizeof(float)};
    Value float_value = {.type = FLOAT};
    float_value.value.float_val = 1.0f;

    ASSERT(serialize_value_data(&float_value, &float_column, buffer));
    const uint8_t expected_float[] = {0x00, 0x00, 0x80, 0x3F};
    ASSERT(memcmp(buffer, expected_float, sizeof(expected_float)) == 0);
    Value *decoded_float = deserialize_value_data(&float_column, false, buffer);
    ASSERT(decoded_float != NULL);
    ASSERT(decoded_float->value.float_val == 1.0f);
    value_free(decoded_float);

    Column double_column = {.type = DOUBLE, .serialized_size = sizeof(double)};
    Value double_value = {.type = DOUBLE};
    double_value.value.double_val = -2.5;
    ASSERT(serialize_value_data(&double_value, &double_column, buffer));
    const uint8_t expected_double[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xC0};
    ASSERT(memcmp(buffer, expected_double, sizeof(expected_double)) == 0);
    Value *decoded_double = deserialize_value_data(&double_column, false, buffer);
    ASSERT(decoded_double != NULL);
    ASSERT(decoded_double->value.double_val == -2.5);
    value_free(decoded_double);
    return 0;
}

static int test_string_values(void) {
    uint8_t buffer[32] = {0};
    Column char_column = {.type = CHAR, .type_parameter = 8, .serialized_size = 8};
    Value char_value = {.type = CHAR};
    char_value.value.char_val.string = "abc";
    ASSERT(serialize_value_data(&char_value, &char_column, buffer));
    ASSERT(memcmp(buffer, "abc\0\0\0\0\0", 8) == 0);
    Value *decoded_char = deserialize_value_data(&char_column, false, buffer);
    ASSERT(decoded_char != NULL);
    ASSERT(strcmp(decoded_char->value.char_val.string, "abc") == 0);
    value_free(decoded_char);

    Column varchar_column = {.type = VARCHAR, .type_parameter = 10, .serialized_size = 10};
    Value varchar_value = {.type = VARCHAR};
    varchar_value.value.varchar_val.string = "hello";
    ASSERT(serialize_value_data(&varchar_value, &varchar_column, buffer));
    Value *decoded_varchar = deserialize_value_data(&varchar_column, false, buffer);
    ASSERT(decoded_varchar != NULL);
    ASSERT(strcmp(decoded_varchar->value.varchar_val.string, "hello") == 0);
    value_free(decoded_varchar);

    Column text_column = {.type = TEXT, .serialized_size = 16};
    Value text_value = {.type = TEXT};
    text_value.value.text_val = "text value";
    ASSERT(serialize_value_data(&text_value, &text_column, buffer));
    Value *decoded_text = deserialize_value_data(&text_column, false, buffer);
    ASSERT(decoded_text != NULL);
    ASSERT(strcmp(decoded_text->value.text_val, "text value") == 0);
    value_free(decoded_text);
    return 0;
}

static int test_date_timestamp_bool_values(void) {
    uint8_t buffer[16] = {0};
    Column date_column = {.type = DATE, .serialized_size = sizeof(uint64_t)};
    Value date_value = {.type = DATE};
    date_value.value.date_val = UINT64_C(1700000000);
    ASSERT(serialize_value_data(&date_value, &date_column, buffer));
    Value *date = deserialize_value_data(&date_column, false, buffer);
    ASSERT(date != NULL && date->value.date_val == date_value.value.date_val);
    value_free(date);

    Column timestamp_column = {.type = TIMESTAMP, .serialized_size = sizeof(uint64_t)};
    Value timestamp_value = {.type = TIMESTAMP};
    timestamp_value.value.timestamp_val = UINT64_C(1850000000);
    ASSERT(serialize_value_data(&timestamp_value, &timestamp_column, buffer));
    Value *timestamp = deserialize_value_data(&timestamp_column, false, buffer);
    ASSERT(timestamp != NULL && timestamp->value.timestamp_val == timestamp_value.value.timestamp_val);
    value_free(timestamp);

    Column bool_column = {.type = BOOL, .serialized_size = sizeof(uint8_t)};
    Value bool_value = {.type = BOOL};
    bool_value.value.bool_val = true;
    ASSERT(serialize_value_data(&bool_value, &bool_column, buffer));
    ASSERT(buffer[0] == 1);
    Value *boolean = deserialize_value_data(&bool_column, false, buffer);
    ASSERT(boolean != NULL && boolean->value.bool_val == true);
    value_free(boolean);
    return 0;
}

static int test_null_value(void) {
    uint8_t buffer[8];
    memset(buffer, 0xAB, sizeof(buffer));
    Column column = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    Value value = {.type = INTEGER, .null_val = true};
    ASSERT(serialize_value_data(&value, &column, buffer));
    ASSERT(buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 0);

    Value *decoded = deserialize_value_data(&column, true, buffer);
    ASSERT(decoded != NULL && decoded->null_val == true);
    value_free(decoded);
    return 0;
}

static int test_null_bitmap(void) {
    uint8_t buffer[8] = {0};
    Value values[10] = {0};
    Value *keys[10] = {0};
    for (uint32_t i = 0; i < 10; i++) {
        keys[i] = &values[i];
    }
    values[0].null_val = true;
    values[3].null_val = true;
    values[9].null_val = true;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_null_bitmap(&write_offset, keys, 10, 10));
    ASSERT(write_offset == buffer + 2);
    ASSERT(buffer[0] == 0x09);
    ASSERT(buffer[1] == 0x02);

    uint8_t *bitmap = NULL;
    uint8_t *read_offset = buffer;
    ASSERT(deserialize_null_bitmap(&read_offset, &bitmap, 10));
    ASSERT(read_offset == buffer + 2);
    ASSERT(bitmap != NULL && bitmap[0] == 0x09 && bitmap[1] == 0x02);
    free(bitmap);
    return 0;
}

/* ---------- Schema / Constraint Metadata ---------- */

static int test_column_metadata(void) {
    uint8_t buffer[128] = {0};
    Column column = {.type = VARCHAR, .type_parameter = 200, .non_null_rows = 7, .null_rows = 3};
    strcpy(column.name, "username");

    uint8_t *write_offset = buffer;
    ASSERT(serialize_column(&write_offset, &column));
    ASSERT((size_t)(write_offset - buffer) == serialized_column_size(&column));
    ASSERT(buffer[64] == (uint8_t)VARCHAR);
    ASSERT(load_u32_le(buffer + 65) == 200);

    uint8_t *read_offset = buffer;
    Column *decoded = deserialize_column(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(strcmp(decoded->name, "username") == 0);
    ASSERT(decoded->type == VARCHAR && decoded->type_parameter == 200);
    ASSERT(decoded->non_null_rows == 7 && decoded->null_rows == 3);
    free(decoded);
    return 0;
}

static int test_not_null_constraint(void) {
    uint8_t buffer[128] = {0};
    Constraint constraint = {.type = NOT_NULL};
    strcpy(constraint.constraint_name, "required_name");
    constraint.constraint_data.not_null.column_ref = 3;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_constraint(&write_offset, &constraint));
    ASSERT((size_t)(write_offset - buffer) == serialized_constraint_size(&constraint));
    ASSERT(buffer[64] == (uint8_t)NOT_NULL);
    ASSERT(load_u32_le(buffer + 65) == 3);

    uint8_t *read_offset = buffer;
    Constraint *decoded = deserialize_constraint(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == NOT_NULL);
    ASSERT(decoded->constraint_data.not_null.column_ref == 3);
    constraint_free(decoded);
    return 0;
}

static int test_primary_key_constraint(void) {
    uint8_t buffer[128] = {0};
    uint32_t refs[] = {1, 3};
    Constraint constraint = {.type = PRIMARY_KEY};
    strcpy(constraint.constraint_name, "pk_test");
    constraint.constraint_data.primary_key.amount_columns = 2;
    constraint.constraint_data.primary_key.primary_key_columns = refs;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_constraint(&write_offset, &constraint));
    ASSERT((size_t)(write_offset - buffer) == serialized_constraint_size(&constraint));
    ASSERT(load_u32_le(buffer + 65) == 2);
    ASSERT(load_u32_le(buffer + 69) == 1);
    ASSERT(load_u32_le(buffer + 73) == 3);

    uint8_t *read_offset = buffer;
    Constraint *decoded = deserialize_constraint(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == PRIMARY_KEY);
    ASSERT(decoded->constraint_data.primary_key.amount_columns == 2);
    ASSERT(decoded->constraint_data.primary_key.primary_key_columns[0] == 1);
    ASSERT(decoded->constraint_data.primary_key.primary_key_columns[1] == 3);
    constraint_free(decoded);
    return 0;
}

static int test_schema_metadata(void) {
    uint8_t buffer[512] = {0};
    Column id = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    Column name = {.type = VARCHAR, .type_parameter = 20, .serialized_size = 20};
    strcpy(id.name, "id");
    strcpy(name.name, "name");
    Column *columns[] = {&id, &name};
    Constraint constraint = {.type = NOT_NULL};
    strcpy(constraint.constraint_name, "id_not_null");
    constraint.constraint_data.not_null.column_ref = 0;
    Constraint *constraints[] = {&constraint};
    Schema schema = {.columns = columns, .num_columns = 2, .constraints = constraints, .num_constraints = 1};

    uint8_t *write_offset = buffer;
    ASSERT(serialize_schema_metadata(&write_offset, &schema));
    ASSERT((size_t)(write_offset - buffer) == serialized_schema_size(&schema));
    ASSERT(load_u32_le(buffer) == 2);

    uint8_t *read_offset = buffer;
    Schema *decoded = deserialize_schema_metadata(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->num_columns == 2 && decoded->num_constraints == 1);
    ASSERT(strcmp(decoded->columns[0]->name, "id") == 0);
    ASSERT(strcmp(decoded->columns[1]->name, "name") == 0);
    ASSERT(decoded->constraints[0]->constraint_data.not_null.column_ref == 0);
    schema_free(decoded);
    return 0;
}

/* ---------- Literal / Expression Metadata ---------- */

static int test_literal_values(void) {
    uint8_t buffer[128] = {0};
    Value literal = {.type = NUMERIC};
    literal.value.numeric_val.val = -987654321;
    literal.value.numeric_val.scale = 3;
    uint8_t *write_offset = buffer;
    ASSERT(serialize_literal_value(&write_offset, &literal));
    ASSERT((size_t)(write_offset - buffer) == serialized_literal_value_size(&literal));
    ASSERT(buffer[0] == (uint8_t)NUMERIC && buffer[1] == 0);
    ASSERT(load_i64_le(buffer + 2) == literal.value.numeric_val.val);
    ASSERT(load_u32_le(buffer + 10) == 3);

    uint8_t *read_offset = buffer;
    Value *decoded = deserialize_literal_value(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == NUMERIC && decoded->value.numeric_val.val == literal.value.numeric_val.val);
    ASSERT(decoded->value.numeric_val.scale == 3);
    value_free(decoded);
    return 0;
}

static int test_null_literal(void) {
    uint8_t buffer[16] = {0};
    Value literal = {.type = INTEGER, .null_val = true};
    uint8_t *write_offset = buffer;
    ASSERT(serialize_literal_value(&write_offset, &literal));
    ASSERT(write_offset == buffer + 2);
    ASSERT(serialized_literal_value_size(&literal) == 2);

    uint8_t *read_offset = buffer;
    Value *decoded = deserialize_literal_value(&read_offset);
    ASSERT(decoded != NULL && decoded->null_val && decoded->type == INTEGER);
    ASSERT(read_offset == write_offset);
    value_free(decoded);
    return 0;
}

static int test_column_expression(void) {
    uint8_t buffer[128] = {0};
    ExpressionNode expression = {.type = EXPR_COLUMN_REF};
    strcpy(expression.expression_data.column_value.column_name, "price");
    expression.expression_data.column_value.column_index = -1;
    expression.expression_data.column_value.relation_index = 7;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &expression));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
    ASSERT(load_i32_le(buffer + 65) == -1);
    ASSERT(load_i32_le(buffer + 69) == 7);

    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL && decoded->type == EXPR_COLUMN_REF);
    ASSERT(read_offset == write_offset);
    ASSERT(strcmp(decoded->expression_data.column_value.column_name, "price") == 0);
    ASSERT(decoded->expression_data.column_value.column_index == -1);
    ASSERT(decoded->expression_data.column_value.relation_index == 7);
    expression_node_free(decoded);
    return 0;
}

static int test_nested_expression(void) {
    uint8_t buffer[256] = {0};
    Value left_value = {.type = INTEGER};
    Value right_value = {.type = INTEGER};
    left_value.value.int32_val = 11;
    right_value.value.int32_val = 22;
    ExpressionNode left = {.type = EXPR_LITERAL};
    ExpressionNode right = {.type = EXPR_LITERAL};
    left.expression_data.literal_value.literal = &left_value;
    right.expression_data.literal_value.literal = &right_value;
    ExpressionNode binary = {.type = EXPR_BINARY};
    binary.expression_data.binary_expr.left_operand = &left;
    binary.expression_data.binary_expr.right_operand = &right;
    binary.expression_data.binary_expr.op = OP_LT;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &binary));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&binary));
    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL && decoded->type == EXPR_BINARY);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->expression_data.binary_expr.op == OP_LT);
    ASSERT(decoded->expression_data.binary_expr.left_operand->expression_data.literal_value.literal->value.int32_val == 11);
    ASSERT(decoded->expression_data.binary_expr.right_operand->expression_data.literal_value.literal->value.int32_val == 22);
    expression_node_free(decoded);
    return 0;
}

/* ---------- Reserved Page Metadata ---------- */

static int test_page_zero_metadata(void) {
    uint8_t buffer[64] = {0};
    PageZeroMetadata page_zero = {0};
    memcpy(page_zero.magic, "rdbms-c-v", 9);
    page_zero.version = UINT32_C(0xAABBCCDD);
    page_zero.page_size = 4096;
    page_zero.catalog_root = 11;
    page_zero.free_list_head = 15;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_page_zero_metadata(&write_offset, &page_zero));
    ASSERT(load_u32_le(buffer + DB_MAGIC_STRING_LEN) == UINT32_C(0xAABBCCDD));
    ASSERT(load_u16_le(buffer + DB_MAGIC_STRING_LEN + 4) == 4096);

    uint8_t *read_offset = buffer;
    PageZeroMetadata decoded = {0};
    ASSERT(deserialize_page_zero_metadata(&read_offset, &decoded));
    ASSERT(read_offset == write_offset);
    ASSERT(memcmp(page_zero.magic, decoded.magic, DB_MAGIC_STRING_LEN) == 0);
    ASSERT(decoded.version == page_zero.version);
    ASSERT(decoded.page_size == page_zero.page_size);
    ASSERT(decoded.catalog_root == page_zero.catalog_root);
    ASSERT(decoded.free_list_head == page_zero.free_list_head);
    return 0;
}


/* ---------- Additional Constraint Metadata ---------- */

static int test_unique_constraint(void) {
    uint8_t buffer[128] = {0};
    uint32_t refs[] = {0, 2};
    Constraint constraint = {.type = UNIQUE};
    strcpy(constraint.constraint_name, "unique_pair");
    constraint.constraint_data.unique_cols.amount_columns = 2;
    constraint.constraint_data.unique_cols.column_refs = refs;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_constraint(&write_offset, &constraint));
    ASSERT((size_t)(write_offset - buffer) == serialized_constraint_size(&constraint));
    uint8_t *read_offset = buffer;
    Constraint *decoded = deserialize_constraint(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == UNIQUE);
    ASSERT(decoded->constraint_data.unique_cols.amount_columns == 2);
    ASSERT(decoded->constraint_data.unique_cols.column_refs[0] == 0);
    ASSERT(decoded->constraint_data.unique_cols.column_refs[1] == 2);
    constraint_free(decoded);
    return 0;
}

static int test_foreign_key_constraint(void) {
    uint8_t buffer[256] = {0};
    uint32_t local[] = {1, 2};
    uint32_t referenced[] = {0, 3};
    Constraint constraint = {.type = FOREIGN_KEY};
    strcpy(constraint.constraint_name, "fk_test");
    constraint.constraint_data.foreign_key.amount_columns = 2;
    constraint.constraint_data.foreign_key.foreign_key_columns = local;
    constraint.constraint_data.foreign_key.amount_referenced_columns = 2;
    constraint.constraint_data.foreign_key.referenced_columns = referenced;
    strcpy(constraint.constraint_data.foreign_key.referenced_table_name, "parent_table");

    uint8_t *write_offset = buffer;
    ASSERT(serialize_constraint(&write_offset, &constraint));
    ASSERT((size_t)(write_offset - buffer) == serialized_constraint_size(&constraint));
    uint8_t *read_offset = buffer;
    Constraint *decoded = deserialize_constraint(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == FOREIGN_KEY);
    ASSERT(decoded->constraint_data.foreign_key.amount_columns == 2);
    ASSERT(decoded->constraint_data.foreign_key.foreign_key_columns[1] == 2);
    ASSERT(decoded->constraint_data.foreign_key.amount_referenced_columns == 2);
    ASSERT(decoded->constraint_data.foreign_key.referenced_columns[1] == 3);
    ASSERT(strcmp(decoded->constraint_data.foreign_key.referenced_table_name, "parent_table") == 0);
    constraint_free(decoded);
    return 0;
}

static int test_check_constraint(void) {
    uint8_t buffer[256] = {0};
    uint32_t refs[] = {0};
    Value literal = {.type = INTEGER};
    literal.value.int32_val = 21;
    ExpressionNode expression = {.type = EXPR_LITERAL};
    expression.expression_data.literal_value.literal = &literal;
    Constraint constraint = {.type = CHECK};
    strcpy(constraint.constraint_name, "check_age");
    constraint.constraint_data.check.amount_columns = 1;
    constraint.constraint_data.check.column_refs = refs;
    constraint.constraint_data.check.constraint_expr = &expression;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_constraint(&write_offset, &constraint));
    ASSERT((size_t)(write_offset - buffer) == serialized_constraint_size(&constraint));
    uint8_t *read_offset = buffer;
    Constraint *decoded = deserialize_constraint(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == CHECK);
    ASSERT(decoded->constraint_data.check.column_refs[0] == 0);
    ASSERT(decoded->constraint_data.check.constraint_expr->type == EXPR_LITERAL);
    ASSERT(decoded->constraint_data.check.constraint_expr->expression_data.literal_value.literal->value.int32_val == 21);
    constraint_free(decoded);
    return 0;
}

static int test_default_constraint(void) {
    uint8_t buffer[256] = {0};
    Value literal = {.type = INTEGER};
    literal.value.int32_val = 100;
    ExpressionNode expression = {.type = EXPR_LITERAL};
    expression.expression_data.literal_value.literal = &literal;
    Constraint constraint = {.type = DEFAULT};
    strcpy(constraint.constraint_name, "default_score");
    constraint.constraint_data.default_value.column_ref = 2;
    constraint.constraint_data.default_value.default_expr = &expression;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_constraint(&write_offset, &constraint));
    ASSERT((size_t)(write_offset - buffer) == serialized_constraint_size(&constraint));
    uint8_t *read_offset = buffer;
    Constraint *decoded = deserialize_constraint(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == DEFAULT);
    ASSERT(decoded->constraint_data.default_value.column_ref == 2);
    ASSERT(decoded->constraint_data.default_value.default_expr->expression_data.literal_value.literal->value.int32_val == 100);
    constraint_free(decoded);
    return 0;
}

/* ---------- Remaining Literal Types ---------- */

static int test_supported_literals(void) {
    uint8_t buffer[128] = {0};
    Value values[5] = {0};
    values[0].type = INTEGER;
    values[0].value.int32_val = -42;
    values[1].type = CHAR;
    values[1].value.char_val.n = 5;
    values[1].value.char_val.string = "hello";
    values[2].type = DATE;
    values[2].value.date_val = 123456789;
    values[3].type = TIMESTAMP;
    values[3].value.timestamp_val = 987654321;
    values[4].type = BOOL;
    values[4].value.bool_val = true;

    for (uint32_t i = 0; i < 5; i++) {
        memset(buffer, 0, sizeof(buffer));
        uint8_t *write_offset = buffer;
        ASSERT(serialize_literal_value(&write_offset, &values[i]));
        ASSERT((size_t)(write_offset - buffer) == serialized_literal_value_size(&values[i]));

        uint8_t *read_offset = buffer;
        Value *decoded = deserialize_literal_value(&read_offset);
        ASSERT(decoded != NULL);
        ASSERT(read_offset == write_offset);
        ASSERT(decoded->type == values[i].type);

        switch (values[i].type) {
            case INTEGER: ASSERT(decoded->value.int32_val == -42); break;
            case CHAR:
                ASSERT(decoded->value.char_val.n == 5);
                ASSERT(strcmp(decoded->value.char_val.string, "hello") == 0);
                break;
            case DATE: ASSERT(decoded->value.date_val == 123456789); break;
            case TIMESTAMP: ASSERT(decoded->value.timestamp_val == 987654321); break;
            case BOOL: ASSERT(decoded->value.bool_val == true); break;
            default: ASSERT(false);
        }
        value_free(decoded);
    }
    return 0;
}

/* ---------- Pager-backed Index / Table Metadata ---------- */

static int test_index_metadata(void) {
    const char *pathname = "build/serialize_index_test.db";
    remove(pathname);
    Pager *pager = pager_open(pathname);
    ASSERT(pager != NULL);
    uint32_t root_page_num = 0;
    uint32_t metadata_page_num = 0;
    ASSERT(pager_allocate_page(pager, &root_page_num));
    ASSERT(pager_allocate_page(pager, &metadata_page_num));
    Page *page = pager_get_page(pager, metadata_page_num);
    ASSERT(page != NULL);

    uint32_t refs[] = {0, 2};
    IndexKey key = {.num_columns = 2, .column_index_array = refs};
    Index index = {.type = PRIMARY_INDEX, .key = &key, .is_unique = true};
    strcpy(index.name, "idx_users");
    uint8_t *write_offset = page->page_data + sizeof(uint32_t);
    ASSERT(serialize_index_metadata(&write_offset, &index));
    ASSERT((size_t)(write_offset - page->page_data - sizeof(uint32_t)) == serialized_index_metadata_size(&index));

    Value name_value = {.type = CHAR};
    name_value.value.char_val.string = "idx_users";
    Value *keys[] = {&name_value, &name_value, &name_value};
    CatalogPayload payload = {.type = CATALOG_INDEX, .root_page_num = root_page_num, .metadata_page_num = metadata_page_num};
    BTreeCellContents catalog_cell = {0};
    catalog_cell.keys = keys;
    catalog_cell.BTreePayload.catalog = &payload;
    Index *decoded = NULL;
    ASSERT(deserialize_index_metadata(pager, &catalog_cell, &decoded));
    ASSERT(decoded != NULL);
    ASSERT(strcmp(decoded->name, "idx_users") == 0);
    ASSERT(decoded->type == PRIMARY_INDEX && decoded->is_unique);
    ASSERT(decoded->root_page_num == root_page_num);
    ASSERT(decoded->key->num_columns == 2);
    ASSERT(decoded->key->column_index_array[0] == 0);
    ASSERT(decoded->key->column_index_array[1] == 2);
    index_free(decoded);
    ASSERT(pager_close(pager));
    remove(pathname);
    return 0;
}

static int test_table_metadata(void) {
    const char *pathname = "build/serialize_table_test.db";
    remove(pathname);
    Pager *pager = pager_open(pathname);
    ASSERT(pager != NULL);
    uint32_t root_page_num = 0;
    uint32_t metadata_page_num = 0;
    ASSERT(pager_allocate_page(pager, &root_page_num));
    ASSERT(pager_allocate_page(pager, &metadata_page_num));
    Page *page = pager_get_page(pager, metadata_page_num);
    ASSERT(page != NULL);

    Column col = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    strcpy(col.name, "id");
    Column *columns[] = {&col};
    Schema schema = {.num_columns = 1, .columns = columns};
    Table table = {.table_schema = &schema, .row_count = 8, .is_materialized = true};
    strcpy(table.name, "users");
    uint8_t *write_offset = page->page_data + sizeof(uint32_t);
    ASSERT(serialize_table_metadata(&write_offset, &table));
    ASSERT((size_t)(write_offset - page->page_data - sizeof(uint32_t)) == serialized_table_metadata_size(&table));

    Value name_value = {.type = CHAR};
    name_value.value.char_val.string = "users";
    Value *keys[] = {&name_value};
    CatalogPayload payload = {.type = CATALOG_TABLE, .root_page_num = root_page_num, .metadata_page_num = metadata_page_num};
    BTreeCellContents catalog_cell = {0};
    catalog_cell.keys = keys;
    catalog_cell.BTreePayload.catalog = &payload;
    Table *decoded = NULL;
    ASSERT(deserialize_table_metadata(pager, &catalog_cell, &decoded));
    ASSERT(decoded != NULL);
    ASSERT(strcmp(decoded->name, "users") == 0);
    ASSERT(decoded->is_materialized && decoded->row_count == 8);
    ASSERT(decoded->table_schema->num_columns == 1);
    ASSERT(strcmp(decoded->table_schema->columns[0]->name, "id") == 0);
    ASSERT(decoded->table_schema->columns[0]->type == INTEGER);
    table_free(decoded);
    ASSERT(pager_close(pager));
    remove(pathname);
    return 0;
}

/* ---------- B+ Tree Cell Contents ---------- */

static int test_internal_cell(void) {
    uint8_t buffer[64] = {0};
    Column column = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    Column *columns[] = {&column};
    Schema schema = {.num_columns = 1, .columns = columns};
    uint32_t refs[] = {0};
    IndexKey key = {.num_columns = 1, .column_index_array = refs};
    BTreeIndexSpec spec = {.index_key = &key, .schema = &schema, .key_size = 5, .payload_type = BTREE_ROW_PAYLOAD};
    Value value = {.type = INTEGER};
    value.value.int32_val = -1234;
    Value *keys[] = {&value};
    BTreeCellContents cell = {.type = BTREE_INTERNAL_NODE, .num_keys = 1, .keys = keys};
    cell.BTreePayload.child_pointer = 42;
    BTreePage page = {.type = BTREE_INTERNAL_NODE};
    Page physical = {0};
    page.page = &physical;
    page.data = buffer;

    ASSERT(serialize_cell_contents(buffer, &page, &cell, &spec));
    ASSERT(load_u32_le(buffer) == 42);

    BTreeCellView view = {.payload_size = sizeof(uint32_t)};
    view.key.key_size = 5;
    BTreeCellContents decoded = {0};

    ASSERT(deserialize_cell_contents(buffer, &page, &view, &decoded, &spec));
    ASSERT(decoded.BTreePayload.child_pointer == 42);
    ASSERT(decoded.num_keys == 1);
    ASSERT(decoded.keys[0]->value.int32_val == -1234);

    value_free_array(decoded.keys, decoded.num_keys);
    return 0;
}

static int test_row_leaf_cell(void) {
    uint8_t buffer[128] = {0};
    Column column = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    Column *columns[] = {&column};
    Schema schema = {.num_columns = 1, .columns = columns};
    uint32_t refs[] = {0};
    IndexKey key = {.num_columns = 1, .column_index_array = refs};
    BTreeIndexSpec spec = {.index_key = &key, .schema = &schema, .key_size = 5, .payload_type = BTREE_ROW_PAYLOAD};
    Value value = {.type = INTEGER};
    value.value.int32_val = 123;
    Value *values[] = {&value};
    Row row = {.n_columns = 1, .values = values, .is_deleted = false};
    BTreeCellContents cell = {.type = BTREE_LEAF_NODE, .num_keys = 1, .keys = values};
    cell.BTreePayload.row = &row;
    BTreePage page = {.type = BTREE_LEAF_NODE};
    Page physical = {0};
    page.page = &physical;
    page.data = buffer;
    ASSERT(serialize_cell_contents(buffer, &page, &cell, &spec));

    BTreeCellView view = {.payload_size = 10};
    view.key.key_size = 5;
    BTreeCellContents decoded = {0};
    ASSERT(deserialize_cell_contents(buffer, &page, &view, &decoded, &spec));
    ASSERT(decoded.keys[0]->value.int32_val == 123);
    ASSERT(decoded.BTreePayload.row->n_columns == 1);
    ASSERT(decoded.BTreePayload.row->values[0]->value.int32_val == 123);

    value_free_array(decoded.keys, decoded.num_keys);
    row_free(decoded.BTreePayload.row);
    return 0;
}

static int test_catalog_leaf_cell(void) {
    uint8_t buffer[128] = {0};
    Column column = {.type = INTEGER, .serialized_size = sizeof(int32_t)};
    Column *columns[] = {&column};
    Schema schema = {.num_columns = 1, .columns = columns};
    uint32_t refs[] = {0};
    IndexKey key = {.num_columns = 1, .column_index_array = refs};
    BTreeIndexSpec spec = {.index_key = &key, .schema = &schema, .key_size = 5, .payload_type = BTREE_CATALOG_PAYLOAD};
    Value value = {.type = INTEGER};
    value.value.int32_val = 7;
    Value *keys[] = {&value};
    CatalogPayload payload = {.type = CATALOG_TABLE, .root_page_num = 21, .metadata_page_num = 22};
    BTreeCellContents cell = {.type = BTREE_LEAF_NODE, .num_keys = 1, .keys = keys};
    cell.BTreePayload.catalog = &payload;
    BTreePage page = {.type = BTREE_LEAF_NODE};
    Page physical = {0};
    page.page = &physical;
    page.data = buffer;

    ASSERT(serialize_cell_contents(buffer, &page, &cell, &spec));

    BTreeCellView view = {.payload_size = 9};
    view.key.key_size = 5;
    BTreeCellContents decoded = {0};

    ASSERT(deserialize_cell_contents(buffer, &page, &view, &decoded, &spec));
    ASSERT(decoded.keys[0]->value.int32_val == 7);
    ASSERT(decoded.BTreePayload.catalog->type == CATALOG_TABLE);
    ASSERT(decoded.BTreePayload.catalog->root_page_num == 21);
    ASSERT(decoded.BTreePayload.catalog->metadata_page_num == 22);

    value_free_array(decoded.keys, decoded.num_keys);
    free(decoded.BTreePayload.catalog);
    return 0;
}

/* ---------- Additional Expression Metadata ---------- */

static int test_in_expression(void) {
    uint8_t buffer[256] = {0};
    Value operand_value = {.type = INTEGER};
    operand_value.value.int32_val = 5;
    Value option_value1 = {.type = INTEGER};
    option_value1.value.int32_val = 3;
    Value option_value2 = {.type = INTEGER};
    option_value2.value.int32_val = 5;
    ExpressionNode operand = {.type = EXPR_LITERAL};
    operand.expression_data.literal_value.literal = &operand_value;
    ExpressionNode option1 = {.type = EXPR_LITERAL};
    option1.expression_data.literal_value.literal = &option_value1;
    ExpressionNode option2 = {.type = EXPR_LITERAL};
    option2.expression_data.literal_value.literal = &option_value2;
    ExpressionNode *options[] = {&option1, &option2};
    ExpressionNode expression = {.type = EXPR_IN};
    expression.expression_data.in_expr.operand = &operand;
    expression.expression_data.in_expr.option_count = 2;
    expression.expression_data.in_expr.set_options = options;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &expression));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == EXPR_IN);
    ASSERT(decoded->expression_data.in_expr.option_count == 2);
    ASSERT(decoded->expression_data.in_expr.set_options[0]->expression_data.literal_value.literal->value.int32_val == 3);
    ASSERT(decoded->expression_data.in_expr.set_options[1]->expression_data.literal_value.literal->value.int32_val == 5);
    expression_node_free(decoded);
    return 0;
}

static int test_between_expression(void) {
    uint8_t buffer[256] = {0};
    Value val = {.type = INTEGER};
    val.value.int32_val = 9;
    Value lower = {.type = INTEGER};
    lower.value.int32_val = 5;
    Value upper = {.type = INTEGER};
    upper.value.int32_val = 10;
    ExpressionNode operand_expr = {.type = EXPR_LITERAL};
    operand_expr.expression_data.literal_value.literal = &val;
    ExpressionNode lower_expr = {.type = EXPR_LITERAL};
    lower_expr.expression_data.literal_value.literal = &lower;
    ExpressionNode upper_expr = {.type = EXPR_LITERAL};
    upper_expr.expression_data.literal_value.literal = &upper;
    ExpressionNode expression = {.type = EXPR_BETWEEN};
    expression.expression_data.between_expr.operand = &operand_expr;
    expression.expression_data.between_expr.lower = &lower_expr;
    expression.expression_data.between_expr.upper = &upper_expr;

    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &expression));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == EXPR_BETWEEN);
    ASSERT(decoded->expression_data.between_expr.operand->expression_data.literal_value.literal->value.int32_val == 9);
    ASSERT(decoded->expression_data.between_expr.lower->expression_data.literal_value.literal->value.int32_val == 5);
    ASSERT(decoded->expression_data.between_expr.upper->expression_data.literal_value.literal->value.int32_val == 10);
    expression_node_free(decoded);
    return 0;
}

static int test_is_null_expressions(void) {
    uint8_t buffer[128] = {0};
    Value literal = {.type = INTEGER, .null_val = true};
    ExpressionNode operand = {.type = EXPR_LITERAL};
    operand.expression_data.literal_value.literal = &literal;
    ExpressionType types[] = {EXPR_IS_NULL, EXPR_IS_NOT_NULL};
    for (uint32_t i = 0; i < 2; i++) {
        ExpressionNode expression = {.type = types[i]};
        if (types[i] == EXPR_IS_NULL) {
            expression.expression_data.is_null_expr.operand = &operand;
        } else {
            expression.expression_data.is_not_null_expr.operand = &operand;
        }
        uint8_t *write_offset = buffer;
        ASSERT(serialize_expression_node(&write_offset, &expression));
        ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
        uint8_t *read_offset = buffer;
        ExpressionNode *decoded = deserialize_expression_node(&read_offset);
        ASSERT(decoded != NULL);
        ASSERT(read_offset == write_offset);
        ASSERT(decoded->type == types[i]);
        ExpressionNode *decoded_operand = types[i] == EXPR_IS_NULL
            ? decoded->expression_data.is_null_expr.operand
            : decoded->expression_data.is_not_null_expr.operand;
        ASSERT(decoded_operand->type == EXPR_LITERAL);
        ASSERT(decoded_operand->expression_data.literal_value.literal->null_val);
        expression_node_free(decoded);
    }
    return 0;
}

static int test_table_reference_expression(void) {
    uint8_t buffer[128] = {0};
    ExpressionNode expression = {.type = EXPR_TABLE_REF};
    strcpy(expression.expression_data.table_value.table_name, "orders");
    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &expression));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == EXPR_TABLE_REF);
    ASSERT(strcmp(decoded->expression_data.table_value.table_name, "orders") == 0);
    expression_node_free(decoded);
    return 0;
}

static int test_unary_expression(void) {
    uint8_t buffer[128] = {0};
    Value literal = {.type = BOOL};
    literal.value.bool_val = true;
    ExpressionNode operand = {.type = EXPR_LITERAL};
    operand.expression_data.literal_value.literal = &literal;
    ExpressionNode expression = {.type = EXPR_UNARY};
    expression.expression_data.unary_expr.op = OP_NOT;
    expression.expression_data.unary_expr.operand = &operand;
    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &expression));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == EXPR_UNARY);
    ASSERT(decoded->expression_data.unary_expr.op == OP_NOT);
    ASSERT(decoded->expression_data.unary_expr.operand->expression_data.literal_value.literal->value.bool_val);
    expression_node_free(decoded);
    return 0;
}

static int test_aggregate_expression(void) {
    uint8_t buffer[128] = {0};
    Value literal = {.type = INTEGER};
    literal.value.int32_val = 7;
    ExpressionNode operand = {.type = EXPR_LITERAL};
    operand.expression_data.literal_value.literal = &literal;
    ExpressionNode expression = {.type = EXPR_FUNCTIONS};
    expression.expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_SUM;
    expression.expression_data.aggregate_func_expr.expression = &operand;
    expression.expression_data.aggregate_func_expr.wildcard = false;
    uint8_t *write_offset = buffer;
    ASSERT(serialize_expression_node(&write_offset, &expression));
    ASSERT((size_t)(write_offset - buffer) == serialized_expression_node_size(&expression));
    uint8_t *read_offset = buffer;
    ExpressionNode *decoded = deserialize_expression_node(&read_offset);
    ASSERT(decoded != NULL);
    ASSERT(read_offset == write_offset);
    ASSERT(decoded->type == EXPR_FUNCTIONS);
    ASSERT(decoded->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_SUM);
    ASSERT(!decoded->expression_data.aggregate_func_expr.wildcard);
    ASSERT(decoded->expression_data.aggregate_func_expr.expression->expression_data.literal_value.literal->value.int32_val == 7);
    expression_node_free(decoded);
    return 0;
}

/* ---------- Serialized Size Calculations ---------- */

static int test_fixed_metadata_sizes(void) {
    uint32_t refs[] = {0, 2, 4};
    IndexKey key = {.num_columns = 3, .column_index_array = refs};
    Index index = {.key = &key};

    ASSERT(serialized_index_metadata_size(&index) == sizeof(uint8_t) + 4*sizeof(uint32_t) + sizeof(uint8_t));

    Column column = {.type = INTEGER};
    ASSERT(serialized_column_size(&column) == 64 + sizeof(uint8_t) + 3*sizeof(uint32_t));

    Column *columns[] = {&column};
    Schema schema = {.num_columns = 1, .columns = columns};
    Table table = {.table_schema = &schema};

    ASSERT(serialized_schema_size(&schema) == 2*sizeof(uint32_t) + serialized_column_size(&column));
    ASSERT(serialized_table_metadata_size(&table) == sizeof(uint8_t) + sizeof(uint32_t) + serialized_schema_size(&schema));
    return 0;
}

static int test_constraint_sizes(void) {
    const size_t header_size = 64 + sizeof(uint8_t);
    uint32_t refs[] = {0, 1, 2};
    Constraint constraint = {0};

    constraint.type = PRIMARY_KEY;
    constraint.constraint_data.primary_key.amount_columns = 3;
    constraint.constraint_data.primary_key.primary_key_columns = refs;
    ASSERT(serialized_constraint_size(&constraint) == header_size + 4*sizeof(uint32_t));

    constraint = (Constraint){0};
    constraint.type = FOREIGN_KEY;
    constraint.constraint_data.foreign_key.amount_columns = 2;
    constraint.constraint_data.foreign_key.foreign_key_columns = refs;
    constraint.constraint_data.foreign_key.amount_referenced_columns = 3;
    constraint.constraint_data.foreign_key.referenced_columns = refs;
    ASSERT(serialized_constraint_size(&constraint) == header_size + 2*sizeof(uint32_t) +
        5*sizeof(uint32_t) + 64);

    constraint = (Constraint){0};
    constraint.type = UNIQUE;
    constraint.constraint_data.unique_cols.amount_columns = 2;
    constraint.constraint_data.unique_cols.column_refs = refs;
    ASSERT(serialized_constraint_size(&constraint) == header_size + 3*sizeof(uint32_t));

    constraint = (Constraint){0};
    constraint.type = NOT_NULL;
    ASSERT(serialized_constraint_size(&constraint) == header_size + sizeof(uint32_t));

    Value literal = {.type = INTEGER};
    literal.value.int32_val = 10;
    ExpressionNode expr = {.type = EXPR_LITERAL};
    expr.expression_data.literal_value.literal = &literal;
    const size_t expr_size = sizeof(uint8_t) + 2*sizeof(uint8_t) + sizeof(int32_t);

    constraint = (Constraint){0};
    constraint.type = CHECK;
    constraint.constraint_data.check.amount_columns = 1;
    constraint.constraint_data.check.column_refs = refs;
    constraint.constraint_data.check.constraint_expr = &expr;
    ASSERT(serialized_constraint_size(&constraint) == header_size + 2*sizeof(uint32_t) + expr_size);

    constraint = (Constraint){0};
    constraint.type = DEFAULT;
    constraint.constraint_data.default_value.default_expr = &expr;
    ASSERT(serialized_constraint_size(&constraint) == header_size + sizeof(uint32_t) + expr_size);
    return 0;
}

static int test_literal_sizes(void) {
    Value literal = {.type = INTEGER};
    ASSERT(serialized_literal_value_size(&literal) == 2 + sizeof(int32_t));

    literal.type = NUMERIC;
    ASSERT(serialized_literal_value_size(&literal) == 2 + sizeof(int64_t) + sizeof(uint32_t));

    literal.type = CHAR;
    literal.value.char_val.string = "hello";
    literal.value.char_val.n = 5;
    ASSERT(serialized_literal_value_size(&literal) == 2 + sizeof(uint32_t) + 5);

    literal.type = DATE;
    ASSERT(serialized_literal_value_size(&literal) == 2 + sizeof(uint64_t));
    literal.type = TIMESTAMP;
    ASSERT(serialized_literal_value_size(&literal) == 2 + sizeof(uint64_t));
    literal.type = BOOL;
    ASSERT(serialized_literal_value_size(&literal) == 3);

    literal.type = INTEGER;
    literal.null_val = true;
    ASSERT(serialized_literal_value_size(&literal) == 2);
    return 0;
}

static int test_expression_sizes(void) {
    Value literal = {.type = INTEGER};
    literal.value.int32_val = 5;
    ExpressionNode value_expr = {.type = EXPR_LITERAL};
    value_expr.expression_data.literal_value.literal = &literal;
    const size_t literal_expr_size = 1 + 2 + sizeof(int32_t);
    ASSERT(serialized_expression_node_size(&value_expr) == literal_expr_size);

    ExpressionNode column_expr = {.type = EXPR_COLUMN_REF};
    ASSERT(serialized_expression_node_size(&column_expr) == 1 + 64 + 2*sizeof(int32_t));

    ExpressionNode binary = {.type = EXPR_BINARY};
    binary.expression_data.binary_expr.left_operand = &column_expr;
    binary.expression_data.binary_expr.right_operand = &value_expr;
    ASSERT(serialized_expression_node_size(&binary) == 1 +
        serialized_expression_node_size(&column_expr) + 1 + literal_expr_size);

    ExpressionNode *options[] = {&value_expr, &value_expr, &value_expr};
    ExpressionNode in_expr = {.type = EXPR_IN};
    in_expr.expression_data.in_expr.operand = &column_expr;
    in_expr.expression_data.in_expr.option_count = 3;
    in_expr.expression_data.in_expr.set_options = options;
    ASSERT(serialized_expression_node_size(&in_expr) == 1 +
        serialized_expression_node_size(&column_expr) + sizeof(uint32_t) + 3*literal_expr_size);
    return 0;
}

static int test_invalid_serialized_sizes(void) {
    ASSERT(serialized_index_metadata_size(NULL) == 0);
    ASSERT(serialized_table_metadata_size(NULL) == 0);
    ASSERT(serialized_schema_size(NULL) == 0);
    ASSERT(serialized_column_size(NULL) == 0);
    ASSERT(serialized_constraint_size(NULL) == 0);
    ASSERT(serialized_literal_value_size(NULL) == 0);
    ASSERT(serialized_expression_node_size(NULL) == 0);

    IndexKey key = {.num_columns = 1, .column_index_array = NULL};
    Index index = {.key = &key};
    ASSERT(serialized_index_metadata_size(&index) == 0);

    Schema schema = {.num_columns = 1, .columns = NULL};
    ASSERT(serialized_schema_size(&schema) == 0);

    Constraint constraint = {.type = UNIQUE};
    constraint.constraint_data.unique_cols.amount_columns = 2;
    ASSERT(serialized_constraint_size(&constraint) == 0);

    Value literal = {.type = CHAR};
    ASSERT(serialized_literal_value_size(&literal) == 0);

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

    result = test_integer_value();
    generate_output(result, 0, "test_integer_value");
    result = test_unsigned_value();
    generate_output(result, 1, "test_unsigned_value");
    result = test_numeric_value();
    generate_output(result, 2, "test_numeric_value");
    result = test_floating_values();
    generate_output(result, 3, "test_floating_values");
    result = test_string_values();
    generate_output(result, 4, "test_string_values");
    result = test_date_timestamp_bool_values();
    generate_output(result, 5, "test_date_timestamp_bool_values");
    result = test_null_value();
    generate_output(result, 6, "test_null_value");
    result = test_null_bitmap();
    generate_output(result, 7, "test_null_bitmap");

    result = test_column_metadata();
    generate_output(result, 8, "test_column_metadata");
    result = test_not_null_constraint();
    generate_output(result, 9, "test_not_null_constraint");
    result = test_primary_key_constraint();
    generate_output(result, 10, "test_primary_key_constraint");
    result = test_schema_metadata();
    generate_output(result, 11, "test_schema_metadata");

    result = test_literal_values();
    generate_output(result, 12, "test_literal_values");
    result = test_null_literal();
    generate_output(result, 13, "test_null_literal");

    result = test_column_expression();
    generate_output(result, 14, "test_column_expression");
    result = test_nested_expression();
    generate_output(result, 15, "test_nested_expression");

    result = test_page_zero_metadata();
    generate_output(result, 16, "test_page_zero_metadata");

    result = test_unique_constraint();
    generate_output(result, 17, "test_unique_constraint");
    result = test_foreign_key_constraint();
    generate_output(result, 18, "test_foreign_key_constraint");
    result = test_check_constraint();
    generate_output(result, 19, "test_check_constraint");
    result = test_default_constraint();
    generate_output(result, 20, "test_default_constraint");

    result = test_supported_literals();
    generate_output(result, 21, "test_supported_literals");

    result = test_index_metadata();
    generate_output(result, 22, "test_index_metadata");
    result = test_table_metadata();
    generate_output(result, 23, "test_table_metadata");

    result = test_internal_cell();
    generate_output(result, 24, "test_internal_cell");
    result = test_row_leaf_cell();
    generate_output(result, 25, "test_row_leaf_cell");
    result = test_catalog_leaf_cell();
    generate_output(result, 26, "test_catalog_leaf_cell");

    result = test_in_expression();
    generate_output(result, 27, "test_in_expression");
    result = test_between_expression();
    generate_output(result, 28, "test_between_expression");
    result = test_is_null_expressions();
    generate_output(result, 29, "test_is_null_expressions");
    result = test_table_reference_expression();
    generate_output(result, 30, "test_table_reference_expression");
    result = test_unary_expression();
    generate_output(result, 31, "test_unary_expression");
    result = test_aggregate_expression();
    generate_output(result, 32, "test_aggregate_expression");

    result = test_fixed_metadata_sizes();
    generate_output(result, 33, "test_fixed_metadata_sizes");
    result = test_constraint_sizes();
    generate_output(result, 34, "test_constraint_sizes");
    result = test_literal_sizes();
    generate_output(result, 35, "test_literal_sizes");
    result = test_expression_sizes();
    generate_output(result, 36, "test_expression_sizes");
    result = test_invalid_serialized_sizes();
    generate_output(result, 37, "test_invalid_serialized_sizes");

    return 0;
}