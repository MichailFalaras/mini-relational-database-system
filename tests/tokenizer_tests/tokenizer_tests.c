#include <stdio.h>
//#include <ASSERT.h>
#include <string.h>
#include "../../include/tokenizer.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return -1; \
    }

int test_empty_query() {
    char *query = strdup("");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer == NULL);

    tokenizer_free(tokenizer);
    return 1;
}

int test_select_query() {
    char *query = strdup("SELECT * FROM users;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 5);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "FROM") == 0);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "users") == 0);
    ASSERT(strcasecmp(token_array->tokens[4]->token, ";") == 0);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 1;
}

int test_update_query() {
    char *query = strdup("UPDATE table SET col1 = 1.23 WHERE id = 1;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 11);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "UPDATE") == 0);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "table") == 0);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "SET") == 0);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "col1") == 0);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "=") == 0);
    ASSERT(strcasecmp(token_array->tokens[5]->token, "1.23") == 0);
    ASSERT(strcasecmp(token_array->tokens[6]->token, "WHERE") == 0);
    ASSERT(strcasecmp(token_array->tokens[7]->token, "id") == 0);
    ASSERT(strcasecmp(token_array->tokens[8]->token, "=") == 0);
    ASSERT(strcasecmp(token_array->tokens[9]->token, "1") == 0);
    ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 1;
}

int test_insert_query() {
    char *query = strdup("INSERT INTO table (col1, col2) VALUES ('John', 'Doe');");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 15);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "INSERT") == 0);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "INTO") == 0);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "table") == 0);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "(") == 0);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "col1") == 0);
    ASSERT(strcasecmp(token_array->tokens[5]->token, ",") == 0);
    ASSERT(strcasecmp(token_array->tokens[6]->token, "col2") == 0);
    ASSERT(strcasecmp(token_array->tokens[7]->token, ")") == 0);
    ASSERT(strcasecmp(token_array->tokens[8]->token, "VALUES") == 0);
    ASSERT(strcasecmp(token_array->tokens[9]->token, "(") == 0);
    ASSERT(strcasecmp(token_array->tokens[10]->token, "'John'") == 0);
    ASSERT(strcasecmp(token_array->tokens[11]->token, ",") == 0);
    ASSERT(strcasecmp(token_array->tokens[12]->token, "'Doe'") == 0);
    ASSERT(strcasecmp(token_array->tokens[13]->token, ")") == 0);
    ASSERT(strcasecmp(token_array->tokens[14]->token, ";") == 0);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 1;
}

int test_delete_query() {
    char *query = strdup("DELETE FROM table WHERE age < 18;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 8);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "DELETE") == 0);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "FROM") == 0);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "table") == 0);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "WHERE") == 0);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "age") == 0);
    ASSERT(strcasecmp(token_array->tokens[5]->token, "<") == 0);
    ASSERT(strcasecmp(token_array->tokens[6]->token, "18") == 0);
    ASSERT(strcasecmp(token_array->tokens[7]->token, ";") == 0);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 1;
}

int test_create_table_query() {
    char *query = strdup("CREATE TABLE table (col1 INTEGER PRIMARY KEY);");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 9);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "CREATE TABLE") == 0);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "table") == 0);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "col1") == 0);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "INTEGER") == 0);
    ASSERT(strcasecmp(token_array->tokens[5]->token, "PRIMARY") == 0);
    ASSERT(strcasecmp(token_array->tokens[6]->token, "KEY") == 0);
    ASSERT(strcasecmp(token_array->tokens[7]->token, ")") == 0);
    ASSERT(strcasecmp(token_array->tokens[8]->token, ";") == 0);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 1;
}

int test_invalid_character_query() {
    char *query = strdup("SELECT @ FROM table;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array == NULL);

    tokenizer_free(tokenizer);
    return 1;
}

int test_comment_query() {
    char *query = strdup("SELECT * -- comment1_@*+\nFROM table;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 6);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "-- comment1_@*+") == 0);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "FROM") == 0);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "table") == 0);
    ASSERT(strcasecmp(token_array->tokens[5]->token, ";") == 0);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 1;
}

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - strlen(test_desc);
    char *result_str = result ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main() {
    int result;
    printf("> STARTING TESTS\n");

    result = test_empty_query();
    generate_output(result, 0, "test_empty_query");
    result = test_select_query();
    generate_output(result, 1, "test_select_query");
    result = test_update_query();
    generate_output(result, 2, "test_update_query");
    result = test_insert_query();
    generate_output(result, 3, "test_insert_query");
    result = test_delete_query();
    generate_output(result, 4, "test_delete_query");
    result = test_create_table_query();
    generate_output(result, 5, "test_create_table_query");
    result = test_invalid_character_query();
    generate_output(result, 6, "test_invalid_character_query");
    result = test_comment_query();
    generate_output(result, 7, "test_comment_query");

    printf("> TESTS RAN SUCCESSFULLY\n");
    return 0;
}