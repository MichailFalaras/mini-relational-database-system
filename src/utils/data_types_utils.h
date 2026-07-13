#ifndef DATA_TYPES_UTILS_H_
#define DATA_TYPES_UTILS_H_

#include "../../include/data_types.h"

// Creates a duplicate string for creation of CHAR(n), VARCHAR(n), TEXT data types
char *duplicate_string(const char *str);

// Deallocation helper also used in value_assign for the temporary copy
void value_free_internal(Value *value);

// Comparison Utilities
int compare_int32(const int32_t left, const int32_t right);
int compare_uint32(const uint32_t left, const uint32_t right);
int compare_uint64(const uint64_t left, const uint64_t right);
bool compare_numeric(const numeric_t *left, const numeric_t *right, int *result);
bool compare_float(const float left, const float right, int *result);
bool compare_double(const double left, const double right, int *result);
bool compare_char_n(const char_n_t *left, const char_n_t *right, int *result);
bool compare_varchar_n(const varchar_n_t *left, const varchar_n_t *right, int *result);
bool compare_text(const char *left, const char *right, int *result);
bool compare_binary(const blob_t *left, const blob_t *right, int *result);
int compare_bool(const bool left, const bool right);


#endif