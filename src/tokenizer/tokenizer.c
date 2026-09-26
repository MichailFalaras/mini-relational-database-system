#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include "../../include/tokenizer.h"
#include "tokenizer_utils.h"

/* Tokenizer Initialization. */
Tokenizer *tokenizer_init(char *query) {

    /* If there's no query to begin with, return NULL. */
    if (!query) {
        return NULL;
    }

    /* Search for query terminating semicolon. */
    if (!strchr(query, ';')) {
        return NULL;
    }

    /* Allocate memory for tokenizer. */
    Tokenizer *tokenizer = (Tokenizer *) calloc(1, sizeof(Tokenizer));
    if (!tokenizer) {
        return NULL;
    }

    tokenizer->query = query;
    tokenizer->current_position = 0;
    tokenizer->length = strlen(tokenizer->query);

    return tokenizer;
}

/* Dynamically allocate memory for TokenArray. */
TokenArray *token_array_create() {
    TokenArray *token_array = (TokenArray *) calloc(1, sizeof(TokenArray));
    if (!token_array) {
        return NULL;
    }

    token_array->tokens = NULL;
    token_array->amount_tokens = 0;

    return token_array;
}

/* Dynamically allocate memory for Token. */
Token *token_create(char *token, TokenType type) {
    if (!token || type > COMMENT) {
        return NULL;
    }

    Token *token_struct = (Token *) calloc(1, sizeof(Token));
    if (!token_struct) {
        return NULL;
    }

    token_struct->token = token;
    token_struct->type = type;

    return token_struct;
}

/* TokenArray reallocation to store pointer for new token. */
void token_array_push(TokenArray *token_array, Token *token) {
    if (!token_array || !token) {
        return;
    }

    token_array->amount_tokens++;
    Token **new_token_array = (Token **) realloc(token_array->tokens,
                                         (token_array->amount_tokens)*sizeof(Token*));
    if (!new_token_array) {
        return;
    }
    token_array->tokens = new_token_array;

    token_array->tokens[token_array->amount_tokens-1] = token;
}

/* Tokenize query. */
TokenArray *tokenize_query(Tokenizer *tokenizer) {
    if (!tokenizer || !tokenizer->query || !tokenizer->length) {
        return NULL;
    }

    TokenArray *token_array = token_array_create();
    if (!token_array) {
        return NULL;
    }

    while (tokenizer->current_position < tokenizer->length) {

        if (isspace(tokenizer->query[tokenizer->current_position])) {
            tokenizer->current_position++;
            continue;
        }

        /* Read token. */
        Token *token = read_token(tokenizer);

        /* If one token is invalid, then query fails. (EOF or Invalid Token) */
        if (!token) {
            free(token_array);
            return NULL;
        }

        token_array_push(token_array, token);
    }

    return token_array;
}

/* Read token and figure out the TokenType. */
Token *read_token(Tokenizer *tokenizer) {
    if (!tokenizer || !tokenizer->query || !tokenizer->length) {
        return NULL;
    }

    /* Buffer data. */
    char *buffer = NULL;
    int buffer_size = 1;

    /* Initialize buffer. */
    buffer = calloc(1, sizeof(char));
    if (!buffer) {
        return NULL;
    }
    buffer[buffer_size-1] = tokenizer->query[tokenizer->current_position];

    /* Identify Token Type. */
    bool peek_forwards = false; // Only for operator handling
    if (isdigit(tokenizer->query[tokenizer->current_position])) {
        return digit_handling(tokenizer, buffer, &buffer_size);

    } else if (tokenizer->query[tokenizer->current_position] == '\'') {
        return string_handling(tokenizer, buffer, &buffer_size);

    } else if (isoperator(tokenizer->query[tokenizer->current_position], &peek_forwards)) {
        return operator_handling(tokenizer, peek_forwards, buffer, &buffer_size);

    } else if (ispunctuation(tokenizer->query[tokenizer->current_position])) {
        return punctuation_handling(tokenizer, buffer, &buffer_size);

    } else {
        if (isalpha(buffer[buffer_size-1]) || buffer[buffer_size-1] == '_') {
            return keyword_identifier_handling(tokenizer, buffer, &buffer_size);
        }
    }

    return NULL;
}

/* Deallocate Tokenizer Memory. */
void tokenizer_free(Tokenizer *tokenizer) {
    if (tokenizer) {
        if (tokenizer->query) {
            free(tokenizer->query);
        }

        free(tokenizer);
    }
}

/* Deallocate TokenArray & Token memory. */
void token_array_free(TokenArray *token_array) {
    if (token_array) {
        if (token_array->tokens) {
            for (int i = 0; i < token_array->amount_tokens; i++) {
                if (token_array->tokens[i]) {
                    token_free(token_array->tokens[i]);
                }   
            }

            free(token_array->tokens);
        }

        free(token_array);
    }
}

/* Deallocate single Token memory. */
void token_free(Token *token_struct) {
    if (token_struct) {
        if (token_struct->token) {
            free(token_struct->token);
        }

        free(token_struct);
    }
}