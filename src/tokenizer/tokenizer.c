#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include "../../include/tokenizer.h"
#include "tokenizer_internal.h"

/* Tokenizer Initialization. */
Tokenizer *tokenizer_init(char *query) {

    /* If there's no query to begin with, return NULL. */
    if (query == NULL) {
        return NULL;
    }

    /* Search for query terminating semicolon. */
    if (strchr(query, ';') == NULL) {
        return NULL;
    }

    /* Allocate memory for tokenizer. */
    Tokenizer *tokenizer = (Tokenizer *) malloc(sizeof(Tokenizer));
    if (tokenizer == NULL) {
        printf("Memory error.\n");
        exit(1);
    }

    tokenizer->query = query;
    tokenizer->current_position = 0;
    tokenizer->length = strlen(tokenizer->query);

    return tokenizer;
}

/* Dynamically allocate memory for TokenArray. */
TokenArray *token_array_create() {
    TokenArray *token_array = (TokenArray *) malloc(sizeof(TokenArray));
    if (token_array == NULL) {
        printf("Memory error.\n");
        exit(1);
    }

    token_array->tokens = NULL;
    token_array->amount_tokens = 0;

    return token_array;
}

/* Dynamically allocate memory for Token. */
Token *token_create(char *token, TokenType type) {
    Token *token_struct = (Token *) malloc(sizeof(Token));
    if (token == NULL) {
        printf("Memory error.\n");
        exit(1);
    }

    token_struct->token = token;
    token_struct->type = type;

    return token_struct;
}

/* TokenArray reallocation to store pointer for new token. */
void token_array_push(TokenArray *token_array, Token *token) {

    token_array->amount_tokens++;
    Token **new_token_array = (Token **) realloc(token_array->tokens,
                                         (token_array->amount_tokens)*sizeof(Token*));
    if (new_token_array == NULL) {
        printf("Memory error.\n");
        exit(1);
    }
    token_array->tokens = new_token_array;

    token_array->tokens[token_array->amount_tokens-1] = token;
}

/* Tokenize query. */
TokenArray *tokenize_query(Tokenizer *tokenizer) {
    TokenArray *token_array = token_array_create();

    while (tokenizer->current_position < tokenizer->length) {

        if (tokenizer->query[tokenizer->current_position] == ' ') {
            tokenizer->current_position++;
            continue;
        }

        Token *token = read_token(tokenizer);

        /* If one token is invalid, then query fails. (EOF or Invalid Token) */
        if (token == NULL) {
            free(token_array);
            return NULL;
        }

        token_array_push(token_array, token);
    }

    return token_array;
}

/* Read token and figure out the TokenType. */
Token *read_token(Tokenizer *tokenizer) {
    Token *token = NULL;

    char *buffer = NULL;
    int buffer_size = 1;

    buffer = expand_buffer(buffer, buffer_size);
    buffer[buffer_size-1] = tokenizer->query[tokenizer->current_position];

    if (isdigit(tokenizer->query[tokenizer->current_position])) {
        token = digit_handling(tokenizer, buffer, &buffer_size);
    } else if (tokenizer->query[tokenizer->current_position] == '\'') {
        token = string_handling(tokenizer, buffer, &buffer_size);
    } else if (isoperator(tokenizer->query[tokenizer->current_position])) {
        token = operator_handling(tokenizer, buffer, &buffer_size);
    } else if (ispunctuation(tokenizer->query[tokenizer->current_position])) {
        token = punctuation_handling(tokenizer, buffer, &buffer_size);
    } else {
        if (isalpha(buffer[buffer_size-1])) {
            token = keyword_identifier_handling(tokenizer, buffer, &buffer_size);
        }
    }

    return token;
}

/* Deallocate Tokenizer Memory. */
void tokenizer_free(Tokenizer *tokenizer) {
    if (tokenizer != NULL) {
        free(tokenizer->query);
        free(tokenizer);
    }
}

/* Deallocate TokenArray & Token memory. */
void token_array_free(TokenArray *token_array) {
    if (token_array != NULL) {
        if (token_array->tokens != NULL) {
            for (int i = 0; i < token_array->amount_tokens; i++) {
                token_free(token_array->tokens[i]);
                free(token_array->tokens[i]);
            }

            free(token_array->tokens);
        }

        free(token_array);
    }
}

void token_free(Token *token_struct) {
    if (token_struct != NULL) {
        free(token_struct->token);
    }
}