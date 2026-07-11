#include <stdio.h>
#include <stdlib.h>
#include "../../include/parser.h"
#include "../../include/tokenizer.h"

Parser *parser_init(TokenArray *token_array) {

    if (token_array == NULL) {
        return NULL;
    }

    Parser *parser = (Parser *) malloc(sizeof(Parser));
    if (parser == NULL) {
        perror("Parser");
        exit(1);
    }

    parser->token_array = token_array;
    parser->current_position = 0;

    return parser;
}

void parser_free(Parser *parser) {
    if (parser != NULL) {
        token_array_free(parser->token_array);
        free(parser);
    }
}

