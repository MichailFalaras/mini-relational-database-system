#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/row.h"
#include "../include/schema.h"
#include "../src/schema/schema_utils.h"
#include "../include/constraints.h"
#include "../include/database.h"
#include "../include/table.h"
#include "../include/expressions.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return -1; \
    }

/* Several, if not all, constraints.c functions are being used through
 * schema.c function calls. */

/* ---------- Column & Constraint helpers ---------- */
Column **create_column_array(int amount_columns) {
    Column **columns = (Column **) calloc(amount_columns, sizeof(Column *));
    if (columns == NULL) {
        perror("(helper) create_column_array");
        exit(1);
    }

    return columns;
}

Constraint **create_constraint_array(int amount_constraints) {
    Constraint **constraints = (Constraint **) calloc(amount_constraints, sizeof(Constraint *));
    if (constraints == NULL) {
        perror("(helper) create_constraint_array");
        exit(1);
    }

    return constraints;
}

Table *create_table_component(uint32_t table_num) {

    Table *table = (Table *) calloc(1, sizeof(Table));
    if (table == NULL) {
        perror("(helper) create_table_component");
        exit(1);
    }

    char table_name[64] = {0};
    snprintf(table_name, 64, "table%d", table_num);
    memcpy(table->name, table_name, 64);

    table_num++;
    return table;
}

Database *create_database_component(uint32_t table_count) {
    Database *db = (Database *) calloc(1, sizeof(Database));
    if (db == NULL) {
        perror("(helper) create_database_component");
        exit(1);
    }

    db->table_count = table_count;
    Table **tables = (Table **) calloc(db->table_count, sizeof(Table *));
    if (tables == NULL) {
        perror("(helper) create_database_component");
        exit(1);
    }
    db->tables = tables;

    return db;
}

ExpressionNode **create_expression_node_array(int amount_expressions) {
    ExpressionNode **expr = (ExpressionNode **) calloc(amount_expressions, sizeof(ExpressionNode*));
    if (expr == NULL) {
        perror("create_expression_node_array");
        exit(1);
    }

    return expr;
}

void constraint_array_free(Constraint **constraint_array, int amount_constraints) {
    if (!constraint_array || amount_constraints <= 0) {
        return;
    }

    if (constraint_array) {
        for (int i = 0; i < amount_constraints; i++) {
            if (constraint_array[i]) {
                free(constraint_array[i]);
            }
        } 

        free(constraint_array);
    }
}

void column_array_free(Column **column_array, int amount_columns) {
    if (!column_array || amount_columns <= 0) {
        return;
    }

    if (column_array) {
        for (int i = 0; i < amount_columns; i++) {
            if (column_array[i]) {
                free(column_array[i]);
            }
        } 

        free(column_array);
    }
}

void expression_node_array_free(ExpressionNode **expr_node_array, int amount_expr_nodes) {
    if (!expr_node_array || amount_expr_nodes <= 0) {
        return;
    }

    if (expr_node_array) {
        for (int i = 0; i < amount_expr_nodes; i++) {
            if (expr_node_array[i]) {
                expression_node_free(expr_node_array[i]);
            }
        }

        free(expr_node_array);
    } 
}

void test_database_free(Database *db) {
    if (db != NULL) {
        for (uint32_t i = 0; i < db->table_count; i++) {
            if (db->tables[i] != NULL) {
                free(db->tables[i]);
            }
        }
        free(db->tables);
        free(db);
    }
}

/* ---------- Unit Tests ---------- */
static int test_column_alloc() {
    char *column_name = "Column1";
    DataType type = INTEGER;
    uint32_t not_null_rows = 5;
    uint32_t null_rows = 3;

    Column *column = column_alloc(column_name, type, 0, not_null_rows, null_rows);
    ASSERT(column != NULL);

    free(column);
    return 0;
}

static int test_constraint_alloc() {
    char *constraint_name = "Constraint1";
    ConstraintType type = PRIMARY_KEY;

    Constraint *constraint = constraint_alloc(constraint_name, type);
    ASSERT(constraint != NULL);

    constraint_free(constraint);
    return 0;
}

static int test_schema_create() {
    int num_columns = 1;
    int num_constraints = 1;

    Column *column = column_alloc("id", INTEGER, 0, 5, 3);
    ASSERT(column != NULL);
    Column **columns = create_column_array(num_columns);
    ASSERT(columns != NULL);

    uint32_t column_refs[1];
    column_refs[0] = 0;

    Constraint *constraint = constraint_create_primary_key("Constraint1", column_refs, 1);
    ASSERT(constraint != NULL);
    Constraint **constraints = create_constraint_array(num_constraints);
    ASSERT(constraints != NULL);

    columns[0] = column;
    constraints[0] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    ASSERT(schema != NULL);

    schema_free(schema);
    return 0;
}

static int test_schema_drop_true() {
    int num_columns = 2;
    int num_constraints = 1;

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;

    /* Constraints */
    uint32_t column_refs[1];
    column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", column_refs, 1);
    constraints[0] = constraint;

    /* Needed components/structs */
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    Database *db = create_database_component(1);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[0] = table;

    /* This should work because there aren't tables with foreign keys referencing
    the schema. */
    bool res = schema_can_drop(schema, db);
    ASSERT(res == true);

    test_database_free(db);
    schema_free(schema);
    return 0;
}

static int test_schema_drop_false() {
    int num_columns = 2;
    int num_constraints = 1;

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;

    /* Constraints */
    uint32_t column_refs[1];
    column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", column_refs, 1);
    constraints[0] = constraint;

    /* Needed components/structs */
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    /* Schema2 */
    column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("department", VARCHAR, 64, 5, 0);
    columns[1] = column;

    uint32_t foreign_key_columns[1];
    column_refs[0] = 0;
    uint32_t referenced_columns[1];
    referenced_columns[0] = 0;
    constraint = constraint_create_foreign_keys("Constraint2", foreign_key_columns,
                                 1, "table0", referenced_columns, 1);
    constraints[0] = constraint;

    Schema *schema2 = schema_create(columns, constraints, num_columns, num_constraints);

    Database *db = create_database_component(2);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[0] = table;
    table = create_table_component(1);
    table->table_schema = schema2;
    db->tables[1] = table;

    /* This shouldn't work because there are tables with foreign keys referencing
    the schema. */
    bool res = schema_can_drop(schema, db);
    ASSERT(res == false);

    test_database_free(db);
    schema_free(schema);
    return 0;
}

static int test_schema_find_column_true() {
    int num_columns = 3;
    int num_constraints = 0;
    char *column_name = "name";

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    column = schema_find_column(schema, column_name);
    ASSERT(column != NULL);
    ASSERT(!strcasecmp(column->name, column_name));

    schema_free(schema);
    return 0;
}

static int test_schema_find_column_false() {
    int num_columns = 3;
    int num_constraints = 0;
    char *column_name = "surname";

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    column = schema_find_column(schema, column_name);
    ASSERT(column == NULL);

    schema_free(schema);
    return 0;
}

static int test_schema_find_column_index() {
    int num_columns = 3;
    int num_constraints = 0;
    char *column_name = "name";

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 128, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    int32_t index = schema_find_column_index(schema, column_name);
    ASSERT(index != -1 && index != -2);
    ASSERT(!strcasecmp(columns[index]->name, column_name));

    schema_free(schema);
    return 0;
}

static int test_schema_add_column() {
    int num_columns = 3;
    int num_constraints = 0;

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 128, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    Column *new_column = column_alloc("age", INTEGER, 0, 5, 0);

    bool res = schema_add_column(schema, new_column);
    ASSERT(res == true);
    ASSERT(schema->num_columns == 4);
    ASSERT(schema->columns[schema->num_columns-1] != NULL);
    free(new_column);

    schema_free(schema);
    return 0;
}

static int test_schema_add_duplicate_column() {
    int num_columns = 3;
    int num_constraints = 0;

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 128, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    Column *new_column = column_alloc("name", VARCHAR, 64, 5, 0);

    bool res = schema_add_column(schema, new_column);
    ASSERT(res == false);
    ASSERT(schema->num_columns == 3);
    free(new_column);

    schema_free(schema);
    return 0;
}

static int test_schema_drop_column() {
    int num_columns = 3;
    int num_constraints = 1;

    /* Schema1: 3 columns, 1 constraint */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 128, 5, 0);
    columns[1] = column;
    column = column_alloc("salary", NUMERIC, 0, 5, 0);
    columns[2] = column;

    uint32_t column_refs[1];
    column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", column_refs, 1);
    constraints[0] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    free(columns);

    /* Schema2: 2 columns, 1 constraint */
    columns = create_column_array(--num_columns);
    column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("department", VARCHAR, 64, 5, 0);
    columns[1] = column;

    uint32_t foreign_key_columns[1];
    foreign_key_columns[0] = 1;
    uint32_t referenced_columns[1];
    referenced_columns[0] = 2;
    constraints = create_constraint_array(num_constraints);
    constraint = constraint_create_foreign_keys("Constraint2", foreign_key_columns,
                                 1, "table0", referenced_columns, 1);
    constraints[0] = constraint;

    Schema *schema2 = schema_create(columns, constraints, num_columns, num_constraints);

    Database *db = create_database_component(2);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[1] = table;
    table = create_table_component(1);
    table->table_schema = schema2;
    db->tables[0] = table;

    /* This column should be dropped because no constraint is referencing it.*/
    bool res = schema_drop_column(schema, db, "name");
    ASSERT(res == true);
    ASSERT(schema->num_columns == 2);
    
    /* This should fail because id is a PRIMARY KEY.*/
    res = schema_drop_column(schema, db, "id");
    ASSERT(res == false);
    ASSERT(schema->num_columns == 2);

    /* This should also fail because it is referenced by a FOREIGN KEY. */
    res = schema_drop_column(schema, db, "salary");
    ASSERT(res == false);
    ASSERT(schema->num_columns == 2);

    schema_free(schema);
    return 0;
}

static int test_schema_rename_column() {
    int num_columns = 3;
    int num_constraints = 0;
    char *old_column_name = "name";
    char *new_col_name = "surname";

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    bool res = schema_rename_column(schema, old_column_name, new_col_name);
    ASSERT(res == true);
    ASSERT(!strcasecmp(schema->columns[1]->name, new_col_name));

    schema_free(schema);
    return 0;
}

static int test_schema_modify_column() {
    int num_columns = 3;
    int num_constraints = 0;
    char *old_column_name = "height";
    Column *new_column = column_alloc("department", CHAR, 64, 5, 0);

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 128, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    Constraint **constraints = NULL;
    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    Database *db = create_database_component(1);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[0] = table;

    bool res = schema_modify_column(schema, db, old_column_name, new_column);
    ASSERT(res == true);
    ASSERT(!strcasecmp(schema->columns[2]->name, old_column_name));
    ASSERT(schema->columns[2]->type == CHAR);
    ASSERT(schema->columns[2]->type_parameter == 64);
    ASSERT(schema->columns[2]->non_null_rows == 5);
    ASSERT(schema->columns[2]->null_rows == 0);

    free(new_column);
    test_database_free(db);
    schema_free(schema);
    return 0;
}

static int test_schema_find_constraint_index() {
    int num_columns = 0;
    int num_constraints = 2;
    char *constraint_name = "Constraint2";

    Column **columns = NULL;

    uint32_t primary_key_column_refs[1];
    primary_key_column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", primary_key_column_refs, 1);
    constraints[0] = constraint;

    uint32_t unique_column_refs[3];
    unique_column_refs[0] = 2;
    unique_column_refs[0] = 3;
    unique_column_refs[0] = 4;
    constraint = constraint_create_unique("Constraint2", unique_column_refs, 1);
    constraints[1] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    int32_t res = schema_find_constraint_index(schema, constraint_name);
    ASSERT(res == 1);
    ASSERT(!strcmp(schema->constraints[res]->constraint_name, constraint_name))

    schema_free(schema);
    return 0;
}

static int test_schema_add_constraint_true() {
    int num_columns = 3;
    int num_constraints = 2;
    uint32_t new_constraint_column_ref = 1;
    Constraint *new_constraint = constraint_create_not_null("Constraint3", new_constraint_column_ref);

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 128, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    /* Constraints */
    uint32_t primary_key_column_refs[1];
    primary_key_column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", primary_key_column_refs, 1);
    constraints[0] = constraint;

    uint32_t unique_column_refs[3];
    unique_column_refs[0] = 0;
    unique_column_refs[1] = 1;
    unique_column_refs[2] = 2;
    constraint = constraint_create_unique("Constraint2", unique_column_refs, 1);
    constraints[1] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    Database *db = create_database_component(1);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[0] = table;

    bool res = schema_add_constraint(schema, db, new_constraint);
    ASSERT(res == true);
    ASSERT(!strcmp(schema->constraints[num_constraints]->constraint_name, new_constraint->constraint_name));

    constraint_free(new_constraint);
    test_database_free(db);
    schema_free(schema);
    return 0;
}

static int test_schema_add_constraint_false() {
    int num_columns = 3;
    int num_constraints = 2;
    uint32_t new_constraint_column_ref = 4;
    Constraint *new_constraint = constraint_create_not_null("Constraint3", new_constraint_column_ref);

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    /* Constraints */
    uint32_t primary_key_column_refs[1];
    primary_key_column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", primary_key_column_refs, 1);
    constraints[0] = constraint;

    uint32_t unique_column_refs[3];
    unique_column_refs[0] = 0;
    unique_column_refs[1] = 1;
    unique_column_refs[2] = 2;
    constraint = constraint_create_unique("Constraint2", unique_column_refs, 1);
    constraints[1] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    Database *db = create_database_component(1);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[0] = table;

    bool res = schema_add_constraint(schema, db, new_constraint);
    ASSERT(res == false);

    constraint_free(new_constraint);
    test_database_free(db);
    schema_free(schema);
    return 0;
}

static int test_schema_add_duplicate_constraint() {
    int num_columns = 3;
    int num_constraints = 2;
    uint32_t new_constraint_column_ref = 1;
    Constraint *new_constraint = constraint_create_not_null("Constraint1", new_constraint_column_ref);

    /* Columns */
    Column **columns = create_column_array(num_columns);
    Column *column = column_alloc("id", INTEGER, 0, 5, 0);
    columns[0] = column;
    column = column_alloc("name", VARCHAR, 64, 5, 0);
    columns[1] = column;
    column = column_alloc("height", NUMERIC, 0, 5, 0);
    columns[2] = column;

    /* Constraints */
    uint32_t primary_key_column_refs[1];
    primary_key_column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", primary_key_column_refs, 1);
    constraints[0] = constraint;

    uint32_t unique_column_refs[3];
    unique_column_refs[0] = 0;
    unique_column_refs[1] = 1;
    unique_column_refs[2] = 2;
    constraint = constraint_create_unique("Constraint2", unique_column_refs, 1);
    constraints[1] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);
    Database *db = create_database_component(1);
    Table *table = create_table_component(0);
    table->table_schema = schema;
    db->tables[0] = table;

    bool res = schema_add_constraint(schema, db, new_constraint);
    ASSERT(res == false);

    constraint_free(new_constraint);
    test_database_free(db);
    schema_free(schema);
    return 0;
}

static int test_schema_drop_constraint() {
    int num_columns = 0;
    int num_constraints = 2;
    char *constraint_name = "Constraint1";

    Column **columns = NULL;

    /* Constraints */
    uint32_t primary_key_column_refs[1];
    primary_key_column_refs[0] = 0;
    Constraint **constraints = create_constraint_array(num_constraints);
    Constraint *constraint = constraint_create_primary_key("Constraint1", primary_key_column_refs, 1);
    constraints[0] = constraint;

    uint32_t unique_column_refs[3];
    unique_column_refs[0] = 0;
    unique_column_refs[1] = 1;
    unique_column_refs[2] = 2;
    constraint = constraint_create_unique("Constraint2", unique_column_refs, 1);
    constraints[1] = constraint;

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    bool res = schema_drop_constraint(schema, constraint_name);
    ASSERT(res == true);
    ASSERT(schema->num_constraints == 1);

    schema_free(schema);
    return 0;
}

static int test_schema_validate_row() {
    int num_columns = 3;
    int num_constraints = 2;

    Column **columns = create_column_array(num_columns);
    if (!columns) { return -1; }

    columns[0] = column_alloc("id", INTEGER, 0, 5, 0);
    if (!columns[0]) { return -1; }
    columns[1] = column_alloc("name", VARCHAR, 64, 5, 0);;
    if (!columns[1]) { return -1; }
    columns[2] = column_alloc("salary", NUMERIC, 0, 5, 0);;
    if (!columns[2]) { return -1; }

    Constraint **constraints = create_constraint_array(num_constraints);
    if (!constraints) { return -1; }

    uint32_t column_refs[] = {0};
    uint32_t num_column_refs = 1;
    constraints[0] = constraint_create_primary_key("PRIMARY_KEY", column_refs, num_column_refs);
    if (!constraints[0]) { return -1; } 
    constraints[1] = constraint_create_not_null("NOT_NULL_CONSTRAINT", 1);
    if (!constraints[1]) { return -1; }

    Schema *schema = schema_create(columns, constraints, num_columns, num_constraints);

    ExpressionNode **expressions = create_expression_node_array(3);
    if (!expressions) { return -1; }

    int id = 0;
    expressions[0] = expression_node_create(EXPR_LITERAL);
    expressions[0]->expression_data.literal_value.literal = value_create(INTEGER, &id);
    if (!expressions[0]->expression_data.literal_value.literal) { return -1; }

    varchar_n_t name = {0};
    name.max_n = 64;
    name.string = "George";
    expressions[1] = expression_node_create(EXPR_LITERAL);
    expressions[1]->expression_data.literal_value.literal = value_create(VARCHAR, &name);
    if (!expressions[1]->expression_data.literal_value.literal) { return -1; }
    Value *reset_varchar = expressions[1]->expression_data.literal_value.literal;


    numeric_t salary = {0};
    salary.val  = 1500000;
    salary.scale = 2;
    expressions[2] = expression_node_create(EXPR_LITERAL);
    expressions[2]->expression_data.literal_value.literal = value_create(NUMERIC, &salary);
    if (!expressions[2]->expression_data.literal_value.literal) { return -1; }

    Row *row = row_create(expressions, 3);
    if (!row) { return -1; }

    /* --- VALID ROW --- */
    EvaluationContext eval_ctx = {0}; // Only used for CHECK constraint
    ASSERT(schema_validate_row(schema, row, &eval_ctx));

    /* --- ROW COLUMNS DATA TYPE MISMATCH --- */
    bool boolean_val = true;
    expressions[1]->expression_data.literal_value.literal = value_create(BOOL, &boolean_val);
    if (!expressions[1]->expression_data.literal_value.literal) { return -1; }

    row_free(row);
    row = row_create(expressions, 3);
    if (!row) { return -1; }

    ASSERT(!schema_validate_row(schema, row, &eval_ctx));

    /* --- PRIMARY KEY & NOT NULL CONSTRAINT VIOLATION --- */
    value_free(expressions[1]->expression_data.literal_value.literal);
    expressions[1]->expression_data.literal_value.literal = value_create_null(VARCHAR);
    if (!expressions[1]->expression_data.literal_value.literal) { return -1; }

    row_free(row);
    row = row_create(expressions, 3);
    if (!row) { return -1; }

    ASSERT(!schema_validate_row(schema, row, &eval_ctx));

    value_free(expressions[1]->expression_data.literal_value.literal);
    expressions[1]->expression_data.literal_value.literal = reset_varchar;

    value_free(expressions[0]->expression_data.literal_value.literal);
    expressions[0]->expression_data.literal_value.literal = value_create_null(INTEGER);
    if (!expressions[0]->expression_data.literal_value.literal) { return -1; }

    row_free(row);
    row = row_create(expressions, 3);
    if (!row) { return -1; }

    ASSERT(!schema_validate_row(schema, row, &eval_ctx));
    
    column_array_free(columns, num_columns);
    constraint_array_free(constraints, num_constraints);
    expression_node_array_free(expressions, num_columns);
    row_free(row);
    schema_free(schema);
    return 0;
}

void generate_output(int result, int test_num, char *test_desc) {
    int space = 64 - strlen(test_desc);
    char *result_str = !result ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main (int argc, char *argv[]) {
    int result;
    printf("> STARTING TESTS\n");

    result = test_column_alloc();
    generate_output(result, 0, "test_column_alloc");
    result = test_constraint_alloc();
    generate_output(result, 1, "test_constraint_alloc");
    result = test_schema_create();
    generate_output(result, 2, "test_schema_create");
    result = test_schema_drop_true();
    generate_output(result, 3, "test_schema_drop_true");
    result = test_schema_drop_false();
    generate_output(result, 4, "test_schema_drop_false");
    result = test_schema_find_column_true();
    generate_output(result, 5, "test_schema_find_column_true");
    result = test_schema_find_column_false();
    generate_output(result, 6, "test_schema_find_column_false");
    result = test_schema_find_column_index();
    generate_output(result, 7, "test_schema_find_column_index");
    result = test_schema_add_column();
    generate_output(result, 8, "test_schema_add_column");
    result = test_schema_add_duplicate_column();
    generate_output(result, 9, "test_schema_add_duplicate_column");
    result = test_schema_drop_column();
    generate_output(result, 10, "test_schema_drop_column");
    result = test_schema_rename_column();
    generate_output(result, 11, "test_schema_rename_column");
    result = test_schema_modify_column();
    generate_output(result, 12, "test_schema_modify_column");
    result = test_schema_find_constraint_index();
    generate_output(result, 13, "test_schema_find_constraint_index");
    result = test_schema_add_constraint_true();
    generate_output(result, 14, "test_schema_add_constraint_true");
    result = test_schema_add_constraint_false();
    generate_output(result, 15, "test_schema_add_constraint_false");
    result = test_schema_add_duplicate_constraint();
    generate_output(result, 16, "test_schema_add_duplicate_constraint");
    result = test_schema_drop_constraint();
    generate_output(result, 17, "test_schema_drop_constraint");
    result = test_schema_validate_row();
    generate_output(result, 18, "test_schema_validate_row");

    printf("> TESTS RAN SUCCESSFULLY\n");
    return 0;
}