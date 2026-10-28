#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../include/semantic_binder.h"
#include "../src/semantic_binder/semantic_binder_utils.h"
#include "../include/expressions.h"
#include "../include/table.h"
#include "../include/database.h"
#include "../include/schema.h"
#include "../include/data_types.h"


#define ASSERT(condition) \
    if (!(condition)) { \
        goto cleanup;  \
    }

/* ---------- Helpers ---------- */

typedef struct binder_test_fixture {
    Database db;

    Table tables[2];
    Schema schemas[2];

    Column employee_columns[4];
    Column department_columns[3];

    Column *employee_column_ptrs[4];
    Column *department_column_ptrs[3];

    BindingRelation relations[2];
    BindingContext context;
} BinderTestFixture;

// Initialize test table's column
static void init_test_column(Column *column, const char *name, DataType type) {
    memset(column, 0, sizeof(Column));
    snprintf(column->name, sizeof(column->name), "%s", name);
    column->type = type;
}

// Initialize fixture
static void init_binder_fixture(BinderTestFixture *fixture) {
    memset(fixture, 0, sizeof(BinderTestFixture));

    // Employees table columns
    init_test_column(&fixture->employee_columns[0], "id", INTEGER);
    init_test_column(&fixture->employee_columns[1], "name", VARCHAR);
    init_test_column(&fixture->employee_columns[2], "salary", DOUBLE);
    init_test_column(&fixture->employee_columns[3], "department_id", INTEGER);
    
    // Departments table columns
    init_test_column(&fixture->department_columns[0], "id", INTEGER);
    init_test_column(&fixture->department_columns[1], "name", VARCHAR);
    init_test_column(&fixture->department_columns[2], "budget", DOUBLE);

    // Set up column pointer arrays
    for (uint32_t i = 0; i < 4; i++) {
        fixture->employee_column_ptrs[i] = &fixture->employee_columns[i];
    }

    for (uint32_t i = 0; i < 3; i++) {
        fixture->department_column_ptrs[i] = &fixture->department_columns[i];
    }

    // Employees schema
    fixture->schemas[0].columns = fixture->employee_column_ptrs;
    fixture->schemas[0].num_columns = 4;
    
    // Departments schema
    fixture->schemas[1].columns = fixture->department_column_ptrs;
    fixture->schemas[1].num_columns = 3;

    // Tables
    snprintf(fixture->tables[0].name, sizeof(fixture->tables[0].name), "%s", "employees");
    snprintf(fixture->tables[1].name, sizeof(fixture->tables[1].name), "%s", "departments");

    fixture->tables[0].table_schema = &fixture->schemas[0];
    fixture->tables[1].table_schema = &fixture->schemas[1];

    // Relations
    fixture->relations[0].table = &fixture->tables[0];
    fixture->relations[1].table = &fixture->tables[1];

    snprintf(fixture->relations[0].alias, sizeof(fixture->relations[0].alias), "%s", "e");
    snprintf(fixture->relations[1].alias, sizeof(fixture->relations[1].alias), "%s", "d");

    // Binding context
    fixture->context.db = &fixture->db;
    fixture->context.relations = fixture->relations;
    fixture->context.num_relations = 2;
}


/* ---------- bind_literal unit tests ---------- */

int test_bind_literal_integer() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    int32_t number = 42;

    expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(expr->expression_data.literal_value.literal != NULL);

    expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);
    
    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;
cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_literal_typed_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    expr->expression_data.literal_value.literal = value_create_null(INTEGER);
    ASSERT(expr->expression_data.literal_value.literal != NULL);

    expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_literal_untyped_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    expr->expression_data.literal_value.literal = NULL;
    expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.is_untyped_null == true);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_literal_invalid_type() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    int32_t number = 42;

    expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(expr->expression_data.literal_value.literal != NULL);

    // Intentionally corrupt the type for testing.
    expr->expression_data.literal_value.literal->type = (DataType) 999;
    expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_literal_missing_value() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    // Invalid literal: missing Value without the untyped NULL flag.
    expr->expression_data.literal_value.literal = NULL;
    expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_column_ref unit tests ---------- */

int test_bind_column_ref_unqualified_column() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.column_name, 
        "salary", 
        sizeof(expr->expression_data.column_value.column_name)
    );

    expr->expression_data.column_value.relation_index = -1;
    expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == DOUBLE);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(expr->expression_data.column_value.relation_index == 0);
    ASSERT(expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_column_ref_qualified_column() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(expr->expression_data.column_value.table_name)
    );

    strncpy(
        expr->expression_data.column_value.column_name, 
        "salary", 
        sizeof(expr->expression_data.column_value.column_name)
    );

    expr->expression_data.column_value.relation_index = -1;
    expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == DOUBLE);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(expr->expression_data.column_value.relation_index == 0);
    ASSERT(expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_column_ref_qualified_common_column_name() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.table_name, 
        "d", 
        sizeof(expr->expression_data.column_value.table_name)
    );

    strncpy(
        expr->expression_data.column_value.column_name, 
        "id", 
        sizeof(expr->expression_data.column_value.column_name)
    );

    expr->expression_data.column_value.relation_index = -1;
    expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(expr->expression_data.column_value.relation_index == 1);
    ASSERT(expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_column_ref_ambiguous_column() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.column_name, 
        "id", 
        sizeof(expr->expression_data.column_value.column_name)
    );

    expr->expression_data.column_value.relation_index = -1;
    expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    ASSERT(expr->expression_data.column_value.relation_index == -1);
    ASSERT(expr->expression_data.column_value.column_index == -1);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_column_ref_unknown_qualified_column() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(expr->expression_data.column_value.table_name)
    );

    strncpy(
        expr->expression_data.column_value.column_name, 
        "unknown_name", 
        sizeof(expr->expression_data.column_value.column_name)
    );

    expr->expression_data.column_value.relation_index = -1;
    expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    ASSERT(expr->expression_data.column_value.relation_index == -1);
    ASSERT(expr->expression_data.column_value.column_index == -1);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_column_ref_unknown_table_alias() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.table_name, 
        "x", 
        sizeof(expr->expression_data.column_value.table_name)
    );

    strncpy(
        expr->expression_data.column_value.column_name, 
        "salary", 
        sizeof(expr->expression_data.column_value.column_name)
    );

    expr->expression_data.column_value.relation_index = -1;
    expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    ASSERT(expr->expression_data.column_value.relation_index == -1);
    ASSERT(expr->expression_data.column_value.column_index == -1);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_column_ref_already_bound_column() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.column_value.column_name,
        "salary",
        sizeof(expr->expression_data.column_value.column_name)
    );

    BoundExpressionType bound_type = {0};

    // First binding should succeed.
    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == DOUBLE);
    ASSERT(expr->expression_data.column_value.relation_index == 0);
    ASSERT(expr->expression_data.column_value.column_index == 2);

    // Second binding should fail because indexes are already assigned.
    success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    // Verify the previously assigned indexes remain unchanged.
    ASSERT(expr->expression_data.column_value.relation_index == 0);
    ASSERT(expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_table_ref unit tests ---------- */

int test_bind_table_ref_success() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_TABLE_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.table_value.table_name,
        "employees",
        sizeof(expr->expression_data.table_value.table_name)
    );

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_table_ref_unknown_table() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_TABLE_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.table_value.table_name,
        "sales",
        sizeof(expr->expression_data.table_value.table_name)
    );

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_table_ref_empty_table_name() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_TABLE_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.table_value.table_name,
        "",
        sizeof(expr->expression_data.table_value.table_name)
    );

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_unary unit tests ---------- */

int test_bind_unary_minus_integer() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_SUB;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    int32_t number = 10;

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    unary_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(unary_expr->expression_data.literal_value.literal != NULL);

    unary_expr->expression_data.literal_value.is_untyped_null = false;


    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_not_boolean() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_NOT;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    bool true_value = true;

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    unary_expr->expression_data.literal_value.literal = value_create(BOOL, &true_value);
    ASSERT(unary_expr->expression_data.literal_value.literal != NULL);

    unary_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_bitwise_not_integer() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_BITWISE_NOT;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    int32_t number = 10;

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    unary_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(unary_expr->expression_data.literal_value.literal != NULL);

    unary_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_not_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_NOT;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;

    // Untyped SQL NULL literal
    unary_expr->expression_data.literal_value.literal = NULL;
    unary_expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_invalid_not_text() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_NOT;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    char word[] = "text";

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    unary_expr->expression_data.literal_value.literal = value_create(TEXT, &word);
    ASSERT(unary_expr->expression_data.literal_value.literal != NULL);

    unary_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_invalid_not_integer() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_NOT;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    int32_t number = 10;

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    unary_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(unary_expr->expression_data.literal_value.literal != NULL);

    unary_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_invalid_bitwise_not_double() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_BITWISE_NOT;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    double number = 3.5;

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    unary_expr->expression_data.literal_value.literal = value_create(DOUBLE, &number);
    ASSERT(unary_expr->expression_data.literal_value.literal != NULL);

    unary_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_unary_invalid_minus_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_UNARY);
    ASSERT(expr != NULL);

    expr->expression_data.unary_expr.op = OP_SUB;
    expr->expression_data.unary_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.unary_expr.operand != NULL);

    ExpressionNode *unary_expr = expr->expression_data.unary_expr.operand;
    
    // Untyped SQL NULL literal
    unary_expr->expression_data.literal_value.literal = NULL;
    unary_expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_binary unit tests ---------- */

int test_bind_binary_addition() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    number = 20;

    right_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;


    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_modulo() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_MODULO;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    number = 3;

    right_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;


    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_equality() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_EQ;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    number = 20;

    right_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;


    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_less_than() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_LT;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    number = 20;

    right_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;


    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_and() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_AND;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    bool boolean = true;

    left_operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    boolean = false;

    right_operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;


    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_integer_plus_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    right_operand_expr->expression_data.literal_value.literal = NULL;
    right_operand_expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_integer_null_equality() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_EQ;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    left_operand_expr->expression_data.literal_value.literal = NULL;
    left_operand_expr->expression_data.literal_value.is_untyped_null = true;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    right_operand_expr->expression_data.literal_value.literal = NULL;
    right_operand_expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_invalid_integer_text_addition() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    char word[] = "text";

    right_operand_expr->expression_data.literal_value.literal = value_create(TEXT, &word);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_invalid_integer_double_addition() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    double number_1 = 3.5;

    right_operand_expr->expression_data.literal_value.literal = value_create(DOUBLE, &number_1);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}
    
int test_bind_binary_invalid_double_double_modulo() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_MODULO;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    double number_1 = 3.5;

    left_operand_expr->expression_data.literal_value.literal = value_create(DOUBLE, &number_1);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    double number_2 = 2.0;

    right_operand_expr->expression_data.literal_value.literal = value_create(DOUBLE, &number_2);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_invalid_boolean_less_than() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_LT;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    bool boolean_1 = true;

    left_operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean_1);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    bool boolean_2 = false;

    right_operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean_2);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}
    
int test_bind_binary_invalid_boolean_integer_and() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_AND;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    bool boolean = true;

    left_operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    int32_t number = 10;

    right_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_invalid_integer_text_equality() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_EQ;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    int32_t number = 10;

    left_operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_operand_expr->expression_data.literal_value.literal != NULL);

    left_operand_expr->expression_data.literal_value.is_untyped_null = false;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    char word[] = "text";

    right_operand_expr->expression_data.literal_value.literal = value_create(TEXT, &word);
    ASSERT(right_operand_expr->expression_data.literal_value.literal != NULL);

    right_operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_binary_invalid_null_addition() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_operand_expr = expr->expression_data.binary_expr.left_operand;

    left_operand_expr->expression_data.literal_value.literal = NULL;
    left_operand_expr->expression_data.literal_value.is_untyped_null = true;

    ExpressionNode *right_operand_expr = expr->expression_data.binary_expr.right_operand;

    right_operand_expr->expression_data.literal_value.literal = NULL;
    right_operand_expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_is_null unit tests ---------- */

int test_bind_is_null_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IS_NULL);
    ASSERT(expr != NULL);

    expr->expression_data.is_null_expr.operand = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.is_null_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.is_null_expr.operand;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "salary", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;
    
    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}   

int test_bind_null_is_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IS_NULL);
    ASSERT(expr != NULL);

    expr->expression_data.is_null_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.is_null_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.is_null_expr.operand;

    operand_expr->expression_data.literal_value.literal = NULL;
    operand_expr->expression_data.literal_value.is_untyped_null = true;
    
    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}   

int test_bind_is_null_missing_operand() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IS_NULL);
    ASSERT(expr != NULL);

    expr->expression_data.is_null_expr.operand = NULL;
    
    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}   

/* ---------- bind_is_not_null unit tests ---------- */

int test_bind_is_not_null_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IS_NOT_NULL);
    ASSERT(expr != NULL);

    expr->expression_data.is_null_expr.operand = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.is_null_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.is_null_expr.operand;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "salary", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;
    
    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}  

int test_bind_is_not_null_missing_operand() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IS_NOT_NULL);
    ASSERT(expr != NULL);

    expr->expression_data.is_null_expr.operand = NULL;
    
    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}   

/* ---------- bind_in unit tests ---------- */

int test_bind_in_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IN);
    ASSERT(expr != NULL);

    expr->expression_data.in_expr.operand = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.in_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.in_expr.operand;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "id", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;

    expr->expression_data.in_expr.option_count = 3;

    expr->expression_data.in_expr.set_options = (ExpressionNode **) calloc(3, sizeof(ExpressionNode *));
    ASSERT(expr->expression_data.in_expr.set_options != NULL);

    ExpressionNode **options = expr->expression_data.in_expr.set_options;

    options[0] = expression_node_create(EXPR_LITERAL);
    options[1] = expression_node_create(EXPR_LITERAL);
    options[2] = expression_node_create(EXPR_LITERAL);

    int32_t number = 1;
    options[0]->expression_data.literal_value.literal = value_create(INTEGER, &number);
    options[0]->expression_data.literal_value.is_untyped_null = false;

    number = 2;
    options[1]->expression_data.literal_value.literal = value_create(INTEGER, &number);
    options[1]->expression_data.literal_value.is_untyped_null = false;

    number = 3;
    options[2]->expression_data.literal_value.literal = value_create(INTEGER, &number);
    options[2]->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_in_null_operand() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IN);
    ASSERT(expr != NULL);

    expr->expression_data.in_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.in_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.in_expr.operand;

    operand_expr->expression_data.literal_value.literal = NULL;
    operand_expr->expression_data.literal_value.is_untyped_null = true;

    expr->expression_data.in_expr.option_count = 2;

    expr->expression_data.in_expr.set_options = (ExpressionNode **) calloc(2, sizeof(ExpressionNode *));
    ASSERT(expr->expression_data.in_expr.set_options != NULL);

    ExpressionNode **options = expr->expression_data.in_expr.set_options;

    options[0] = expression_node_create(EXPR_LITERAL);
    options[1] = expression_node_create(EXPR_LITERAL);

    int32_t number = 1;
    options[0]->expression_data.literal_value.literal = value_create(INTEGER, &number);
    options[0]->expression_data.literal_value.is_untyped_null = false;

    number = 2;
    options[1]->expression_data.literal_value.literal = value_create(INTEGER, &number);
    options[1]->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_in_null_operand_in_null_options() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IN);
    ASSERT(expr != NULL);

    expr->expression_data.in_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.in_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.in_expr.operand;

    operand_expr->expression_data.literal_value.literal = NULL;
    operand_expr->expression_data.literal_value.is_untyped_null = true;

    expr->expression_data.in_expr.option_count = 2;

    expr->expression_data.in_expr.set_options = (ExpressionNode **) calloc(2, sizeof(ExpressionNode *));
    ASSERT(expr->expression_data.in_expr.set_options != NULL);

    ExpressionNode **options = expr->expression_data.in_expr.set_options;

    options[0] = expression_node_create(EXPR_LITERAL);
    options[1] = expression_node_create(EXPR_LITERAL);

    options[0]->expression_data.literal_value.literal = NULL;
    options[0]->expression_data.literal_value.is_untyped_null = true;

    options[1]->expression_data.literal_value.literal = NULL;
    options[1]->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;   
}

int test_bind_in_integer_in_text_options() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IN);
    ASSERT(expr != NULL);

    expr->expression_data.in_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.in_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.in_expr.operand;

    int32_t number = 10;
    operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    operand_expr->expression_data.literal_value.is_untyped_null = false;

    expr->expression_data.in_expr.option_count = 2;

    expr->expression_data.in_expr.set_options = (ExpressionNode **) calloc(2, sizeof(ExpressionNode *));
    ASSERT(expr->expression_data.in_expr.set_options != NULL);

    ExpressionNode **options = expr->expression_data.in_expr.set_options;

    options[0] = expression_node_create(EXPR_LITERAL);
    options[1] = expression_node_create(EXPR_LITERAL);

    char word_1[] = "first_option";
    options[0]->expression_data.literal_value.literal = value_create(TEXT, word_1);
    options[0]->expression_data.literal_value.is_untyped_null = false;

    char word_2[] = "second_option";
    options[1]->expression_data.literal_value.literal = value_create(TEXT, word_2);
    options[1]->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_in_integer_empty_options() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IN);
    ASSERT(expr != NULL);

    expr->expression_data.in_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.in_expr.operand != NULL);

    ExpressionNode *operand_expr = expr->expression_data.in_expr.operand;

    int32_t number = 10;
    operand_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    operand_expr->expression_data.literal_value.is_untyped_null = false;

    expr->expression_data.in_expr.option_count = 2;

    expr->expression_data.in_expr.set_options = (ExpressionNode **) calloc(2, sizeof(ExpressionNode *));
    ASSERT(expr->expression_data.in_expr.set_options != NULL);

    ExpressionNode **options = expr->expression_data.in_expr.set_options;

    options[0] = expression_node_create(EXPR_LITERAL);
    options[1] = expression_node_create(EXPR_LITERAL);

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_in_integer_missing_operand() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_IN);
    ASSERT(expr != NULL);

    expr->expression_data.in_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.in_expr.operand != NULL);

    expr->expression_data.in_expr.option_count = 2;

    expr->expression_data.in_expr.set_options = (ExpressionNode **) calloc(2, sizeof(ExpressionNode *));
    ASSERT(expr->expression_data.in_expr.set_options != NULL);

    ExpressionNode **options = expr->expression_data.in_expr.set_options;

    options[0] = expression_node_create(EXPR_LITERAL);
    options[1] = expression_node_create(EXPR_LITERAL);

    char word_1[] = "first_option";
    options[0]->expression_data.literal_value.literal = value_create(TEXT, word_1);
    options[0]->expression_data.literal_value.is_untyped_null = false;

    char word_2[] = "second_option";
    options[1]->expression_data.literal_value.literal = value_create(TEXT, word_2);
    options[1]->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_between unit tests ---------- */

int test_between_integers() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BETWEEN);
    ASSERT(expr != NULL);

    expr->expression_data.between_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.operand != NULL);

    expr->expression_data.between_expr.lower = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.lower != NULL);

    expr->expression_data.between_expr.upper = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.upper != NULL);

    ExpressionNode *operand = expr->expression_data.between_expr.operand;
    ExpressionNode *lower = expr->expression_data.between_expr.lower;
    ExpressionNode *upper = expr->expression_data.between_expr.upper;

    int32_t number = 10;
    operand->expression_data.literal_value.literal = value_create(INTEGER, &number);
    operand->expression_data.literal_value.is_untyped_null = false;

    number = 1;
    lower->expression_data.literal_value.literal = value_create(INTEGER, &number);
    lower->expression_data.literal_value.is_untyped_null = false;

    number = 20;
    upper->expression_data.literal_value.literal = value_create(INTEGER, &number);
    upper->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_between_null_integer_bounds() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BETWEEN);
    ASSERT(expr != NULL);

    expr->expression_data.between_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.operand != NULL);

    expr->expression_data.between_expr.lower = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.lower != NULL);

    expr->expression_data.between_expr.upper = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.upper != NULL);

    ExpressionNode *operand = expr->expression_data.between_expr.operand;
    ExpressionNode *lower = expr->expression_data.between_expr.lower;
    ExpressionNode *upper = expr->expression_data.between_expr.upper;

    operand->expression_data.literal_value.literal = NULL;
    operand->expression_data.literal_value.is_untyped_null = true;

    int32_t number = 1;
    lower->expression_data.literal_value.literal = value_create(INTEGER, &number);
    lower->expression_data.literal_value.is_untyped_null = false;

    number = 20;
    upper->expression_data.literal_value.literal = value_create(INTEGER, &number);
    upper->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_between_null_null_bounds() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BETWEEN);
    ASSERT(expr != NULL);

    expr->expression_data.between_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.operand != NULL);

    expr->expression_data.between_expr.lower = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.lower != NULL);

    expr->expression_data.between_expr.upper = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.upper != NULL);

    ExpressionNode *operand = expr->expression_data.between_expr.operand;
    ExpressionNode *lower = expr->expression_data.between_expr.lower;
    ExpressionNode *upper = expr->expression_data.between_expr.upper;

    operand->expression_data.literal_value.literal = NULL;
    operand->expression_data.literal_value.is_untyped_null = true;

    lower->expression_data.literal_value.literal = NULL;
    lower->expression_data.literal_value.is_untyped_null = true;

    upper->expression_data.literal_value.literal = NULL;
    upper->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == BOOL);
    ASSERT(bound_type.is_untyped_null == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_between_invalid_integer_text_bounds() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BETWEEN);
    ASSERT(expr != NULL);

    expr->expression_data.between_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.operand != NULL);

    expr->expression_data.between_expr.lower = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.lower != NULL);

    expr->expression_data.between_expr.upper = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.upper != NULL);

    ExpressionNode *operand = expr->expression_data.between_expr.operand;
    ExpressionNode *lower = expr->expression_data.between_expr.lower;
    ExpressionNode *upper = expr->expression_data.between_expr.upper;

    int32_t number = 10;
    operand->expression_data.literal_value.literal = value_create(INTEGER, &number);
    operand->expression_data.literal_value.is_untyped_null = false;

    char word_1[] = "word_1";
    lower->expression_data.literal_value.literal = value_create(TEXT, word_1);
    lower->expression_data.literal_value.is_untyped_null = false;

    char word_2[] = "word_2";
    upper->expression_data.literal_value.literal = value_create(TEXT, word_2);
    upper->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_between_invalid_boolean_boolean_bounds() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BETWEEN);
    ASSERT(expr != NULL);

    expr->expression_data.between_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.operand != NULL);

    expr->expression_data.between_expr.lower = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.lower != NULL);

    expr->expression_data.between_expr.upper = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.upper != NULL);

    ExpressionNode *operand = expr->expression_data.between_expr.operand;
    ExpressionNode *lower = expr->expression_data.between_expr.lower;
    ExpressionNode *upper = expr->expression_data.between_expr.upper;

    bool bool_op = true;
    operand->expression_data.literal_value.literal = value_create(BOOL, &bool_op);
    operand->expression_data.literal_value.is_untyped_null = false;

    bool bool_lower = true;
    lower->expression_data.literal_value.literal = value_create(BOOL, &bool_lower);
    lower->expression_data.literal_value.is_untyped_null = false;

    bool bool_upper = false;
    upper->expression_data.literal_value.literal = value_create(BOOL, &bool_upper);
    upper->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;   
}

int test_between_invalid_missing_bounds() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BETWEEN);
    ASSERT(expr != NULL);

    expr->expression_data.between_expr.operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.operand != NULL);

    expr->expression_data.between_expr.lower = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.lower != NULL);

    expr->expression_data.between_expr.upper = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.between_expr.upper != NULL);

    ExpressionNode *operand = expr->expression_data.between_expr.operand;

    int32_t number = 10;
    operand->expression_data.literal_value.literal = value_create(INTEGER, &number);
    operand->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result; 
}

/* ---------- bind_function unit tests ---------- */

int test_function_count_wildcard() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = NULL;
    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_COUNT;
    expr->expression_data.aggregate_func_expr.wildcard = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == UNSIGNED_INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_count_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_COUNT;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "id", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == UNSIGNED_INTEGER);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_sum_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_SUM;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "salary", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == DOUBLE);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_avg_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_AVG;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "id", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == DOUBLE);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_min_column_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_MIN;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "name", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == VARCHAR);
    ASSERT(bound_type.is_untyped_null == false);
    ASSERT(operand_expr->expression_data.column_value.relation_index == 0);
    ASSERT(operand_expr->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_invalid_sum_of_texts() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_SUM;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    strncpy(
        operand_expr->expression_data.column_value.table_name, 
        "e", 
        sizeof(operand_expr->expression_data.column_value.table_name)
    );

    strncpy(
        operand_expr->expression_data.column_value.column_name, 
        "name", 
        sizeof(operand_expr->expression_data.column_value.column_name)
    );

    operand_expr->expression_data.column_value.relation_index = -1;
    operand_expr->expression_data.column_value.column_index = -1;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_invalid_avg_of_bool() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_AVG;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    bool boolean = true;
    operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean);
    operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_invalid_min_of_bool() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_MIN;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    bool boolean = true;
    operand_expr->expression_data.literal_value.literal = value_create(BOOL, &boolean);
    operand_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_invalid_sum_of_null() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_SUM;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    ExpressionNode *operand_expr = expr->expression_data.aggregate_func_expr.expression;

    operand_expr->expression_data.literal_value.literal = NULL;
    operand_expr->expression_data.literal_value.is_untyped_null = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_invalid_sum_wildcard() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = NULL;
    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_SUM;
    expr->expression_data.aggregate_func_expr.wildcard = true;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_function_invalid_count_missing_operand() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_FUNCTIONS);
    ASSERT(expr != NULL);

    expr->expression_data.aggregate_func_expr.expression = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.aggregate_func_expr.expression != NULL);

    expr->expression_data.aggregate_func_expr.type = EXPR_AGGREGATE_COUNT;
    expr->expression_data.aggregate_func_expr.wildcard = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}


/* ---------- scalar helper unit tests ---------- */

int test_scalar_helper_arithmetic_expression() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);

    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_BINARY);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);
    
    expr->expression_data.binary_expr.op = OP_ADD;

    ExpressionNode *left_expr = expr->expression_data.binary_expr.left_operand;
    ExpressionNode *right_expr = expr->expression_data.binary_expr.right_operand;

    int number = 10;
    left_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    left_expr->expression_data.literal_value.is_untyped_null = false;

    right_expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(right_expr->expression_data.binary_expr.left_operand != NULL);

    right_expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(right_expr->expression_data.binary_expr.right_operand != NULL);

    right_expr->expression_data.binary_expr.op = OP_MUL;

    ExpressionNode *right_left_expr = right_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *right_right_expr = right_expr->expression_data.binary_expr.right_operand;

    number = 20;
    right_left_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    right_left_expr->expression_data.literal_value.is_untyped_null = false;

    number = 30;
    right_right_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    right_right_expr->expression_data.literal_value.is_untyped_null = false;

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_scalar_helper_reject_table_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_TABLE_REF);
    ASSERT(expr != NULL);

    strncpy(
        expr->expression_data.table_value.table_name,
        "employees",
        sizeof(expr->expression_data.table_value.table_name)
    );

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);
    
    result = 0;

cleanup:
    expression_node_free(expr);
    return result;   
}

int test_scalar_helper_nested_table_ref() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);

    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_TABLE_REF);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    ExpressionNode *left_expr = expr->expression_data.binary_expr.left_operand;
    ExpressionNode *right_expr = expr->expression_data.binary_expr.right_operand;

    int32_t number = 10;

    left_expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(left_expr->expression_data.literal_value.literal != NULL);
    left_expr->expression_data.literal_value.is_untyped_null = false;

    snprintf(
        right_expr->expression_data.table_value.table_name,
        sizeof(right_expr->expression_data.table_value.table_name),
        "%s",
        "employees"
    );

    BoundExpressionType bound_type = {0};

    bool success = bind_scalar_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

/* ---------- bind_expression dispatcher unit tests ---------- */

int test_bind_expression_valid_binary() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);

    expr->expression_data.binary_expr.op = OP_ADD;

    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);

    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);

    int32_t left_number = 10;
    int32_t right_number = 20;

    ExpressionNode *left_expr = expr->expression_data.binary_expr.left_operand;
    ExpressionNode *right_expr = expr->expression_data.binary_expr.right_operand;

    left_expr->expression_data.literal_value.literal = value_create(INTEGER, &left_number);
    ASSERT(left_expr->expression_data.literal_value.literal != NULL);

    right_expr->expression_data.literal_value.literal = value_create(INTEGER, &right_number);
    ASSERT(right_expr->expression_data.literal_value.literal != NULL);

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success);
    ASSERT(bound_type.type == INTEGER);
    ASSERT(bound_type.is_untyped_null == false);

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_expression_null_expression() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(NULL, &binder.context, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    return result;
}

int test_bind_expression_invalid_type() {
    int result = 1;
    BinderTestFixture binder = {0};
    init_binder_fixture(&binder);

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    expr->type = (ExpressionType) 999;

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, &binder.context, &bound_type);

    ASSERT(success == false);

    // Restore the type so expression_node_free() can free it normally.
    expr->type = EXPR_LITERAL;

    result = 0;

cleanup:
    expression_node_free(expr);
    return result;
}

int test_bind_expression_null_context() {
    int result = 1;

    ExpressionNode *expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);

    int32_t number = 10;

    expr->expression_data.literal_value.literal = value_create(INTEGER, &number);
    ASSERT(expr->expression_data.literal_value.literal != NULL);

    BoundExpressionType bound_type = {0};

    bool success = bind_expression(expr, NULL, &bound_type);

    ASSERT(success == false);

    result = 0;

cleanup:
    expression_node_free(expr);
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

    /* ---------- bind_literal unit tests ---------- */
    result = test_bind_literal_integer();
    generate_output(result, 0, "test_bind_literal_integer");
    result = test_bind_literal_typed_null();
    generate_output(result, 1, "test_bind_literal_typed_null");
    result = test_bind_literal_untyped_null();
    generate_output(result, 2, "test_bind_literal_untyped_null");
    result = test_bind_literal_invalid_type();
    generate_output(result, 3, "test_bind_literal_invalid_type");
    result = test_bind_literal_missing_value();
    generate_output(result, 4, "test_bind_literal_missing_value");

    /* ---------- bind_column_ref unit tests ---------- */
    result = test_bind_column_ref_unqualified_column();
    generate_output(result, 5, "test_bind_column_ref_unqualified_column");
    result = test_bind_column_ref_qualified_column();
    generate_output(result, 6, "test_bind_column_ref_qualified_column");
    result = test_bind_column_ref_qualified_common_column_name();
    generate_output(result, 7, "test_bind_column_ref_qualified_common_column_name");
    result = test_bind_column_ref_ambiguous_column();
    generate_output(result, 8, "test_bind_column_ref_ambiguous_column");
    result = test_bind_column_ref_unknown_qualified_column();
    generate_output(result, 9, "test_bind_column_ref_unknown_qualified_column");
    result = test_bind_column_ref_unknown_table_alias();
    generate_output(result, 10, "test_bind_column_ref_unknown_table_alias");
    result = test_bind_column_ref_already_bound_column();
    generate_output(result, 11, "test_bind_column_ref_already_bound_column");

    /* ---------- bind_table_ref unit tests ---------- */
    result = test_bind_table_ref_success();
    generate_output(result, 12, "test_bind_table_ref_success");
    result = test_bind_table_ref_unknown_table();
    generate_output(result, 13, "test_bind_table_ref_unknown_table");
    result = test_bind_table_ref_empty_table_name();
    generate_output(result, 14, "test_bind_table_ref_empty_table_name");

    /* ---------- bind_unary unit tests ---------- */
    result = test_bind_unary_minus_integer();
    generate_output(result, 15, "test_bind_unary_minus_integer");
    result = test_bind_unary_not_boolean();
    generate_output(result, 16, "test_bind_unary_not_boolean");
    result = test_bind_unary_bitwise_not_integer();
    generate_output(result, 17, "test_bind_unary_bitwise_not_integer");
    result = test_bind_unary_not_null();
    generate_output(result, 18, "test_bind_unary_not_null");
    result = test_bind_unary_invalid_not_text();
    generate_output(result, 19, "test_bind_unary_invalid_not_text");
    result = test_bind_unary_invalid_not_integer();
    generate_output(result, 20, "test_bind_unary_invalid_not_integer");
    result = test_bind_unary_invalid_bitwise_not_double();
    generate_output(result, 21, "test_bind_unary_invalid_bitwise_not_double");
    result = test_bind_unary_invalid_minus_null();
    generate_output(result, 22, "test_bind_unary_invalid_minus_null");

    /* ---------- bind_binary unit tests ---------- */
    result = test_bind_binary_addition();
    generate_output(result, 23, "test_bind_binary_addition");
    result = test_bind_binary_modulo();
    generate_output(result, 24, "test_bind_binary_modulo");
    result = test_bind_binary_equality();
    generate_output(result, 25, "test_bind_binary_equality");
    result = test_bind_binary_less_than();
    generate_output(result, 26, "test_bind_binary_less_than");
    result = test_bind_binary_and();
    generate_output(result, 27, "test_bind_binary_and");
    result = test_bind_binary_integer_plus_null();
    generate_output(result, 28, "test_bind_binary_integer_plus_null");
    result = test_bind_binary_integer_null_equality();
    generate_output(result, 29, "test_bind_binary_integer_null_equality");
    result = test_bind_binary_invalid_integer_text_addition();
    generate_output(result, 30, "test_bind_binary_invalid_integer_text_addition");
    result = test_bind_binary_invalid_integer_double_addition();
    generate_output(result, 31, "test_bind_binary_invalid_integer_double_addition");
    result = test_bind_binary_invalid_double_double_modulo();
    generate_output(result, 32, "test_bind_binary_invalid_double_double_modulo");
    result = test_bind_binary_invalid_boolean_less_than();
    generate_output(result, 33, "test_bind_binary_invalid_boolean_less_than");
    result = test_bind_binary_invalid_boolean_integer_and();
    generate_output(result, 34, "test_bind_binary_invalid_boolean_integer_and");
    result = test_bind_binary_invalid_integer_text_equality();
    generate_output(result, 35, "test_bind_binary_invalid_integer_text_equality");
    result = test_bind_binary_invalid_null_addition();
    generate_output(result, 36, "test_bind_binary_invalid_null_addition");

    /* ---------- bind_is_null unit tests ---------- */
    result = test_bind_is_null_column_ref();
    generate_output(result, 37, "test_bind_is_null_column_ref");
    result = test_bind_null_is_null();
    generate_output(result, 38, "test_bind_null_is_null");
    result = test_bind_is_null_missing_operand();
    generate_output(result, 39, "test_bind_is_null_missing_operand");

    /* ---------- bind_is_not_null unit tests ---------- */
    result = test_bind_is_not_null_column_ref();
    generate_output(result, 40, "test_bind_is_not_null_column_ref");
    result = test_bind_is_not_null_missing_operand();
    generate_output(result, 41, "test_bind_is_not_null_missing_operand");

    /* ---------- bind_in unit tests ---------- */
    result = test_bind_in_column_ref();
    generate_output(result, 42, "test_bind_in_column_ref");
    result = test_bind_in_null_operand();
    generate_output(result, 43, "test_bind_in_null_operand");
    result = test_bind_in_null_operand_in_null_options();
    generate_output(result, 44, "test_bind_in_null_operand_in_null_options");
    result = test_bind_in_integer_in_text_options();
    generate_output(result, 45, "test_bind_in_integer_in_text_options");
    result = test_bind_in_integer_empty_options();
    generate_output(result, 46, "test_bind_in_integer_empty_options");
    result = test_bind_in_integer_missing_operand();
    generate_output(result, 47, "test_bind_in_integer_missing_operand");

    /* ---------- bind_between unit tests ---------- */
    result = test_between_integers();
    generate_output(result, 48, "test_between_integers");
    result = test_between_null_integer_bounds();
    generate_output(result, 49, "test_between_null_integer_bounds");
    result = test_between_null_null_bounds();
    generate_output(result, 50, "test_between_null_null_bounds");
    result = test_between_invalid_integer_text_bounds();
    generate_output(result, 51, "test_between_invalid_integer_text_bounds");
    result = test_between_invalid_boolean_boolean_bounds();
    generate_output(result, 52, "test_between_invalid_boolean_boolean_bounds");
    result = test_between_invalid_missing_bounds();
    generate_output(result, 53, "test_between_invalid_missing_bounds");
    
    /* ---------- bind_function unit tests ---------- */
    result = test_function_count_wildcard();
    generate_output(result, 54, "test_function_count_wildcard");
    result = test_function_count_column_ref();
    generate_output(result, 55, "test_function_count_column_ref");
    result = test_function_sum_column_ref();
    generate_output(result, 56, "test_function_sum_column_ref");
    result = test_function_avg_column_ref();
    generate_output(result, 57, "test_function_avg_column_ref");
    result = test_function_min_column_ref();
    generate_output(result, 58, "test_function_min_column_ref");
    result = test_function_invalid_sum_of_texts();
    generate_output(result, 59, "test_function_invalid_sum_of_texts");
    result = test_function_invalid_avg_of_bool();
    generate_output(result, 60, "test_function_invalid_avg_of_bool");
    result = test_function_invalid_min_of_bool();
    generate_output(result, 61, "test_function_invalid_min_of_bool");
    result = test_function_invalid_sum_of_null();
    generate_output(result, 62, "test_function_invalid_sum_of_null");
    result = test_function_invalid_sum_wildcard();
    generate_output(result, 63, "test_function_invalid_sum_wildcard");
    result = test_function_invalid_count_missing_operand();
    generate_output(result, 64, "test_function_invalid_count_missing_operand");

    /* ---------- scalar_helper unit tests ---------- */
    result = test_scalar_helper_arithmetic_expression();
    generate_output(result, 65, "test_scalar_helper_arithmetic_expression");
    result = test_scalar_helper_reject_table_ref();
    generate_output(result, 66, "test_scalar_helper_reject_table_ref");
    result = test_scalar_helper_nested_table_ref();
    generate_output(result, 67, "test_scalar_helper_nested_table_ref");

    /* ---------- bind_expression dispatcher unit tests ---------- */
    result = test_bind_expression_valid_binary();
    generate_output(result, 68, "test_bind_expression_valid_binary");
    result = test_bind_expression_null_expression();
    generate_output(result, 69, "test_bind_expression_null_expression");
    result = test_bind_expression_invalid_type();
    generate_output(result, 70, "test_bind_expression_invalid_type");
    result = test_bind_expression_null_context();
    generate_output(result, 71, "test_bind_expression_null_context");

    return 0;
}