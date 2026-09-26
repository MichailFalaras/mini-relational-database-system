#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include "tokenizer_utils.h"
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
        Replace with whitespace. */

        /* If there's comments before dont remove '\n'
        so that we know when the comments stop. */
        temp = strchr(buffer+old_buffer_size, '\n');
        if ((temp != NULL) && (comment == false)) {
            *temp = ' ';
        }
    }
    if (n < 0) {
        return NULL;
    }

    /* If EOF and buffer isn't empty then add terminating null byte. */
    if (buffer != NULL) {
        *(buffer + buffer_size-1) = '\0';
    }
    
    return buffer;
}

/* Expand token buffer. */
char *expand_buffer(char *buffer, int size) {
    if (!buffer || !size) {
        return NULL;
    }

    char *new_buffer = realloc(buffer, size);
    if (!new_buffer) {
        return NULL;
    }

    return new_buffer;
}

/* Move tokenizer position forward while also expanding token buffer. */
char *move_tokenizer(Tokenizer *tokenizer, char *buffer, int *size) {
    if (!tokenizer || !tokenizer->query
        || !tokenizer->length
        || !buffer || !size) {
        return NULL;
    }

    /* Return NULL and free buffer to signal EOF.
     *
     * When we are moving tokenizer, this should never happen.
     * It's an invalid state. To end token identification it should find ';'
     * before ending. */
    if (tokenizer->current_position >= tokenizer->length){
        if (buffer) {
            free(buffer);
        }

        return NULL;
    }

    (*size)++;
    tokenizer->current_position++;
    buffer = expand_buffer(buffer, *size);
    if (!buffer) {
        return NULL;
    }
    
    buffer[*size-1] = tokenizer->query[tokenizer->current_position];

    return buffer;
}

/* Check if operator. */
bool isoperator(char c, bool *peek_forward) {
    if (c == '\0' || !peek_forward) {
        return false;
    }

    /* If peek_forward is already true, we have less
     * valid operator characters to read. */
    if (*peek_forward) {
        switch (c) {
            case '-':
            case '=':
            case '>':
                return true;
            default:
                return false;
        }
    }

    /* First time checking for operator. */
    switch (c) {
        case '-':
        case '!':
        case '<':
        case '>':
            *peek_forward = true;
        case '+':
        case '*':
        case '/':
        case '%':
        case '=':
        case '~':
            return true;
        default:
            return false;
    } 

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
    if (!token || !double_token_keyword) {
        return false;
    }

    /* Double Token KEYWORDs. */
    if (!strcasecmp(token, "CREATE TABLE")
        || !strcasecmp(token, "ALTER TABLE") 
        || !strcasecmp(token, "TRUNCATE TABLE") 
        || !strcasecmp(token, "DROP TABLE") 
        || !strcasecmp(token, "CREATE INDEX") 
        || !strcasecmp(token, "DROP INDEX") 
        || !strcasecmp(token, "GROUP BY") 
        || !strcasecmp(token, "ORDER BY")) {
        return true;
    }

    /* Trigger double token search. */
    if (!strcasecmp(token, "CREATE")
        || !strcasecmp(token, "ALTER")
        || !strcasecmp(token, "TRUNCATE")
        || !strcasecmp(token, "DROP")
        || !strcasecmp(token, "GROUP")
        || !strcasecmp(token, "ORDER")) {
        *double_token_keyword = true;
        return true;
    }

    /* Identify that it has the first token as KEYWORD. */
    if (!strcasecmp(token, "SELECT")
        || !strcasecmp(token, "INSERT")
        || !strcasecmp(token, "UPDATE")
        || !strcasecmp(token, "DELETE")
        || !strcasecmp(token, "FROM")
        || !strcasecmp(token, "WHERE")
        || !strcasecmp(token, "HAVING")
        || !strcasecmp(token, "JOIN")
        || !strcasecmp(token, "ON")
        || !strcasecmp(token, "LIMIT")
        || !strcasecmp(token, "OFFSET")
        || !strcasecmp(token, "INTO")
        || !strcasecmp(token, "VALUES")
        || !strcasecmp(token, "SET")) {
        *double_token_keyword = false;
        return true;
    }
    
    // Boolean Literals are KEYWORDs but not KEYWORDs that organize/comprise query
    if (!strcasecmp(token, "TRUE")
        || !strcasecmp(token, "FALSE")
        || !strcasecmp(token, "AND")
        || !strcasecmp(token, "OR")
        || !strcasecmp(token, "NOT")) {
        *double_token_keyword = false;
        return true;
    }

    return false;
}

/* Handles reading query for digits/numbers. */
Token *digit_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    if (!tokenizer || !tokenizer->query 
        || !tokenizer->length
        || !buffer || !buffer_size) {
        return NULL;
    }
    bool decimal_found = false;

    while (isdigit(tokenizer->query[tokenizer->current_position]) ||
     (!decimal_found && tokenizer->query[tokenizer->current_position] == '.')) {
        if (tokenizer->query[tokenizer->current_position] == '.') {
            decimal_found = true;
        }
        
        buffer = move_tokenizer(tokenizer, buffer, buffer_size);
        if (!buffer) {
            return NULL;
        }
    }

    /* Make token a string. */
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, NUMBER);
}

/* Handles reading query for strings. */
Token *string_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    if (!tokenizer || !tokenizer->query 
        || !tokenizer->length
        || !buffer || !buffer_size) {
        return NULL;
    }

    do {
        buffer = move_tokenizer(tokenizer, buffer, buffer_size);
        if (!buffer) {
            return NULL;
        }

    } while (tokenizer->query[tokenizer->current_position] != '\'');
        
    buffer = move_tokenizer(tokenizer, buffer, buffer_size);
    if (!buffer) {
        return NULL;
    }

    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, STRING);
}

/* Handles reading query for operators and comments. */
Token *operator_handling(Tokenizer *tokenizer, bool peek_forwards, char *buffer, int *buffer_size) {
    if (!tokenizer || !tokenizer->query 
        || !tokenizer->length
        || !buffer || !buffer_size) {
        return NULL;
    }

    if (peek_forwards && ((tokenizer->current_position+1) < tokenizer->length)
        && isoperator(tokenizer->query[tokenizer->current_position+1], &peek_forwards)) {
        
        if (buffer[(*buffer_size)-1] == '-') {
            while (tokenizer->query[tokenizer->current_position] != '\n') {

                buffer = move_tokenizer(tokenizer, buffer, buffer_size);
                if (!buffer) {
                    return NULL;
                }
            }

            buffer[(*buffer_size)-1] = '\0';
            tokenizer->current_position++;
            return token_create(buffer, COMMENT);
        } else {
            buffer = move_tokenizer(tokenizer, buffer, buffer_size);
            if (!buffer) {
                return NULL;
            }
        }
    }

    buffer = move_tokenizer(tokenizer, buffer, buffer_size);
    if (!buffer) {
        return NULL;
    }
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, OPERATOR);
}

/* Handles reading query for punctuation. */
Token *punctuation_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    if (!tokenizer || !tokenizer->query 
        || !tokenizer->length
        || !buffer || !buffer_size) {
        return NULL;
    }
    
    buffer = move_tokenizer(tokenizer, buffer, buffer_size);
    if (!buffer) {
        return NULL;
    }
    buffer[(*buffer_size)-1] = '\0';

    return token_create(buffer, PUNCTUATION);
}

/* Handles reading query for keywords/identifiers. */
Token *keyword_identifier_handling(Tokenizer *tokenizer, char *buffer, int *buffer_size) {
    if (!tokenizer || !tokenizer->query 
        || !tokenizer->length
        || !buffer || !buffer_size) {
        return NULL;
    }

    while (!isspace((unsigned char) tokenizer->query[tokenizer->current_position])) {
        buffer = move_tokenizer(tokenizer, buffer, buffer_size);
        if (!buffer) {
            return NULL;
        }

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
                if (!buffer) {
                    return NULL;
                }

            } while (isalnum(tokenizer->query[tokenizer->current_position])
                    || tokenizer->query[tokenizer->current_position] == '_');
        
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