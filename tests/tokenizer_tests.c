#include <stdio.h>
#include <string.h>
#include "./../include/tokenizer.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return 1; \
    }


static int test_identifier_format() {
    char *query = strdup("_digits user_id table123 _value;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 5);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "_digits") == 0);
    ASSERT(token_array->tokens[0]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "user_id") == 0);
    ASSERT(token_array->tokens[1]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "table123") == 0);
    ASSERT(token_array->tokens[2]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "_value") == 0);
    ASSERT(token_array->tokens[3]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[4]->token, ";") == 0);
    ASSERT(token_array->tokens[4]->type == PUNCTUATION);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 0;
}

static int test_numbers() {

    /* ----- TEST NUMBER-DECIMAL ----- */
    {
        char *query = strdup("0 123 0.5 123.456;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 5);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "0") == 0);
        ASSERT(token_array->tokens[0]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "123") == 0);
        ASSERT(token_array->tokens[1]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "0.5") == 0);
        ASSERT(token_array->tokens[2]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "123.456") == 0);
        ASSERT(token_array->tokens[3]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ";") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- TEST NEGATIVE NUMBERS ----- */
    {
        char *query = strdup("-5 -0.5 -123.12;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 7);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "-") == 0);
        ASSERT(token_array->tokens[0]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "5") == 0);
        ASSERT(token_array->tokens[1]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "-") == 0);
        ASSERT(token_array->tokens[2]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "0.5") == 0);
        ASSERT(token_array->tokens[3]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "-") == 0);
        ASSERT(token_array->tokens[4]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "123.12") == 0);
        ASSERT(token_array->tokens[5]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[6]->token, ";") == 0);
        ASSERT(token_array->tokens[6]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- TEST MALFORMED NUMBER TESTS ----- */
    {
        char *query = strdup("1.2.3 ;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 4);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "1.2") == 0);
        ASSERT(token_array->tokens[0]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[1]->token, ".") == 0);
        ASSERT(token_array->tokens[1]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "3") == 0);
        ASSERT(token_array->tokens[2]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[3]->token, ";") == 0);
        ASSERT(token_array->tokens[3]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    return 0;
}

static int test_strings() {

    /* ----- TEST STRING WITH SPACE ----- */
    {
        char *query = strdup("'John  Doe';");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 2);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "'John  Doe'") == 0);
        ASSERT(token_array->tokens[0]->type == STRING);
        ASSERT(strcasecmp(token_array->tokens[1]->token, ";") == 0);
        ASSERT(token_array->tokens[1]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- TEST STRING WITH SYMBOLS ----- */
    {
        char *query = strdup("'John @#$, () +';");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 2);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "'John @#$, () +'") == 0);
        ASSERT(token_array->tokens[0]->type == STRING);
        ASSERT(strcasecmp(token_array->tokens[1]->token, ";") == 0);
        ASSERT(token_array->tokens[1]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- UNTERMINATED STRING TEST ----- */
    {
        char *query = strdup("'John Doe");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer == NULL);

        tokenizer_free(tokenizer);
    }

    /* ----- BEYOND ORDINARY WHITE SPACE TEST ----- */
    {
        char *query = strdup("SELECT\t*\tFROM\tusers;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 5);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
        ASSERT(token_array->tokens[1]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "FROM") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "users") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ";") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    return 0;
}

static int test_operator_identification() {

    /* --- SINGLE CHARACTER OPERATORS ----- */
    {
        char *query = strdup("+ - * / % = < > ~ ! ;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);
    
        ASSERT(strcasecmp(token_array->tokens[0]->token, "+") == 0);
        ASSERT(token_array->tokens[0]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "-") == 0);
        ASSERT(token_array->tokens[1]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "*") == 0);
        ASSERT(token_array->tokens[2]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "/") == 0);
        ASSERT(token_array->tokens[3]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "%") == 0);
        ASSERT(token_array->tokens[4]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "=") == 0);
        ASSERT(token_array->tokens[5]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "<") == 0);
        ASSERT(token_array->tokens[6]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ">") == 0);
        ASSERT(token_array->tokens[7]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "~") == 0);
        ASSERT(token_array->tokens[8]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "!") == 0);
        ASSERT(token_array->tokens[9]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* --- DOUBLE CHARACTER OPERATORS ----- */
    {
        char *query = strdup("!= <> < <= > >= ;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 7);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "!=") == 0);
        ASSERT(token_array->tokens[0]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "<>") == 0);
        ASSERT(token_array->tokens[1]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "<") == 0);
        ASSERT(token_array->tokens[2]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "<=") == 0);
        ASSERT(token_array->tokens[3]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ">") == 0);
        ASSERT(token_array->tokens[4]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[5]->token, ">=") == 0);
        ASSERT(token_array->tokens[5]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[6]->token, ";") == 0);
        ASSERT(token_array->tokens[6]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- MALFORMED OPERATOR TEST ----- */
    {
        char *query = strdup("!+ <* >/ ;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 7);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "!") == 0);
        ASSERT(token_array->tokens[0]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "+") == 0);
        ASSERT(token_array->tokens[1]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "<") == 0);
        ASSERT(token_array->tokens[2]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "*") == 0);
        ASSERT(token_array->tokens[3]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ">") == 0);
        ASSERT(token_array->tokens[4]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "/") == 0);
        ASSERT(token_array->tokens[5]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[6]->token, ";") == 0);
        ASSERT(token_array->tokens[6]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }
    
    return 0;
}

static int test_parentheses_identification() {
    char *query = strdup("(a + b) * c;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 8);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "(") == 0);
    ASSERT(token_array->tokens[0]->type == PUNCTUATION);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "a") == 0);
    ASSERT(token_array->tokens[1]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "+") == 0);
    ASSERT(token_array->tokens[2]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "b") == 0);
    ASSERT(token_array->tokens[3]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[4]->token, ")") == 0);
    ASSERT(token_array->tokens[4]->type == PUNCTUATION);
    ASSERT(strcasecmp(token_array->tokens[5]->token, "*") == 0);
    ASSERT(token_array->tokens[5]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[6]->token, "c") == 0);
    ASSERT(token_array->tokens[6]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[7]->token, ";") == 0);
    ASSERT(token_array->tokens[7]->type == PUNCTUATION);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 0;
}

static int test_comment_query() {
    char *query = strdup("SELECT * -- comment1_@*+\nFROM table;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 6);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
    ASSERT(token_array->tokens[0]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
    ASSERT(token_array->tokens[1]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "-- comment1_@*+") == 0);
    ASSERT(token_array->tokens[2]->type == COMMENT);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "FROM") == 0);
    ASSERT(token_array->tokens[3]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "table") == 0);
    ASSERT(token_array->tokens[4]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[5]->token, ";") == 0);
    ASSERT(token_array->tokens[5]->type == PUNCTUATION);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 0;
}

static int test_date_query() {
    char *query = strdup("SELECT * FROM table WHERE created_date < '2026-09-21';");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 9);

    
    ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
    ASSERT(token_array->tokens[0]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
    ASSERT(token_array->tokens[1]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "FROM") == 0);
    ASSERT(token_array->tokens[2]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "table") == 0);
    ASSERT(token_array->tokens[3]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "WHERE") == 0);
    ASSERT(token_array->tokens[4]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[5]->token, "created_date") == 0);
    ASSERT(token_array->tokens[5]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[6]->token, "<") == 0);
    ASSERT(token_array->tokens[6]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[7]->token, "'2026-09-21'") == 0);
    ASSERT(token_array->tokens[7]->type == STRING);
    ASSERT(strcasecmp(token_array->tokens[8]->token, ";") == 0);
    ASSERT(token_array->tokens[8]->type == PUNCTUATION);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 0;
}

static int test_timestamp_query() {
    char *query = strdup("SELECT * FROM users WHERE created_at >= '2026-09-21 15:00:00+01';");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array->tokens != NULL);
    ASSERT(token_array->amount_tokens == 9);

    ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
    ASSERT(token_array->tokens[0]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
    ASSERT(token_array->tokens[1]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[2]->token, "FROM") == 0);
    ASSERT(token_array->tokens[2]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[3]->token, "users") == 0);
    ASSERT(token_array->tokens[3]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[4]->token, "WHERE") == 0);
    ASSERT(token_array->tokens[4]->type == KEYWORD);
    ASSERT(strcasecmp(token_array->tokens[5]->token, "created_at") == 0);
    ASSERT(token_array->tokens[5]->type == IDENTIFIER);
    ASSERT(strcasecmp(token_array->tokens[6]->token, ">=") == 0);
    ASSERT(token_array->tokens[6]->type == OPERATOR);
    ASSERT(strcasecmp(token_array->tokens[7]->token, "'2026-09-21 15:00:00+01'") == 0);
    ASSERT(token_array->tokens[7]->type == STRING);
    ASSERT(strcasecmp(token_array->tokens[8]->token, ";") == 0);
    ASSERT(token_array->tokens[8]->type == PUNCTUATION);

    tokenizer_free(tokenizer);
    token_array_free(token_array);
    return 0;
}

static int test_keyword_query() {

    /* ----- TEST BOOLEAN KEYWORDS ----- */
    {
        char *query = strdup("True False true false;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 5);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "True") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "False") == 0);
        ASSERT(token_array->tokens[1]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "true") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "false") == 0);
        ASSERT(token_array->tokens[3]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ";") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }
    
    
    /* ----- IS NULL / IS NOT NULL QUERY ----- */
    {
        char *query = strdup("SELECT username FROM Employees WHERE Department IS NULL AND ManagerID IS NOT NULL;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 14);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "username") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "FROM") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "Employees") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "WHERE") == 0);
        ASSERT(token_array->tokens[4]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "Department") == 0);
        ASSERT(token_array->tokens[5]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "IS") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, "NULL") == 0);
        ASSERT(token_array->tokens[7]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "AND") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "ManagerID") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, "IS") == 0);
        ASSERT(token_array->tokens[10]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[11]->token, "NOT") == 0);
        ASSERT(token_array->tokens[11]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[12]->token, "NULL") == 0);
        ASSERT(token_array->tokens[12]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[13]->token, ";") == 0);
        ASSERT(token_array->tokens[13]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- IN QUERY ----- */
    {
        char *query = strdup("SELECT FirstName, LastName, Department FROM Employees WHERE Department IN ('IT', 'Sales');");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 17);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "FirstName") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ",") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "LastName") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ",") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "Department") == 0);
        ASSERT(token_array->tokens[5]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "FROM") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, "Employees") == 0);
        ASSERT(token_array->tokens[7]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "WHERE") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "Department") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, "IN") == 0);
        ASSERT(token_array->tokens[10]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[11]->token, "(") == 0);
        ASSERT(token_array->tokens[11]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[12]->token, "'IT'") == 0);
        ASSERT(token_array->tokens[12]->type == STRING);
        ASSERT(strcasecmp(token_array->tokens[13]->token, ",") == 0);
        ASSERT(token_array->tokens[13]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[14]->token, "'Sales'") == 0);
        ASSERT(token_array->tokens[14]->type == STRING);
        ASSERT(strcasecmp(token_array->tokens[15]->token, ")") == 0);
        ASSERT(token_array->tokens[15]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[16]->token, ";") == 0);
        ASSERT(token_array->tokens[16]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- BETWEEN QUERY ----- */
    {
        char *query = strdup("SELECT FirstName, LastName, Department FROM Employees WHERE Salary BETWEEN 1000 AND 2000;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 15);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "FirstName") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ",") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "LastName") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ",") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "Department") == 0);
        ASSERT(token_array->tokens[5]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "FROM") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, "Employees") == 0);
        ASSERT(token_array->tokens[7]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "WHERE") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "Salary") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, "BETWEEN") == 0);
        ASSERT(token_array->tokens[10]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[11]->token, "1000") == 0);
        ASSERT(token_array->tokens[11]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[12]->token, "AND") == 0);
        ASSERT(token_array->tokens[12]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[13]->token, "2000") == 0);
        ASSERT(token_array->tokens[13]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[14]->token, ";") == 0);
        ASSERT(token_array->tokens[14]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

        /* ----- SQL DATA TYPE KEYWORDS ----- */
    {
        char *query = strdup(
            "INTEGER INT UNSIGNED NUMERIC FLOAT DOUBLE CHAR VARCHAR "
            "TEXT DATE TIMESTAMP BLOB JSONB BOOL;"
        );

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 15);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "INTEGER") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "INT") == 0);
        ASSERT(token_array->tokens[1]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "UNSIGNED") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "NUMERIC") == 0);
        ASSERT(token_array->tokens[3]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "FLOAT") == 0);
        ASSERT(token_array->tokens[4]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "DOUBLE") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "CHAR") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, "VARCHAR") == 0);
        ASSERT(token_array->tokens[7]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "TEXT") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "DATE") == 0);
        ASSERT(token_array->tokens[9]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[10]->token, "TIMESTAMP") == 0);
        ASSERT(token_array->tokens[10]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[11]->token, "BLOB") == 0);
        ASSERT(token_array->tokens[11]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[12]->token, "JSONB") == 0);
        ASSERT(token_array->tokens[12]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[13]->token, "BOOL") == 0);
        ASSERT(token_array->tokens[13]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[14]->token, ";") == 0);
        ASSERT(token_array->tokens[14]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- CONSTRAINT-RELATED KEYWORDS ----- */
    {
        char *query = strdup(
            "CONSTRAINT PRIMARY FOREIGN KEY UNIQUE CHECK REFERENCES "
            "NOT NULL DEFAULT;"
        );

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "CONSTRAINT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "PRIMARY") == 0);
        ASSERT(token_array->tokens[1]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "FOREIGN") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "KEY") == 0);
        ASSERT(token_array->tokens[3]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "UNIQUE") == 0);
        ASSERT(token_array->tokens[4]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "CHECK") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "REFERENCES") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, "NOT") == 0);
        ASSERT(token_array->tokens[7]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "NULL") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "DEFAULT") == 0);
        ASSERT(token_array->tokens[9]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

        /* ----- CREATE TABLE WITH DATA TYPES AND CONSTRAINTS ----- */
    {
        char *query = strdup(
            "CREATE TABLE users ("
            "id INTEGER PRIMARY KEY, "
            "username VARCHAR NOT NULL UNIQUE, "
            "age INT DEFAULT 18, "
            "account_id INTEGER REFERENCES accounts(id), "
            "CHECK (age >= 18)"
            ");"
        );

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 35);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "CREATE TABLE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        ASSERT(strcasecmp(token_array->tokens[3]->token, "id") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "INTEGER") == 0);
        ASSERT(token_array->tokens[4]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "PRIMARY") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "KEY") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ",") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);

        ASSERT(strcasecmp(token_array->tokens[8]->token, "username") == 0);
        ASSERT(token_array->tokens[8]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "VARCHAR") == 0);
        ASSERT(token_array->tokens[9]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[10]->token, "NOT") == 0);
        ASSERT(token_array->tokens[10]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[11]->token, "NULL") == 0);
        ASSERT(token_array->tokens[11]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[12]->token, "UNIQUE") == 0);
        ASSERT(token_array->tokens[12]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[13]->token, ",") == 0);
        ASSERT(token_array->tokens[13]->type == PUNCTUATION);

        ASSERT(strcasecmp(token_array->tokens[14]->token, "age") == 0);
        ASSERT(token_array->tokens[14]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[15]->token, "INT") == 0);
        ASSERT(token_array->tokens[15]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[16]->token, "DEFAULT") == 0);
        ASSERT(token_array->tokens[16]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[17]->token, "18") == 0);
        ASSERT(token_array->tokens[17]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[18]->token, ",") == 0);
        ASSERT(token_array->tokens[18]->type == PUNCTUATION);

        ASSERT(strcasecmp(token_array->tokens[19]->token, "account_id") == 0);
        ASSERT(token_array->tokens[19]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[20]->token, "INTEGER") == 0);
        ASSERT(token_array->tokens[20]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[21]->token, "REFERENCES") == 0);
        ASSERT(token_array->tokens[21]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[22]->token, "accounts") == 0);
        ASSERT(token_array->tokens[22]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[23]->token, "(") == 0);
        ASSERT(token_array->tokens[23]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[24]->token, "id") == 0);
        ASSERT(token_array->tokens[24]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[25]->token, ")") == 0);
        ASSERT(token_array->tokens[25]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[26]->token, ",") == 0);
        ASSERT(token_array->tokens[26]->type == PUNCTUATION);

        ASSERT(strcasecmp(token_array->tokens[27]->token, "CHECK") == 0);
        ASSERT(token_array->tokens[27]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[28]->token, "(") == 0);
        ASSERT(token_array->tokens[28]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[29]->token, "age") == 0);
        ASSERT(token_array->tokens[29]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[30]->token, ">=") == 0);
        ASSERT(token_array->tokens[30]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[31]->token, "18") == 0);
        ASSERT(token_array->tokens[31]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[32]->token, ")") == 0);
        ASSERT(token_array->tokens[32]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[33]->token, ")") == 0);
        ASSERT(token_array->tokens[33]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[34]->token, ";") == 0);
        ASSERT(token_array->tokens[34]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    return 0;
}

static int test_aggregate_function_query() {

    /* ----- COUNT() QUERY -----*/
    {
        char *query = strdup("SELECT\n \
                        COUNT(*) AS total_employees,\n \
                        FROM employees;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "COUNT") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "*") == 0);
        ASSERT(token_array->tokens[3]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ")") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "AS") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "total_employees") == 0);
        ASSERT(token_array->tokens[6]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ",") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "FROM") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "employees") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- SUM() QUERY -----*/
    {
        char *query = strdup("SELECT\n \
                        SUM(salary) AS total_payroll,\n \
                        FROM employees;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "SUM") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "salary") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ")") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "AS") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "total_payroll") == 0);
        ASSERT(token_array->tokens[6]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ",") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "FROM") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "employees") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- AVG() QUERY -----*/
    {
        char *query = strdup("SELECT\n \
                        AVG(salary) AS average_salary,\n \
                        FROM employees;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "AVG") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "salary") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ")") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "AS") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "average_salary") == 0);
        ASSERT(token_array->tokens[6]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ",") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "FROM") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "employees") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- MIN() QUERY -----*/
    {
        char *query = strdup("SELECT\n \
                        MIN(salary) AS lowest_salary,\n \
                        FROM employees;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);

        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "MIN") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "salary") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ")") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "AS") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "lowest_salary") == 0);
        ASSERT(token_array->tokens[6]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ",") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "FROM") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "employees") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- MAX() QUERY -----*/
    {
        char *query = strdup("SELECT\n \
                        MAX(salary) AS highest_salary,\n \
                        FROM employees;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "MAX") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "(") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "salary") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ")") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "AS") == 0);
        ASSERT(token_array->tokens[5]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "highest_salary") == 0);
        ASSERT(token_array->tokens[6]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ",") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "FROM") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "employees") == 0);
        ASSERT(token_array->tokens[9]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }
    
    return 0;
}

static int test_empty_query() {
    char *query = strdup("");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer == NULL);

    tokenizer_free(tokenizer);
    return 0;
}

static int test_invalid_character_query() {
    char *query = strdup("SELECT @ FROM table;");

    Tokenizer *tokenizer = tokenizer_init(query);
    ASSERT(tokenizer != NULL);

    TokenArray *token_array = tokenize_query(tokenizer);
    ASSERT(token_array == NULL);

    tokenizer_free(tokenizer);
    return 0;
}

static int test_dml_queries() {

    /* ----- SELECT QUERY ----- */
    {
        char *query = strdup("SELECT * FROM users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 5);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "SELECT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "*") == 0);
        ASSERT(token_array->tokens[1]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "FROM") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "users") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, ";") == 0);
        ASSERT(token_array->tokens[4]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- UPDATE QUERY ----- */
    {
        char *query = strdup("UPDATE table SET col1 = 1.23 WHERE id = 1;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 11);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "UPDATE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "table") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "SET") == 0);
        ASSERT(token_array->tokens[2]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "col1") == 0);
        ASSERT(token_array->tokens[3]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "=") == 0);
        ASSERT(token_array->tokens[4]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "1.23") == 0);
        ASSERT(token_array->tokens[5]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "WHERE") == 0);
        ASSERT(token_array->tokens[6]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[7]->token, "id") == 0);
        ASSERT(token_array->tokens[7]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "=") == 0);
        ASSERT(token_array->tokens[8]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "1") == 0);
        ASSERT(token_array->tokens[9]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[10]->token, ";") == 0);
        ASSERT(token_array->tokens[10]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- INSERT QUERY ----- */
    {
        char *query = strdup("INSERT INTO table (col1, col2) VALUES ('John', 'Doe');");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 15);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "INSERT") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "INTO") == 0);
        ASSERT(token_array->tokens[1]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "table") == 0);
        ASSERT(token_array->tokens[2]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "(") == 0);
        ASSERT(token_array->tokens[3]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "col1") == 0);
        ASSERT(token_array->tokens[4]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[5]->token, ",") == 0);
        ASSERT(token_array->tokens[5]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "col2") == 0);
        ASSERT(token_array->tokens[6]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ")") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[8]->token, "VALUES") == 0);
        ASSERT(token_array->tokens[8]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[9]->token, "(") == 0);
        ASSERT(token_array->tokens[9]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[10]->token, "'John'") == 0);
        ASSERT(token_array->tokens[10]->type == STRING);
        ASSERT(strcasecmp(token_array->tokens[11]->token, ",") == 0);
        ASSERT(token_array->tokens[11]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[12]->token, "'Doe'") == 0);
        ASSERT(token_array->tokens[12]->type == STRING);
        ASSERT(strcasecmp(token_array->tokens[13]->token, ")") == 0);
        ASSERT(token_array->tokens[13]->type == PUNCTUATION);
        ASSERT(strcasecmp(token_array->tokens[14]->token, ";") == 0);
        ASSERT(token_array->tokens[14]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- DELETE QUERY ----- */
    {
        char *query = strdup("DELETE FROM table WHERE age < 18;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 8);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "DELETE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "FROM") == 0);
        ASSERT(token_array->tokens[1]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[2]->token, "table") == 0);
        ASSERT(token_array->tokens[2]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[3]->token, "WHERE") == 0);
        ASSERT(token_array->tokens[3]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[4]->token, "age") == 0);
        ASSERT(token_array->tokens[4]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[5]->token, "<") == 0);
        ASSERT(token_array->tokens[5]->type == OPERATOR);
        ASSERT(strcasecmp(token_array->tokens[6]->token, "18") == 0);
        ASSERT(token_array->tokens[6]->type == NUMBER);
        ASSERT(strcasecmp(token_array->tokens[7]->token, ";") == 0);
        ASSERT(token_array->tokens[7]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    return 0;
}

static int test_ddl_queries() {

    /* ----- CREATE TABLE ----- */
    {
        char *query = strdup("CREATE TABLE users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "CREATE TABLE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- ALTER TABLE ----- */
    {
        char *query = strdup("ALTER TABLE users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "ALTER TABLE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- DROP TABLE ----- */
    {
        char *query = strdup("DROP TABLE users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "DROP TABLE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- TRUNCATE TABLE ----- */
    {
        char *query = strdup("TRUNCATE TABLE users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "TRUNCATE TABLE") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- CREATE INDEX ----- */
    {
        char *query = strdup("CREATE INDEX users_idx;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "CREATE INDEX") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users_idx") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- DROP INDEX ----- */
    {
        char *query = strdup("DROP INDEX users_idx;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "DROP INDEX") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users_idx") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- GROUP BY ----- */
    {
        char *query = strdup("GROUP BY users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "GROUP BY") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- ORDER BY ----- */
    {
        char *query = strdup("ORDER BY users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array->tokens != NULL);
        ASSERT(token_array->amount_tokens == 3);

        ASSERT(strcasecmp(token_array->tokens[0]->token, "ORDER BY") == 0);
        ASSERT(token_array->tokens[0]->type == KEYWORD);
        ASSERT(strcasecmp(token_array->tokens[1]->token, "users") == 0);
        ASSERT(token_array->tokens[1]->type == IDENTIFIER);
        ASSERT(strcasecmp(token_array->tokens[2]->token, ";") == 0);
        ASSERT(token_array->tokens[2]->type == PUNCTUATION);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }


    /* ---------- INVALID DOUBLE KEYWORD DDL QUERIES ---------- */

    /* ----- CREATE users ----- */
    {
        char *query = strdup("CREATE users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array == NULL);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- ALTER users ----- */
    {
        char *query = strdup("ALTER users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array == NULL);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- DROP users ----- */
    {
        char *query = strdup("DROP users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array == NULL);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- GROUP users ----- */
    {
        char *query = strdup("GROUP users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array == NULL);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- ORDER users ----- */
    {
        char *query = strdup("ORDER users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array == NULL);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    /* ----- TRUNCATE users ----- */
    {
        char *query = strdup("TRUNCATE users;");

        Tokenizer *tokenizer = tokenizer_init(query);
        ASSERT(tokenizer != NULL);

        TokenArray *token_array = tokenize_query(tokenizer);
        ASSERT(token_array == NULL);

        tokenizer_free(tokenizer);
        token_array_free(token_array);
    }

    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;
    printf("> STARTING TESTS\n");

    result = test_identifier_format();
    generate_output(result, 0, "test_identifier_format");
    result = test_numbers();
    generate_output(result, 1, "test_numbers");
    result = test_strings();
    generate_output(result, 2, "test_strings");
    result = test_operator_identification();
    generate_output(result, 3, "test_operator_identification");
    result = test_parentheses_identification();
    generate_output(result, 4, "test_parentheses_identification");
    result = test_comment_query();
    generate_output(result, 5, "test_comment_query");
    result = test_date_query();
    generate_output(result, 6, "test_date_query");
    result = test_timestamp_query();
    generate_output(result, 7, "test_timestamp_query");
    result = test_keyword_query();
    generate_output(result, 8, "test_keyword_query");
    result = test_aggregate_function_query();
    generate_output(result, 9, "test_aggregate_function_query");
    result = test_empty_query();
    generate_output(result, 10, "test_empty_query");
    result = test_invalid_character_query();
    generate_output(result, 11, "test_invalid_character_query");
    result = test_dml_queries();
    generate_output(result, 12, "test_dml_queries");
    result = test_ddl_queries();
    generate_output(result, 13, "test_ddl_queries");
    
    printf("> TESTS RAN SUCCESSFULLY\n");
    return 0;
}