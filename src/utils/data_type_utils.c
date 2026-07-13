#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "data_types_utils.h"


// Creates a duplicate string for creation of CHAR(n), VARCHAR(n), TEXT data types
char *duplicate_string(const char *str) {
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
void value_free_internal(Value *value) {
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

// Comparison Utilities
int compare_int32(const int32_t left, const int32_t right) {
    if (left < right) return -1;
    else if (left == right) return 0;
    else return 1;
}

int compare_uint32(const uint32_t left, const uint32_t right) {
    if (left < right) return -1;
    else if (left == right) return 0;
    else return 1;
}

int compare_uint64(const uint64_t left, const uint64_t right) {
    if (left < right) return -1;
    else if (left == right) return 0;
    else return 1;
}


// Static helper -> Remove trailing 0's
static void normalize_numeric(int64_t *value, uint32_t *scale) {
    if (!value || !scale) {
        printf("normalize_numeric: Input parameters contain NULL.");
        return;
    }

    while (*scale > 0 && *value % 10 == 0) {
        *value /= 10;
        (*scale)--;
    }
}

// Static helper -> Equalize lower scale value by multiplying by 10
static bool multiply_by_10_checked(int64_t value, int64_t *result) {
    if (!result) {
        printf("multiply_by_10_checked: result value is NULL.");
        return false;
    }

    if (value > INT64_MAX / 10) {
        printf("multiply_by_10_checked: input value overflows.");
        return false;
    } 
    
    if(value < INT64_MIN / 10) {
        printf("multiply_by_10_checked: input value underflows.");
        return false;
    }

    *result = value * 10;
    return true;
}

bool compare_numeric(const numeric_t *left, const numeric_t *right, int *result) {
    if (!left || !right) {
        printf("compare_char_n: Input operands contain NULL.");
        return false;
    }

    if (!result) {
        printf("compare_char_n: Input result flag is NULL.");
        return false;
    }

    int64_t left_value = left->val;
    int64_t right_value = right->val;

    // Reversing sign if numeric value is negative
    if (left->sign < 0)
        left_value = -left_value;

    if (right->sign < 0)
        right_value = -right_value;

    uint32_t left_scale = left->scale;
    uint32_t right_scale = right->scale;

    normalize_numeric(&left_value, &left_scale);
    normalize_numeric(&right_value, &right_scale);

    while (left_scale < right_scale) {
        if (!multiply_by_10_checked(left_value, &left_value)) {
            return false;
        }

        left_scale++;
    }

    while (right_scale < left_scale) {
        if (!multiply_by_10_checked(right_value, &right_value)) {
            return false;
        }

        right_scale++;
    }

    if (left_value < right_value) *result = -1;
    else if (left_value == right_value) *result = 0;
    else *result = 1;

    return true;
}

bool compare_float(const float left, const float right, int *result) {
    if (isnan(left) || isnan(right)) {
        printf("compare_float: NaN comparison operands.");
        return false;
    }

    if (!result) {
        printf("compare_float: Input result flag is NULL.");
        return false;
    }

    if (left < right) *result = -1;
    else if (left == right) *result = 0;
    else *result = 1;

    return true;
}

bool compare_double(const double left, const double right, int *result) {
    if (isnan(left) || isnan(right)) {
        printf("compare_double: NaN comparison operands.");
        return false;
    }

    if (!result) {
        printf("compare_double: Input result flag is NULL.");
        return false;
    }

    if (left < right) *result = -1;
    else if (left == right) *result = 0;
    else *result = 1;

    return true;
}

bool compare_char_n(const char_n_t *left, const char_n_t *right, int *result) {
    if (!left || !right) {
        printf("compare_char_n: Input operands contain NULL.");
        return false;
    }

    if (!result) {
        printf("compare_char_n: Input result flag is NULL.");
        return false;
    }

    int comparison = strcmp(left->string, right->string);

    if (comparison > 0) *result = 1;
    else if (comparison < 0) *result = -1;
    else *result = 0;

    return true;
}

bool compare_varchar_n(const varchar_n_t *left, const varchar_n_t *right, int *result) {
    if (!left || !right) {
        printf("compare_varchar_n: Input operands contain NULL.");
        return false;
    }

    if (!result) {
        printf("compare_varchar_n: Input result flag is NULL.");
        return false;
    }

    int comparison  = strcmp(left->string, right->string);
    
    if (comparison > 0) *result = 1;
    else if (comparison < 0) *result = -1;
    else *result = 0;
    
    return true;
}

bool compare_text(const char *left, const char *right, int *result) {
    if (!left || !right) {
        printf("compare_text: Input operands contain NULL.");
        return false;
    }

    if (!result) {
        printf("compare_text: Input result flag is NULL.");
        return false;
    }

    int comparison = strcmp(left, right);

    if (comparison > 0) *result = 1;
    else if (comparison < 0) *result = -1;
    else *result = 0;

    return true; 
}

bool compare_binary(const blob_t *left, const blob_t *right, int *result) {
    if (!left || !right) {
        printf("compare_binary: Input operands contain NULL.");
        return false;
    }

    if (!result) {
        printf("compare_binary: Input result flag is NULL.");
        return false;
    }


    uint32_t common_size = left->size < right->size ? left->size : right->size;

    // Comparing the common-sizes portions of the 2 binary blobs
    if (common_size > 0) {
        int comparison = memcmp(left->buffer, right->buffer, common_size);

        // They already have a difference
        if (comparison != 0) {
            if (comparison > 0) *result = 1;
            else if (comparison < 0) *result = -1;
            else *result = 0;
            
            return true;
        }
        
    }

    // if the common-sized portions are equal, we compare the sizes of the blobs
    if (left->size < right->size) *result = -1;
    else if (left->size == right->size) *result = 0;
    else *result = 1;

    return true;
}

int compare_bool(const bool left, const bool right) {
    if (left < right) return -1;
    else if (left == right) return 0;
    else return 1;
}