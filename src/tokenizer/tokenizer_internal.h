#ifndef UTILS_H_
#define UTILS_H_
#include "tokenizer.h"

extern char *read_query();

extern char *expand_buffer(char *buffer, int size);

extern char *move_tokenizer(Tokenizer *tokenizer, char *buffer, int *size);

extern bool isoperator(char c);

extern bool ispunctuation(char c);

extern bool iskeyword(char *token, bool *double_token_keyword);

extern Token *digit_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size);

extern Token *string_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size);

extern Token *operator_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size);

extern Token *punctuation_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size);

extern Token *keyword_identifier_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size);

#endif