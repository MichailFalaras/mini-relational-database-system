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


/* ---------- parse_insert unit tests ---------- */

int test_parse_insert_with_columns() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER}, {")", PUNCTUATION},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION},
        {"'John'", STRING}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    IntoNode *into = root->node_contents.insert.into;
    ASSERT(into != NULL);

    ValuesNode *values = root->node_contents.insert.values;
    ASSERT(values != NULL);

    ASSERT(strcmp(into->table_name, "users") == 0);
    ASSERT(into->num_column_refs == 2);
    ASSERT(into->column_refs != NULL);
    ASSERT(into->column_refs[0] != NULL);
    ASSERT(into->column_refs[1] != NULL);
    ASSERT(into->column_refs[0]->type == EXPR_COLUMN_REF);
    ASSERT(into->column_refs[1]->type == EXPR_COLUMN_REF);
    ASSERT(strcmp(into->column_refs[0]->expression_data.column_value.column_name, "id") == 0);
    ASSERT(strcmp(into->column_refs[1]->expression_data.column_value.column_name, "name") == 0);

    ASSERT(values->num_values == 2);
    ASSERT(values->values != NULL);
    ASSERT(values->values[0] != NULL);
    ASSERT(values->values[1] != NULL);
    ASSERT(values->values[0]->type == EXPR_LITERAL);
    ASSERT(values->values[1]->type == EXPR_LITERAL);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_insert_without_columns() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER},
        {",", PUNCTUATION}, {"'John'", STRING}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);
    
    IntoNode *into = root->node_contents.insert.into;
    ASSERT(into != NULL);

    ValuesNode *values = root->node_contents.insert.values;
    ASSERT(values != NULL);

    ASSERT(strcmp(into->table_name, "users") == 0);

    ASSERT(into->num_column_refs == 0);
    ASSERT(values->num_values == 2);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_insert_with_expression() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"(", PUNCTUATION}, {"age", IDENTIFIER}, {")", PUNCTUATION},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"20", NUMBER},
        {"+", OPERATOR}, {"5", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ValuesNode *values = root->node_contents.insert.values;
    ASSERT(values != NULL);

    ASSERT(values->num_values == 1);
    ASSERT(values->values[0] != NULL);
    ASSERT(values->values[0]->type == EXPR_BINARY);
    ASSERT(values->values[0]->expression_data.binary_expr.op == OP_ADD);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_insert_multiple_values() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {",", PUNCTUATION},
        {"'John'", STRING}, {",", PUNCTUATION},
        {"25", NUMBER}, {",", PUNCTUATION},
        {"100", NUMBER}, {",", PUNCTUATION},
        {"'Athens'", STRING},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    IntoNode *into = root->node_contents.insert.into;
    ValuesNode *values = root->node_contents.insert.values;

    ASSERT(into != NULL);
    ASSERT(values != NULL);
    ASSERT(strcmp(into->table_name, "users") == 0);
    ASSERT(into->num_column_refs == 0);

    ASSERT(values->num_values == 5);
    ASSERT(values->values != NULL);

    for (uint32_t i = 0; i < values->num_values; i++) {
        ASSERT(values->values[i] != NULL);
        ASSERT(values->values[i]->type == EXPR_LITERAL);
    }

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_insert_null_values() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"NULL", KEYWORD}, {",", PUNCTUATION},
        {"TRUE", KEYWORD}, {",", PUNCTUATION},
        {"FALSE", KEYWORD},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_INSERT);

    ValuesNode *values = root->node_contents.insert.values;
    ASSERT(values != NULL);

    ASSERT(values->num_values == 3);
    ASSERT(values->values != NULL);

    ASSERT(values->values[0] != NULL);
    ASSERT(values->values[1] != NULL);
    ASSERT(values->values[2] != NULL);

    ASSERT(values->values[0]->type == EXPR_LITERAL);
    ASSERT(values->values[0]->expression_data.literal_value.literal == NULL);

    ASSERT(values->values[1]->type == EXPR_LITERAL);
    ASSERT(values->values[1]->expression_data.literal_value.literal != NULL);

    ASSERT(values->values[2]->type == EXPR_LITERAL);
    ASSERT(values->values[2]->expression_data.literal_value.literal != NULL);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- parse_update unit tests ---------- */

int test_parse_update_single_assignment() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {"25", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    UpdateNode *update = &root->node_contents.update;

    ASSERT(strcmp(update->table_name, "users") == 0);

    ASSERT(update->set != NULL);
    ASSERT(update->set->num_assignments == 1);

    ASSERT(strcmp(update->set->assignments[0].column_name, "age") == 0);
    ASSERT(update->set->assignments[0].value != NULL);
    ASSERT(update->set->assignments[0].value->type == EXPR_LITERAL);

    ASSERT(update->where == NULL);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_update_multiple_assignments() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"name", IDENTIFIER}, {"=", OPERATOR}, {"'John'", STRING},
        {",", PUNCTUATION}, {"age", IDENTIFIER}, {"=", OPERATOR}, {"30", NUMBER},
        {"WHERE", KEYWORD}, {"id", IDENTIFIER}, {"=", OPERATOR},
        {"1", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);
    
    UpdateNode *update = &root->node_contents.update;
    
    ASSERT(update->set != NULL);
    ASSERT(update->set->num_assignments == 2);
    
    ASSERT(strcmp(update->set->assignments[0].column_name, "name") == 0);
    ASSERT(strcmp(update->set->assignments[1].column_name, "age") == 0);
    
    ASSERT(update->where != NULL);
    ASSERT(update->where->expression != NULL);
    ASSERT(update->where->expression->type == EXPR_BINARY);
    ASSERT(update->where->expression->expression_data.binary_expr.op == OP_EQ);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_update_arithmetic_assignment() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {"age", IDENTIFIER},
        {"+", OPERATOR}, {"1", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);    
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);
    
    SetNode *set = root->node_contents.update.set;
    ASSERT(set != NULL);
    
    ASSERT(set->num_assignments == 1);
    ASSERT(set->assignments[0].value != NULL);
    ASSERT(set->assignments[0].value->type == EXPR_BINARY);
    ASSERT(set->assignments[0].value->expression_data.binary_expr.op == OP_ADD);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_update_complex_where() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {"30", NUMBER},
        {"WHERE", KEYWORD},
        {"age", IDENTIFIER}, {">", OPERATOR}, {"18", NUMBER},
        {"AND", KEYWORD}, {"(", PUNCTUATION},
        {"id", IDENTIFIER}, {"=", OPERATOR}, {"1", NUMBER},
        {"OR", KEYWORD},
        {"id", IDENTIFIER}, {"=", OPERATOR}, {"2", NUMBER},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_UPDATE);

    UpdateNode *update = &root->node_contents.update;
    ASSERT(update->set != NULL);
    ASSERT(update->set->num_assignments == 1);
    ASSERT(strcmp(update->set->assignments[0].column_name, "age") == 0);

    ASSERT(update->where != NULL);
    ASSERT(update->where->expression != NULL);

    ExpressionNode *expression = update->where->expression;
    ASSERT(expression->type == EXPR_BINARY);
    ASSERT(expression->expression_data.binary_expr.op == OP_AND);

    ExpressionNode *left = expression->expression_data.binary_expr.left_operand;
    ExpressionNode *right = expression->expression_data.binary_expr.right_operand;

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    ASSERT(left->type == EXPR_BINARY);
    ASSERT(left->expression_data.binary_expr.op == OP_GT);

    ASSERT(right->type == EXPR_BINARY);
    ASSERT(right->expression_data.binary_expr.op == OP_OR);

    ExpressionNode *or_left = right->expression_data.binary_expr.left_operand;
    ExpressionNode *or_right = right->expression_data.binary_expr.right_operand;

    ASSERT(or_left != NULL);
    ASSERT(or_right != NULL);
    ASSERT(or_left->type == EXPR_BINARY);
    ASSERT(or_right->type == EXPR_BINARY);

    ASSERT(or_left->expression_data.binary_expr.op == OP_EQ);
    ASSERT(or_right->expression_data.binary_expr.op == OP_EQ);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- parse_delete unit tests ---------- */

int test_parse_delete_without_where() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {"users", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_DELETE);
    
    DeleteNode *delete_node = &root->node_contents.delete;
    
    ASSERT(delete_node->from != NULL);
    ASSERT(delete_node->from->num_expressions == 1);
    ASSERT(delete_node->from->expressions != NULL);
    ASSERT(delete_node->from->expressions[0] != NULL);
    
    ASSERT(delete_node->where == NULL);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_delete_with_where() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {"users", IDENTIFIER},
        {"WHERE", KEYWORD}, {"id", IDENTIFIER}, {"=", OPERATOR},
        {"1", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_DELETE);
    
    DeleteNode *delete_node = &root->node_contents.delete;
    
    ASSERT(delete_node->from != NULL);
    ASSERT(delete_node->from->num_expressions == 1);
    
    ASSERT(delete_node->where != NULL);
    ASSERT(delete_node->where->expression != NULL);
    ASSERT(delete_node->where->expression->type == EXPR_BINARY);
    ASSERT(delete_node->where->expression->expression_data.binary_expr.op == OP_EQ);
    
    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_parse_delete_complex_where() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {"users", IDENTIFIER},
        {"WHERE", KEYWORD},
        {"age", IDENTIFIER}, {"<", OPERATOR}, {"18", NUMBER},
        {"OR", KEYWORD},
        {"status", IDENTIFIER}, {"=", OPERATOR}, {"'inactive'", STRING},
        {"AND", KEYWORD},
        {"id", IDENTIFIER}, {">", OPERATOR}, {"100", NUMBER},
        {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    ASSERT(root != NULL);
    ASSERT(root->type == AST_DELETE);

    DeleteNode *delete_node = &root->node_contents.delete;

    ASSERT(delete_node->from != NULL);
    ASSERT(delete_node->from->num_expressions == 1);
    ASSERT(delete_node->where != NULL);
    ASSERT(delete_node->where->expression != NULL);

    ExpressionNode *expression = delete_node->where->expression;
    ASSERT(expression->type == EXPR_BINARY);
    ASSERT(expression->expression_data.binary_expr.op == OP_OR);

    ExpressionNode *left = expression->expression_data.binary_expr.left_operand;
    ExpressionNode *right = expression->expression_data.binary_expr.right_operand;

    ASSERT(left != NULL);
    ASSERT(right != NULL);

    ASSERT(left->type == EXPR_BINARY);
    ASSERT(left->expression_data.binary_expr.op == OP_LT);

    ASSERT(right->type == EXPR_BINARY);
    ASSERT(right->expression_data.binary_expr.op == OP_AND);

    ExpressionNode *and_left = right->expression_data.binary_expr.left_operand;
    ExpressionNode *and_right = right->expression_data.binary_expr.right_operand;

    ASSERT(and_left != NULL);
    ASSERT(and_right != NULL);

    ASSERT(and_left->type == EXPR_BINARY);
    ASSERT(and_right->type == EXPR_BINARY);

    ASSERT(and_left->expression_data.binary_expr.op == OP_EQ);
    ASSERT(and_right->expression_data.binary_expr.op == OP_GT);

    free_test_parser(parser, root);
    return 0;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- Invalid DML test cases ---------- */

int test_invalid_insert_missing_into() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"users", IDENTIFIER}, {"VALUES", KEYWORD},
        {"(", PUNCTUATION}, {"1", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_insert_missing_values() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_insert_empty_values() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_insert_missing_table() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"VALUES", KEYWORD}, 
        {"(", PUNCTUATION}, {"1", NUMBER}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_insert_missing_parentheses() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"1", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_insert_trailing_comma() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION},
        {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_insert_missing_column_parenthesis() {
    TestToken tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"id", IDENTIFIER}, {",", PUNCTUATION}, {"name", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION}, {"1", NUMBER}, {",", PUNCTUATION},
        {"'John'", STRING}, {")", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_update_missing_set() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"age", IDENTIFIER},
        {"=", OPERATOR}, {"25", NUMBER}, {";", PUNCTUATION}
    };
    
    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_update_missing_assignment_value() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_update_missing_table() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_update_missing_equals() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"25", NUMBER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_update_trailing_comma() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {"25", NUMBER}, 
        {",", PUNCTUATION}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_update_empty_where() {
    TestToken tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {"25", NUMBER}, 
        {"WHERE", KEYWORD}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_delete_missing_from() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"users", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_delete_multiple_tables() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {"users", IDENTIFIER},
        {",", PUNCTUATION}, {"orders", IDENTIFIER}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_delete_missing_table() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_delete_empty_where() {
    TestToken tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {"users", IDENTIFIER},
        {"WHERE", KEYWORD}, {";", PUNCTUATION}
    };

    Parser *parser = create_test_parser(tokens, ARRAY_SIZE(tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);
    int result = root == NULL ? 0 : 1;

    free_test_parser(parser, root);
    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

int test_invalid_dml_missing_semicolon() {
    TestToken insert_tokens[] = {
        {"INSERT", KEYWORD}, {"INTO", KEYWORD}, {"users", IDENTIFIER},
        {"VALUES", KEYWORD}, {"(", PUNCTUATION},
        {"1", NUMBER}, {")", PUNCTUATION}
    };

    TestToken update_tokens[] = {
        {"UPDATE", KEYWORD}, {"users", IDENTIFIER}, {"SET", KEYWORD},
        {"age", IDENTIFIER}, {"=", OPERATOR}, {"25", NUMBER}
    };

    TestToken delete_tokens[] = {
        {"DELETE", KEYWORD}, {"FROM", KEYWORD}, {"users", IDENTIFIER}
    };

    Parser *parser = create_test_parser(insert_tokens, ARRAY_SIZE(insert_tokens));
    ASTNode *root = parse(parser);

    ASSERT(parser != NULL);

    int result = root == NULL ? 0 : 1;
    free_test_parser(parser, root);
    
    if (result != 0)  {
        return 1;
    }

    parser = create_test_parser(update_tokens, ARRAY_SIZE(update_tokens));
    root = parse(parser);
    
    ASSERT(parser != NULL);

    result = root == NULL ? 0 : 1;
    free_test_parser(parser, root);
    
    if (result != 0) {
        return 1;
    }

    parser = create_test_parser(delete_tokens, ARRAY_SIZE(delete_tokens));
    ASSERT(parser != NULL);

    root = parse(parser);
    result = root == NULL ? 0 : 1;
    free_test_parser(parser, root);

    return result;

cleanup:
    free_test_parser(parser, root);
    return 1;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;

    /* ---------- parse_insert unit tests ---------- */
    result = test_parse_insert_with_columns();
    generate_output(result, 0, "test_parse_insert_with_columns");
    result = test_parse_insert_without_columns();
    generate_output(result, 1, "test_parse_insert_without_columns");
    result = test_parse_insert_with_expression();
    generate_output(result, 2, "test_parse_insert_with_expression");
    result = test_parse_insert_multiple_values();
    generate_output(result, 3, "test_parse_insert_multiple_values");
    result = test_parse_insert_null_values();
    generate_output(result, 4, "test_parse_insert_null_values");

    /* ---------- parse_update unit tests ---------- */
    result = test_parse_update_single_assignment();
    generate_output(result, 5, "test_parse_update_single_assignment");
    result = test_parse_update_multiple_assignments();
    generate_output(result, 6, "test_parse_update_multiple_assignments");
    result = test_parse_update_arithmetic_assignment();
    generate_output(result, 7, "test_parse_update_arithmetic_assignment");
    result = test_parse_update_complex_where();
    generate_output(result, 8, "test_parse_update_complex_where");

    /* ---------- parse_delete unit tests ---------- */
    result = test_parse_delete_without_where();
    generate_output(result, 9, "test_parse_delete_without_where");
    result = test_parse_delete_with_where();
    generate_output(result, 10, "test_parse_delete_with_where");
    result = test_parse_delete_complex_where();
    generate_output(result, 11, "test_parse_delete_complex_where");

    /* ---------- Invalid DML test cases ---------- */
    result = test_invalid_insert_missing_into();
    generate_output(result, 12, "test_invalid_insert_missing_into");
    result = test_invalid_insert_missing_values();
    generate_output(result, 13, "test_invalid_insert_missing_values");
    result = test_invalid_insert_empty_values();
    generate_output(result, 14, "test_invalid_insert_empty_values");
    result = test_invalid_insert_missing_table();
    generate_output(result, 15, "test_invalid_insert_missing_table");
    result = test_invalid_insert_missing_parentheses();
    generate_output(result, 16, "test_invalid_insert_missing_parentheses");
    result = test_invalid_insert_trailing_comma();
    generate_output(result, 17, "test_invalid_insert_trailing_comma");
    result = test_invalid_insert_missing_column_parenthesis();
    generate_output(result, 18, "test_invalid_insert_missing_column_parenthesis");

    result = test_invalid_update_missing_set();
    generate_output(result, 19, "test_invalid_update_missing_set");
    result = test_invalid_update_missing_assignment_value();
    generate_output(result, 20, "test_invalid_update_missing_assignment_value");
    result = test_invalid_update_missing_table();
    generate_output(result, 21, "test_invalid_update_missing_table");
    result = test_invalid_update_missing_equals();
    generate_output(result, 22, "test_invalid_update_missing_equals");
    result = test_invalid_update_trailing_comma();
    generate_output(result, 23, "test_invalid_update_trailing_comma");
    result = test_invalid_update_empty_where();
    generate_output(result, 24, "test_invalid_update_empty_where");
    
    result = test_invalid_delete_missing_from();
    generate_output(result, 25, "test_invalid_delete_missing_from");
    result = test_invalid_delete_multiple_tables();
    generate_output(result, 26, "test_invalid_delete_multiple_tables");
    result = test_invalid_delete_missing_table();
    generate_output(result, 27, "test_invalid_delete_missing_table");
    result = test_invalid_delete_empty_where();
    generate_output(result, 28, "test_invalid_delete_empty_where");
    result = test_invalid_dml_missing_semicolon();
    generate_output(result, 29, "test_invalid_dml_missing_semicolon");

    return 0;
}