#ifndef TOKENIZER_H_
#define TOKENIZER_H_

#include <stdint.h>

/* Token types. */
typedef enum token_type {
	WHITESPACE,
	KEYWORD,  
	IDENTIFIER, 
	DIGIT, 
	STRING, 
	OPERATOR, 
	PUNCTUATION, 
	COMMENT 
} TokenType;

/* Token struct represented by the token
itself and its type. */
typedef struct token{
	char *token;
	TokenType type;
} Token;

/* Tokenizer component containing:
 * query: the query input string
 * index: index in query
 * length: query length*/
typedef struct tokenizer {
    char *query;
    uint32_t current_position;
    uint32_t length;
} Tokenizer;

#endif