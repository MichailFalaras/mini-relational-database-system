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

static void cleanup_select_binder_fixture(BinderTestFixture *fixture, BoundProjection *projection) {
    if (projection) {
        free(projection->columns);
        projection->columns = NULL;
        projection->num_columns = 0;
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
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == VARCHAR);

    ASSERT(strcmp(projection_result.columns[0].output_name, "id") == 0);
    ASSERT(strcmp(projection_result.columns[1].output_name, "name") == 0);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_qualified_columns() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 1);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *id_expr = select->projection->expressions[0];

    ASSERT(id_expr->type == EXPR_COLUMN_REF);

    ASSERT(id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(id_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_table_alias() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(strcmp(binder.context.relations[0].alias, "e") == 0);

    // Verify projection
    ASSERT(projection_result.num_columns == 1);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *id_expr = select->projection->expressions[0];

    ASSERT(id_expr->type == EXPR_COLUMN_REF);

    ASSERT(id_expr->expression_data.column_value.relation_index == 0);
    ASSERT(id_expr->expression_data.column_value.column_index == 0);

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_projection_alias() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 1);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == DOUBLE);
    ASSERT(strcmp(projection_result.columns[0].output_name, "income") == 0);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *salary_expr = select->projection->expressions[0];

    ASSERT(salary_expr->type == EXPR_COLUMN_REF);

    ASSERT(salary_expr->expression_data.column_value.relation_index == 0);
    ASSERT(salary_expr->expression_data.column_value.column_index == 2);

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_multiple_from_tables() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 2);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(binder.context.relations[1].table == binder.table_ptrs[1]);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == VARCHAR);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
} 

int test_bind_select_join() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 2);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(binder.context.relations[1].table == binder.table_ptrs[1]);
    ASSERT(strcmp(binder.context.relations[0].alias, "e") == 0);
    ASSERT(strcmp(binder.context.relations[1].alias, "d") == 0);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == VARCHAR);
    ASSERT(projection_result.columns[1].result_type.type == VARCHAR);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_where() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 1);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == VARCHAR);

    // Verify bound column references
    SelectNode *select = &root->node_contents.select;

    ExpressionNode *name_expr = select->projection->expressions[0];

    ASSERT(name_expr->type == EXPR_COLUMN_REF);

    ASSERT(name_expr->expression_data.column_value.relation_index == 0);
    ASSERT(name_expr->expression_data.column_value.column_index == 1);

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_group_by() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == UNSIGNED_INTEGER);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_having() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == UNSIGNED_INTEGER);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_order_by() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 1);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == VARCHAR);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_grouped_order_by() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == UNSIGNED_INTEGER);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_implicit_aggregation() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == UNSIGNED_INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == DOUBLE);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_implicit_having() {
    int result = 1;
    
    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    // Verify registered relations
    ASSERT(binder.context.num_relations == 1);
    ASSERT(binder.context.relations != NULL);
    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);

    // Verify projection
    ASSERT(projection_result.num_columns == 1);
    ASSERT(projection_result.columns != NULL);
    ASSERT(projection_result.columns[0].result_type.type == UNSIGNED_INTEGER);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_all_clauses() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(bind_select(root, &binder.context, &projection_result));

    SelectNode *select = &root->node_contents.select;

    // Verify registered relations
    ASSERT(binder.context.num_relations == 2);
    ASSERT(binder.context.relations != NULL);

    ASSERT(binder.context.relations[0].table == binder.table_ptrs[0]);
    ASSERT(binder.context.relations[1].table == binder.table_ptrs[1]);

    ASSERT(strcmp(binder.context.relations[0].alias, "e") == 0);
    ASSERT(strcmp(binder.context.relations[1].alias, "d") == 0);

    // Verify projection
    ASSERT(projection_result.num_columns == 2);
    ASSERT(projection_result.columns != NULL);

    ASSERT(projection_result.columns[0].result_type.type == INTEGER);
    ASSERT(projection_result.columns[1].result_type.type == UNSIGNED_INTEGER);

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
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_unknown_table() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_unknown_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_ambiguous_column() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_unknown_qualifier() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_duplicate_relation_alias() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_join_on() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_aggregate_in_join_on() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_where_type() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_aggregate_in_where() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_aggregate_in_group_by() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_ungrouped_projection() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_implicit_aggregation() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_nested_aggregates() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_having() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_grouped_order_by() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;   
}

int test_bind_select_invalid_implicit_having() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(root, &binder.context, &projection_result));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_null_arguments() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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
    ASSERT(!bind_select(NULL, &binder.context, &projection_result));

    // NULL context
    ASSERT(!bind_select(root, NULL, &projection_result));

    // NULL output
    ASSERT(!bind_select(root, &binder.context, NULL));

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_invalid_ast_structure() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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

    ASSERT(!bind_select(root, &binder.context, &projection_result));

    // Restore the original expression
    select->where->expression = original_expression;

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_nonempty_output() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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

    projection_result.columns = calloc(1, sizeof(BoundProjectionColumn));
    ASSERT(projection_result.columns != NULL);

    projection_result.num_columns = 1;

    BoundProjectionColumn *original_columns = projection_result.columns;

    ASSERT(!bind_select(root, &binder.context, &projection_result));

    ASSERT(projection_result.columns == original_columns);
    ASSERT(projection_result.num_columns == 1);

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
    free_test_parser(parser, root);
    return result;
}

int test_bind_select_projection_cleanup() {
    int result = 1;

    BinderTestFixture binder = {0};
    BoundProjection projection_result = {0};

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

    ASSERT(!bind_select(root, &binder.context, &projection_result));

    ASSERT(projection_result.columns == NULL);
    ASSERT(projection_result.num_columns == 0);

    result = 0;

cleanup:
    cleanup_select_binder_fixture(&binder, &projection_result);
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

    return 0;
}