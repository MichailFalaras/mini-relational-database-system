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
#include "../include/ast.h"
#include "../include/tokenizer.h"
#include "../include/parser.h"
#include "../include/constraints.h"
#include "../include/index.h"
#include "../src/data_types/data_types_utils.h"


#define ASSERT(condition) \
    if (!(condition)) { \
        goto cleanup;  \
    }


/* ---------- Helpers ---------- */

typedef struct ddl_binder_test_fixture {
    Database db;

    Table tables[3];
    Table *table_ptrs[3];

    Schema schemas[3];

    Column employee_columns[4];
    Column department_columns[3];
    Column department_locations_columns[2];

    Column *employee_column_ptrs[4];
    Column *department_column_ptrs[3];
    Column *department_locations_columns_ptrs[2];

    // Existing constraints for FK target validation
    Constraint department_pk;
    Constraint department_unique;
    Constraint *department_constraint_ptrs[2];

    uint32_t department_pk_columns[1];
    uint32_t department_unique_columns[1];

    // Composite PRIMARY KEY for department_locations
    Constraint department_locations_pk;
    Constraint *department_locations_constraint_ptrs[1];
    uint32_t department_locations_pk_columns[2];

    // Existing index for DROP/CREATE INDEX binding
    Index employee_name_index;
    IndexKey employee_name_index_key;
    uint32_t employee_name_index_columns[1];
    Index *employee_secondary_indexes[1];

    // Canonical indexes
    Index canonical_indexes[3];
    IndexKey canonical_index_keys[3];

    uint32_t employee_canonical_columns[1];
    uint32_t department_canonical_columns[1];
    uint32_t department_locations_canonical_columns[2];

    BindingContext context;
} DDLBinderTestFixture;


// Initialize a Column structure
static void init_ddl_test_column(Column *column, const char *name, DataType type, DataTypeArgs args) {
    memset(column, 0, sizeof(*column));

    snprintf(column->name, sizeof(column->name), "%s", name);

    column->type = type;
    column->type_args = args;
    column->serialized_size = get_serialized_value_size(type, args);
}

// Initialize DDL binder fixture
static void init_ddl_binder_fixture(DDLBinderTestFixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));

    // Employees 
    init_ddl_test_column(&fixture->employee_columns[0], "id", INTEGER, (DataTypeArgs){0});
    init_ddl_test_column(&fixture->employee_columns[1], "name", VARCHAR, (DataTypeArgs){.length = 64});
    init_ddl_test_column(&fixture->employee_columns[2], "salary", DOUBLE, (DataTypeArgs){0});
    init_ddl_test_column(&fixture->employee_columns[3], "department_id", INTEGER, (DataTypeArgs){0});

    for (uint32_t i = 0; i < 4; i++) {
        fixture->employee_column_ptrs[i] = &fixture->employee_columns[i];
    }

    fixture->schemas[0].columns = fixture->employee_column_ptrs;
    fixture->schemas[0].num_columns = 4;

    // Departments
    init_ddl_test_column(&fixture->department_columns[0], "id", INTEGER, (DataTypeArgs){0});
    init_ddl_test_column(&fixture->department_columns[1], "name", VARCHAR, (DataTypeArgs){.length = 64});
    init_ddl_test_column(&fixture->department_columns[2], "budget", DOUBLE, (DataTypeArgs){0});

    for (uint32_t i = 0; i < 3; i++) {
        fixture->department_column_ptrs[i] = &fixture->department_columns[i];
    }

    fixture->schemas[1].columns = fixture->department_column_ptrs;
    fixture->schemas[1].num_columns = 3;

    // Department PRIMARY KEY 
    fixture->department_pk.type = PRIMARY_KEY;

    snprintf(
        fixture->department_pk.constraint_name,
        sizeof(fixture->department_pk.constraint_name),
        "%s",
        "pk_departments"
    );

    fixture->department_pk_columns[0] = 0;
    fixture->department_pk.constraint_data.primary_key.amount_columns = 1;
    fixture->department_pk.constraint_data.primary_key.primary_key_columns = fixture->department_pk_columns;

    // Department UNIQUE
    fixture->department_unique.type = UNIQUE;

    snprintf(
        fixture->department_unique.constraint_name,
        sizeof(fixture->department_unique.constraint_name),
        "%s",
        "uq_departments_name"
    );

    fixture->department_unique_columns[0] = 1;
    fixture->department_unique.constraint_data.unique_cols.amount_columns = 1;
    fixture->department_unique.constraint_data.unique_cols.column_refs = fixture->department_unique_columns;

    fixture->department_constraint_ptrs[0] = &fixture->department_pk;
    fixture->department_constraint_ptrs[1] = &fixture->department_unique;

    fixture->schemas[1].constraints = fixture->department_constraint_ptrs;
    fixture->schemas[1].num_constraints = 2;

    // Department Locations
    init_ddl_test_column(&fixture->department_locations_columns[0], "department_id", INTEGER, (DataTypeArgs){0});
    init_ddl_test_column(&fixture->department_locations_columns[1], "location_id", INTEGER, (DataTypeArgs){0});

    for (uint32_t i = 0; i < 2; i++) {
        fixture->department_locations_columns_ptrs[i] = &fixture->department_locations_columns[i];
    }

    fixture->schemas[2].columns = fixture->department_locations_columns_ptrs;
    fixture->schemas[2].num_columns = 2;

    // Department Locations PRIMARY KEY
    fixture->department_locations_pk.type = PRIMARY_KEY;

    snprintf(
        fixture->department_locations_pk.constraint_name,
        sizeof(fixture->department_locations_pk.constraint_name),
        "%s",
        "pk_department_locations"
    );

    // Composite primary key:
    // PRIMARY KEY (department_id, location_id)
    fixture->department_locations_pk_columns[0] = 0;
    fixture->department_locations_pk_columns[1] = 1;

    fixture->department_locations_pk.constraint_data.primary_key.amount_columns = 2;

    fixture->department_locations_pk.constraint_data.primary_key.primary_key_columns =
        fixture->department_locations_pk_columns;

    // Register constraint within the schema
    fixture->department_locations_constraint_ptrs[0] = &fixture->department_locations_pk;
    fixture->schemas[2].constraints = fixture->department_locations_constraint_ptrs;
    fixture->schemas[2].num_constraints = 1;

    // Tables
    snprintf(
        fixture->tables[0].name,
        sizeof(fixture->tables[0].name),
        "%s",
        "employees"
    );

    snprintf(
        fixture->tables[1].name,
        sizeof(fixture->tables[1].name),
        "%s",
        "departments"
    );

    snprintf(
        fixture->tables[2].name,
        sizeof(fixture->tables[2].name),
        "%s",
        "department_locations"
    );

    fixture->tables[0].table_schema = &fixture->schemas[0];
    fixture->tables[1].table_schema = &fixture->schemas[1];
    fixture->tables[2].table_schema = &fixture->schemas[2];

    // Employees canonical index: id
    fixture->employee_canonical_columns[0] = 0;

    fixture->canonical_index_keys[0].num_columns = 1;
    fixture->canonical_index_keys[0].column_index_array = fixture->employee_canonical_columns;

    fixture->canonical_indexes[0].key = &fixture->canonical_index_keys[0];
    fixture->tables[0].primary_index = &fixture->canonical_indexes[0];

    // Departments canonical index: id
    fixture->department_canonical_columns[0] = 0;

    fixture->canonical_index_keys[1].num_columns = 1;
    fixture->canonical_index_keys[1].column_index_array = fixture->department_canonical_columns;

    fixture->canonical_indexes[1].key = &fixture->canonical_index_keys[1];
    fixture->tables[1].primary_index = &fixture->canonical_indexes[1];

    // Department locations canonical index:
    // (department_id, location_id)
    fixture->department_locations_canonical_columns[0] = 0;
    fixture->department_locations_canonical_columns[1] = 1;

    fixture->canonical_index_keys[2].num_columns = 2;
    fixture->canonical_index_keys[2].column_index_array = fixture->department_locations_canonical_columns;
    
    fixture->canonical_indexes[2].key = &fixture->canonical_index_keys[2];
    fixture->tables[2].primary_index = &fixture->canonical_indexes[2];
 
    // Existing Secondary Index
    snprintf(
        fixture->employee_name_index.name,
        sizeof(fixture->employee_name_index.name),
        "%s",
        "idx_employees_name"
    );

    fixture->employee_name_index_columns[0] = 1;
    fixture->employee_name_index_key.num_columns = 1;
    fixture->employee_name_index_key.column_index_array = fixture->employee_name_index_columns;
    fixture->employee_name_index.key = &fixture->employee_name_index_key;
    fixture->employee_name_index.type = SECONDARY_INDEX;

    fixture->employee_secondary_indexes[0] = &fixture->employee_name_index;
    fixture->tables[0].secondary_indexes = fixture->employee_secondary_indexes;
    fixture->tables[0].total_secondary_indexes = 1;

    // Database Registry
    fixture->table_ptrs[0] = &fixture->tables[0];
    fixture->table_ptrs[1] = &fixture->tables[1];
    fixture->table_ptrs[2] = &fixture->tables[2];

    fixture->db.tables = fixture->table_ptrs;
    fixture->db.table_count = 3;

    // Binding Context
    fixture->context.db = &fixture->db;

    fixture->context.relations = NULL;
    fixture->context.num_relations = 0;

    fixture->context.aggregate_depth = 0;
    fixture->context.current_clause = BIND_CLAUSE_SELECT;
}

static void cleanup_ddl_binder_fixture(DDLBinderTestFixture *fixture) {
    if (!fixture) {
        return;
    }

    free(fixture->context.relations);

    fixture->context.relations = NULL;
    fixture->context.num_relations = 0;
}


/* ---------- Parsing Helpers ---------- */

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

typedef struct test_token {
    const char *value;
    TokenType type;
} TestToken;

// Create a parser from manually specified tokens
static Parser *create_test_parser(const TestToken *tokens, uint32_t count) {
    if (!tokens || count == 0) return NULL;

    TokenArray *token_array = token_array_create();

    if (!token_array) {
        return NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        size_t length = strlen(tokens[i].value) + 1;
        char *token_str = (char *) malloc(length);
        if (!token_str) {
            token_array_free(token_array);
            return NULL;
        }
        memcpy(token_str, tokens[i].value, length);

        Token *token = token_create(token_str, tokens[i].type);
        if (!token) {
            free(token_str);
            token_array_free(token_array);
            return NULL;
        }
        token_array_push(token_array, token);
    }

    Parser *parser = parser_init(token_array);

    if (!parser) {
        token_array_free(token_array);
    }

    return parser;
}

// Free parser and AST resources
static void free_test_parser(Parser *parser, ASTNode *root) {
    if (root) {
        ast_free_node(root);
    }

    if (parser) {
        parser_free(parser);
    }
}

/* ---------- bind_create_table unit tests ---------- */

int test_bind_create_table_basic() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, 
        {"64", NUMBER}, {")", PUNCTUATION}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_all_data_types() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"data_types_test", IDENTIFIER}, {"(", PUNCTUATION},
        {"integer_col", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"unsigned_col", IDENTIFIER}, {"UNSIGNED", KEYWORD}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"numeric_col", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, {"2", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"float_col", IDENTIFIER}, {"FLOAT", KEYWORD}, {",", PUNCTUATION},
        {"double_col", IDENTIFIER}, {"DOUBLE", KEYWORD}, {",", PUNCTUATION},
        {"char_col", IDENTIFIER}, {"CHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"16", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"varchar_col", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"text_col", IDENTIFIER}, {"TEXT", KEYWORD}, {",", PUNCTUATION},
        {"date_col", IDENTIFIER}, {"DATE", KEYWORD}, {",", PUNCTUATION},
        {"timestamp_col", IDENTIFIER}, {"TIMESTAMP", KEYWORD}, {",", PUNCTUATION},
        {"bool_col", IDENTIFIER}, {"BOOL", KEYWORD}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_parameterized_types() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"code", IDENTIFIER}, {"CHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"12", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"description", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"255", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"price", IDENTIFIER}, {"NUMERIC", KEYWORD}, {"(", PUNCTUATION}, {"12", NUMBER}, 
        {",", PUNCTUATION}, {"3", NUMBER}, {")", PUNCTUATION}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_duplicate_name() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"employees", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_duplicate_columns() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION}, 
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_type_args() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_ddl_binder_fixture(&binder);

    // CHAR(0) invalid case
    TestToken tokens_1[] = {
        {"CREATE TABLE", KEYWORD}, {"invalid_char", IDENTIFIER}, {"(", PUNCTUATION},
        {"code", IDENTIFIER}, {"CHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_1, ARRAY_SIZE(tokens_1));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    // VARCHAR(0) invalid case
    TestToken tokens_2[] = {
        {"CREATE TABLE", KEYWORD}, {"invalid_varchar", IDENTIFIER}, {"(", PUNCTUATION},
        {"code", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_2, ARRAY_SIZE(tokens_2));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    // NUMERIC(0,0) invalid case
    TestToken tokens_3[] = {
        {"CREATE TABLE", KEYWORD}, {"invalid_numeric", IDENTIFIER}, {"(", PUNCTUATION},
        {"amount", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"0", NUMBER}, {",", PUNCTUATION}, {"0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_3, ARRAY_SIZE(tokens_3));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    // NUMERIC(5,8) invalid scale > precision
    TestToken tokens_4[] = {
        {"CREATE TABLE", KEYWORD}, {"invalid_numeric", IDENTIFIER}, {"(", PUNCTUATION},
        {"amount", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"5", NUMBER}, {",", PUNCTUATION}, {"8", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_4, ARRAY_SIZE(tokens_4));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_column_primary_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {",", PUNCTUATION}, 
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);

    ColumnDefNode *id_column_def = create_table->columns->column_defs[0];
    ASSERT(id_column_def != NULL);

    PrimaryKeyConstraintNode *pk = &id_column_def->constraints[0]->constraint_data.primary_key;
    ASSERT(pk != NULL);

    ASSERT(pk->column_refs != NULL);
    ASSERT(pk->num_columns == 1);

    ASSERT(pk->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[0]->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_composite_primary_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"assignments", IDENTIFIER}, {"(", PUNCTUATION},
        {"employee_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION}, 
        {"project_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION}, 
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"employee_id", IDENTIFIER}, {",", PUNCTUATION},
        {"project_id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);

    PrimaryKeyConstraintNode *pk = &create_table->constraints[0]->constraint_data.primary_key;
    ASSERT(pk != NULL);

    ASSERT(pk->column_refs != NULL);
    ASSERT(pk->num_columns == 2);

    ASSERT(pk->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[0]->expression_data.column_value.column_index == 0);

    ASSERT(pk->column_refs[1]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[1]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[1]->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_multiple_primary_keys() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, 
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {",", PUNCTUATION}, 
        {"code", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION}, 
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"code", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_unique_constraints() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {",", PUNCTUATION}, 
        {"email", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"128", NUMBER}, {")", PUNCTUATION}, {"UNIQUE", KEYWORD}, {",", PUNCTUATION},
        {"first_name", IDENTIFIER}, {"VARCHAR", KEYWORD},
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"last_name", IDENTIFIER}, {"VARCHAR", KEYWORD},
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"UNIQUE", KEYWORD}, {"(", PUNCTUATION}, 
        {"first_name", IDENTIFIER}, {",", PUNCTUATION}, {"last_name", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);

    // Column-level PRIMARY KEY
    ColumnDefNode *id_column_def = create_table->columns->column_defs[0]; 
    ASSERT(id_column_def != NULL);

    PrimaryKeyConstraintNode *pk = &id_column_def->constraints[0]->constraint_data.primary_key;
    ASSERT(pk != NULL);

    ASSERT(pk->column_refs != NULL);
    ASSERT(pk->num_columns == 1);

    ASSERT(pk->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[0]->expression_data.column_value.column_index == 0);

    // Column-level UNIQUE 
    ColumnDefNode *email_column_def = create_table->columns->column_defs[1]; 
    ASSERT(email_column_def != NULL);
    
    UniqueConstraintNode *unique = &email_column_def->constraints[0]->constraint_data.unique;
    ASSERT(unique != NULL);

    ASSERT(unique->column_refs != NULL);
    ASSERT(unique->num_columns == 1);
    
    ASSERT(unique->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(unique->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(unique->column_refs[0]->expression_data.column_value.column_index == 1);

    // Table-level UNIQUE
    ASSERT(create_table->num_constraints == 1);

    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_UNIQUE);

    UniqueConstraintNode *uq = &constraint->constraint_data.unique;
    ASSERT(uq != NULL);

    ASSERT(uq->column_refs != NULL);
    ASSERT(uq->num_columns == 2);

    ASSERT(uq->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(uq->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(uq->column_refs[0]->expression_data.column_value.column_index == 2);

    ASSERT(uq->column_refs[1]->type == EXPR_COLUMN_REF);
    ASSERT(uq->column_refs[1]->expression_data.column_value.relation_index == 0);
    ASSERT(uq->column_refs[1]->expression_data.column_value.column_index == 3);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_duplicate_constraint_names() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"code", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"CONSTRAINT", KEYWORD}, {"uq_project", IDENTIFIER}, {"UNIQUE", KEYWORD},
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"CONSTRAINT", KEYWORD}, {"uq_project", IDENTIFIER}, {"UNIQUE", KEYWORD},
        {"(", PUNCTUATION}, {"code", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_not_null() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, 
        {"NOT", KEYWORD}, {"NULL", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, 
        {"NOT", KEYWORD}, {"NULL", KEYWORD}, {",", PUNCTUATION},
        {"budget", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"12", NUMBER}, {",", PUNCTUATION}, {"2", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);

    // id NOT NULL 
    ColumnDefNode *id_column_def = create_table->columns->column_defs[0]; 
    ASSERT(id_column_def != NULL);

    NotNullConstraintNode *id_not_null = &id_column_def->constraints[0]->constraint_data.not_null;
    ASSERT(id_not_null != NULL);

    ASSERT(id_not_null->column_ref != NULL);
    ASSERT(id_not_null->column_ref->type == EXPR_COLUMN_REF);
    ASSERT(id_not_null->column_ref->expression_data.column_value.relation_index == 0);
    ASSERT(id_not_null->column_ref->expression_data.column_value.column_index == 0);

    // name NOT NULL
    ColumnDefNode *name_column_def = create_table->columns->column_defs[1]; 
    ASSERT(name_column_def != NULL);

    NotNullConstraintNode *name_not_null = &name_column_def->constraints[0]->constraint_data.not_null;
    ASSERT(name_not_null != NULL);

    ASSERT(name_not_null->column_ref != NULL);
    ASSERT(name_not_null->column_ref->type == EXPR_COLUMN_REF);
    ASSERT(name_not_null->column_ref->expression_data.column_value.relation_index == 0);
    ASSERT(name_not_null->column_ref->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_check() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"price", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, {"2", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"quantity", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, {"price", IDENTIFIER}, {">", OPERATOR}, {"0", NUMBER},
        {"AND", OPERATOR}, {"quantity", IDENTIFIER}, {">=", OPERATOR}, {"0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->num_constraints == 1);

    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_CHECK);

    CheckConstraintNode *check = &constraint->constraint_data.check;
    ASSERT(check != NULL);
    ASSERT(check->check_expr != NULL);
    ASSERT(check->check_expr->type == EXPR_BINARY);
    ASSERT(check->check_expr->expression_data.binary_expr.op == OP_AND);

    ExpressionNode *left_expr = check->check_expr->expression_data.binary_expr.left_operand;
    ASSERT(left_expr != NULL);
    ASSERT(left_expr->type == EXPR_BINARY);

    ExpressionNode *price_expr = left_expr->expression_data.binary_expr.left_operand;
    ASSERT(price_expr != NULL);
    ASSERT(price_expr->type == EXPR_COLUMN_REF);
    ASSERT(price_expr->expression_data.column_value.relation_index == 0);
    ASSERT(price_expr->expression_data.column_value.column_index == 1);
    
    ExpressionNode *right_expr = check->check_expr->expression_data.binary_expr.right_operand;
    ASSERT(right_expr != NULL);
    ASSERT(right_expr->type == EXPR_BINARY);

    ExpressionNode *quantity_expr = right_expr->expression_data.binary_expr.left_operand;
    ASSERT(quantity_expr != NULL);
    ASSERT(quantity_expr->type == EXPR_COLUMN_REF);
    ASSERT(quantity_expr->expression_data.column_value.relation_index == 0);
    ASSERT(quantity_expr->expression_data.column_value.column_index == 2);
    
    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_check() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_ddl_binder_fixture(&binder);

    TestToken tokens_1[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"price", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, {"2", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, 
        {"price", IDENTIFIER}, {"+", OPERATOR}, {"100", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_1, ARRAY_SIZE(tokens_1));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    TestToken tokens_2[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, 
        {"unknown_column", IDENTIFIER}, {">", OPERATOR}, {"0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_2, ARRAY_SIZE(tokens_2));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_default_literal() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"quantity", IDENTIFIER}, {"INTEGER", KEYWORD}, {"DEFAULT", KEYWORD}, {"10", NUMBER}, {",", PUNCTUATION},
        {"label", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {"DEFAULT", KEYWORD}, {"'Unknown'", STRING},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));
    
    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns->num_column_defs == 3);

    // Column-level quantity constraint
    ColumnDefNode *quantity_column_def = create_table->columns->column_defs[1];
    ASSERT(quantity_column_def != NULL);
    ASSERT(quantity_column_def->num_constraints == 1);
    ASSERT(quantity_column_def->constraints[0]->type == AST_CONSTRAINT_DEFAULT);

    DefaultConstraintNode *default_quantity = &quantity_column_def->constraints[0]->constraint_data.default_value;
    ASSERT(default_quantity != NULL);
    ASSERT(default_quantity->default_expr != NULL);
    ASSERT(default_quantity->default_expr->type == EXPR_LITERAL);
    ASSERT(!default_quantity->default_expr->expression_data.literal_value.is_untyped_null);
    ASSERT(default_quantity->default_expr->expression_data.literal_value.literal->type == UNSIGNED_INTEGER);
    ASSERT(default_quantity->default_expr->expression_data.literal_value.literal->value.uint32_val == 10);

    // Column-level label constraint
    ColumnDefNode *label_column_def = create_table->columns->column_defs[2];
    ASSERT(label_column_def != NULL);
    ASSERT(label_column_def->num_constraints == 1);
    ASSERT(label_column_def->constraints[0]->type == AST_CONSTRAINT_DEFAULT);

    DefaultConstraintNode *default_label = &label_column_def->constraints[0]->constraint_data.default_value;
    ASSERT(default_label != NULL);
    ASSERT(default_label->default_expr != NULL);
    ASSERT(default_label->default_expr->type == EXPR_LITERAL);
    ASSERT(!default_label->default_expr->expression_data.literal_value.is_untyped_null);
    ASSERT(default_label->default_expr->expression_data.literal_value.literal->type == CHAR);
    
    ASSERT(
        !strcmp(
            default_label->default_expr->expression_data.literal_value.literal->value.varchar_val.string,
            "Unknown"
        )
    ); 

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_default_expression() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"quantity", IDENTIFIER}, {"INTEGER", KEYWORD}, {"DEFAULT", KEYWORD},
        {"(", PUNCTUATION}, {"10", NUMBER}, {"+", OPERATOR}, {"5", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"price", IDENTIFIER}, {"NUMERIC", KEYWORD}, 
        {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, {"2", NUMBER}, {")", PUNCTUATION},
        {"DEFAULT", KEYWORD},
        {"(", PUNCTUATION}, {"100.0", NUMBER}, {"+", OPERATOR}, {"25.0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));
    
    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns->num_column_defs == 3);

    // Column-level quantity constraint
    ColumnDefNode *quantity_column_def = create_table->columns->column_defs[1];
    ASSERT(quantity_column_def != NULL);
    ASSERT(quantity_column_def->num_constraints == 1);
    ASSERT(quantity_column_def->constraints[0]->type == AST_CONSTRAINT_DEFAULT);

    DefaultConstraintNode *default_quantity = &quantity_column_def->constraints[0]->constraint_data.default_value;
    ASSERT(default_quantity != NULL);
    ASSERT(default_quantity->default_expr != NULL);
    ASSERT(default_quantity->default_expr->type == EXPR_BINARY);
    ASSERT(default_quantity->default_expr->expression_data.binary_expr.op == OP_ADD);

    // Column-level price constraint
    ColumnDefNode *price_column_def = create_table->columns->column_defs[2];
    ASSERT(price_column_def != NULL);
    ASSERT(price_column_def->num_constraints == 1);
    ASSERT(price_column_def->constraints[0]->type == AST_CONSTRAINT_DEFAULT);

    DefaultConstraintNode *default_price = &price_column_def->constraints[0]->constraint_data.default_value;
    ASSERT(default_price != NULL);
    ASSERT(default_price->default_expr != NULL);
    ASSERT(default_price->default_expr->type == EXPR_BINARY);
    ASSERT(default_price->default_expr->expression_data.binary_expr.op == OP_ADD);
    

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_incompatible_default() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"price", IDENTIFIER}, {"DOUBLE", KEYWORD}, {"DEFAULT", KEYWORD}, {"'invalid'", STRING},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_duplicate_defaults() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"quantity", IDENTIFIER}, {"INTEGER", KEYWORD}, 
        {"DEFAULT", KEYWORD}, {"10", NUMBER}, {"DEFAULT", KEYWORD}, {"20", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_default_null() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"description", IDENTIFIER}, {"VARCHAR", KEYWORD}, {"(", PUNCTUATION},
        {"64", NUMBER}, {")", PUNCTUATION}, {"DEFAULT", KEYWORD}, {"NULL", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns != NULL);
    ASSERT(create_table->columns->num_column_defs == 2);

    // Column-level description constraint
    ColumnDefNode *desc_column_def = create_table->columns->column_defs[1];
    ASSERT(desc_column_def != NULL);
    ASSERT(desc_column_def->num_constraints == 1);

    ConstraintNode *desc_constraint = desc_column_def->constraints[0];
    ASSERT(desc_constraint != NULL);
    ASSERT(desc_constraint->type == AST_CONSTRAINT_DEFAULT);
    
    DefaultConstraintNode *default_desc = &desc_constraint->constraint_data.default_value;
    ASSERT(default_desc != NULL);
    ASSERT(default_desc->default_expr != NULL);
    ASSERT(default_desc->default_expr->type == EXPR_LITERAL);
    ASSERT(default_desc->default_expr->expression_data.literal_value.is_untyped_null);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_default_null_not_null() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"64", NUMBER},
        {")", PUNCTUATION}, {"NOT", KEYWORD}, {"NULL", KEYWORD},
        {"DEFAULT", KEYWORD}, {"NULL", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_default_null_primary_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_ddl_binder_fixture(&binder);

    TestToken tokens_1[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD},
        {"DEFAULT", KEYWORD}, {"NULL", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_1, ARRAY_SIZE(tokens_1));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    TestToken tokens_2[] = {
        {"CREATE TABLE", KEYWORD}, {"products", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"DEFAULT", KEYWORD}, {"NULL", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD},
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_2, ARRAY_SIZE(tokens_2));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_foreign_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"new_employees", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, 
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"department_id", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER},
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns != NULL);
    ASSERT(create_table->columns->num_column_defs == 2);

    // Column-level id PRIMARY KEY
    ColumnDefNode *id_column_def = create_table->columns->column_defs[0];
    ASSERT(id_column_def != NULL);
    ASSERT(id_column_def->num_constraints == 1);
    ASSERT(id_column_def->constraints != NULL);
    ASSERT(id_column_def->constraints[0] != NULL);
    ASSERT(id_column_def->constraints[0]->type == AST_CONSTRAINT_PRIMARY_KEY);

    // Table-level FOREIGN KEY
    ASSERT(create_table->num_constraints == 1);
    ASSERT(create_table->constraints != NULL);

    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_FOREIGN_KEY);

    ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;
    ASSERT(fk != NULL);
    ASSERT(fk->num_local_columns == 1);
    ASSERT(fk->num_referenced_columns == 1);

    ASSERT(fk->local_column_refs != NULL && fk->local_column_refs[0] != NULL);
    ASSERT(fk->referenced_column_refs != NULL && fk->referenced_column_refs[0] != NULL);

    ExpressionNode *local_column_ref = fk->local_column_refs[0];
    ExpressionNode *ref_column_ref = fk->referenced_column_refs[0];

    ASSERT(local_column_ref != NULL);
    ASSERT(local_column_ref->type == EXPR_COLUMN_REF);
    ASSERT(local_column_ref->expression_data.column_value.relation_index == 0);
    ASSERT(local_column_ref->expression_data.column_value.column_index == 1);
    
    ASSERT(ref_column_ref != NULL);
    ASSERT(ref_column_ref->type == EXPR_COLUMN_REF);
    ASSERT(ref_column_ref->expression_data.column_value.relation_index == 0);
    ASSERT(ref_column_ref->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_foreign_key_unique() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"department_aliases", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_name", IDENTIFIER}, {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"64", NUMBER},
        {")", PUNCTUATION}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, {"(", PUNCTUATION}, {"department_name", IDENTIFIER},
        {")", PUNCTUATION}, {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns != NULL);
    ASSERT(create_table->columns->num_column_defs == 2);

    // Table-level FOREIGN KEY
    ASSERT(create_table->num_constraints == 1);
    ASSERT(create_table->constraints != NULL);

    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_FOREIGN_KEY);

    ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;
    ASSERT(fk != NULL);
    ASSERT(fk->num_local_columns == 1);
    ASSERT(fk->num_referenced_columns == 1);

    ASSERT(fk->local_column_refs != NULL && fk->local_column_refs[0] != NULL);
    ASSERT(fk->referenced_column_refs != NULL && fk->referenced_column_refs[0] != NULL);

    ExpressionNode *local_column_ref = fk->local_column_refs[0];
    ExpressionNode *ref_column_ref = fk->referenced_column_refs[0];

    ASSERT(local_column_ref != NULL);
    ASSERT(local_column_ref->type == EXPR_COLUMN_REF);
    ASSERT(local_column_ref->expression_data.column_value.relation_index == 0);
    ASSERT(local_column_ref->expression_data.column_value.column_index == 1);
    
    ASSERT(ref_column_ref != NULL);
    ASSERT(ref_column_ref->type == EXPR_COLUMN_REF);
    ASSERT(ref_column_ref->expression_data.column_value.relation_index == 0);
    ASSERT(ref_column_ref->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_composite_foreign_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"department_assignments", IDENTIFIER}, {"(", PUNCTUATION},
        {"employee_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"location_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"department_id", IDENTIFIER}, {",", PUNCTUATION}, {"location_id", IDENTIFIER},
        {")", PUNCTUATION}, {"REFERENCES", KEYWORD}, {"department_locations", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"department_id", IDENTIFIER}, {",", PUNCTUATION}, 
        {"location_id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns != NULL);
    ASSERT(create_table->columns->num_column_defs == 3);

    ASSERT(create_table->num_constraints == 1);
    ASSERT(create_table->constraints != NULL);

    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_FOREIGN_KEY);

    ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;
    ASSERT(fk != NULL);
    ASSERT(fk->local_column_refs != NULL);
    ASSERT(fk->referenced_column_refs != NULL);

    ASSERT(fk->num_local_columns == 2);
    ASSERT(fk->num_referenced_columns == 2);

    ASSERT(fk->local_column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(fk->local_column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->local_column_refs[0]->expression_data.column_value.column_index == 1);

    ASSERT(fk->local_column_refs[1]->type == EXPR_COLUMN_REF);
    ASSERT(fk->local_column_refs[1]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->local_column_refs[1]->expression_data.column_value.column_index == 2);

    ASSERT(fk->referenced_column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(fk->referenced_column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->referenced_column_refs[0]->expression_data.column_value.column_index == 0);

    ASSERT(fk->referenced_column_refs[1]->type == EXPR_COLUMN_REF);
    ASSERT(fk->referenced_column_refs[1]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->referenced_column_refs[1]->expression_data.column_value.column_index == 1);
   
    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_self_referencing_fk() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"categories", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {",", PUNCTUATION},
        {"parent_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"parent_id", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"categories", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->columns != NULL);
    ASSERT(create_table->columns->num_column_defs == 2);

    ASSERT(create_table->num_constraints == 1);
    ASSERT(create_table->constraints != NULL);

    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_FOREIGN_KEY);

    ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;
    ASSERT(fk != NULL);
    ASSERT(fk->local_column_refs != NULL);
    ASSERT(fk->referenced_column_refs != NULL);
    ASSERT(fk->num_local_columns == 1);
    ASSERT(fk->num_referenced_columns == 1);

    ASSERT(fk->local_column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(fk->local_column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->local_column_refs[0]->expression_data.column_value.column_index == 1);

    ASSERT(fk->referenced_column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(fk->referenced_column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->referenced_column_refs[0]->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_missing_referenced_table() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"owner_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"owner_id", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"missing_table", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_missing_referenced_column() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"department_id", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"missing_column", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_foreign_key_type_mismatch() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_ddl_binder_fixture(&binder);

    TestToken tokens_1[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_name", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"department_name", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_1, ARRAY_SIZE(tokens_1));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    TestToken tokens_2[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"department_name", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"32", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"department_name", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens_2, ARRAY_SIZE(tokens_2));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_foreign_key_count_mismatch() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"department_id", IDENTIFIER}, 
        {")", PUNCTUATION}, {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));


    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_foreign_key_non_unique_target() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"budget", IDENTIFIER}, {"DOUBLE", KEYWORD}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"budget", IDENTIFIER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"budget", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));


    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_invalid_column_references() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, {"(", PUNCTUATION},
        {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"UNIQUE", KEYWORD}, {"(", PUNCTUATION},
        {"missing_column", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(!bind_create_table(root, &binder.context));


    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_create_table_reference_resolution() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"price", IDENTIFIER}, {"DOUBLE", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, 
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"department_id", IDENTIFIER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"UNIQUE", KEYWORD}, {"(", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, {"price", IDENTIFIER}, {">", OPERATOR}, {"0", NUMBER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD}, {"(", PUNCTUATION}, {"department_id", IDENTIFIER}, {")", PUNCTUATION},
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    ASSERT(bind_create_table(root, &binder.context));

    // CREATE TABLE binding must not create the runtime table. That belongs to execution
    ASSERT(binder.db.table_count == 3);

    // Binding context is not modified
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table != NULL);
    ASSERT(create_table->constraints != NULL);
    ASSERT(create_table->num_constraints == 4);

    // Table-level PRIMARY KEY
    ConstraintNode *constraint = create_table->constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_PRIMARY_KEY);

    PrimaryKeyConstraintNode *pk = &constraint->constraint_data.primary_key;
    ASSERT(pk != NULL);
    ASSERT(pk->column_refs != NULL);
    ASSERT(pk->num_columns == 2);

    ASSERT(pk->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[0]->expression_data.column_value.column_index == 0);

    ASSERT(pk->column_refs[1]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[1]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[1]->expression_data.column_value.column_index == 1);

    // Table-level UNIQUE
    constraint = create_table->constraints[1];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_UNIQUE);

    UniqueConstraintNode *uq = &constraint->constraint_data.unique;
    ASSERT(uq != NULL);
    ASSERT(uq->column_refs != NULL);
    ASSERT(uq->num_columns == 1);

    ASSERT(uq->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(uq->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(uq->column_refs[0]->expression_data.column_value.column_index == 3);

    // Table-level CHECK
    constraint = create_table->constraints[2];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_CHECK);

    CheckConstraintNode *check = &constraint->constraint_data.check;
    ASSERT(check != NULL);
    ASSERT(check->check_expr != NULL);
    ASSERT(check->check_expr->type == EXPR_BINARY);

    Binary *binary_expr = &check->check_expr->expression_data.binary_expr;
    ASSERT(binary_expr != NULL);
    ASSERT(binary_expr->left_operand != NULL);
    ASSERT(binary_expr->left_operand->type == EXPR_COLUMN_REF);
    ASSERT(binary_expr->left_operand->expression_data.column_value.relation_index == 0);
    ASSERT(binary_expr->left_operand->expression_data.column_value.column_index == 2);

    // Table-level FOREIGN KEY
    constraint = create_table->constraints[3];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_FOREIGN_KEY);

    ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;
    ASSERT(fk != NULL);
    ASSERT(fk->local_column_refs != NULL);
    ASSERT(fk->referenced_column_refs != NULL);
    ASSERT(fk->num_local_columns == 1);
    ASSERT(fk->num_referenced_columns == 1);

    ASSERT(fk->local_column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(fk->local_column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->local_column_refs[0]->expression_data.column_value.column_index == 1);

    ASSERT(fk->referenced_column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(fk->referenced_column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(fk->referenced_column_refs[0]->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}


int test_bind_create_table_binding_atomicity() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"FOREIGN", KEYWORD}, {"KEY", KEYWORD},
        {"(", PUNCTUATION}, {"department_id", IDENTIFIER}, {")", PUNCTUATION},
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER},
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION}, {",", PUNCTUATION},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, {"unknown_column", IDENTIFIER}, {">", OPERATOR},
        {"0", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    CreateTableNode *create_table = &root->node_contents.create_table;
    ASSERT(create_table->num_constraints == 3);
    ASSERT(create_table->constraints != NULL);

    ConstraintNode *pk_constraint = create_table->constraints[0];
    ConstraintNode *fk_constraint = create_table->constraints[1];
    ConstraintNode *check_constraint = create_table->constraints[2];

    ASSERT(pk_constraint != NULL);
    ASSERT(fk_constraint != NULL);
    ASSERT(check_constraint != NULL);

    ASSERT(pk_constraint->type == AST_CONSTRAINT_PRIMARY_KEY);
    ASSERT(fk_constraint->type == AST_CONSTRAINT_FOREIGN_KEY);
    ASSERT(check_constraint->type == AST_CONSTRAINT_CHECK);

    PrimaryKeyConstraintNode *pk = &pk_constraint->constraint_data.primary_key;
    ForeignKeyConstraintNode *fk = &fk_constraint->constraint_data.foreign_key;
    CheckConstraintNode *check = &check_constraint->constraint_data.check;

    ASSERT(pk->num_columns == 1);
    ASSERT(pk->column_refs != NULL);
    ASSERT(pk->column_refs[0] != NULL);

    ASSERT(fk->num_local_columns == 1);
    ASSERT(fk->num_referenced_columns == 1);
    ASSERT(fk->local_column_refs != NULL);
    ASSERT(fk->referenced_column_refs != NULL);
    ASSERT(fk->local_column_refs[0] != NULL);
    ASSERT(fk->referenced_column_refs[0] != NULL);

    ASSERT(check->check_expr != NULL);
    ASSERT(check->check_expr->type == EXPR_BINARY);

    ExpressionNode *pk_ref = pk->column_refs[0];
    ExpressionNode *fk_local = fk->local_column_refs[0];
    ExpressionNode *fk_referenced = fk->referenced_column_refs[0];

    ExpressionNode *check_ref = check->check_expr->expression_data.binary_expr.left_operand;

    ASSERT(pk_ref->type == EXPR_COLUMN_REF);
    ASSERT(fk_local->type == EXPR_COLUMN_REF);
    ASSERT(fk_referenced->type == EXPR_COLUMN_REF);
    ASSERT(check_ref != NULL);
    ASSERT(check_ref->type == EXPR_COLUMN_REF);

    // References must initially be unresolved.
    ExpressionNode *refs[] = { pk_ref, fk_local, fk_referenced, check_ref };

    for (uint32_t i = 0; i < ARRAY_SIZE(refs); i++) {
        ASSERT(refs[i]->expression_data.column_value.relation_index == -1);
        ASSERT(refs[i]->expression_data.column_value.column_index == -1);
    }

    // CHECK (unknown_column > 0) must fail.
    ASSERT(!bind_create_table(root, &binder.context));

    // No partial reference publication is allowed.
    for (uint32_t i = 0; i < ARRAY_SIZE(refs); i++) {
        ASSERT(refs[i]->expression_data.column_value.relation_index == -1);
        ASSERT(refs[i]->expression_data.column_value.column_index == -1);
    }

    // Neither the database nor binding context changes.
    ASSERT(binder.db.table_count == 3);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}


int test_bind_create_table_invalid_arguments() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    // 1. NULL AST
    ASSERT(!bind_create_table(NULL, &binder.context));

    // 2. NULL BindingContext
    ASSERT(!bind_create_table(root, NULL));

    // 3. NULL database
    Database *original_db = binder.context.db;

    binder.context.db = NULL;
    bool rejected = !bind_create_table(root, &binder.context);
    binder.context.db = original_db;

    ASSERT(rejected);

    // 4. Incorrect AST root type
    ASTNodeType original_type = root->type;

    root->type = AST_SELECT;
    rejected = !bind_create_table(root, &binder.context);
    root->type = original_type;

    ASSERT(rejected);

    // 5. Non-empty relations state
    BindingRelation relation = {0};

    binder.context.relations = &relation;
    binder.context.num_relations = 1;

    rejected = !bind_create_table(root, &binder.context);

    // Restore before cleanup: relation is stack-backed.
    binder.context.relations = NULL;
    binder.context.num_relations = 0;

    ASSERT(rejected);

    // 6. Inconsistent relation count
    binder.context.num_relations = 1;

    rejected = !bind_create_table(root, &binder.context);

    binder.context.num_relations = 0;

    ASSERT(rejected);

    // 7. Nonzero aggregate depth
    binder.context.aggregate_depth = 1;

    rejected = !bind_create_table(root, &binder.context);

    binder.context.aggregate_depth = 0;

    ASSERT(rejected);

    // 8. Ensure the original valid input still binds.
    ASSERT(bind_create_table(root, &binder.context));

    ASSERT(binder.db.table_count == 3);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);
    ASSERT(binder.context.aggregate_depth == 0);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}


int test_bind_create_table_invalid_ast_structure() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"projects", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD},
        {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);

    CreateTableNode *create_table = &root->node_contents.create_table;

    ASSERT(create_table->columns != NULL);
    ASSERT(create_table->columns->column_defs != NULL);
    ASSERT(create_table->columns->num_column_defs == 2);

    ColumnsNode *columns = create_table->columns;

    // 1. Empty table name
    char original_first_char = create_table->table_name[0];

    create_table->table_name[0] = '\0';
    bool rejected = !bind_create_table(root, &binder.context);
    create_table->table_name[0] = original_first_char;

    ASSERT(rejected);

    // 2. Missing ColumnsNode
    ColumnsNode *original_columns = create_table->columns;

    create_table->columns = NULL;
    rejected = !bind_create_table(root, &binder.context);
    create_table->columns = original_columns;

    ASSERT(rejected);

    // 3. Missing column definitions array
    ColumnDefNode **original_defs = columns->column_defs;

    columns->column_defs = NULL;
    rejected = !bind_create_table(root, &binder.context);
    columns->column_defs = original_defs;

    ASSERT(rejected);

    // 4. Zero column definitions
    uint32_t original_count = columns->num_column_defs;

    columns->num_column_defs = 0;
    rejected = !bind_create_table(root, &binder.context);
    columns->num_column_defs = original_count;

    ASSERT(rejected);

    // 5. NULL column definition
    ColumnDefNode *original_first_def = columns->column_defs[0];

    columns->column_defs[0] = NULL;
    rejected = !bind_create_table(root, &binder.context);
    columns->column_defs[0] = original_first_def;

    ASSERT(rejected);

    // 6. Empty column name
    ColumnDefNode *id_column = columns->column_defs[0];

    ASSERT(id_column != NULL);

    char original_column_char = id_column->column_name[0];

    id_column->column_name[0] = '\0';
    rejected = !bind_create_table(root, &binder.context);
    id_column->column_name[0] = original_column_char;

    ASSERT(rejected);

    // 7. Invalid datatype enum
    DataType original_data_type = id_column->type;

    id_column->type = (DataType)-1;
    rejected = !bind_create_table(root, &binder.context);
    id_column->type = original_data_type;

    ASSERT(rejected);

    // 8. Missing column-level constraint array
    ASSERT(id_column->num_constraints == 1);
    ASSERT(id_column->constraints != NULL);

    ConstraintNode **original_constraints = id_column->constraints;

    id_column->constraints = NULL;
    rejected = !bind_create_table(root, &binder.context);
    id_column->constraints = original_constraints;

    ASSERT(rejected);

    // 9. NULL constraint within the array
    ConstraintNode *original_constraint = id_column->constraints[0];

    id_column->constraints[0] = NULL;
    rejected = !bind_create_table(root, &binder.context);
    id_column->constraints[0] = original_constraint;

    ASSERT(rejected);

    // 10. Invalid constraint enumeration
    ASSERT(original_constraint != NULL);

    ASTConstraintType original_constraint_type = original_constraint->type;

    original_constraint->type = (ASTConstraintType)-1;
    rejected = !bind_create_table(root, &binder.context);
    original_constraint->type = original_constraint_type;

    ASSERT(rejected);

    // 11. Valid AST must still bind successfully.
    ASSERT(bind_create_table(root, &binder.context));

    // Verify PRIMARY KEY reference resolution
    PrimaryKeyConstraintNode *pk = &id_column->constraints[0]->constraint_data.primary_key;

    ASSERT(pk->num_columns == 1);
    ASSERT(pk->column_refs != NULL);
    ASSERT(pk->column_refs[0] != NULL);
    ASSERT(pk->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(pk->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(pk->column_refs[0]->expression_data.column_value.column_index == 0);

    ASSERT(binder.db.table_count == 3);
    ASSERT(binder.context.relations == NULL);
    ASSERT(binder.context.num_relations == 0);

    result = 0;

cleanup:
    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

/* ---------- bind_alter_table unit tests ---------- */

int test_bind_alter_table_add_column_basic() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, {"INTEGER", KEYWORD},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "age") == 0);
    ASSERT(new_column->type == INTEGER);

    // ADD COLUMN has no constraints in this test
    ASSERT(bound_alter.result_schema->num_constraints == 0);

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_invalid_duplicate_name() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER}, {"INTEGER", KEYWORD},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_invalid_data_type_args() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"nickname", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"0", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_constraint_validation() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, {"INTEGER", KEYWORD},
        {"NOT", KEYWORD}, {"NULL", KEYWORD}, {"DEFAULT", KEYWORD}, {"18", NUMBER},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "age") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 2);
    ASSERT(schema->constraints != NULL);

    Constraint *not_null = schema->constraints[0];
    Constraint *default_constraint = schema->constraints[1];

    ASSERT(not_null != NULL);
    ASSERT(default_constraint != NULL);

    // Verify NOT NULL constraint
    ASSERT(not_null->type == NOT_NULL);
    ASSERT(!strcmp(not_null->constraint_name, "employees_nn_1"));

    ASSERT(not_null->constraint_data.not_null.column_ref == 4);

    // Verify DEFAULT constraint
    ASSERT(default_constraint->type == DEFAULT);
    ASSERT(!strcmp(default_constraint->constraint_name, "employees_df_1"));

    ASSERT(default_constraint->constraint_data.default_value.column_ref == 4);

    // Verify DEFAULT expression
    ExpressionNode *default_expr = default_constraint->constraint_data.default_value.default_expr;

    ASSERT(default_expr != NULL);
    ASSERT(default_expr->type == EXPR_LITERAL);

    Literal *literal = &default_expr->expression_data.literal_value;

    ASSERT(!literal->is_untyped_null);
    ASSERT(literal->literal != NULL);
    ASSERT(!literal->literal->null_val);

    ASSERT(literal->literal->type == UNSIGNED_INTEGER);
    ASSERT(literal->literal->value.uint32_val == 18);

    // Verify constraint names are unique
    ASSERT(strcmp(not_null->constraint_name, default_constraint->constraint_name));

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_primary_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"employee_code", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "employee_code") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 1);
    ASSERT(schema->constraints != NULL);

    Constraint *primary_key = schema->constraints[0];

    ASSERT(primary_key != NULL);

    // Verify PRIMARY KEY constraint
    ASSERT(primary_key->type == PRIMARY_KEY);
    ASSERT(!strcmp(primary_key->constraint_name, "employees_pk_1"));

    ASSERT(primary_key->constraint_data.primary_key.amount_columns == 1);
    ASSERT(primary_key->constraint_data.primary_key.primary_key_columns[0] == 4);

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_conflict_default_null() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"NOT", KEYWORD}, {"NULL", KEYWORD}, 
        {"DEFAULT", KEYWORD}, {"NULL", KEYWORD},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_check_expr() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"CHECK", KEYWORD}, {"(", PUNCTUATION}, 
        {"age", IDENTIFIER}, {">=", OPERATOR}, {"18", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "age") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 1);
    ASSERT(schema->constraints != NULL);

    Constraint *check = schema->constraints[0];

    ASSERT(check != NULL);

    // Verify CHECK constraint
    ASSERT(check->type == CHECK);
    ASSERT(!strcmp(check->constraint_name, "employees_ck_1"));

    ASSERT(check->constraint_data.check.amount_columns == 1);
    ASSERT(check->constraint_data.check.column_refs[0] == 4);

    // Validate binary expression (age >= 18)
    ExpressionNode *check_expr = check->constraint_data.check.constraint_expr;
    
    ASSERT(check_expr != NULL);
    ASSERT(check_expr->type == EXPR_BINARY);
    ASSERT(check_expr->expression_data.binary_expr.op == OP_GTE);

    ExpressionNode *age_column_expr = check_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *number_literal = check_expr->expression_data.binary_expr.right_operand;

    ASSERT(age_column_expr != NULL);
    ASSERT(age_column_expr->type == EXPR_COLUMN_REF);
    ASSERT(age_column_expr->expression_data.column_value.relation_index == 0);
    ASSERT(age_column_expr->expression_data.column_value.column_index == 4);

    ASSERT(number_literal != NULL);
    ASSERT(number_literal->type == EXPR_LITERAL);
    ASSERT(!number_literal->expression_data.literal_value.is_untyped_null);
    
    ASSERT(number_literal->expression_data.literal_value.literal != NULL);
    ASSERT(number_literal->expression_data.literal_value.literal->type == UNSIGNED_INTEGER);
    ASSERT(number_literal->expression_data.literal_value.literal->value.uint32_val == 18);

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_invalid_check_expr() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"CHECK", KEYWORD}, {"(", PUNCTUATION}, 
        {"unknown_column", IDENTIFIER}, {">", OPERATOR}, {"0", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_fk_references_pk() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"parent_department_id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "parent_department_id") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 1);
    ASSERT(schema->constraints != NULL);

    Constraint *foreign_key = schema->constraints[0];

    ASSERT(foreign_key != NULL);

    // Verify FOREIGN KEY constraint
    ASSERT(foreign_key->type == FOREIGN_KEY);
    ASSERT(!strcmp(foreign_key->constraint_name, "employees_fk_1"));

    ASSERT(foreign_key->constraint_data.foreign_key.amount_columns == 1);
    ASSERT(foreign_key->constraint_data.foreign_key.foreign_key_columns[0] == 4);
    
    ASSERT(!strcmp(foreign_key->constraint_data.foreign_key.referenced_table_name, "departments"));
    ASSERT(foreign_key->constraint_data.foreign_key.amount_referenced_columns == 1);
    ASSERT(foreign_key->constraint_data.foreign_key.referenced_columns[0] == 0);
    

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_fk_references_unique() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"department_name", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "department_name") == 0);
    ASSERT(new_column->type == VARCHAR);
    ASSERT(new_column->type_args.length == 64);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 1);
    ASSERT(schema->constraints != NULL);

    Constraint *foreign_key = schema->constraints[0];

    ASSERT(foreign_key != NULL);

    // Verify FOREIGN KEY constraint
    ASSERT(foreign_key->type == FOREIGN_KEY);
    ASSERT(!strcmp(foreign_key->constraint_name, "employees_fk_1"));

    ASSERT(foreign_key->constraint_data.foreign_key.amount_columns == 1);
    ASSERT(foreign_key->constraint_data.foreign_key.foreign_key_columns[0] == 4);
    
    ASSERT(!strcmp(foreign_key->constraint_data.foreign_key.referenced_table_name, "departments"));
    ASSERT(foreign_key->constraint_data.foreign_key.amount_referenced_columns == 1);
    ASSERT(foreign_key->constraint_data.foreign_key.referenced_columns[0] == 1);
    

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_fk_references_non_unique() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"department_budget", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"64", NUMBER}, {")", PUNCTUATION}, 
        {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"budget", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_datatype_mismatch() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"department_code", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"REFERENCES", KEYWORD}, {"departments", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_missing_referenced_table() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"manager_id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"REFERENCES", KEYWORD}, {"managers", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_self_reference() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"mentor_id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"UNIQUE", KEYWORD}, 
        {"REFERENCES", KEYWORD}, {"employees", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"mentor_id", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "mentor_id") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 2);
    ASSERT(schema->constraints != NULL);

    // Validate UNIQUE constraint
    Constraint *unique = schema->constraints[0];

    ASSERT(unique != NULL);
    ASSERT(unique->type == UNIQUE);
    ASSERT(!strcmp(unique->constraint_name, "employees_uq_1"));
    ASSERT(unique->constraint_data.unique_cols.amount_columns == 1);
    ASSERT(unique->constraint_data.unique_cols.column_refs[0] == 4);

    // Validate FOREIGN KEY constraint
    Constraint *foreign_key = schema->constraints[1];

    ASSERT(foreign_key != NULL);

    // Verify FOREIGN KEY constraint
    ASSERT(foreign_key->type == FOREIGN_KEY);
    ASSERT(!strcmp(foreign_key->constraint_name, "employees_fk_1"));

    ASSERT(foreign_key->constraint_data.foreign_key.amount_columns == 1);
    ASSERT(foreign_key->constraint_data.foreign_key.foreign_key_columns[0] == 4);
    
    ASSERT(!strcmp(foreign_key->constraint_data.foreign_key.referenced_table_name, "employees"));
    ASSERT(foreign_key->constraint_data.foreign_key.amount_referenced_columns == 1);
    ASSERT(foreign_key->constraint_data.foreign_key.referenced_columns[0] == 4);
    

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_self_reference_not_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"manager_id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"REFERENCES", KEYWORD}, {"employees", IDENTIFIER}, 
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_preserve_constraint_name() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"CONSTRAINT", KEYWORD}, {"ck_employee_age", IDENTIFIER},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, {"age", IDENTIFIER},
        {">=", OPERATOR}, {"18", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "age") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 1);
    ASSERT(schema->constraints != NULL);

    // Validate CHECK constraint
    Constraint *check = schema->constraints[0];

    ASSERT(check != NULL);

    // Verify FOREIGN KEY constraint
    ASSERT(check->type == CHECK);
    ASSERT(!strcmp(check->constraint_name, "ck_employee_age"));

    // Not, validating the whole binary expression (age >= 18),
    // because it's verified in the corresponding test above
    ASSERT(check->constraint_data.check.amount_columns == 1);
    ASSERT(check->constraint_data.check.column_refs[0] == 4);
    
    
    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_multiple_constraint_names() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, 
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, {"age", IDENTIFIER},
        {">=", OPERATOR}, {"18", NUMBER}, {")", PUNCTUATION},
        {"CHECK", KEYWORD}, {"(", PUNCTUATION}, {"age", IDENTIFIER},
        {"<=", OPERATOR}, {"65", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 5);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "age") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 2);
    ASSERT(schema->constraints != NULL);

    // Validate CHECK constraint
    Constraint *check_1 = schema->constraints[0];
    Constraint *check_2 = schema->constraints[1];

    ASSERT(check_1 != NULL);
    ASSERT(check_2 != NULL);

    // Verify unique constraint names
    ASSERT(check_1->type == CHECK);
    ASSERT(!strcmp(check_1->constraint_name, "employees_ck_1"));
    
    ASSERT(check_2->type == CHECK);
    ASSERT(!strcmp(check_2->constraint_name, "employees_ck_2"));

    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_duplicate_constraint_names() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, {"INTEGER", KEYWORD},
        {"CONSTRAINT", KEYWORD}, {"age_check", IDENTIFIER}, {"CHECK", KEYWORD}, 
        {"(", PUNCTUATION}, {"age", IDENTIFIER}, {">=", OPERATOR}, {"18", NUMBER}, {")", PUNCTUATION},
        {"CONSTRAINT", KEYWORD}, {"age_check", IDENTIFIER}, {"CHECK", KEYWORD}, 
        {"(", PUNCTUATION}, {"age", IDENTIFIER}, {"<=", OPERATOR}, {"65", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_ref_preceding_add_column() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"code", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {"UNIQUE", KEYWORD}, {",", PUNCTUATION},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"alternate_code", IDENTIFIER},
        {"INTEGER", KEYWORD}, {"REFERENCES", KEYWORD}, {"employees", IDENTIFIER},
        {"(", PUNCTUATION}, {"code", IDENTIFIER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Verify the bound output
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.alter_table == &root->node_contents.alter_table);
    ASSERT(bound_alter.result_schema != NULL);

    // Original employees schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // Proposed schema must contain the new fifth column
    ASSERT(bound_alter.result_schema->num_columns == 6);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[4] != NULL);

    Column *new_column = bound_alter.result_schema->columns[4];

    ASSERT(strcmp(new_column->name, "code") == 0);
    ASSERT(new_column->type == INTEGER);

    // Verify the runtime constraints
    Schema *schema = bound_alter.result_schema;

    ASSERT(schema->num_constraints == 2);
    ASSERT(schema->constraints != NULL);

    // Validate UNIQUE constraint
    Constraint *unique = schema->constraints[0];

    ASSERT(unique != NULL);
    ASSERT(unique->type == UNIQUE);
    ASSERT(!strcmp(unique->constraint_name, "employees_uq_1"));

    // Validate FOREIGN KEY constraint
    Constraint *foreign_key = schema->constraints[1];

    ASSERT(foreign_key != NULL);
    ASSERT(foreign_key->type == FOREIGN_KEY);
    ASSERT(!strcmp(foreign_key->constraint_name, "employees_fk_1"));


    // The committed table schema must remain unchanged
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(binder.tables[0].table_schema != bound_alter.result_schema);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_table_add_column_reject_and_rollback_later_action() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"ADD", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER}, {"INTEGER", KEYWORD},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    // No bound ALTER result should be published
    ASSERT(bound_alter.table == NULL);
    ASSERT(bound_alter.alter_table == NULL);
    ASSERT(bound_alter.result_schema == NULL);

    // The original table must retain its original schema
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);

    // Original schema remains unchanged
    ASSERT(binder.schemas[0].num_columns == 4);
    ASSERT(binder.schemas[0].columns != NULL);

    // Verify original column names and ordering
    ASSERT(strcmp(binder.schemas[0].columns[0]->name, "id") == 0);
    ASSERT(strcmp(binder.schemas[0].columns[1]->name, "name") == 0);
    ASSERT(strcmp(binder.schemas[0].columns[2]->name, "salary") == 0);
    ASSERT(strcmp(binder.schemas[0].columns[3]->name, "department_id") == 0);

    // The original schema still has no constraints
    ASSERT(binder.schemas[0].num_constraints == 0);

    // No additional tables should have been created
    ASSERT(binder.db.table_count == 3);
    
    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_basic() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER}, {"DOUBLE", KEYWORD},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Original schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // MODIFY COLUMN preserves the number of columns
    ASSERT(bound_alter.result_schema->num_columns == 4);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[2] != NULL);

    Column *modified_column =
        bound_alter.result_schema->columns[2];

    ASSERT(strcmp(modified_column->name, "salary") == 0);
    ASSERT(modified_column->type == DOUBLE);

    // Verify other columns are preserved
    ASSERT(strcmp(bound_alter.result_schema->columns[0]->name, "id") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[1]->name, "name") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[3]->name, "department_id") == 0);

    // Original schema remains unchanged
    ASSERT(binder.schemas[0].columns[2]->type == DOUBLE);
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);

    // The proposed schema must be a separate allocation
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_varchar_length() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"128", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Original schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // MODIFY COLUMN preserves the number of columns
    ASSERT(bound_alter.result_schema->num_columns == 4);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[1] != NULL);

    Column *modified_column = bound_alter.result_schema->columns[1];

    ASSERT(strcmp(modified_column->name, "name") == 0);
    ASSERT(modified_column->type == VARCHAR);
    ASSERT(modified_column->type_args.length == 128);

    // Verify other columns are preserved
    ASSERT(strcmp(bound_alter.result_schema->columns[0]->name, "id") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[2]->name, "salary") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[3]->name, "department_id") == 0);

    // Original schema remains unchanged
    ASSERT(binder.schemas[0].columns[1]->type == VARCHAR);
    ASSERT(binder.schemas[0].columns[1]->type_args.length == 64);
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);

    // The proposed schema must be a separate allocation
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_missing_column() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_missing_table() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"projects", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_incompatible_type() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER}, 
        {"JSONB", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_invalid_varchar_length() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"0", NUMBER}, {")", PUNCTUATION},
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_invalid_numeric_precision() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER}, 
        {"NUMERIC", KEYWORD}, {"(", PUNCTUATION}, {"0", NUMBER}, {",", PUNCTUATION}, 
        {"0", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_invalid_numeric_scale() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER}, 
        {"NUMERIC", KEYWORD}, {"(", PUNCTUATION}, {"5", NUMBER}, {",", PUNCTUATION}, 
        {"8", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_primary_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"departments", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Original schema contains 3 columns
    ASSERT(binder.schemas[1].num_columns == 3);

    // MODIFY COLUMN preserves the number of columns
    ASSERT(bound_alter.result_schema->num_columns == 3);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[0] != NULL);

    Column *modified_column = bound_alter.result_schema->columns[0];

    ASSERT(strcmp(modified_column->name, "id") == 0);
    ASSERT(modified_column->type == INTEGER);

    // Verify other columns are preserved
    ASSERT(strcmp(bound_alter.result_schema->columns[1]->name, "name") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[2]->name, "budget") == 0);

    // Original schema remains unchanged
    ASSERT(binder.schemas[1].columns[0]->type == INTEGER);
    ASSERT(binder.tables[1].table_schema == &binder.schemas[1]);

    // The proposed schema must be a separate allocation
    ASSERT(bound_alter.result_schema != &binder.schemas[1]);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[1]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_unique() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"departments", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"128", NUMBER}, 
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Original schema contains 3 columns
    ASSERT(binder.schemas[1].num_columns == 3);

    // MODIFY COLUMN preserves the number of columns
    ASSERT(bound_alter.result_schema->num_columns == 3);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[0] != NULL);

    Column *modified_column = bound_alter.result_schema->columns[1];

    ASSERT(strcmp(modified_column->name, "name") == 0);
    ASSERT(modified_column->type == VARCHAR);
    ASSERT(modified_column->type_args.length == 128);

    // Verify other columns are preserved
    ASSERT(strcmp(bound_alter.result_schema->columns[0]->name, "id") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[2]->name, "budget") == 0);

    // Original schema remains unchanged
    ASSERT(binder.schemas[1].columns[1]->type == VARCHAR);
    ASSERT(binder.schemas[1].columns[1]->type_args.length == 64);
    ASSERT(binder.tables[1].table_schema == &binder.schemas[1]);

    // The proposed schema must be a separate allocation
    ASSERT(bound_alter.result_schema != &binder.schemas[1]);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[1]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_composite_pk() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"department_locations", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"location_id", IDENTIFIER}, 
        {"INTEGER", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Original schema contains 3 columns
    ASSERT(binder.schemas[2].num_columns == 2);

    // MODIFY COLUMN preserves the number of columns
    ASSERT(bound_alter.result_schema->num_columns == 2);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[1] != NULL);

    Column *modified_column = bound_alter.result_schema->columns[1];

    ASSERT(strcmp(modified_column->name, "location_id") == 0);
    ASSERT(modified_column->type == INTEGER);

    // Verify other columns are preserved
    ASSERT(strcmp(bound_alter.result_schema->columns[0]->name, "department_id") == 0);

    // Original schema remains unchanged
    ASSERT(binder.schemas[2].columns[1]->type == INTEGER);
    ASSERT(binder.tables[2].table_schema == &binder.schemas[2]);

    // The proposed schema must be a separate allocation
    ASSERT(bound_alter.result_schema != &binder.schemas[2]);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[2]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_incompatible_pk() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"departments", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"id", IDENTIFIER}, 
        {"JSONB", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_incompatible_unique() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"departments", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, 
        {"JSONB", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_indexed_varchar() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"128", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    // Original schema contains 4 columns
    ASSERT(binder.schemas[0].num_columns == 4);

    // MODIFY COLUMN preserves the number of columns
    ASSERT(bound_alter.result_schema->num_columns == 4);
    ASSERT(bound_alter.result_schema->columns != NULL);
    ASSERT(bound_alter.result_schema->columns[1] != NULL);

    Column *modified_column = bound_alter.result_schema->columns[1];

    ASSERT(strcmp(modified_column->name, "name") == 0);
    ASSERT(modified_column->type == VARCHAR);
    ASSERT(modified_column->type_args.length == 128);

    // Verify other columns are preserved
    ASSERT(strcmp(bound_alter.result_schema->columns[0]->name, "id") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[2]->name, "salary") == 0);
    ASSERT(strcmp(bound_alter.result_schema->columns[3]->name, "department_id") == 0);

    // Original schema remains unchanged
    ASSERT(binder.schemas[0].columns[1]->type == VARCHAR);
    ASSERT(binder.schemas[0].columns[1]->type_args.length == 64);
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);

    // The proposed schema must be a separate allocation
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);

    // Binding must not create additional database tables
    ASSERT(binder.db.table_count == 3);

    // Verify binding context
    ASSERT(binder.context.db == &binder.db);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table == &binder.tables[0]);
    ASSERT(binder.context.aggregate_depth == 0);
    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_oversized_index_key() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    // Initialize the bound ALTER TABLE output
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, 
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"5000", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);

    return result;
}

int test_bind_alter_modify_column_outgoing_fk_mismatch() {
    int result = 1;
    
    DDLBinderTestFixture binder = {0};
    Parser *parser = NULL;
    ASTNode *root = NULL;
    
    BoundAlterTable bound_alter = {0};
    
    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"department_id", IDENTIFIER},
        {"NUMERIC", KEYWORD}, {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, {"0", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Existing employees.department_id -> departments.id FOREIGN KEY.
    uint32_t local_column = 3;
    uint32_t referenced_column = 0;
    
    constraint.type = FOREIGN_KEY;
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "fk_employee_department");
    constraint.constraint_data.foreign_key.amount_columns = 1;
    constraint.constraint_data.foreign_key.foreign_key_columns = &local_column;
    constraint.constraint_data.foreign_key.amount_referenced_columns = 1;
    constraint.constraint_data.foreign_key.referenced_columns = &referenced_column;
    snprintf(constraint.constraint_data.foreign_key.referenced_table_name,
             sizeof(constraint.constraint_data.foreign_key.referenced_table_name), "%s", "departments");
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);
    
    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    
    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));
    
    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_incoming_fk_mismatch() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    BoundAlterTable bound_alter = {0};

    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"departments", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"id", IDENTIFIER},
        {"NUMERIC", KEYWORD}, {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, 
        {"0", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Existing employees.department_id -> departments.id FOREIGN KEY.
    uint32_t local_column = 3;
    uint32_t referenced_column = 0;
    constraint.type = FOREIGN_KEY;
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "fk_employee_department");
    constraint.constraint_data.foreign_key.amount_columns = 1;
    constraint.constraint_data.foreign_key.foreign_key_columns = &local_column;
    constraint.constraint_data.foreign_key.amount_referenced_columns = 1;
    constraint.constraint_data.foreign_key.referenced_columns = &referenced_column;
    snprintf(constraint.constraint_data.foreign_key.referenced_table_name,
             sizeof(constraint.constraint_data.foreign_key.referenced_table_name), "%s", "departments");
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_valid_check() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    BoundAlterTable bound_alter = {0};

    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};
    ExpressionNode *expr = NULL;

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER},
        {"DOUBLE", KEYWORD}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Attach existing runtime CHECK(salary > 0).
    uint32_t check_column = 2;
    constraint.type = CHECK;
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "ck_salary_positive");
    constraint.constraint_data.check.amount_columns = 1;
    constraint.constraint_data.check.column_refs = &check_column;

    expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);
    expr->expression_data.binary_expr.op = OP_GT;
    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    
    ExpressionNode *column_ref = expr->expression_data.binary_expr.left_operand;
    snprintf(column_ref->expression_data.column_value.column_name,
             sizeof(column_ref->expression_data.column_value.column_name), "%s", "salary");
    column_ref->expression_data.column_value.relation_index = -1;
    column_ref->expression_data.column_value.column_index = -1;

    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);
    uint32_t zero = 0;
    expr->expression_data.binary_expr.right_operand->expression_data.literal_value.literal =
        value_create(UNSIGNED_INTEGER, &zero);
    ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.literal_value.literal != NULL);

    constraint.constraint_data.check.constraint_expr = expr;
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(bound_alter.table == &binder.tables[0]);
    ASSERT(bound_alter.result_schema != NULL);
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);
    ASSERT(bound_alter.result_schema->num_constraints == 1);
    ASSERT(bound_alter.result_schema->constraints[0]->type == CHECK);
    ASSERT(bound_alter.result_schema->columns[2]->type == DOUBLE);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);
    expression_node_free(expr);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_invalid_check() {
    int result = 1;
    
    DDLBinderTestFixture binder = {0};
    Parser *parser = NULL;
    ASTNode *root = NULL;
    
    BoundAlterTable bound_alter = {0};
    
    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};
    ExpressionNode *expr = NULL;

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"department_id", IDENTIFIER},
        {"NUMERIC", KEYWORD}, {"(", PUNCTUATION}, {"10", NUMBER}, {",", PUNCTUATION}, 
        {"0", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Existing CHECK(department_id % 2 = 0): modulo requires integer operands.
    uint32_t check_column = 3;
    constraint.type = CHECK;
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "ck_department_even");
    constraint.constraint_data.check.amount_columns = 1;
    constraint.constraint_data.check.column_refs = &check_column;

    expr = expression_node_create(EXPR_BINARY);
    ASSERT(expr != NULL);
    expr->expression_data.binary_expr.op = OP_EQ;
    expr->expression_data.binary_expr.left_operand = expression_node_create(EXPR_BINARY);
    ASSERT(expr->expression_data.binary_expr.left_operand != NULL);
    
    ExpressionNode *modulo = expr->expression_data.binary_expr.left_operand;
    modulo->expression_data.binary_expr.op = OP_MODULO;
    modulo->expression_data.binary_expr.left_operand = expression_node_create(EXPR_COLUMN_REF);
    ASSERT(modulo->expression_data.binary_expr.left_operand != NULL);
    
    ExpressionNode *column_ref = modulo->expression_data.binary_expr.left_operand;
    snprintf(column_ref->expression_data.column_value.column_name,
             sizeof(column_ref->expression_data.column_value.column_name), "%s", "department_id");
    column_ref->expression_data.column_value.relation_index = -1;
    column_ref->expression_data.column_value.column_index = -1;

    uint32_t two = 2, zero = 0;
    modulo->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(modulo->expression_data.binary_expr.right_operand != NULL);
    modulo->expression_data.binary_expr.right_operand->expression_data.literal_value.literal =
        value_create(UNSIGNED_INTEGER, &two);
    ASSERT(modulo->expression_data.binary_expr.right_operand->expression_data.literal_value.literal != NULL);

    expr->expression_data.binary_expr.right_operand = expression_node_create(EXPR_LITERAL);
    ASSERT(expr->expression_data.binary_expr.right_operand != NULL);
    expr->expression_data.binary_expr.right_operand->expression_data.literal_value.literal =
        value_create(UNSIGNED_INTEGER, &zero);
    ASSERT(expr->expression_data.binary_expr.right_operand->expression_data.literal_value.literal != NULL);

    constraint.constraint_data.check.constraint_expr = expr;
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);
    expression_node_free(expr);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_compatible_default() {
    int result = 1;
    
    DDLBinderTestFixture binder = {0};
    
    Parser *parser = NULL;
    ASTNode *root = NULL;
    
    BoundAlterTable bound_alter = {0};
    
    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};
    
    ExpressionNode *expr = NULL;

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"10", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Existing DEFAULT 'Unknown' for employees.name.
    constraint.type = DEFAULT;
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "df_employee_name");
    constraint.constraint_data.default_value.column_ref = 1;
    expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);
    
    varchar_n_t value = {.max_n = 64, .string = "Unknown"};
    expr->expression_data.literal_value.literal = value_create(VARCHAR, &value);
    ASSERT(expr->expression_data.literal_value.literal != NULL);
    
    constraint.constraint_data.default_value.default_expr = expr;
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(bound_alter.table == &binder.tables[0]);

    ASSERT(bound_alter.result_schema != NULL);
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);
    ASSERT(bound_alter.result_schema->num_constraints == 1);

    ASSERT(bound_alter.result_schema->constraints[0]->type == DEFAULT);
    ASSERT(bound_alter.result_schema->columns[1]->type_args.length == 10);
    ASSERT(binder.schemas[0].columns[1]->type_args.length == 64);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);
    expression_node_free(expr);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_default_exceeds_length() {
    int result = 1;
    DDLBinderTestFixture binder = {0};
    Parser *parser = NULL;
    ASTNode *root = NULL;
    BoundAlterTable bound_alter = {0};
    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};
    ExpressionNode *expr = NULL;

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"3", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Existing DEFAULT 'Unknown' for employees.name.
    constraint.type = DEFAULT;
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "df_employee_name");
    
    constraint.constraint_data.default_value.column_ref = 1;

    expr = expression_node_create(EXPR_LITERAL);
    ASSERT(expr != NULL);
    
    varchar_n_t value = {.max_n = 64, .string = "Unknown"};
    expr->expression_data.literal_value.literal = value_create(VARCHAR, &value);
    ASSERT(expr->expression_data.literal_value.literal != NULL);
    
    constraint.constraint_data.default_value.default_expr = expr;
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);
    
    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    
    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));
    
    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);
    expression_node_free(expr);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_preserve_not_null() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    BoundAlterTable bound_alter = {0};

    Constraint constraint = {0};
    Constraint *constraints[] = {&constraint};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"100", NUMBER}, {")", PUNCTUATION}, 
        {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    // Existing employees.name NOT NULL constraint.
    constraint.type = NOT_NULL;
    
    snprintf(constraint.constraint_name, sizeof(constraint.constraint_name), "%s", "nn_employee_name");
    constraint.constraint_data.not_null.column_ref = 1;
    
    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(bound_alter.table == &binder.tables[0]);

    ASSERT(bound_alter.result_schema != NULL);
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);
    ASSERT(bound_alter.result_schema->num_constraints == 1);

    ASSERT(bound_alter.result_schema->constraints[0]->type == NOT_NULL);
    ASSERT(bound_alter.result_schema->constraints[0]->constraint_data.not_null.column_ref == 1);
    ASSERT(bound_alter.result_schema->columns[1]->type_args.length == 100);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_multiple_success() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER},
        {"DOUBLE", KEYWORD}, {",", PUNCTUATION},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"128", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL && root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    ASSERT(bound_alter.result_schema != NULL);
    ASSERT(bound_alter.result_schema->num_columns == 4);

    ASSERT(bound_alter.result_schema->columns[2]->type == DOUBLE);
    ASSERT(bound_alter.result_schema->columns[1]->type == VARCHAR);
    ASSERT(bound_alter.result_schema->columns[1]->type_args.length == 128);

    ASSERT(binder.schemas[0].columns[1]->type_args.length == 64);
    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_multiple_rollback() {
    int result = 1;

    DDLBinderTestFixture binder = {0};

    Parser *parser = NULL;
    ASTNode *root = NULL;

    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"salary", IDENTIFIER},
        {"DOUBLE", KEYWORD}, {",", PUNCTUATION},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"0", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(!bind_alter_table(root, &binder.context, &bound_alter));

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
    return result;
}

int test_bind_alter_modify_column_twice() {
    int result = 1;
    
    DDLBinderTestFixture binder = {0};
    Parser *parser = NULL;
    
    ASTNode *root = NULL;
    
    BoundAlterTable bound_alter = {0};

    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"employees", IDENTIFIER},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"128", NUMBER},
        {")", PUNCTUATION}, {",", PUNCTUATION},
        {"MODIFY", KEYWORD}, {"COLUMN", KEYWORD}, {"name", IDENTIFIER},
        {"VARCHAR", KEYWORD}, {"(", PUNCTUATION}, {"32", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    init_ddl_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);

    ASSERT(bind_alter_table(root, &binder.context, &bound_alter));

    ASSERT(binder.tables[0].table_schema == &binder.schemas[0]);
    ASSERT(bound_alter.table == &binder.tables[0]);

    ASSERT(bound_alter.result_schema != NULL);
    ASSERT(bound_alter.result_schema != &binder.schemas[0]);
    ASSERT(bound_alter.result_schema->num_columns == 4);

    ASSERT(bound_alter.result_schema->columns[1]->type == VARCHAR);
    ASSERT(bound_alter.result_schema->columns[1]->type_args.length == 32);
    ASSERT(binder.schemas[0].columns[1]->type_args.length == 64);

    result = 0;

cleanup:
    bound_alter_table_free(&bound_alter);

    cleanup_ddl_binder_fixture(&binder);
    free_test_parser(parser, root);
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

    /* ---------- bind_create_table unit tests ---------- */
    result = test_bind_create_table_basic();
    generate_output(result, 0, "test_bind_create_table_basic");
    result = test_bind_create_table_all_data_types();
    generate_output(result, 1, "test_bind_create_table_all_data_types");
    result = test_bind_create_table_parameterized_types();
    generate_output(result, 2, "test_bind_create_table_parameterized_types");
    result = test_bind_create_table_invalid_duplicate_name();
    generate_output(result, 3, "test_bind_create_table_invalid_duplicate_name");
    result = test_bind_create_table_invalid_duplicate_columns();
    generate_output(result, 4, "test_bind_create_table_invalid_duplicate_columns");
    result = test_bind_create_table_invalid_type_args();
    generate_output(result, 5, "test_bind_create_table_invalid_type_args");

    result = test_bind_create_table_column_primary_key();
    generate_output(result, 6, "test_bind_create_table_column_primary_key");
    result = test_bind_create_table_composite_primary_key();
    generate_output(result, 7, "test_bind_create_table_composite_primary_key");
    result = test_bind_create_table_invalid_multiple_primary_keys();
    generate_output(result, 8, "test_bind_create_table_invalid_multiple_primary_keys");
    result = test_bind_create_table_unique_constraints();
    generate_output(result, 9, "test_bind_create_table_unique_constraints");
    result = test_bind_create_table_invalid_duplicate_constraint_names();
    generate_output(result, 10, "test_bind_create_table_invalid_duplicate_constraint_names");
    result = test_bind_create_table_not_null();
    generate_output(result, 11, "test_bind_create_table_not_null");

    result = test_bind_create_table_check();
    generate_output(result, 12, "test_bind_create_table_check");
    result = test_bind_create_table_invalid_check();
    generate_output(result, 13, "test_bind_create_table_invalid_check");
    result = test_bind_create_table_default_literal();
    generate_output(result, 14, "test_bind_create_table_default_literal");
    result = test_bind_create_table_default_expression();
    generate_output(result, 15, "test_bind_create_table_default_expression");
    result = test_bind_create_table_invalid_incompatible_default();
    generate_output(result, 16, "test_bind_create_table_invalid_incompatible_default");
    result = test_bind_create_table_invalid_duplicate_defaults();
    generate_output(result, 17, "test_bind_create_table_duplicate_defaults");
    result = test_bind_create_table_default_null();
    generate_output(result, 18, "test_bind_create_table_default_null");
    result = test_bind_create_table_invalid_default_null_not_null();
    generate_output(result, 19, "test_bind_create_table_default_null_not_null");
    result = test_bind_create_table_invalid_default_null_primary_key();
    generate_output(result, 20, "test_bind_create_table_invalid_default_null_primary_key");

    result = test_bind_create_table_foreign_key();
    generate_output(result, 21, "test_bind_create_table_foreign_key");
    result = test_bind_create_table_foreign_key_unique();
    generate_output(result, 22, "test_bind_create_table_foreign_key_unique");
    result = test_bind_create_table_composite_foreign_key();
    generate_output(result, 23, "test_bind_create_table_composite_foreign_key");
    result = test_bind_create_table_self_referencing_fk();
    generate_output(result, 24, "test_bind_create_table_self_referencing_fk");
    result = test_bind_create_table_invalid_missing_referenced_table();
    generate_output(result, 25, "test_bind_create_table_missing_referenced_table");
    result = test_bind_create_table_invalid_missing_referenced_column();
    generate_output(result, 26, "test_bind_create_table_invalid_missing_referenced_column");
    result = test_bind_create_table_invalid_foreign_key_type_mismatch();
    generate_output(result, 27, "test_bind_create_table_foreign_key_type_mismatch");
    result = test_bind_create_table_invalid_foreign_key_count_mismatch();
    generate_output(result, 28, "test_bind_create_table_invalid_foreign_key_count_mismatch");
    result = test_bind_create_table_foreign_key_non_unique_target();
    generate_output(result, 29, "test_bind_create_table_foreign_key_non_unique_target");

    result = test_bind_create_table_invalid_column_references();
    generate_output(result, 30, "test_bind_create_table_invalid_column_references");
    result = test_bind_create_table_reference_resolution();
    generate_output(result, 31, "test_bind_create_table_reference_resolution");
    result = test_bind_create_table_binding_atomicity();
    generate_output(result, 32, "test_bind_create_table_binding_atomicity");
    result = test_bind_create_table_invalid_arguments();
    generate_output(result, 33, "test_bind_create_table_invalid_arguments");
    result = test_bind_create_table_invalid_ast_structure();
    generate_output(result, 34, "test_bind_create_table_invalid_ast_structure");

    /* ---------- bind_alter_table unit tests ---------- */
    result = test_bind_alter_table_add_column_basic();
    generate_output(result, 35, "test_bind_alter_table_add_column_basic"); 
    result = test_bind_alter_table_invalid_duplicate_name();
    generate_output(result, 36, "test_bind_alter_table_invalid_duplicate_name");
    result = test_bind_alter_table_invalid_data_type_args();
    generate_output(result, 37, "test_bind_alter_table_invalid_data_type_args");
    result = test_bind_alter_table_add_column_constraint_validation();
    generate_output(result, 38, "test_bind_alter_table_add_column_constraint_validation");
    result = test_bind_alter_table_add_column_primary_key();
    generate_output(result, 39, "test_bind_alter_table_add_column_primary_key");
    result = test_bind_alter_table_add_column_conflict_default_null();
    generate_output(result, 40, "test_bind_alter_table_add_column_conflict_default_null");
    result = test_bind_alter_table_add_column_check_expr();
    generate_output(result, 41, "test_bind_alter_table_add_column_check_expr");
    result = test_bind_alter_table_add_column_invalid_check_expr();
    generate_output(result, 42, "test_bind_alter_table_add_column_invalid_check_expr");
    result = test_bind_alter_table_add_column_fk_references_pk();
    generate_output(result, 43, "test_bind_alter_table_add_column_fk_references_pk");
    result = test_bind_alter_table_add_column_fk_references_unique();
    generate_output(result, 44, "test_bind_alter_table_add_column_fk_references_unique");
    result = test_bind_alter_table_add_column_fk_references_non_unique();
    generate_output(result, 45, "test_bind_alter_table_add_column_fk_references_non_unique");
    result = test_bind_alter_table_add_column_datatype_mismatch();
    generate_output(result, 46, "test_bind_alter_table_add_column_datatype_mismatch");
    result = test_bind_alter_table_add_column_missing_referenced_table();
    generate_output(result, 47, "test_bind_alter_table_add_column_missing_referenced_table");
    result = test_bind_alter_table_add_column_self_reference();
    generate_output(result, 48, "test_bind_alter_table_add_column_self_reference");
    result = test_bind_alter_table_add_column_self_reference_not_key();
    generate_output(result, 49, "test_bind_alter_table_add_column_self_reference_not_key");
    result = test_bind_alter_table_add_column_preserve_constraint_name();
    generate_output(result, 50, "test_bind_alter_table_add_column_preserve_constraint_name");
    result = test_bind_alter_table_add_column_multiple_constraint_names();
    generate_output(result, 51, "test_bind_alter_table_add_column_multiple_constraint_names");
    result = test_bind_alter_table_add_column_duplicate_constraint_names();
    generate_output(result, 52, "test_bind_alter_table_add_column_duplicate_constraint_names");
    result = test_bind_alter_table_add_column_ref_preceding_add_column();
    generate_output(result, 53, "test_bind_alter_table_add_column_ref_preceding_add_column");
    result = test_bind_alter_table_add_column_reject_and_rollback_later_action();
    generate_output(result, 54, "test_bind_alter_table_add_column_reject_and_rollback_later_action");

    result = test_bind_alter_modify_column_basic();
    generate_output(result, 55, "test_bind_alter_modify_column_basic");
    result = test_bind_alter_modify_column_varchar_length();
    generate_output(result, 56, "test_bind_alter_modify_column_varchar_length");
    result = test_bind_alter_modify_column_missing_column();
    generate_output(result, 57, "test_bind_alter_modify_column_missing_column");
    result = test_bind_alter_modify_column_missing_table();
    generate_output(result, 58, "test_bind_alter_modify_column_missing_table");
    result = test_bind_alter_modify_column_incompatible_type();
    generate_output(result, 59, "test_bind_alter_modify_column_incompatible_type");
    result = test_bind_alter_modify_column_invalid_varchar_length();
    generate_output(result, 60, "test_bind_alter_modify_column_invalid_varchar_length");
    result = test_bind_alter_modify_column_invalid_numeric_precision();
    generate_output(result, 61, "test_bind_alter_modify_column_invalid_numeric_precision");
    result = test_bind_alter_modify_column_invalid_numeric_scale();
    generate_output(result, 62, "test_bind_alter_modify_column_invalid_numeric_scale");
    result = test_bind_alter_modify_column_primary_key();
    generate_output(result, 63, "test_bind_alter_modify_column_primary_key");
    result = test_bind_alter_modify_column_unique();
    generate_output(result, 64, "test_bind_alter_modify_column_unique");
    result = test_bind_alter_modify_column_composite_pk();
    generate_output(result, 65, "test_bind_alter_modify_column_composite_pk");
    result = test_bind_alter_modify_column_incompatible_pk();
    generate_output(result, 66, "test_bind_alter_modify_column_incompatible_pk");
    result = test_bind_alter_modify_column_incompatible_unique();
    generate_output(result, 67, "test_bind_alter_modify_column_incompatible_unique");
    result = test_bind_alter_modify_column_indexed_varchar();
    generate_output(result, 68, "test_bind_alter_modify_column_indexed_varchar");
    result = test_bind_alter_modify_column_oversized_index_key();
    generate_output(result, 69, "test_bind_alter_modify_column_oversized_index_key");

    result = test_bind_alter_modify_column_outgoing_fk_mismatch();
    generate_output(result, 70, "test_bind_alter_modify_column_outgoing_fk_mismatch");
    result = test_bind_alter_modify_column_incoming_fk_mismatch();
    generate_output(result, 71, "test_bind_alter_modify_column_incoming_fk_mismatch");
    result = test_bind_alter_modify_column_valid_check();
    generate_output(result, 72, "test_bind_alter_modify_column_valid_check");
    result = test_bind_alter_modify_column_invalid_check();
    generate_output(result, 73, "test_bind_alter_modify_column_invalid_check");
    result = test_bind_alter_modify_column_compatible_default();
    generate_output(result, 74, "test_bind_alter_modify_column_compatible_default");
    result = test_bind_alter_modify_column_default_exceeds_length();
    generate_output(result, 75, "test_bind_alter_modify_column_default_exceeds_length");
    result = test_bind_alter_modify_column_preserve_not_null();
    generate_output(result, 76, "test_bind_alter_modify_column_preserve_not_null");
    result = test_bind_alter_modify_column_multiple_success();
    generate_output(result, 77, "test_bind_alter_modify_column_multiple_success");
    result = test_bind_alter_modify_column_multiple_rollback();
    generate_output(result, 78, "test_bind_alter_modify_column_multiple_rollback");
    result = test_bind_alter_modify_column_twice();
    generate_output(result, 79, "test_bind_alter_modify_column_twice");

    return 0;
}