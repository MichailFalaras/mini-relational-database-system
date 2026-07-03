#ifndef PARSER_H_
#define PARSER_H_

#include <stdint.h>
#include "tokenizer.h"

/* Parser component containing:
 * query_tokens: array of tokens
 * index: index of token array*/
typedef struct parser {
    Token *query_tokens;
    uint32_t current_position;
} Parser;

/* TODO: Abstract Syntax Tree Node Structs */

#endif