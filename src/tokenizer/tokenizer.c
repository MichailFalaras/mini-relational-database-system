#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../include/tokenizer.h"
#include "../../include/utils.h"

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

/* Deallocate Tokenizer Memory. */
void tokenizer_free(Tokenizer *tokenizer) {
    if (tokenizer != NULL) {
        free(tokenizer->query);
        free(tokenizer);
    }
}