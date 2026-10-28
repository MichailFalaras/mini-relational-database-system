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


#define ASSERT(condition) \
    if (!(condition)) { \
        goto cleanup;  \
    }


/* ---------- Helpers ---------- */

typedef struct binder_test_fixture {
    Database db;

    Table tables[2];
    Table *table_ptrs[2];

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

    // Register tables in the test database
    fixture->table_ptrs[0] = &fixture->tables[0];
    fixture->table_ptrs[1] = &fixture->tables[1];

    fixture->db.tables = fixture->table_ptrs;
    fixture->db.table_count = 2;

    // Binding context starts without registered relations.
    // bind_select() registers them through FROM and JOIN.
    fixture->context.db = &fixture->db;
    fixture->context.relations = NULL;
    fixture->context.num_relations = 0;
    fixture->context.aggregate_depth = 0;
    fixture->context.current_clause = BIND_CLAUSE_SELECT;
}

static void cleanup_binder_fixture(BinderTestFixture *fixture, BoundStatement *statement) {
    if (statement) {
        bound_statement_free(statement);
    }

    if (fixture) {
        free(fixture->context.relations);
        fixture->context.relations = NULL;
        fixture->context.num_relations = 0;
    }
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

/* ---------- bind_select unit tests ---------- */

int test_bind_select_basic() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == VARCHAR);

    ASSERT(strcmp(bound_result.data.projection.columns[0].output_name, "id") == 0);
    ASSERT(strcmp(bound_result.data.projection.columns[1].output_name, "name") == 0);

    ASSERT(binder.context.current_clause == BIND_CLAUSE_SELECT);
    ASSERT(binder.context.aggregate_depth == 0);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *id_expr = select->projection->expressions[0];
    ExpressionNode *name_expr = select->projection->expressions[1];

    ASSERT(id_expr->type == EXPR_COLUMN_REF);
    ASSERT(name_expr->type == EXPR_COLUMN_REF);

    ASSERT(id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(id_expr->expression_data.column_value.column_index == 0);

    ASSERT(name_expr->expression_data.column_value.relation_index == 0);
    ASSERT(name_expr->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_qualified_columns() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"employees", IDENTIFIER},  {".", PUNCTUATION},
        {"id", IDENTIFIER}, {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 1);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *id_expr = select->projection->expressions[0];

    ASSERT(id_expr->type == EXPR_COLUMN_REF);

    ASSERT(id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(id_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_table_alias() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"e", IDENTIFIER},  {".", PUNCTUATION}, {"id", IDENTIFIER}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"AS", KEYWORD}, 
        {"e", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(strcmp(binder.context.relations[0].alias, "e") == 0);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 1);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *id_expr = select->projection->expressions[0];

    ASSERT(id_expr->type == EXPR_COLUMN_REF);

    ASSERT(id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(id_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_projection_alias() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"salary", IDENTIFIER}, {"AS", KEYWORD}, {"income", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 1);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == DOUBLE);
    ASSERT(strcmp(bound_result.data.projection.columns[0].output_name, "income") == 0);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *salary_expr = select->projection->expressions[0];

    ASSERT(salary_expr->type == EXPR_COLUMN_REF);

    ASSERT(salary_expr->expression_data.column_value.relation_index == 0);
    ASSERT(salary_expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_multiple_from_tables() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"employees", IDENTIFIER}, {".", PUNCTUATION}, {"id", IDENTIFIER},
        {",", PUNCTUATION}, {"departments", IDENTIFIER}, {".", PUNCTUATION}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {",", PUNCTUATION}, {"departments", IDENTIFIER},
        {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 2);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(binder.context.relations[1].table == binder.table_ptrs[1]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == VARCHAR);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *id_expr = select->projection->expressions[0];
    ExpressionNode *name_expr = select->projection->expressions[1];

    ASSERT(id_expr->type == EXPR_COLUMN_REF);
    ASSERT(name_expr->type == EXPR_COLUMN_REF);

    ASSERT(id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(id_expr->expression_data.column_value.column_index == 0);

    ASSERT(name_expr->expression_data.column_value.relation_index == 1);
    ASSERT(name_expr->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
} 

int test_bind_select_join() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, {"name", IDENTIFIER},
        {",", PUNCTUATION}, {"d", IDENTIFIER}, {".", PUNCTUATION}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"AS", KEYWORD}, {"e", IDENTIFIER},
        {"INNER", KEYWORD}, {"JOIN", KEYWORD}, {"departments", IDENTIFIER}, {"AS", KEYWORD}, 
        {"d", IDENTIFIER}, {"ON", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, 
        {"department_id", IDENTIFIER}, {"=", OPERATOR}, {"d", IDENTIFIER}, {".", PUNCTUATION}, 
        {"id", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 2);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(binder.context.relations[1].table == binder.table_ptrs[1]);
    ASSERT(strcmp(binder.context.relations[0].alias, "e") == 0);
    ASSERT(strcmp(binder.context.relations[1].alias, "d") == 0);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == VARCHAR);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == VARCHAR);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *e_name_expr = select->projection->expressions[0];
    ExpressionNode *d_name_expr = select->projection->expressions[1];

    ASSERT(e_name_expr->type == EXPR_COLUMN_REF);
    ASSERT(d_name_expr->type == EXPR_COLUMN_REF);

    ASSERT(e_name_expr->expression_data.column_value.relation_index == 0);
    ASSERT(e_name_expr->expression_data.column_value.column_index == 1);

    ASSERT(d_name_expr->expression_data.column_value.relation_index == 1);
    ASSERT(d_name_expr->expression_data.column_value.column_index == 1);

    // Validate ON expression
    ExpressionNode *on_expr = root->node_contents.select.joins[0]->on->expression;
    ASSERT(on_expr->type == EXPR_BINARY);

    OperatorType op = on_expr->expression_data.binary_expr.op;
    ASSERT(op == OP_EQ);

    ExpressionNode *left_expr = on_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *right_expr = on_expr->expression_data.binary_expr.right_operand;

    // Validate e.department_id
    ASSERT(left_expr->type == EXPR_COLUMN_REF);
    ASSERT(left_expr->expression_data.column_value.relation_index == 0);
    ASSERT(left_expr->expression_data.column_value.column_index == 3);

    // Validate d.id
    ASSERT(right_expr->type == EXPR_COLUMN_REF);
    ASSERT(right_expr->expression_data.column_value.relation_index == 1);
    ASSERT(right_expr->expression_data.column_value.column_index == 0);
    
    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_where() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER}, {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"WHERE", KEYWORD}, {"id", IDENTIFIER}, {">", OPERATOR}, {"2000", NUMBER},
        {"AND", OPERATOR}, {"department_id", IDENTIFIER}, {"=", OPERATOR}, {"1", NUMBER},
        {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 1);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == VARCHAR);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *name_expr = select->projection->expressions[0];

    ASSERT(name_expr->type == EXPR_COLUMN_REF);

    ASSERT(name_expr->expression_data.column_value.relation_index == 0);
    ASSERT(name_expr->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_group_by() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"department_id", IDENTIFIER}, {",", PUNCTUATION},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"GROUP BY", KEYWORD}, {"department_id", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == UNSIGNED_INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *dept_id_expr = select->projection->expressions[0];
    ExpressionNode *count_expr = select->projection->expressions[1];

    ASSERT(dept_id_expr->type == EXPR_COLUMN_REF);
    ASSERT(count_expr->type == EXPR_FUNCTIONS);

    ASSERT(dept_id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(dept_id_expr->expression_data.column_value.column_index == 3);
    
    ASSERT(count_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(count_expr->expression_data.aggregate_func_expr.wildcard == true);
    
    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_having() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"department_id", IDENTIFIER}, {",", PUNCTUATION},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"GROUP BY", KEYWORD}, {"department_id", IDENTIFIER},  {"HAVING", KEYWORD}, 
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION},
        {">", PUNCTUATION}, {"2", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == UNSIGNED_INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *dept_id_expr = select->projection->expressions[0];
    ExpressionNode *count_expr = select->projection->expressions[1];

    ASSERT(dept_id_expr->type == EXPR_COLUMN_REF);
    ASSERT(count_expr->type == EXPR_FUNCTIONS);

    ASSERT(dept_id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(dept_id_expr->expression_data.column_value.column_index == 3);
    
    ASSERT(count_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(count_expr->expression_data.aggregate_func_expr.wildcard == true);

    // Verify HAVING expression
    ExpressionNode *having_expr = select->having->expression;

    ASSERT(having_expr != NULL);
    ASSERT(having_expr->type == EXPR_BINARY);

    ExpressionNode *having_left = having_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *having_right = having_expr->expression_data.binary_expr.right_operand;
    OperatorType op = having_expr->expression_data.binary_expr.op;

    ASSERT(op == OP_GT);
    
    ASSERT(having_left != NULL);
    ASSERT(having_left->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(having_left->expression_data.aggregate_func_expr.wildcard == true);
    
    ASSERT(having_right != NULL);
    ASSERT(having_right->expression_data.literal_value.is_untyped_null == false);
    ASSERT(having_right->expression_data.literal_value.literal->value.int32_val == 2);
    
    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_order_by() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"ORDER BY", KEYWORD}, {"salary", IDENTIFIER},  {"DESC", KEYWORD}, 
        {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 1);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == VARCHAR);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *name_expr = select->projection->expressions[0];

    ASSERT(name_expr->type == EXPR_COLUMN_REF);

    ASSERT(name_expr->expression_data.column_value.relation_index == 0);
    ASSERT(name_expr->expression_data.column_value.column_index == 1);
    
    // Verify ORDER BY expression
    OrderByNode *order_by = select->order_by;

    ASSERT(order_by->column_refs[0] != NULL);
    ASSERT(order_by->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(order_by->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(order_by->column_refs[0]->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_grouped_order_by() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"department_id", IDENTIFIER}, {",", PUNCTUATION},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"GROUP BY", KEYWORD}, {"department_id", IDENTIFIER},
        {"ORDER BY", KEYWORD}, {"department_id", IDENTIFIER},  {"ASC", KEYWORD}, 
        {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == UNSIGNED_INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *name_expr = select->projection->expressions[0];
    ExpressionNode *count_expr = select->projection->expressions[1];

    ASSERT(name_expr->type == EXPR_COLUMN_REF);
    ASSERT(count_expr->type == EXPR_FUNCTIONS);

    ASSERT(name_expr->expression_data.column_value.relation_index == 0);
    ASSERT(name_expr->expression_data.column_value.column_index == 3);

    ASSERT(count_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(count_expr->expression_data.aggregate_func_expr.wildcard == true);
    
    // Verify GROUP BY expression
    GroupByNode *group_by = select->group_by;

    ASSERT(group_by->column_refs[0] != NULL);
    ASSERT(group_by->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(group_by->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(group_by->column_refs[0]->expression_data.column_value.column_index == 3);

    // Verify ORDER BY expression
    OrderByNode *order_by = select->order_by;

    ASSERT(order_by->column_refs[0] != NULL);
    ASSERT(order_by->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(order_by->column_refs[0]->expression_data.column_value.relation_index == 0);
    ASSERT(order_by->column_refs[0]->expression_data.column_value.column_index == 3);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_implicit_aggregation() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, 
        {")", PUNCTUATION}, {",", PUNCTUATION}, {"MAX", IDENTIFIER}, {"(", PUNCTUATION}, 
        {"salary", IDENTIFIER}, {")", PUNCTUATION},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == UNSIGNED_INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == DOUBLE);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *count_expr = select->projection->expressions[0];
    ExpressionNode *max_expr = select->projection->expressions[1];

    ASSERT(count_expr->type == EXPR_FUNCTIONS);
    ASSERT(max_expr->type == EXPR_FUNCTIONS);

    ASSERT(count_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(count_expr->expression_data.aggregate_func_expr.wildcard == true);

    ASSERT(max_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_MAX);
    ASSERT(max_expr->expression_data.aggregate_func_expr.wildcard == false);
    
    ExpressionNode *max_inner_expr = max_expr->expression_data.aggregate_func_expr.expression;

    ASSERT(max_inner_expr != NULL);
    ASSERT(max_inner_expr->type == EXPR_COLUMN_REF);
    ASSERT(max_inner_expr->expression_data.column_value.relation_index == 0);
    ASSERT(max_inner_expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_implicit_having() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, 
        {")", PUNCTUATION}, {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"HAVING", KEYWORD},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION},
        {">", OPERATOR}, {"0", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 1);
    ASSERT(bound_result.data.projection.columns != NULL);
    ASSERT(bound_result.data.projection.columns[0].result_type.type == UNSIGNED_INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *count_expr = select->projection->expressions[0];

    ASSERT(count_expr->type == EXPR_FUNCTIONS);

    ASSERT(count_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(count_expr->expression_data.aggregate_func_expr.wildcard == true);

    ExpressionNode *having_expr = select->having->expression;

    ASSERT(having_expr != NULL);
    ASSERT(having_expr->type == EXPR_BINARY);

    ExpressionNode *having_left_expr = having_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *having_right_expr = having_expr->expression_data.binary_expr.right_operand;

    ASSERT(having_left_expr->type == EXPR_FUNCTIONS);
    ASSERT(having_left_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(having_left_expr->expression_data.aggregate_func_expr.wildcard == true);

    ASSERT(having_right_expr->type == EXPR_LITERAL);
    ASSERT(having_right_expr->expression_data.literal_value.is_untyped_null == false);
    ASSERT(having_right_expr->expression_data.literal_value.literal->value.int32_val == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_all_clauses() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    TestToken tokens[] = {
        {"SELECT", KEYWORD},
        {"e", IDENTIFIER}, {".", PUNCTUATION},
        {"department_id", IDENTIFIER},
        {",", PUNCTUATION},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION},
        {"*", OPERATOR}, {")", PUNCTUATION},

        {"FROM", KEYWORD},
        {"employees", IDENTIFIER}, {"AS", KEYWORD},
        {"e", IDENTIFIER},

        {"INNER", KEYWORD}, {"JOIN", KEYWORD},
        {"departments", IDENTIFIER}, {"AS", KEYWORD},
        {"d", IDENTIFIER},

        {"ON", KEYWORD},
        {"e", IDENTIFIER}, {".", PUNCTUATION},
        {"department_id", IDENTIFIER},
        {"=", OPERATOR},
        {"d", IDENTIFIER}, {".", PUNCTUATION},
        {"id", IDENTIFIER},

        {"WHERE", KEYWORD},
        {"e", IDENTIFIER}, {".", PUNCTUATION},
        {"department_id", IDENTIFIER},
        {"=", OPERATOR},
        {"d", IDENTIFIER}, {".", PUNCTUATION},
        {"id", IDENTIFIER},

        {"GROUP BY", KEYWORD},
        {"e", IDENTIFIER}, {".", PUNCTUATION},
        {"department_id", IDENTIFIER},

        {"HAVING", KEYWORD},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION},
        {"*", OPERATOR}, {")", PUNCTUATION},
        {">", OPERATOR}, {"2", NUMBER},

        {"ORDER BY", KEYWORD},
        {"e", IDENTIFIER}, {".", PUNCTUATION},
        {"department_id", IDENTIFIER},
        {"ASC", KEYWORD},

        {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(bind_select(root, &binder.context, &bound_result.data.projection));

    SelectNode *select = &root->node_contents.select;

    // Verify registered relations
    ASSERT(binder.context.num_relations == 2);
    ASSERT(binder.context.relations != NULL);

    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(binder.context.relations[1].table == binder.table_ptrs[1]);

    ASSERT(strcmp(binder.context.relations[0].alias, "e") == 0);
    ASSERT(strcmp(binder.context.relations[1].alias, "d") == 0);

    // Verify projection
    ASSERT(bound_result.data.projection.num_columns == 2);
    ASSERT(bound_result.data.projection.columns != NULL);

    ASSERT(bound_result.data.projection.columns[0].result_type.type == INTEGER);
    ASSERT(bound_result.data.projection.columns[1].result_type.type == UNSIGNED_INTEGER);

    ExpressionNode *dept_id_expr = select->projection->expressions[0];
    ExpressionNode *count_expr = select->projection->expressions[1];

    ASSERT(dept_id_expr != NULL);
    ASSERT(count_expr != NULL);

    ASSERT(dept_id_expr->type == EXPR_COLUMN_REF);
    ASSERT(dept_id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(dept_id_expr->expression_data.column_value.column_index == 3);

    ASSERT(count_expr->type == EXPR_FUNCTIONS);
    ASSERT(count_expr->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(count_expr->expression_data.aggregate_func_expr.wildcard == true);

    // Verify JOIN ON expression
    ASSERT(select->joins[0] != NULL);
    ASSERT(select->joins[0]->on != NULL);

    ExpressionNode *on_expr = select->joins[0]->on->expression;

    ASSERT(on_expr != NULL);
    ASSERT(on_expr->type == EXPR_BINARY);
    ASSERT(on_expr->expression_data.binary_expr.op == OP_EQ);

    ExpressionNode *on_left = on_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *on_right = on_expr->expression_data.binary_expr.right_operand;

    ASSERT(on_left != NULL);
    ASSERT(on_right != NULL);
    ASSERT(on_left->type == EXPR_COLUMN_REF);
    ASSERT(on_right->type == EXPR_COLUMN_REF);

    ASSERT(on_left->expression_data.column_value.relation_index == 0);
    ASSERT(on_left->expression_data.column_value.column_index == 3);

    ASSERT(on_right->expression_data.column_value.relation_index == 1);
    ASSERT(on_right->expression_data.column_value.column_index == 0);

    // Verify WHERE expression
    ASSERT(select->where != NULL);

    ExpressionNode *where_expr = select->where->expression;

    ASSERT(where_expr != NULL);
    ASSERT(where_expr->type == EXPR_BINARY);
    ASSERT(where_expr->expression_data.binary_expr.op == OP_EQ);

    ExpressionNode *where_left = where_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *where_right = where_expr->expression_data.binary_expr.right_operand;

    ASSERT(where_left != NULL);
    ASSERT(where_right != NULL);
    ASSERT(where_left->type == EXPR_COLUMN_REF);
    ASSERT(where_right->type == EXPR_COLUMN_REF);

    ASSERT(where_left->expression_data.column_value.relation_index == 0);
    ASSERT(where_left->expression_data.column_value.column_index == 3);

    ASSERT(where_right->expression_data.column_value.relation_index == 1);
    ASSERT(where_right->expression_data.column_value.column_index == 0);

    // Verify GROUP BY expression
    ASSERT(select->group_by != NULL);
    ASSERT(select->group_by->column_refs != NULL);

    ExpressionNode *group_expr = select->group_by->column_refs[0];

    ASSERT(group_expr != NULL);
    ASSERT(group_expr->type == EXPR_COLUMN_REF);
    ASSERT(group_expr->expression_data.column_value.relation_index == 0);
    ASSERT(group_expr->expression_data.column_value.column_index == 3);

    // Verify HAVING expression
    ASSERT(select->having != NULL);

    ExpressionNode *having_expr = select->having->expression;

    ASSERT(having_expr != NULL);
    ASSERT(having_expr->type == EXPR_BINARY);
    ASSERT(having_expr->expression_data.binary_expr.op == OP_GT);

    ExpressionNode *having_left = having_expr->expression_data.binary_expr.left_operand;
    ExpressionNode *having_right = having_expr->expression_data.binary_expr.right_operand;

    ASSERT(having_left != NULL);
    ASSERT(having_right != NULL);

    ASSERT(having_left->type == EXPR_FUNCTIONS);
    ASSERT(having_left->expression_data.aggregate_func_expr.type == EXPR_AGGREGATE_COUNT);
    ASSERT(having_left->expression_data.aggregate_func_expr.wildcard == true);

    ASSERT(having_right->type == EXPR_LITERAL);
    ASSERT(having_right->expression_data.literal_value.is_untyped_null == false);
    ASSERT(having_right->expression_data.literal_value.literal != NULL);
    ASSERT(having_right->expression_data.literal_value.literal->value.int32_val == 2);

    // Verify ORDER BY expression
    ASSERT(select->order_by != NULL);
    ASSERT(select->order_by->column_refs != NULL);

    ExpressionNode *order_expr = select->order_by->column_refs[0];

    ASSERT(order_expr != NULL);
    ASSERT(order_expr->type == EXPR_COLUMN_REF);
    ASSERT(order_expr->expression_data.column_value.relation_index == 0);
    ASSERT(order_expr->expression_data.column_value.column_index == 3);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_unknown_table() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"id", IDENTIFIER}, {"FROM", KEYWORD}, 
        {"unknown_table", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_unknown_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"unknown_column", IDENTIFIER}, {"FROM", KEYWORD}, 
        {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_ambiguous_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"id", IDENTIFIER}, {"FROM", KEYWORD}, 
        {"employees", IDENTIFIER}, {",", PUNCTUATION}, {"departments", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_unknown_qualifier() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"x", IDENTIFIER}, {".", PUNCTUATION}, {"id", IDENTIFIER}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_duplicate_relation_alias() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, {"id", IDENTIFIER}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"AS", KEYWORD}, {"e", IDENTIFIER},
        {",", PUNCTUATION}, {"departments", IDENTIFIER}, {"AS", KEYWORD}, {"e", IDENTIFIER}, 
        {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_join_on() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, {"id", IDENTIFIER}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"AS", KEYWORD}, {"e", IDENTIFIER},
        {"INNER", KEYWORD}, {"JOIN", KEYWORD}, {"departments", IDENTIFIER}, {"AS", KEYWORD},
        {"d", IDENTIFIER}, {"ON", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, 
        {"department_id", IDENTIFIER}, {"+", OPERATOR}, {"d", IDENTIFIER}, {".", PUNCTUATION}, 
        {"id", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_aggregate_in_join_on() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, {"id", IDENTIFIER}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"AS", KEYWORD}, {"e", IDENTIFIER},
        {"INNER", KEYWORD}, {"JOIN", KEYWORD}, {"departments", IDENTIFIER}, {"AS", KEYWORD},
        {"d", IDENTIFIER}, {"ON", KEYWORD}, {"COUNT", IDENTIFIER}, {"(", PUNCTUATION},
        {"*", OPERATOR}, {")", PUNCTUATION}, {">", OPERATOR}, {"1", NUMBER},
        {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_where_type() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER}, {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"WHERE", KEYWORD}, {"salary", IDENTIFIER}, {"+", OPERATOR}, {"100", NUMBER},
        {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_aggregate_in_where() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER}, {"FROM", KEYWORD}, {"employees", IDENTIFIER},
        {"WHERE", KEYWORD}, {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR},
        {")", PUNCTUATION}, {">", OPERATOR}, {"1", NUMBER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_aggregate_in_group_by() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR},
        {")", PUNCTUATION}, {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"GROUP BY", KEYWORD},
        {"MAX", IDENTIFIER}, {"(", PUNCTUATION}, {"salary", IDENTIFIER}, {")", PUNCTUATION}, 
        {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_ungrouped_projection() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER}, {",", PUNCTUATION}, 
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"GROUP BY", KEYWORD},
        {"department_id", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_implicit_aggregation() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER}, {",", PUNCTUATION}, 
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_nested_aggregates() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"SUM", IDENTIFIER}, {"(", PUNCTUATION}, {"MAX", IDENTIFIER},
        {"(", PUNCTUATION}, {"salary", IDENTIFIER}, {")", PUNCTUATION}, {")", PUNCTUATION}, 
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_having() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"department_id", IDENTIFIER}, {",", PUNCTUATION},
        {"COUNT", IDENTIFIER}, {"(", PUNCTUATION}, {"*", OPERATOR}, {")", PUNCTUATION},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"GROUP BY", KEYWORD},
        {"department_id", IDENTIFIER}, {"HAVING", KEYWORD}, {"salary", IDENTIFIER},
        {">", OPERATOR}, {"1000", NUMBER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_grouped_order_by() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"department_id", IDENTIFIER}, {"FROM", KEYWORD}, 
        {"employees", IDENTIFIER}, {"GROUP BY", KEYWORD}, {"department_id", IDENTIFIER}, 
        {"ORDER BY", KEYWORD}, {"salary", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;   
}

int test_bind_select_invalid_implicit_having() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"name", IDENTIFIER}, {"FROM", KEYWORD}, 
        {"employees", IDENTIFIER}, {"HAVING", KEYWORD}, {"TRUE", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // Bind SELECT query
    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_null_arguments() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    // NULL root
    ASSERT(!bind_select(NULL, &binder.context, &bound_result.data.projection));

    // NULL context
    ASSERT(!bind_select(root, NULL, &bound_result.data.projection));

    // NULL output
    ASSERT(!bind_select(root, &binder.context, NULL));

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_ast_structure() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"WHERE", KEYWORD},
        {"id", IDENTIFIER}, {">", OPERATOR}, {"1", NUMBER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    SelectNode *select = &root->node_contents.select;

    ASSERT(select->where != NULL);

    ExpressionNode *original_expression = select->where->expression;

    // Temporarily create a malformed WHERE node
    select->where->expression = NULL;

    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    // Restore the original expression
    select->where->expression = original_expression;

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_nonempty_output() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER},
        {"FROM", KEYWORD}, {"employees", IDENTIFIER}, {"WHERE", KEYWORD},
        {"id", IDENTIFIER}, {">", OPERATOR}, {"1", NUMBER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    bound_result.data.projection.columns = calloc(1, sizeof(BoundProjectionColumn));
    ASSERT(bound_result.data.projection.columns != NULL);

    bound_result.data.projection.num_columns = 1;

    BoundProjectionColumn *original_columns = bound_result.data.projection.columns;

    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    ASSERT(bound_result.data.projection.columns == original_columns);
    ASSERT(bound_result.data.projection.num_columns == 1);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_projection_cleanup() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_SELECT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"SELECT", KEYWORD}, {"department_id", IDENTIFIER}, {"FROM", KEYWORD}, 
        {"employees", IDENTIFIER}, {"GROUP BY", KEYWORD}, {"department_id", IDENTIFIER}, 
        {"ORDER BY", KEYWORD}, {"salary", IDENTIFIER}, {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_SELECT);

    ASSERT(!bind_select(root, &binder.context, &bound_result.data.projection));

    ASSERT(bound_result.data.projection.columns == NULL);
    ASSERT(bound_result.data.projection.num_columns == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

/* ---------- bind_insert unit tests ---------- */

int test_bind_insert_all_columns_implicit_schema() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION}, 
        {"2500.0", NUMBER},  {",", PUNCTUATION}, {"10", NUMBER}, {")", PUNCTUATION}, 
        {";", PUNCTUATION}
    };

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(bind_insert(root, &binder.context, &bound_result.data.insertion));

    BoundInsertionValues *insertion = &bound_result.data.insertion;
    InsertNode *insert = &root->node_contents.insert;

    ASSERT(insertion->num_values == 4);
    ASSERT(insertion->values != NULL);

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(insertion->values[i].expression == insert->values->values[i]);
    }

    ASSERT(insertion->values[0].result_type.type == UNSIGNED_INTEGER);
    ASSERT(insertion->values[1].result_type.type == CHAR);
    ASSERT(insertion->values[2].result_type.type == NUMERIC);
    ASSERT(insertion->values[3].result_type.type == UNSIGNED_INTEGER);

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(!insertion->values[i].result_type.is_untyped_null);
    }

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_explicit_columns_reordered() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"(", PUNCTUATION},
        {"salary", IDENTIFIER}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {",", PUNCTUATION},
        {"id", IDENTIFIER}, {",", PUNCTUATION},
        {"department_id", IDENTIFIER},
        {")", PUNCTUATION},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"2500.0", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(bind_insert(root, &binder.context, &bound_result.data.insertion));

    BoundInsertionValues *insertion = &bound_result.data.insertion;
    InsertNode *insert = &root->node_contents.insert;

    ASSERT(insertion->table == binder.table_ptrs[0]);
    ASSERT(insertion->num_values == 4);
    ASSERT(insertion->values != NULL);

    // Destination schema: id, name, salary, department_id
    // Input order: salary, name, id, department_id
    ASSERT(insertion->values[0].expression == insert->values->values[2]);
    ASSERT(insertion->values[1].expression == insert->values->values[1]);
    ASSERT(insertion->values[2].expression == insert->values->values[0]);
    ASSERT(insertion->values[3].expression == insert->values->values[3]);

    ASSERT(insertion->values[0].result_type.type == UNSIGNED_INTEGER);
    ASSERT(insertion->values[1].result_type.type == CHAR);
    ASSERT(insertion->values[2].result_type.type == NUMERIC);
    ASSERT(insertion->values[3].result_type.type == UNSIGNED_INTEGER);

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(insertion->values[i].kind == BOUND_INSERT_EXPRESSION);
        ASSERT(!insertion->values[i].result_type.is_untyped_null);
    }

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_partial_columns() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(bind_insert(root, &binder.context, &bound_result.data.insertion));

    BoundInsertionValues *insertion = &bound_result.data.insertion;
    InsertNode *insert = &root->node_contents.insert;

    ASSERT(insertion->table == binder.table_ptrs[0]);
    ASSERT(insertion->num_values == 4);
    ASSERT(insertion->values != NULL);

    ASSERT(insertion->values[0].kind == BOUND_INSERT_EXPRESSION);
    ASSERT(insertion->values[0].expression == insert->values->values[0]);

    ASSERT(insertion->values[1].kind == BOUND_INSERT_EXPRESSION);
    ASSERT(insertion->values[1].expression == insert->values->values[1]);

    ASSERT(insertion->values[2].kind == BOUND_INSERT_NULL);
    ASSERT(insertion->values[2].expression == NULL);
    ASSERT(insertion->values[2].result_type.type == DOUBLE);
    ASSERT(insertion->values[2].result_type.is_untyped_null);

    ASSERT(insertion->values[3].kind == BOUND_INSERT_NULL);
    ASSERT(insertion->values[3].expression == NULL);
    ASSERT(insertion->values[3].result_type.type == INTEGER);
    ASSERT(insertion->values[3].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}


int test_bind_insert_default_values() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    Parser *default_parser = NULL;
    ASTNode *root = NULL;
    ASTNode *default_root = NULL;

    Constraint default_constraint = {0};
    Constraint *constraints[1] = { &default_constraint };

    init_binder_fixture(&binder);

    // Supply a default expression through an independently parsed AST.
    // That AST owns the expression for the duration of the test.
    TestToken default_tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER}, {"VALUES", KEYWORD}, 
        {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION}, {"'Alice'", STRING}, {",", PUNCTUATION},
        {"2500.0", NUMBER}, {",", PUNCTUATION}, {"10", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {",", PUNCTUATION},
        {"name", IDENTIFIER},
        {")", PUNCTUATION},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    default_parser = create_test_parser(default_tokens, ARRAY_SIZE(default_tokens));
    ASSERT(default_parser != NULL);

    default_root = parse(default_parser);
    ASSERT(default_root != NULL);
    ASSERT(default_root->type == AST_INSERT);

    ExpressionNode *default_expr = default_root->node_contents.insert.values->values[2];

    ASSERT(default_expr != NULL);

    default_constraint.type = DEFAULT;
    default_constraint.constraint_data.default_value.column_ref = 2;
    default_constraint.constraint_data.default_value.default_expr = default_expr;

    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    // Parse actual tested AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(bind_insert(root, &binder.context, &bound_result.data.insertion));

    BoundInsertionValues *insertion = &bound_result.data.insertion;
    InsertNode *insert = &root->node_contents.insert;

    ASSERT(insertion->num_values == 4);
    ASSERT(insertion->values != NULL);

    ASSERT(insertion->values[0].kind == BOUND_INSERT_EXPRESSION);
    ASSERT(insertion->values[0].expression == insert->values->values[0]);

    ASSERT(insertion->values[1].kind == BOUND_INSERT_EXPRESSION);
    ASSERT(insertion->values[1].expression == insert->values->values[1]);

    ASSERT(insertion->values[2].kind == BOUND_INSERT_DEFAULT);
    ASSERT(insertion->values[2].expression == default_expr);
    ASSERT(insertion->values[2].result_type.type == NUMERIC);
    ASSERT(!insertion->values[2].result_type.is_untyped_null);

    ASSERT(insertion->values[3].kind == BOUND_INSERT_NULL);
    ASSERT(insertion->values[3].expression == NULL);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    free_test_parser(default_parser, default_root);
    return result;
}


int test_bind_insert_null_values() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"NULL", KEYWORD}, {",", PUNCTUATION},
        {"NULL", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(bind_insert(root, &binder.context, &bound_result.data.insertion));

    BoundInsertionValues *insertion = &bound_result.data.insertion;
    InsertNode *insert = &root->node_contents.insert;

    ASSERT(insertion->num_values == 4);
    ASSERT(insertion->values != NULL);

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT(insertion->values[i].kind == BOUND_INSERT_EXPRESSION);
        ASSERT(insertion->values[i].expression == insert->values->values[i]);
    }

    ASSERT(!insertion->values[0].result_type.is_untyped_null);
    ASSERT(!insertion->values[1].result_type.is_untyped_null);

    ASSERT(insertion->values[2].result_type.is_untyped_null);
    ASSERT(insertion->values[3].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_expression_assignment() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"100", NUMBER}, {"+", OPERATOR}, {"200", NUMBER},
        {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(bind_insert(root, &binder.context, &bound_result.data.insertion));

    BoundInsertionValues *insertion = &bound_result.data.insertion;
    InsertNode *insert = &root->node_contents.insert;

    ASSERT(insertion->num_values == 4);
    ASSERT(insertion->values != NULL);

    ASSERT(insertion->values[2].kind == BOUND_INSERT_EXPRESSION);
    ASSERT(insertion->values[2].expression == insert->values->values[2]);

    ExpressionNode *salary_expr = insertion->values[2].expression;

    ASSERT(salary_expr != NULL);
    ASSERT(salary_expr->type == EXPR_BINARY);
    ASSERT(salary_expr->expression_data.binary_expr.op == OP_ADD);

    ASSERT(!insertion->values[2].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_not_null_violation() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    Constraint not_null_constraint = {0};
    Constraint *constraints[1] = { &not_null_constraint };

    init_binder_fixture(&binder);

    not_null_constraint.type = NOT_NULL;
    not_null_constraint.constraint_data.not_null.column_ref = 0;

    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"NULL", KEYWORD}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"2500.0", NUMBER}, {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_incompatible_types() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"'invalid'", STRING}, {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_literal_out_of_range() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"2147483648", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"2500.0", NUMBER}, {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_unknown_table() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD},
        {"unknown_table", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_unknown_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {",", PUNCTUATION},
        {"unknown_column", IDENTIFIER},
        {")", PUNCTUATION},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_duplicate_columns() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"(", PUNCTUATION}, {"id", IDENTIFIER}, {",", PUNCTUATION}, {"id", IDENTIFIER},
        {")", PUNCTUATION}, {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION},
        {"2", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_value_count_mismatch() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"2500.0", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.table == NULL);
    ASSERT(bound_result.data.insertion.values == NULL);
    ASSERT(bound_result.data.insertion.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_insert_invalid_arguments_and_output() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_INSERT };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"employees", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'Alice'", STRING}, {",", PUNCTUATION},
        {"2500.0", NUMBER}, {",", PUNCTUATION},
        {"10", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ASSERT(!bind_insert(NULL, &binder.context, &bound_result.data.insertion));
    ASSERT(!bind_insert(root, NULL, &bound_result.data.insertion));
    ASSERT(!bind_insert(root, &binder.context, NULL));

    // Construct an existing output to verify it is not overwritten.
    bound_result.data.insertion.values = calloc(1, sizeof(BoundInsertValue));
    ASSERT(bound_result.data.insertion.values != NULL);

    bound_result.data.insertion.num_values = 1;

    BoundInsertValue *original_values = bound_result.data.insertion.values;

    ASSERT(!bind_insert(root, &binder.context, &bound_result.data.insertion));

    ASSERT(bound_result.data.insertion.values == original_values);
    ASSERT(bound_result.data.insertion.num_values == 1);
    ASSERT(bound_result.data.insertion.table == NULL);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

/* ---------- bind_update unit tests ---------- */

int test_bind_update_basic_binding() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD}, 
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    // Validate registered relation
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    // Validate SET AST
    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 1);

    ExpressionNode *column_expr = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr = update->set->assignments[0].value;

    ASSERT(column_expr != NULL);
    ASSERT(value_expr != NULL);

    ASSERT(column_expr->type == EXPR_COLUMN_REF);
    ASSERT(value_expr->type == EXPR_LITERAL);

    // Validate resolved destination column
    ASSERT(column_expr->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr->expression_data.column_value.column_index == 2);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr);

    // Validate inferred RHS type
    ASSERT(!update_values->values[0].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_multiple_assignments() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD}, 
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"=", OPERATOR}, {"'Alice'", STRING}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 2);

    ExpressionNode *column_expr_1 = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr_1 = update->set->assignments[0].value;
    
    ExpressionNode *column_expr_2 = update->set->assignments[1].column_ref;
    ExpressionNode *value_expr_2 = update->set->assignments[1].value;

    ASSERT(column_expr_1 != NULL);
    ASSERT(value_expr_1 != NULL);

    ASSERT(column_expr_2 != NULL);
    ASSERT(value_expr_2 != NULL);

    ASSERT(column_expr_1->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_1->type == EXPR_LITERAL);

    ASSERT(column_expr_2->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_2->type == EXPR_LITERAL);

    // Validate resolved destination column
    ASSERT(column_expr_1->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_1->expression_data.column_value.column_index == 2);

    ASSERT(column_expr_2->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_2->expression_data.column_value.column_index == 1);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 2);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr_1);
    
    ASSERT(update_values->values[1].column_index == 1);
    ASSERT(update_values->values[1].expression == value_expr_2);

    // Validate inferred RHS type
    ASSERT(!update_values->values[0].result_type.is_untyped_null);
    ASSERT(!update_values->values[1].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_rhs_expression() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD}, 
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"salary", IDENTIFIER}, 
        {"+", OPERATOR}, {"1000.0", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 1);

    ExpressionNode *column_expr_1 = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr_1 = update->set->assignments[0].value;
    
    ASSERT(column_expr_1 != NULL);
    ASSERT(value_expr_1 != NULL);

    ASSERT(column_expr_1->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_1->type == EXPR_BINARY);

    // Validate resolved destination column
    ASSERT(column_expr_1->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_1->expression_data.column_value.column_index == 2);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr_1);

    // Validate inferred RHS type
    ASSERT(!update_values->values[0].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_where_binding() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD}, 
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, 
        {"WHERE", KEYWORD}, {"id", IDENTIFIER}, {"=", OPERATOR}, {"1", NUMBER},
        {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 1);

    ExpressionNode *column_expr_1 = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr_1 = update->set->assignments[0].value;
    
    ASSERT(column_expr_1 != NULL);
    ASSERT(value_expr_1 != NULL);

    ASSERT(column_expr_1->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_1->type == EXPR_LITERAL);

    // Validate resolved destination column
    ASSERT(column_expr_1->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_1->expression_data.column_value.column_index == 2);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr_1);

    // Validate inferred RHS type
    ASSERT(!update_values->values[0].result_type.is_untyped_null);

    // Bind Where clause
    ASSERT(update->where != NULL);
    ASSERT(update->where->expression != NULL);
    ASSERT(update->where->expression->type == EXPR_BINARY);

    ExpressionNode *column_expr = update->where->expression->expression_data.binary_expr.left_operand;

    ASSERT(column_expr != NULL);
    ASSERT(column_expr->type == EXPR_COLUMN_REF);
    ASSERT(column_expr->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr->expression_data.column_value.column_index == 0);
 
    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_qualified_destination() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD}, 
        {"employees", IDENTIFIER}, {".", PUNCTUATION}, {"salary", IDENTIFIER}, 
        {"=", OPERATOR}, {"2500.0", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 1);

    ExpressionNode *column_expr_1 = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr_1 = update->set->assignments[0].value;
    
    ASSERT(column_expr_1 != NULL);
    ASSERT(value_expr_1 != NULL);

    ASSERT(column_expr_1->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_1->type == EXPR_LITERAL);

    // Validate resolved qualified destination column
    ASSERT(
        strcmp(
            column_expr_1->expression_data.column_value.table_name,
            "employees"
        ) == 0
    );
    ASSERT(column_expr_1->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_1->expression_data.column_value.column_index == 2);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr_1);

    // Validate inferred RHS type
    ASSERT(!update_values->values[0].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_set_where_resolution() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"AS", KEYWORD}, {"e", IDENTIFIER}, 
        {"SET", KEYWORD}, {"e", IDENTIFIER}, {".", PUNCTUATION}, {"salary", IDENTIFIER}, 
        {"=", OPERATOR}, {"e", IDENTIFIER}, {".", PUNCTUATION}, {"salary", IDENTIFIER},
        {"+", OPERATOR}, {"1000.0", NUMBER}, {"WHERE", KEYWORD},
        {"e", IDENTIFIER}, {".", PUNCTUATION}, {"id", IDENTIFIER}, 
        {"=", OPERATOR}, {"1", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 1);

    ExpressionNode *column_expr_1 = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr_1 = update->set->assignments[0].value;
    
    ASSERT(column_expr_1 != NULL);
    ASSERT(value_expr_1 != NULL);

    ASSERT(column_expr_1->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_1->type == EXPR_BINARY);

    // Validate resolved qualified destination column
    ASSERT(
        strcmp(
            column_expr_1->expression_data.column_value.table_name,
            "e"
        ) == 0
    );
    ASSERT(column_expr_1->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_1->expression_data.column_value.column_index == 2);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr_1);

    // Validate inferred RHS type
    ASSERT(!update_values->values[0].result_type.is_untyped_null);

    // Bind Where clause
    ASSERT(update->where != NULL);
    ASSERT(update->where->expression != NULL);
    ASSERT(update->where->expression->type == EXPR_BINARY);

    ExpressionNode *column_expr = update->where->expression->expression_data.binary_expr.left_operand;

    ASSERT(column_expr != NULL);
    ASSERT(column_expr->type == EXPR_COLUMN_REF);
    ASSERT(
        strcmp(
            column_expr->expression_data.column_value.table_name,
            "e"
        ) == 0
    );
    ASSERT(column_expr->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr->expression_data.column_value.column_index == 0);
 

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_set_nullable_column() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD}, 
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"NULL", KEYWORD}, {";", PUNCTUATION}
    };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    // Create parser and AST
    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    // Bind UPDATE query
    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations[0].table != NULL);

    ASSERT(update->set != NULL);
    ASSERT(update->set->assignments != NULL);
    ASSERT(update->set->num_assignments == 1);

    ExpressionNode *column_expr_1 = update->set->assignments[0].column_ref;
    ExpressionNode *value_expr_1 = update->set->assignments[0].value;
    
    ASSERT(column_expr_1 != NULL);
    ASSERT(value_expr_1 != NULL);

    ASSERT(column_expr_1->type == EXPR_COLUMN_REF);
    ASSERT(value_expr_1->type == EXPR_LITERAL);

    // Validate resolved qualified destination column
    ASSERT(column_expr_1->expression_data.column_value.relation_index == 0);
    ASSERT(column_expr_1->expression_data.column_value.column_index == 2);

    // Validate bound UPDATE metadata
    ASSERT(update_values->table == binder.context.relations[0].table);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[0].expression == value_expr_1);

    // Validate inferred RHS type
    ASSERT(update_values->values[0].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_mixed_null_assignments() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"name", IDENTIFIER}, {"=", OPERATOR}, {"'Alice'", STRING},
        {",", PUNCTUATION}, {"salary", IDENTIFIER}, {"=", OPERATOR}, {"NULL", KEYWORD},
        {";", PUNCTUATION}
    };

    init_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(update->set != NULL);
    ASSERT(update->set->num_assignments == 2);

    ASSERT(update_values->table == &binder.tables[0]);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 2);

    ASSERT(update_values->values[0].column_index == 1);
    ASSERT(update_values->values[1].column_index == 2);

    ASSERT(update_values->values[0].expression == update->set->assignments[0].value);
    ASSERT(update_values->values[1].expression == update->set->assignments[1].value);

    ASSERT(!update_values->values[0].result_type.is_untyped_null);
    ASSERT(update_values->values[1].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_nonsequential_column_indexes() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"5000.0", NUMBER},
        {",", PUNCTUATION}, {"name", IDENTIFIER}, {"=", OPERATOR}, {"'Alice'", STRING},
        {";", PUNCTUATION}
    };

    init_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(bind_update(root, &binder.context, &bound_result.data.update));

    BoundUpdateValues *update_values = &bound_result.data.update;
    UpdateNode *update = &root->node_contents.update;

    ASSERT(update->set != NULL);
    ASSERT(update->set->num_assignments == 2);

    ASSERT(update_values->table == &binder.tables[0]);
    ASSERT(update_values->values != NULL);
    ASSERT(update_values->num_values == 2);

    ASSERT(update->set->assignments[0].column_ref->expression_data.column_value.column_index == 2);
    ASSERT(update->set->assignments[1].column_ref->expression_data.column_value.column_index == 1);

    ASSERT(update_values->values[0].column_index == 2);
    ASSERT(update_values->values[1].column_index == 1);

    ASSERT(update_values->values[0].expression == update->set->assignments[0].value);
    ASSERT(update_values->values[1].expression == update->set->assignments[1].value);

    ASSERT(update_values->values[0].result_type.type == NUMERIC);
    ASSERT(!update_values->values[0].result_type.is_untyped_null);
    ASSERT(!update_values->values[1].result_type.is_untyped_null);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_unknown_table() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"missing_table", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_unknown_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"missing", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_unknown_rhs_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"missing", IDENTIFIER}, {"+", OPERATOR},
        {"1000.0", NUMBER}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_duplicate_destination() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, {",", PUNCTUATION},
        {"employees", IDENTIFIER}, {".", PUNCTUATION}, {"salary", IDENTIFIER},
        {"=", OPERATOR}, {"3000.0", NUMBER}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_invalid_destination_qualifier() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"other", IDENTIFIER}, {".", PUNCTUATION}, {"salary", IDENTIFIER}, 
        {"=", OPERATOR}, {"2500.0", NUMBER}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_incompatible_literal() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"'invalid'", STRING}, 
        {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_not_null_constraint() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    Constraint not_null_constraint = {0};
    Constraint *constraints[1] = { &not_null_constraint };

    init_binder_fixture(&binder);

    not_null_constraint.type = NOT_NULL;
    not_null_constraint.constraint_data.not_null.column_ref = 1;

    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"name", IDENTIFIER}, {"=", OPERATOR}, {"NULL", KEYWORD}, 
        {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_primary_key_constraint() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    Constraint primary_key_constraint = {0};
    Constraint *constraints[1] = { &primary_key_constraint };
    uint32_t primary_key_columns[1] = {0};

    init_binder_fixture(&binder);

    primary_key_constraint.type = PRIMARY_KEY;
    primary_key_constraint.constraint_data.primary_key.amount_columns = 1;
    primary_key_constraint.constraint_data.primary_key.primary_key_columns = primary_key_columns;

    binder.schemas[0].constraints = constraints;
    binder.schemas[0].num_constraints = 1;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"id", IDENTIFIER}, {"=", OPERATOR}, {"NULL", KEYWORD}, 
        {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_invalid_aggregate_function() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"SUM", IDENTIFIER}, {"(", PUNCTUATION},
        {"salary", IDENTIFIER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_invalid_no_boolean_where() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    init_binder_fixture(&binder);

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER}, 
        {"WHERE", KEYWORD}, {"name", IDENTIFIER}, {";", PUNCTUATION}
    };

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_incompatible_column_assignment() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"name", IDENTIFIER},
        {";", PUNCTUATION}
    };

    init_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_unknown_where_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER},
        {"WHERE", KEYWORD}, {"missing", IDENTIFIER},
        {"=", OPERATOR}, {"1", NUMBER},
        {";", PUNCTUATION}
    };

    init_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_update_aggregate_in_where() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundStatement bound_result = { .type = BOUND_STATEMENT_UPDATE };

    Parser *parser = NULL;
    ASTNode *root = NULL;

    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"employees", IDENTIFIER}, {"SET", KEYWORD},
        {"salary", IDENTIFIER}, {"=", OPERATOR}, {"2500.0", NUMBER},
        {"WHERE", KEYWORD},
        {"SUM", IDENTIFIER}, {"(", PUNCTUATION},
        {"salary", IDENTIFIER}, {")", PUNCTUATION},
        {">", OPERATOR}, {"100.0", NUMBER},
        {";", PUNCTUATION}
    };

    init_binder_fixture(&binder);

    parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    ASSERT(!bind_update(root, &binder.context, &bound_result.data.update));

    ASSERT(bound_result.data.update.table == NULL);
    ASSERT(bound_result.data.update.values == NULL);
    ASSERT(bound_result.data.update.num_values == 0);

    result = 0;

cleanup:
    cleanup_binder_fixture(&binder, &bound_result);
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

    /* ---------- bind_select unit tests ---------- */
    result = test_bind_select_basic();
    generate_output(result, 0, "test_bind_select_basic");
    result = test_bind_select_qualified_columns();
    generate_output(result, 1, "test_bind_select_qualified_columns");
    result = test_bind_select_table_alias();
    generate_output(result, 2, "test_bind_select_table_alias");
    result = test_bind_select_projection_alias();
    generate_output(result, 3, "test_bind_select_projection_alias");
    result = test_bind_select_multiple_from_tables();
    generate_output(result, 4, "test_bind_select_multiple_from_tables");
    result = test_bind_select_join();
    generate_output(result, 5, "test_bind_select_join");
    result = test_bind_select_where();
    generate_output(result, 6, "test_bind_select_where");
    result = test_bind_select_group_by();
    generate_output(result, 7, "test_bind_select_group_by");
    result = test_bind_select_having();
    generate_output(result, 8, "test_bind_select_having");
    result = test_bind_select_order_by();
    generate_output(result, 9, "test_bind_select_order_by");
    result = test_bind_select_grouped_order_by();
    generate_output(result, 10, "test_bind_select_grouped_order_by");
    result = test_bind_select_implicit_aggregation();
    generate_output(result, 11, "test_bind_select_implicit_aggregation");
    result = test_bind_select_implicit_having();
    generate_output(result, 12, "test_bind_select_implicit_having");
    result = test_bind_select_all_clauses();
    generate_output(result, 13, "test_bind_select_all_clauses");

    result = test_bind_select_unknown_table();
    generate_output(result, 14, "test_bind_select_unknown_table");
    result = test_bind_select_unknown_column();
    generate_output(result, 15, "test_bind_select_unknown_column");
    result = test_bind_select_ambiguous_column();
    generate_output(result, 16, "test_bind_select_ambiguous_column");
    result = test_bind_select_unknown_qualifier();
    generate_output(result, 17, "test_bind_select_unknown_qualifier");
    result = test_bind_select_duplicate_relation_alias();
    generate_output(result, 18, "test_bind_select_duplicate_relation_alias");
    result = test_bind_select_invalid_join_on();
    generate_output(result, 19, "test_bind_select_invalid_join_on");
    result = test_bind_select_aggregate_in_join_on();
    generate_output(result, 20, "test_bind_select_aggregate_in_join_on");
    result = test_bind_select_invalid_where_type();
    generate_output(result, 21, "test_bind_select_invalid_where_type");
    result = test_bind_select_aggregate_in_where();
    generate_output(result, 22, "test_bind_select_aggregate_in_where");
    result = test_bind_select_aggregate_in_group_by();
    generate_output(result, 23, "test_bind_select_aggregate_in_group_by");
    result = test_bind_select_ungrouped_projection();
    generate_output(result, 24, "test_bind_select_ungrouped_projection");
    result = test_bind_select_invalid_implicit_aggregation();
    generate_output(result, 25, "test_bind_select_invalid_implicit_aggregates");
    result = test_bind_select_nested_aggregates();
    generate_output(result, 26, "test_bind_select_nested_aggregates");
    result = test_bind_select_invalid_having();
    generate_output(result, 27, "test_bind_select_invalid_having");
    result = test_bind_select_invalid_grouped_order_by();
    generate_output(result, 28, "test_bind_select_invalid_grouped_order_by");
    result = test_bind_select_invalid_implicit_having();
    generate_output(result, 29, "test_bind_select_invalid_implicit_having");
    result = test_bind_select_null_arguments();
    generate_output(result, 30, "test_bind_select_null_arguments");
    result = test_bind_select_invalid_ast_structure();
    generate_output(result, 31, "test_bind_select_invalid_ast_structure");
    result = test_bind_select_nonempty_output();
    generate_output(result, 32, "test_bind_select_nonempty_output");
    result = test_bind_select_projection_cleanup();
    generate_output(result, 33, "test_bind_select_projection_cleanup");

    /* ---------- bind_insert unit tests ---------- */
    result = test_bind_insert_all_columns_implicit_schema();
    generate_output(result, 34, "test_bind_insert_all_columns_implicit_schema");
    result = test_bind_insert_explicit_columns_reordered();
    generate_output(result, 35, "test_bind_insert_explicit_columns_reordered");
    result = test_bind_insert_partial_columns();
    generate_output(result, 36, "test_bind_insert_partial_columns");
    result = test_bind_insert_default_values();
    generate_output(result, 37, "test_bind_insert_default_values");
    result = test_bind_insert_null_values();
    generate_output(result, 38, "test_bind_insert_null_values");
    result = test_bind_insert_expression_assignment();
    generate_output(result, 39, "test_bind_insert_expression_assignment");
    
    result = test_bind_insert_not_null_violation();
    generate_output(result, 40, "test_bind_insert_not_null_violation");
    result = test_bind_insert_incompatible_types();
    generate_output(result, 41, "test_bind_insert_incompatible_types");
    result = test_bind_insert_literal_out_of_range();
    generate_output(result, 42, "test_bind_insert_literal_out_of_range");
    result = test_bind_insert_unknown_table();
    generate_output(result, 43, "test_bind_insert_unknown_table");
    result = test_bind_insert_unknown_column();
    generate_output(result, 44, "test_bind_insert_unknown_column");
    result = test_bind_insert_duplicate_columns();
    generate_output(result, 45, "test_bind_insert_duplicate_columns");
    result = test_bind_insert_value_count_mismatch();
    generate_output(result, 46, "test_bind_insert_value_count_mismatch");
    result = test_bind_insert_invalid_arguments_and_output();
    generate_output(result, 47, "test_bind_insert_invalid_arguments_and_output");

    /* ---------- bind_update unit tests ---------- */
    result = test_bind_update_basic_binding();
    generate_output(result, 48, "test_bind_update_basic_binding");
    result = test_bind_update_multiple_assignments();
    generate_output(result, 49, "test_bind_update_multiple_assignments");
    result = test_bind_update_rhs_expression();
    generate_output(result, 50, "test_bind_update_rhs_expression");
    result = test_bind_update_where_binding();
    generate_output(result, 51, "test_bind_update_where_binding");
    result = test_bind_update_qualified_destination();
    generate_output(result, 52, "test_bind_update_qualified_destination");
    result = test_bind_update_set_where_resolution();
    generate_output(result, 53, "test_bind_update_set_where_resolution");
    result = test_bind_update_set_nullable_column();
    generate_output(result, 54, "test_bind_update_set_nullable_column");
    result = test_bind_update_mixed_null_assignments();
    generate_output(result, 55, "test_bind_update_mixed_null_assignments");
    result = test_bind_update_nonsequential_column_indexes();
    generate_output(result, 56, "test_bind_update_nonsequential_column_indexes");

    
    result = test_bind_update_unknown_table();
    generate_output(result, 57, "test_bind_update_unknown_table");
    result = test_bind_update_unknown_column();
    generate_output(result, 58, "test_bind_update_unknown_column");
    result = test_bind_update_unknown_rhs_column();
    generate_output(result, 59, "test_bind_update_unknown_rhs_column");
    result = test_bind_update_duplicate_destination();
    generate_output(result, 60, "test_bind_update_duplicate_destination");
    result = test_bind_update_invalid_destination_qualifier();
    generate_output(result, 61, "test_bind_update_invalid_destination_qualifier");
    result = test_bind_update_incompatible_literal();
    generate_output(result, 62, "test_bind_update_incompatible_literal");
    result = test_bind_update_not_null_constraint();
    generate_output(result, 63, "test_bind_update_not_null_constraint");
    result = test_bind_update_primary_key_constraint();
    generate_output(result, 64, "test_bind_update_primary_key_constraint");
    result = test_bind_update_invalid_aggregate_function();
    generate_output(result, 65, "test_bind_update_invalid_aggregate_function");
    result = test_bind_update_invalid_no_boolean_where();
    generate_output(result, 66, "test_bind_update_invalid_no_boolean_where");
    result = test_bind_update_incompatible_column_assignment();
    generate_output(result, 67, "test_bind_update_incompatible_column_assignment");
    result = test_bind_update_unknown_where_column();
    generate_output(result, 68, "test_bind_update_unknown_where_column");
    result = test_bind_update_aggregate_in_where();
    generate_output(result, 69, "test_bind_update_aggregate_in_where");

    return 0;
}