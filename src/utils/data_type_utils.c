#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/data_types.h"

// Forward declaration of helper functions
static char *duplicate_string(const char *str);
static void value_free_internal(Value *value);


/* Allocate Value structure */
Value *value_alloc(DataType type) {
    Value *new_value = (Value *) calloc(1, sizeof(Value));
    
    if (!new_value) {
        printf("value_alloc: Memory for Value could not be allocated.");
        return NULL;
    }

    new_value->type = type;
    return new_value;
}

/* Create Value structure */
Value *value_create(DataType type, const void *value) {
    Value *new_value = value_alloc(type);

    if (!new_value) {
        printf("value_create: New Value structure is NULL.");
        return NULL;
    }

    // NULL value case is checked before the other data types
    // because the !value would falsely invalidate the NULL node
    if (type == NULL_TYPE) {
        new_value->value.null_val = true;
        return new_value;
    }

    if (!value) {
        printf("value_create: Value structure is NULL.");
        value_free(new_value);
        return NULL;
    }

    // Some initialization cases require only a simple typecast and Value assignment
    // Others require verifying nested pointer fields, and duplicating strings & binary data
    switch (type) {

        case INTEGER:
            new_value->value.int32_val = *(const int32_t *) value;
            break;

        case UNSIGNED_INTEGER:
            new_value->value.uint32_val = *(const uint32_t *) value;
            break;

        case NUMERIC:
            new_value->value.numeric_val = *(const numeric_t *) value;
            break;

        case FLOAT:
            new_value->value.float_val = *(const float *) value;
            break;

        case DOUBLE:
            new_value->value.double_val = *(const double *) value;
            break;

        case CHAR: {
            const char_n_t *char_n_value = value;

            // Verifying input string
            if (!char_n_value->string) {
                printf("value_create: char(n) string is NULL.");
                value_free(new_value);
                return NULL;
            }

            // Verifying input string's length
            size_t length = strlen(char_n_value->string);

            if (length > char_n_value->n) {
                printf("value_create: Length of char(n) string exceeds the length structure field.");
                value_free(new_value);
                return NULL;
            }

            new_value->value.char_val.n = char_n_value->n;
            new_value->value.char_val.string = duplicate_string(char_n_value->string);

            // Validating the result
            if (char_n_value->string && !new_value->value.char_val.string) {
                printf("value_create: Failed to allocate char(n) string.");
                value_free(new_value);
                return NULL;
            }

            break;
        }

        case VARCHAR: {
            const varchar_n_t *varchar_n_value = value;

            // Verifying input string
            if (!varchar_n_value->string) {
                printf("value_create: varchar(n) string is NULL.");
                value_free(new_value);
                return NULL;
            }
            
            // Verifying input string's length
            size_t length = strlen(varchar_n_value->string);

            if (length > varchar_n_value->max_n) {
                printf("value_create: Length of varchar(n) string exceeds the length structure field.");
                value_free(new_value);
                return NULL;
            }

            new_value->value.varchar_val.max_n = varchar_n_value->max_n;
            new_value->value.varchar_val.string = duplicate_string(varchar_n_value->string);

            // Validating the result
            if(varchar_n_value->string && !new_value->value.varchar_val.string) {
                printf("value_create: Failed to allocate varchar(n) string.");
                value_free(new_value);
                return NULL;
            }

            break;
        }

        case TEXT:
            new_value->value.text_val = duplicate_string((const char *) value);

            // Validating the result
            if (!new_value->value.text_val) {
                printf("value_create: Failed to allocate text string.");
                value_free(new_value);
                return NULL;
            }

            break;

        case DATE:
            // Unix epoch seconds 
            new_value->value.date_val = *(const uint64_t *) value;
            break;

        case TIMESTAMP:
            // Unix epoch seconds
            new_value->value.timestamp_val = *(const uint64_t *) value;
            break;

        case BLOB: {
            const blob_t *blob_value = value;

            new_value->value.blob_val.size = blob_value->size;

            // empty binary buffer
            if (blob_value->size == 0) {
                new_value->value.blob_val.buffer = NULL;
                break;
            }

            // invalid input blob buffer
            if (!blob_value->buffer) {
                printf("value_create: Input blob is NULL.");
                value_free(new_value);
                return NULL;
            }

            new_value->value.blob_val.buffer = (uint8_t *) malloc(blob_value->size);

            if(!new_value->value.blob_val.buffer) {
                printf("value_create: Memory for Blob could not be allocated.");
                value_free(new_value);
                return NULL;
            }

            // Copying the input blob binary buffer to the new Value's buffer
            memcpy(new_value->value.blob_val.buffer, blob_value->buffer, blob_value->size);

            break;
        }

        case BOOL:
            new_value->value.bool_val = *(const bool *) value;
            break;

        case JSONB: {
            const jsonb_t *jsonb_value = value;

            new_value->value.jsonb_val.size = jsonb_value->size;

            // empty binary buffer
            if (jsonb_value->size == 0) {
                new_value->value.jsonb_val.buffer = NULL;
                break;
            }

            // invalid input jsonb buffer
            if (!jsonb_value->buffer) {
                printf("value_create: Input JSONB is NULL.");
                value_free(new_value);
                return NULL;
            }

            new_value->value.jsonb_val.buffer = (uint8_t *) malloc(jsonb_value->size);

            if (!new_value->value.jsonb_val.buffer) {
                printf("value_create: Memory for JSONB could not be allocated.");
                value_free(new_value);
                return NULL;
            }

            // Copying the input jsonb binary buffer to the new Value's buffer
            memcpy(new_value->value.jsonb_val.buffer, jsonb_value->buffer, jsonb_value->size);

            break;
        }

        default:
            printf("value_create: Unsupported data type.");
            value_free(new_value);
            return NULL;
    }

     return new_value;
}

/* Copy One Value structure to another Value structure */
Value *value_copy(const Value *src_value) {
    if (!src_value) {
        printf("value_copy: Source Value structure is NULL.");
        return NULL;
    }

    // Copying a Value means creating a new value with the same data
    // as the input Value
    switch (src_value->type) {
        case INTEGER:
            return value_create(INTEGER, &src_value->value.int32_val);
        
        case UNSIGNED_INTEGER:
            return value_create(UNSIGNED_INTEGER, &src_value->value.uint32_val);

        case NUMERIC:
            return value_create(NUMERIC, &src_value->value.numeric_val);

        case FLOAT:
            return value_create(FLOAT, &src_value->value.float_val);    

        case DOUBLE:
            return value_create(DOUBLE, &src_value->value.double_val);

        case CHAR: 
            return value_create(CHAR, &src_value->value.char_val);
            
        case VARCHAR: 
            return value_create(VARCHAR, &src_value->value.varchar_val);

        case TEXT:
            return value_create(TEXT, src_value->value.text_val);

        case DATE:
            return value_create(DATE, &src_value->value.date_val);

        case TIMESTAMP:
            return value_create(TIMESTAMP, &src_value->value.timestamp_val);

        case BLOB: 
            return value_create(BLOB, &src_value->value.blob_val);

        case BOOL:
            return value_create(BOOL, &src_value->value.bool_val);

        case JSONB: 
            return value_create(JSONB, &src_value->value.jsonb_val);

        case NULL_TYPE:
            return value_create(NULL_TYPE, NULL);

        default:
            printf("value_copy: Unsupported data type.");
            return NULL;
    }
}

/* Assign the contents of one Value struct to another */
bool value_assign(Value *dest, const Value *src) {
    if (!src) {
        printf("value_assign: Source Value structure is NULL.");
        return false;
    }

    if (!dest) {
        printf("value_assign: Destination Value structure is NULL.");
        return false;
    }

    if (src == dest) {
        printf("value_assign: Source & Destination Value structures are equal.");
        return true;
    }

    Value *copy = value_copy(src);

    if (!copy) {
        printf("value_assign: Temporary copy Value could not be allocated.");
        return false;
    }

    // Clearing previous destination Value
    value_free_internal(dest);

    // The resources of the copy Value are assigned to the destination Value
    dest->type = copy->type;
    dest->value = copy->value;

    // Only free the pointer, not the resource of the copy Value
    free(copy);

    return true;
}

/* Deallocate Value structure */
void value_free(Value *value) {
    if (!value) {
        printf("value_free: Value structure is already NULL.");
        return;
    }

    value_free_internal(value);
    free(value);
}


/* Helper functions */

// Creates a duplicate string for creation of CHAR(n), VARCHAR(n), TEXT data types
static char *duplicate_string(const char *str) {
    if (!str) {
        printf("duplicate_string: Input string is NULL.");
        return NULL;
    }

    size_t length = strlen(str);

    char *new_str = (char *) malloc(length + 1);
    if (!new_str) {
        printf("duplicate_string: Duplicate string memory could not be allocated.");
        return NULL;
    }

    // Copying input string to new memory location
    memcpy(new_str, str, length + 1);
    return new_str;
}

// Deallocation helper also used in value_assign for the temporary copy
static void value_free_internal(Value *value) {
    if (!value) {
        printf("value_free_internal: Value structure is already NULL.");
        return;
    }

    // Deallocate nested allocated fields from certain types of nodes
    switch(value->type) {

        case CHAR:
            free(value->value.char_val.string);
            break;
        
        case VARCHAR:
            free(value->value.varchar_val.string);
            break;

        case TEXT:
            free(value->value.text_val);
            break;

        case BLOB:
            free(value->value.blob_val.buffer);
            break;

        case JSONB:
            free(value->value.jsonb_val.buffer);
            break;
        
        default:
            break;
    }

    // Setting the value union field's memory to 0
    memset(&value->value, 0, sizeof(value->value));
}