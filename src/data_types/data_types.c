#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/data_types.h"
#include "data_types_utils.h"


/* Allocate Value struct */
Value *value_alloc(DataType type) {
    Value *new_value = (Value *) calloc(1, sizeof(Value));
    
    if (!new_value) {
        printf("value_alloc: Memory for Value could not be allocated.");
        return NULL;
    }

    new_value->type = type;
    return new_value;
}


/* Create Value struct */
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


/* Copy One Value struct to another Value struct */
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


/* Compare two Value structs */
bool value_compare(const Value *left, const Value *right, int *result) {
    if (!left || !right || !result) {
        printf("value_compare: Input parameters contain NULL.");
        return false;
    }

    // Check compatibility of data types
    if (!value_types_compatible(left->type, right->type)) {
        printf("value_compare: Data types are not compatible.");
        return false;
    }

    // Creating copies that can actually me modified if data type conversion is required
    Value *left_val = value_copy(left);
    Value *right_val = value_copy(right);

    if (!left_val || !right_val) {
        printf("value_compare: Value copies where not created correctly.");
        value_free(left_val);
        value_free(right_val);
        return false;
    }

    // Convert one data type to another if not the same, but compatible
    if (left_val->type != right_val->type) {
        bool converted = value_convert_data_type(right_val->type, left_val);

        if (!converted) {
            // If the first conversion fails, try the opposite conversion
            converted = value_convert_data_type(left_val->type, right_val);

            if (!converted) {
                // If that fails too, the comparison process fails
                printf("value_compare: Data type conversions couldn't be carried out.");
                value_free(left_val);
                value_free(right_val);
                return false;
            }
        }
    }

    // Final comparison status
    bool is_success;

    switch(left_val->type) {
        case INTEGER:
            *result = compare_int32(left_val->value.int32_val, right_val->value.int32_val); 
            is_success = true;
            break;

        case UNSIGNED_INTEGER:
            *result = compare_uint32(left_val->value.uint32_val, right_val->value.uint32_val);
            is_success = true;
            break;

        case NUMERIC:
            is_success = compare_numeric(&left_val->value.numeric_val, &right_val->value.numeric_val, result);
            break;

        case FLOAT:
            is_success = compare_float(left_val->value.float_val, right_val->value.float_val, result);
            break;

        case DOUBLE:
            is_success = compare_double(left_val->value.double_val, right_val->value.double_val, result);
            break;

        case CHAR: 
            is_success = compare_char_n(&left_val->value.char_val, &right_val->value.char_val, result);
            break;
            
        case VARCHAR: 
            is_success = compare_varchar_n(&left_val->value.varchar_val, &right_val->value.varchar_val, result);
            break;

        case TEXT:
            is_success = compare_text(left_val->value.text_val, right_val->value.text_val, result);
            break;

        case DATE:
            *result = compare_uint64(left_val->value.date_val, right_val->value.date_val);
            is_success = true;
            break;

        case TIMESTAMP:
            *result = compare_uint64(left_val->value.timestamp_val, right_val->value.timestamp_val);
            is_success = true;
            break;

        case BLOB:
            is_success = compare_binary(&left_val->value.blob_val, &right_val->value.blob_val, result);
            break;
        
        case BOOL:
            *result = compare_bool(left_val->value.bool_val, right_val->value.bool_val);
            is_success = true;
            break;

        /* TODO: JSONB comparison requires JSONB deserialization and parsing of key value pairs */

        default:
            printf("value_compare: Unsupported data types.");
            value_free(left_val);
            value_free(right_val);
            return false;
    }

    value_free(left_val);
    value_free(right_val);
    return is_success;
}


/* Check if 2 Data Types are compatible */
bool value_types_compatible(DataType left, DataType right) {
    if (left == NULL_TYPE || right == NULL_TYPE)
        return false;

    if (left == right) 
        return true;

    // INTEGER, UNSIGNED INTEGER, NUMERIC, FLOAT, & DOUBLE are compatible
    if (is_numeric_type(left) && is_numeric_type(right))
        return true;

    // CHAR(n), VARCHAR(n), & TEXT are compatible
    if (is_text_type(left) && is_text_type(right))
        return true;

    // DATE & TIMESTAMP are compatible since they're represented as seconds since Unix epoch
    if ((left == DATE && right == TIMESTAMP) || (left == TIMESTAMP && right == DATE))
        return true;

    return false;
}


/* Converts a Value struct to another data type, if compatible */
bool value_convert_data_type(DataType target, Value *value) {
    if (!value) {
        printf("value_convert_data_type: Input Value is NULL.");
        return false;
    }

    if (target == value->type) 
        return true;

    if (!value_can_assign(target, value))
        return false;

    // So far in this version,
    switch(target) {
        // only INTEGER -> UNSIGNED INTEGER
        case UNSIGNED_INTEGER:
            return convert_to_unsigned_integer(value);

        // only INTEGER -> NUMERIC and UNSIGNED_INTEGER -> NUMERIC
        case NUMERIC: 
            return convert_to_numeric(value);
        
        // only INTEGER -> FLOAT and UNSIGNED_INTEGER -> FLOAT
        case FLOAT: 
            return convert_to_float(value);

        // only INTEGER -> DOUBLE, UNSIGNED_INTEGER -> DOUBLE, and FLOAT -> DOUBLE
        case DOUBLE: 
            return convert_to_double(value);

        // only VARCHAR -> CHAR
        case CHAR: 
            return convert_to_char(value);
        
        // only CHAR -> VARCHAR
        case VARCHAR: 
            return convert_to_varchar(value);
            
        // only CHAR -> TEXT and VARCHAR -> TEXT
        case TEXT: 
           return convert_to_text(value);
        
        // only DATE -> TIMESTAMP
        case TIMESTAMP: 
            return convert_to_timestamp(value);

        default:
            printf("value_convert_data_type: Unsupported target data type.");
            return false;       
    }
}


/* Check if a Value can be assigned to a target Data Type */
bool value_can_assign(DataType target, const Value *value) {
    if (!value) {
        printf("value_can_assign: Input Value is NULL.");
        return false;
    }

    // Any value can be converted to NULL, as long as there's no NOT NULL constraint
    if (value->type == NULL_TYPE) {
        return true;
    }

    // Identical data types
    if (target == value->type)
        return true;

    // Allow value conversions as long as the target data type doesn't narrow down
    // the tested data type.
    switch (target) {
        case INTEGER:
            return (value->type == INTEGER);

        case UNSIGNED_INTEGER:
            if (value->type == INTEGER)
                return value->value.int32_val >= 0;
            
            return false;
        
        case NUMERIC:
            return (value->type == INTEGER || value->type == UNSIGNED_INTEGER);

        case FLOAT:
            return (value->type == INTEGER || value->type == UNSIGNED_INTEGER);

        case DOUBLE:
            return (value->type == INTEGER || value->type == UNSIGNED_INTEGER ||
                    value->type == FLOAT);

        // These cases only examine the possibility, 
        // not the actual string length constraints of CHAR(n) and VARCHAR(n)
        case CHAR:
        case VARCHAR:
        case TEXT:
            return (value->type == CHAR || value->type == VARCHAR || value->type == TEXT);

        case DATE:
            return false;
        
        case TIMESTAMP:
            return value->type == DATE;

        case BOOL:
        case BLOB:
        case JSONB:
        case NULL_TYPE:
            return false;
        
        default:
            return false;
    }
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