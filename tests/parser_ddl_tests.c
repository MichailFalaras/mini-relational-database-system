#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tokenizer.h"
#include "../include/parser.h"
#include "../include/ast.h"
#include "../include/expressions.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        goto cleanup;  \
    }


/* ---------- Helpers ---------- */

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

typedef struct test_token {
    const char *value;
    TokenType type;
} TestToken;

// Create a parser from manually specified tokens, as in parser_dml_tests.c
static Parser *create_test_parser(const TestToken *tokens, uint32_t count) {
    if (!tokens || count == 0) {
        return NULL;
    }

    TokenArray *token_array = token_array_create();
    
    if (!token_array) {
        return NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        size_t length = strlen(tokens[i].value) + 1;
        
        char *token_str = malloc(length);
        
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

/* ---------- parse_create_table unit tests ---------- */

int test_parse_create_table_basic() {
    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"name", IDENTIFIER}, {"VARCHAR", KEYWORD}, {"(", PUNCTUATION},
        {"50", NUMBER}, {")", PUNCTUATION}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);
    
    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    
    ASSERT(root->type == AST_CREATE_TABLE);
    ASSERT(strcmp(root->node_contents.create_table.table_name, "users") == 0);
    
    ColumnsNode *columns = root->node_contents.create_table.columns;
    ASSERT(columns != NULL);
    ASSERT(columns->num_column_defs == 2);
    
    ASSERT(strcmp(columns->column_defs[0]->column_name, "id") == 0);
    ASSERT(columns->column_defs[0]->type == INTEGER);
    
    ASSERT(strcmp(columns->column_defs[1]->column_name, "name") == 0);
    ASSERT(columns->column_defs[1]->type == VARCHAR);
    ASSERT(columns->column_defs[1]->type_args.length == 50);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_create_table_column_constraints() {
    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {"PRIMARY", KEYWORD}, {"KEY", KEYWORD},
        {",", PUNCTUATION}, {"name", IDENTIFIER}, {"TEXT", KEYWORD},
        {"NOT", KEYWORD}, {"NULL", KEYWORD}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);
    
    ColumnsNode *columns = root->node_contents.create_table.columns;
    ASSERT(columns != NULL);
    ASSERT(columns->num_column_defs == 2);
    
    ASSERT(columns->column_defs[0]->num_constraints == 1);
    ASSERT(columns->column_defs[0]->constraints[0]->type == AST_CONSTRAINT_PRIMARY_KEY);
    
    ASSERT(columns->column_defs[1]->num_constraints == 1);
    ASSERT(columns->column_defs[1]->constraints[0]->type == AST_CONSTRAINT_NOT_NULL);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_create_table_table_constraint() {
    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {",", PUNCTUATION},
        {"PRIMARY", KEYWORD}, {"KEY", KEYWORD}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {")", PUNCTUATION}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_CREATE_TABLE);
    ASSERT(root->node_contents.create_table.num_constraints == 1);
    
    ConstraintNode *constraint = root->node_contents.create_table.constraints[0];
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_PRIMARY_KEY);
    ASSERT(constraint->constraint_data.primary_key.num_columns == 1);

    ExpressionNode *column_ref = constraint->constraint_data.primary_key.column_refs[0];
    ASSERT(column_ref != NULL);
    ASSERT(column_ref->type == EXPR_COLUMN_REF);
    ASSERT(strcmp(column_ref->expression_data.column_value.column_name, "id") == 0)
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- parse_drop_table/parse_truncate_table unit tests ---------- */

int test_parse_drop_table() {
    TestToken tokens[] = {
        {"DROP TABLE", KEYWORD}, {"users", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_DROP_TABLE);
    ASSERT(strcmp(root->node_contents.drop_table.table_name, "users") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_truncate_table() {
    TestToken tokens[] = {
        {"TRUNCATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_TRUNCATE_TABLE);
    ASSERT(strcmp(root->node_contents.truncate_table.table_name, "users") == 0);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- parse_alter_table unit tests ---------- */

int test_parse_alter_add_column() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER}, {"ADD", KEYWORD},
        {"COLUMN", KEYWORD}, {"age", IDENTIFIER}, {"INTEGER", KEYWORD}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(strcmp(root->node_contents.alter_table.table_name, "users") == 0);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_ADD_COLUMN);
    ASSERT(action->alter_contents.alter_add.column != NULL);

    ASSERT(strcmp(action->alter_contents.alter_add.column->column_name, "age") == 0);
    ASSERT(action->alter_contents.alter_add.column->type == INTEGER);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_alter_drop_column() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER},
        {"DROP COLUMN", KEYWORD}, {"age", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_DROP_COLUMN);
    ASSERT(strcmp(action->alter_contents.alter_drop.column_name, "age") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_alter_rename_table() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER}, {"RENAME", KEYWORD},
        {"TO", KEYWORD}, {"customers", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_RENAME_TABLE);
    ASSERT(strcmp(action->alter_contents.alter_rename_table.new_table_name, "customers") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_alter_rename_column() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER}, {"RENAME", KEYWORD},
        {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, {"TO", KEYWORD},
        {"full_name", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_RENAME_COLUMN);
    ASSERT(strcmp(action->alter_contents.alter_rename_col.old_col_name, "name") == 0);
    ASSERT(strcmp(action->alter_contents.alter_rename_col.new_col_name, "full_name") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_alter_modify_column() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER}, {"MODIFY", KEYWORD},
        {"COLUMN", KEYWORD}, {"name", IDENTIFIER}, {"VARCHAR", KEYWORD},
        {"(", PUNCTUATION}, {"100", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_MODIFY_COLUMN);
    ASSERT(strcmp(action->alter_contents.alter_modify.column_name, "name") == 0);
    ASSERT(action->alter_contents.alter_modify.new_type == VARCHAR);
    ASSERT(action->alter_contents.alter_modify.type_args.length == 100);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_alter_add_constraint() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER}, {"ADD", KEYWORD},
        {"CONSTRAINT", KEYWORD}, {"users_id_unique", IDENTIFIER},
        {"UNIQUE", KEYWORD}, {"(", PUNCTUATION}, {"id", IDENTIFIER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_ADD_CONSTRAINT);

    ConstraintNode *constraint = action->alter_contents.alter_add_constraint.constraint;
    ASSERT(constraint != NULL);
    ASSERT(constraint->type == AST_CONSTRAINT_UNIQUE);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_alter_drop_constraint() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER},
        {"DROP CONSTRAINT", KEYWORD}, {"users_id_unique", IDENTIFIER}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_ALTER_TABLE);
    ASSERT(root->node_contents.alter_table.num_actions == 1);
    
    AlterActionNode *action = &root->node_contents.alter_table.actions[0];
    ASSERT(action->type == AST_ALTER_DROP_CONSTRAINT);
    ASSERT(strcmp(action->alter_contents.alter_drop_constraint.constraint_name, "users_id_unique") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- parse_create_index/parse_drop_index unit tests ---------- */

int test_parse_create_index() {
    TestToken tokens[] = {
        {"CREATE INDEX", KEYWORD}, {"idx_users_name", IDENTIFIER},
        {"ON", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"name", IDENTIFIER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);

    ASSERT(root->type == AST_CREATE_INDEX);
    ASSERT(strcmp(root->node_contents.create_index.index_name, "idx_users_name") == 0);
    ASSERT(strcmp(root->node_contents.create_index.table_name, "users") == 0);
    ASSERT(root->node_contents.create_index.num_column_refs == 1);
    
    ExpressionNode *column_ref = root->node_contents.create_index.column_refs[0];
    ASSERT(column_ref != NULL);
    ASSERT(column_ref->type == EXPR_COLUMN_REF);
    ASSERT(strcmp(column_ref->expression_data.column_value.column_name, "name") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_create_index_multiple_columns() {
    TestToken tokens[] = {
        {"CREATE INDEX", KEYWORD}, {"idx_users_name_age", IDENTIFIER},
        {"ON", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"name", IDENTIFIER}, {",", PUNCTUATION}, {"age", IDENTIFIER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    
    ASSERT(root->type == AST_CREATE_INDEX);
    ASSERT(strcmp(root->node_contents.create_index.index_name, "idx_users_name_age") == 0);
    ASSERT(strcmp(root->node_contents.create_index.table_name, "users") == 0);
    
    ASSERT(root->node_contents.create_index.num_column_refs == 2);
    ASSERT(strcmp(root->node_contents.create_index.column_refs[0]->expression_data.column_value.column_name, "name") == 0);
    ASSERT(strcmp(root->node_contents.create_index.column_refs[1]->expression_data.column_value.column_name, "age") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_drop_index() {
    TestToken tokens[] = {
        {"DROP INDEX", KEYWORD}, {"idx_users_name", IDENTIFIER}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);

    ASSERT(root->type == AST_DROP_INDEX);
    ASSERT(strcmp(root->node_contents.drop_index.index_name, "idx_users_name") == 0);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- Invalid DDL test cases ---------- */

int test_invalid_create_table_missing_name() {
    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_create_table_missing_closing_parenthesis() {
    TestToken tokens[] = {
        {"CREATE TABLE", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"INTEGER", KEYWORD}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_drop_table_missing_name() {
    TestToken tokens[] = { {"DROP TABLE", KEYWORD}, {";", PUNCTUATION} };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_truncate_table_missing_name() {
    TestToken tokens[] = { {"TRUNCATE TABLE", KEYWORD}, {";", PUNCTUATION} };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_alter_table_missing_operation() {
    TestToken tokens[] = {
        {"ALTER TABLE", KEYWORD}, {"users", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_create_index_missing_on() {
    TestToken tokens[] = {
        {"CREATE INDEX", KEYWORD}, {"idx_name", IDENTIFIER},
        {"users", IDENTIFIER}, {"(", PUNCTUATION}, {"name", IDENTIFIER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_create_index_missing_columns() {
    TestToken tokens[] = {
        {"CREATE INDEX", KEYWORD}, {"idx_name", IDENTIFIER},
        {"ON", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;
}

int test_invalid_drop_index_missing_name() {
    TestToken tokens[] = { {"DROP INDEX", KEYWORD}, {";", PUNCTUATION} };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    if (!parser) {
        return 1;
    }

    ASTNode *root = parse(parser);
    int result = root == NULL ? 0 : 1;

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

    /* ---------- parse_create_table unit tests ---------- */
    result = test_parse_create_table_basic();
    generate_output(result, 0, "test_parse_create_table_basic");
    result = test_parse_create_table_column_constraints();
    generate_output(result, 1, "test_parse_create_table_column_constraints");
    result = test_parse_create_table_table_constraint();
    generate_output(result, 2, "test_parse_create_table_table_constraint");

    /* ---------- parse_drop_table/parse_truncate_table unit tests ---------- */
    result = test_parse_drop_table();
    generate_output(result, 3, "test_parse_drop_table");
    result = test_parse_truncate_table();
    generate_output(result, 4, "test_parse_truncate_table");

    /* ---------- parse_alter_table unit tests ---------- */
    result = test_parse_alter_add_column();
    generate_output(result, 5, "test_parse_alter_add_column");
    result = test_parse_alter_drop_column();
    generate_output(result, 6, "test_parse_alter_drop_column");
    result = test_parse_alter_rename_table();
    generate_output(result, 7, "test_parse_alter_rename_table");
    result = test_parse_alter_rename_column();
    generate_output(result, 8, "test_parse_alter_rename_column");
    result = test_parse_alter_modify_column();
    generate_output(result, 9, "test_parse_alter_modify_column");
    result = test_parse_alter_add_constraint();
    generate_output(result, 10, "test_parse_alter_add_constraint");
    result = test_parse_alter_drop_constraint();
    generate_output(result, 11, "test_parse_alter_drop_constraint");

    /* ---------- parse_create_index/parse_drop_index unit tests ---------- */
    result = test_parse_create_index();
    generate_output(result, 12, "test_parse_create_index");
    result = test_parse_create_index_multiple_columns();
    generate_output(result, 13, "test_parse_create_index_multiple_columns");
    result = test_parse_drop_index();
    generate_output(result, 14, "test_parse_drop_index");

    /* ---------- Invalid DDL test cases ---------- */
    result = test_invalid_create_table_missing_name();
    generate_output(result, 15, "test_invalid_create_table_missing_name");
    result = test_invalid_create_table_missing_closing_parenthesis();
    generate_output(result, 16, "test_invalid_create_table_missing_closing_parenthesis");
    result = test_invalid_drop_table_missing_name();
    generate_output(result, 17, "test_invalid_drop_table_missing_name");
    result = test_invalid_truncate_table_missing_name();
    generate_output(result, 18, "test_invalid_truncate_table_missing_name");
    result = test_invalid_alter_table_missing_operation();
    generate_output(result, 19, "test_invalid_alter_table_missing_operation");
    result = test_invalid_create_index_missing_on();
    generate_output(result, 20, "test_invalid_create_index_missing_on");
    result = test_invalid_create_index_missing_columns();
    generate_output(result, 21, "test_invalid_create_index_missing_columns");
    result = test_invalid_drop_index_missing_name();
    generate_output(result, 22, "test_invalid_drop_index_missing_name");

    return 0;
}