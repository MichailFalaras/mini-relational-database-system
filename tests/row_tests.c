#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/data_types.h"
#include "../include/row.h"
#include "../include/expressions.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return 1;  \
    }


/* ---------- ExpressionNode helpers ---------- */

static ExpressionNode *create_literal_expression(Value *value) {
    ExpressionNode *expr_node = (ExpressionNode *) calloc(1, sizeof(ExpressionNode));
    if (!expr_node)
        return NULL;

    expr_node->expression_data.literal_value.literal = value;
    return expr_node;
}

static void free_literal_expression(ExpressionNode *expr) {
    free(expr);
}

/* ---------- row_create unit tests ---------- */

// Success case
static int test_row_create() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    Row *row = row_create(expressions, 2);

    ASSERT(row != NULL);
    ASSERT(row->is_deleted == false);
    ASSERT(row->n_columns == 2);
    ASSERT(row->values != NULL);

    ASSERT(row->values[0] != int_value);
    ASSERT(row->values[0]->type == INTEGER);
    ASSERT(row->values[0]->value.int32_val == -42);

    ASSERT(row->values[1] != text_value);
    ASSERT(row->values[1]->type == TEXT);
    ASSERT(strcmp(row->values[1]->value.text_val, "hello") == 0);
    ASSERT(row->values[1]->value.text_val != text_value->value.text_val);

    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

// Failure case
static int test_row_create_null_input() {
    Row *row = row_create(NULL, 4);
    
    ASSERT(row == NULL);

    return 0;
}

/* ---------- row_mark_deleted unit tests ---------- */

static int test_row_mark_deleted() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);
    ASSERT(row->is_deleted == false);

    ASSERT(row_mark_deleted(row));
    ASSERT(row->is_deleted == true);

    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

static int test_row_mark_deleted_null_input() {
    ASSERT(!row_mark_deleted(NULL));
    
    return 0;
}


/* ---------- row_get_value unit tests ---------- */

static int test_row_get_value() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);

    // Columns are 1-based indexed
    Value *expr_1 = row_get_value(row, 1);
    ASSERT(expr_1 != NULL);
    ASSERT(expr_1->type == INTEGER);
    ASSERT(expr_1->value.int32_val == -42);

    Value *expr_2 = row_get_value(row, 2);
    ASSERT(expr_2 != NULL);
    ASSERT(expr_2->type == TEXT);
    ASSERT(strcmp(expr_2->value.text_val, "hello") == 0);

    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

static int test_row_get_value_invalid_positions() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);

    // Out of bounds 1-based indexes at the start of the ExpressionNode ** array
    Value *expr_1 = row_get_value(row, 0);
    ASSERT(expr_1 == NULL);

    // Out of bounds 1-based indexes at the end of the ExpressionNode ** array
    Value *expr_2 = row_get_value(row, 3);
    ASSERT(expr_2 == NULL);

    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

static int test_row_get_value_null_input() {
    ASSERT(!row_get_value(NULL, 4));
    
    return 0;
}


/* ---------- row_set_value unit tests ---------- */

static int test_row_set_value() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);

    // New Values for both columns
    int32_t new_int_input = 123;
    char new_text_input[] = "world";
    Value *new_int_value = value_create(INTEGER, &new_int_input);
    Value *new_text_value = value_create(TEXT, new_text_input);

    ASSERT(row_set_value(row, 1, new_int_value));
    ASSERT(row_set_value(row, 2, new_text_value));

    Value *expr_1 = row_get_value(row, 1);
    ASSERT(expr_1 != NULL);
    ASSERT(expr_1->type == INTEGER);
    ASSERT(expr_1->value.int32_val == 123);

    Value *expr_2 = row_get_value(row, 2);
    ASSERT(expr_2 != NULL);
    ASSERT(expr_2->type == TEXT);
    ASSERT(strcmp(expr_2->value.text_val, "world") == 0);


    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

static int test_row_set_value_change_types() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);

    // New Values for both columns
    double double_input = 123.456;
    Value *new_double_value = value_create(DOUBLE, &double_input);

    ASSERT(row_set_value(row, 1, new_double_value));

    Value *expr_1 = row_get_value(row, 1);
    ASSERT(expr_1 != NULL);
    ASSERT(expr_1->type == DOUBLE);
    ASSERT(expr_1->value.double_val == 123.456);

    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

static int test_row_set_value_invalid_positions() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);

    // New Values for both columns
    int32_t new_int_input = 123;
    char new_text_input[] = "world";
    Value *new_int_value = value_create(INTEGER, &new_int_input);
    Value *new_text_value = value_create(TEXT, new_text_input);

    ASSERT(!row_set_value(row, 0, new_int_value));
    ASSERT(!row_set_value(row, 3, new_text_value));

    Value *expr_1 = row_get_value(row, 1);
    ASSERT(expr_1 != NULL);
    ASSERT(expr_1->type == INTEGER);
    ASSERT(expr_1->value.int32_val == -42);

    Value *expr_2 = row_get_value(row, 2);
    ASSERT(expr_2 != NULL);
    ASSERT(expr_2->type == TEXT);
    ASSERT(strcmp(expr_2->value.text_val, "hello") == 0);


    row_free(row);
    free_literal_expression(expressions[0]);
    free_literal_expression(expressions[1]);
    return 0;
}

static int test_row_set_value_null_input_row() {
    int32_t int_input = -42;
    Value *int_value = value_create(INTEGER, &int_input);

    ASSERT(!row_set_value(NULL, 1, int_value));

    value_free(int_value);
    return 0;
}

static int test_row_set_value_null_input_value() {
    int32_t int_input = -42;
    char text_input[] = "hello";

    Value *int_value = value_create(INTEGER, &int_input);
    Value *text_value = value_create(TEXT, text_input);

    ASSERT(int_value != NULL);
    ASSERT(text_value != NULL);

    ExpressionNode *expressions[2];
    
    expressions[0] = create_literal_expression(int_value);
    expressions[1] = create_literal_expression(text_value);

    ASSERT(expressions[0] != NULL);
    ASSERT(expressions[1] != NULL);

    // Not repeating all row creation assertions, since it's not the goal of this test
    Row *row = row_create(expressions, 2);
    ASSERT(row != NULL);

    ASSERT(!row_set_value(row, 1, NULL));

    row_free(row);
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

    /* ---------- row_create unit tests ---------- */
    result = test_row_create();
    generate_output(result, 0, "test_row_create");
    result = test_row_create_null_input();
    generate_output(result, 1, "test_row_create_null_input");
    
    /* ---------- row_mark_deleted unit tests ---------- */
    result = test_row_mark_deleted();
    generate_output(result, 2, "test_row_mark_deleted");
    result = test_row_mark_deleted_null_input();
    generate_output(result, 3, "test_row_mark_deleted_null_input");

    /* ---------- row_get_value unit tests ---------- */
    result = test_row_get_value();
    generate_output(result, 4, "test_row_get_value");
    result = test_row_get_value_invalid_positions();
    generate_output(result, 5, "test_row_get_value_invalid_positions");
    result = test_row_get_value_null_input();
    generate_output(result, 6, "test_row_get_value_null_input");

    /* ---------- row_set_value unit tests ---------- */
    result = test_row_set_value();
    generate_output(result, 7, "test_row_set_value");
    result = test_row_set_value_change_types();
    generate_output(result, 8, "test_row_set_value_change_types");
    result = test_row_set_value_invalid_positions();
    generate_output(result, 9, "test_row_set_value_invalid_positions");
    result = test_row_set_value_null_input_row();
    generate_output(result, 10, "test_row_set_value_null_input_row");
    result = test_row_set_value_null_input_value();
    generate_output(result, 12, "test_row_set_value_null_input_value");

    return 0;
}
