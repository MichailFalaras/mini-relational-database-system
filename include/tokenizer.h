#ifndef TOKENIZER_H_
#define TOKENIZER_H_

#include <stdint.h>
#include <stdbool.h>

/* Token types. */
typedef enum token_type {
	WHITESPACE,
	KEYWORD,  
	IDENTIFIER, 
	NUMBER, 
	STRING, 
	OPERATOR, 
	PUNCTUATION, 
	COMMENT 
} TokenType;

/* Token struct represented by the token
itself and its type. */
typedef struct token_struct {
	char *token;
	TokenType type;
} Token;

/* Array of tokens being sent to parser. */
typedef struct token_array {
	Token **tokens;
	uint32_t amount_tokens;
} TokenArray;

/* Tokenizer component containing:
 * query: the query input string
 * index: index in query
 * length: query length*/
typedef struct tokenizer {
    char *query;
    uint32_t current_position;
    uint32_t length;
} Tokenizer;

extern Tokenizer *tokenizer_init(char *query);

extern TokenArray *token_array_create();

extern Token *token_create(char *token, TokenType type);

extern void token_array_push(TokenArray *token_array, Token *token);

extern TokenArray *tokenize_query(Tokenizer *tokenizer);

extern Token *read_token(Tokenizer *tokenizer);

extern void tokenizer_free(Tokenizer *tokenizer);

extern void token_array_free(TokenArray *token_array);

extern void token_free(Token *token);

#endif