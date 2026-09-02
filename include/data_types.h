#ifndef DATA_TYPES_H_
#define DATA_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

#define DATA_TYPE_TEXT_SIZE 512
// TODO: BLOB list of pages in disk, probably something similar for jsonb

/* All possible data types used in this database. */
typedef enum data_types {
    INTEGER,
    UNSIGNED_INTEGER,
    NUMERIC,
    FLOAT,
    DOUBLE,
    CHAR,
    VARCHAR,
    TEXT,
    DATE,
    TIMESTAMP,
    BLOB,
    BOOL,
    JSONB
} DataType;

/* NUMERIC/DECIMAL Data Type. */
typedef struct numeric {
    int64_t val;
    uint32_t scale; // 10^(-scale)
} numeric_t;

/* CHAR(n) */
typedef struct char_n {
    uint32_t n;
    char *string;
} char_n_t;

/* VARCHAR(n) */
typedef struct varchar_n {
    uint32_t max_n;
    char *string;
} varchar_n_t;

/* DATE, only used for results display */
typedef struct date {
    uint32_t year;
    uint8_t month;
    uint8_t day;
} date_t;

/* TIMESTAMP, only used for results display */
typedef struct timestamp {
    uint32_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} timestamp_t;

/* BLOB/JSONB - NOT IN USE */
typedef struct blob {
    uint32_t size;
    uint8_t *buffer;
} blob_t, jsonb_t;

/* Value and corresponding data type both in this struct. */
typedef struct value {
    DataType type;
    bool null_val;
    union {
        int32_t int32_val;
        uint32_t uint32_val;
        numeric_t numeric_val;
        float float_val;
        double double_val;
        char_n_t char_val;
        varchar_n_t varchar_val;
        char *text_val;
        uint64_t date_val;  // seconds from Unix epoch
        uint64_t timestamp_val; // seconds from Unix epoch
        blob_t blob_val;
        bool bool_val;
        jsonb_t jsonb_val;
    } value;
} Value;

/* Allocate a Value struct */
Value *value_alloc(DataType type);

/* Create a Value struct */
Value *value_create(DataType type, const void *value);

/* Create Value struct of type with NULL value. */
Value *value_create_null(DataType type);

/* Copy One Value struct to another Value struct */
Value *value_copy(const Value *value);

/* Assign the contents of one Value struct to another */
bool value_assign(Value *dest, const Value *src);

/* Compare two Value structs */
bool value_compare(const Value *left, const Value *right, int *result);

/* Check if 2 Data Types are compatible */
bool value_types_compatible(DataType left, DataType right);

/* Check if a Value can be assigned to a target Data Type */
bool value_can_assign(DataType target, const Value *value);

/* Converts a Value struct to another data type, if compatible */
bool value_convert_data_type(DataType target, Value *value);

/* Deallocate Value struct */
void value_free(Value *value);

#endif