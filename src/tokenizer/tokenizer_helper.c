#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include "tokenizer_internal.h"
#include "../../include/tokenizer.h"

/* Read query. Supports one-line & multi-line queries. */
char *read_query() {
    char temp[64];
    char *buffer = NULL, *new_buffer;
    int n;
    int buffer_size = 0, old_buffer_size = 0;
    bool comment = false;

    /* Reads from stdin and stores in dynamically allocated buffer. */
    while ((n = read(STDIN_FILENO, temp, 64)) > 0) {
        old_buffer_size = buffer_size;
        buffer_size += n;
        new_buffer = (char *) realloc(buffer, buffer_size);
        if (new_buffer == NULL) {
            printf("Memory error.\n");
            exit(1);
        }
        buffer = new_buffer;

        memcpy(buffer + old_buffer_size, temp, n);

        /* Check if theres "--" comment.*/
        char *temp = strchr(buffer+old_buffer_size, '-');
        if (temp != NULL) {
            if (*(temp+1) == '-') {
                comment = true;
            }
        }

         /* Remove '\n' from multi-line queries.
        Replace with ' '. */

        /* If there's comments before dont remove '\n'
        so that we know when the comments stop. */
        temp = strchr(buffer+old_buffer_size, '\n');
        if ((temp != NULL) && (comment == false)) {
            *temp = ' ';
        }
    }
    if (n < 0) {
        perror("read");
        exit(1);
    }

    /* If EOF and buffer isn't empty then add terminating null byte. */
    if (buffer != NULL) {
        *(buffer + buffer_size-1) = '\0';
    }
    
    return buffer;
}

/* Expand token buffer. */
char *expand_buffer(char *buffer, int size) {
    char *new_buffer = realloc(buffer, size);
    if (new_buffer == NULL) {
        printf("Memory error.\n");
        exit(1);
    }

    return new_buffer;
}

/* Move tokenizer position forward while also expanding token buffer. */
char *move_tokenizer(Tokenizer *tokenizer, char *buffer, int *size) {
    
    /* EOF */
    if (tokenizer->current_position >= tokenizer->length){
        return buffer;
    }

    (*size)++;
    tokenizer->current_position++;
    buffer = expand_buffer(buffer, *size);
    
    buffer[*size-1] = tokenizer->query[tokenizer->current_position];

    return buffer;
}

/* Check if operator. */
bool isoperator(char c) {
    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '=':
        case '<':
        case '>':
        case '!': return true;
        default: break;
    }

    return false;
}

/* Check if punctuation */
bool ispunctuation(char c) {
    switch (c) {
        case ',':
        case '(':
        case ')':
        case ';':
        case '.': return true;
        default: break;
    }

    return false;
}

/* Check if One/Double Token Keyword. */
bool iskeyword(char *token, bool *double_token_keyword) {

    if (strcasecmp(token, "CREATE TABLE") == 0) {
        return true;
    } else if (strcasecmp(token, "ALTER TABLE") == 0) {
        return true;
    } else if (strcasecmp(token, "TRUNCATE TABLE") == 0) {
        return true;
    } else if (strcasecmp(token, "DROP TABLE") == 0) {
        return true;
    } else if (strcasecmp(token, "CREATE INDEX") == 0) {
        return true;
    } else if (strcasecmp(token, "DROP INDEX") == 0) {
        return true;
    } else if (strcasecmp(token, "GROUP BY") == 0) {
        return true;
    }  else if (strcasecmp(token, "ORDER BY") == 0) {
        return true;
    }
    
    /* Identify that it has the first token as KEYWORD. */
    if (strcasecmp(token, "SELECT") == 0) {
        return true;
    } else if (strcasecmp(token, "INSERT") == 0) {
        return true;
    } else if (strcasecmp(token, "UPDATE") == 0) {
        return true;
    } else if (strcasecmp(token, "DELETE") == 0) {
        return true;
    } else if (strcasecmp(token, "CREATE") == 0) {
        *double_token_keyword = true;
        return true;
    } else if (strcasecmp(token, "ALTER") == 0) {
        *double_token_keyword = true;
        return true;
    } else if (strcasecmp(token, "TRUNCATE") == 0) {
        *double_token_keyword = true;
        return true;
    } else if (strcasecmp(token, "DROP") == 0) {
        *double_token_keyword = true;
        return true;
    } else if (strcasecmp(token, "FROM") == 0) {
        return true;
    } else if (strcasecmp(token, "WHERE") == 0) {
        return true;
    } else if (strcasecmp(token, "GROUP") == 0) {
        *double_token_keyword = true;
        return true;
    } else if (strcasecmp(token, "HAVING") == 0) {
        return true;
    } else if (strcasecmp(token, "ORDER") == 0) {
        *double_token_keyword = true;
        return true;
    } else if (strcasecmp(token, "JOIN") == 0) {
        return true;
    } else if (strcasecmp(token, "ON") == 0) {
        return true;
    } else if (strcasecmp(token, "LIMIT") == 0) {
        return true;
    } else if (strcasecmp(token, "OFFSET") == 0) {
        return true;
    } else if (strcasecmp(token, "INTO") == 0) {
        return true;
    } else if (strcasecmp(token, "VALUES") == 0) {
        return true;
    } else if (strcasecmp(token, "SET") == 0) {
        return true;
    } 

    return false;
}

/* Handles reading query for digits/numbers. */
Token *digit_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    bool decimal_found = false;

    while (isdigit(tokenizer->query[tokenizer->current_position]) ||
     (!decimal_found && tokenizer->query[tokenizer->current_position] == '.' )) {
        if (tokenizer->query[tokenizer->current_position] == '.') {
            decimal_found = true;
        }
        
        buffer = move_tokenizer(tokenizer, buffer, buffer_size);
    }

    /* Make token a string. */
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, NUMBER);
}

/* Handles reading query for strings. */
Token *string_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    do {
        if (tokenizer->current_position == tokenizer->length) {
            return NULL;
        }

        buffer = move_tokenizer(tokenizer, buffer, buffer_size); 
    } while (tokenizer->query[tokenizer->current_position] != '\'');
        
    buffer = move_tokenizer(tokenizer, buffer, buffer_size); 
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, STRING);
}

/* Handles reading query for operators and comments. */
Token *operator_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    bool peek_forwards = false;
    switch (tokenizer->query[tokenizer->current_position]) {
        case '-':
        case '!':
        case '<':
        case '>':
            peek_forwards = true;
            break;
        default: break;
    }

    if (peek_forwards && ((tokenizer->current_position+1) < tokenizer->length)
            && (isoperator(tokenizer->query[tokenizer->current_position+1]))) {
        
        if (buffer[(*buffer_size)-1] == '-') {
            while (tokenizer->query[tokenizer->current_position] != '\n') {

                if (tokenizer->current_position == tokenizer->length) {
                    return NULL;
                }

                buffer = move_tokenizer(tokenizer, buffer, buffer_size);
            }

            buffer[(*buffer_size)-1] = '\0';
            tokenizer->current_position++;
            return token_create(buffer, COMMENT);
        } else {
            buffer = move_tokenizer(tokenizer, buffer, buffer_size);
        }
    }

    buffer = move_tokenizer(tokenizer, buffer, buffer_size);
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, OPERATOR);
}

/* Handles reading query for punctuation. */
Token *punctuation_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    
    buffer = move_tokenizer(tokenizer, buffer, buffer_size);
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, PUNCTUATION);
}

/* Handles reading query for keywords/identifiers. */
Token *keyword_identifier_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    while (tokenizer->query[tokenizer->current_position] != ' ') {
        buffer = move_tokenizer(tokenizer, buffer, buffer_size); 

        if (!(isalnum(buffer[(*buffer_size)-1]) || (buffer[(*buffer_size)-1] == '_'))) {
            buffer[(*buffer_size)-1] = ' ';
            break;
        }
    }

    bool double_token_keyword = false;
    buffer[(*buffer_size)-1] = '\0';
    if (iskeyword(buffer, &double_token_keyword)) {
        if (double_token_keyword) {
            buffer[(*buffer_size)-1] = ' ';

            do {
                buffer = move_tokenizer(tokenizer, buffer, buffer_size);       
            } while (tokenizer->query[tokenizer->current_position] != ' ');
        
            buffer[(*buffer_size)-1] = '\0';
            if (!(iskeyword(buffer, &double_token_keyword))) {
                return NULL; // Invalid
            }
        } 

        return token_create(buffer, KEYWORD);
    } else {
        // If it isn't a keyword, its an identifier
        return token_create(buffer, IDENTIFIER);
    }
}