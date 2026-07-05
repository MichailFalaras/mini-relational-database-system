#ifndef DATA_TYPES_H_
#define DATA_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

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
    int sign;
    int64_t val;
    uint32_t scale;
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

/* DATE */
typedef struct date {
    uint32_t year;
    uint8_t month;
    uint8_t day;
} date_t;

/* TIMESTAMP */
typedef struct timestamp {
    uint32_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} timestamp_t;

/* BLOB */
typedef struct blob {
    uint32_t size;
    uint8_t *buffer;
} blob_t, jsonb_t;

/* Value and corresponding data type both in this struct. */
typedef struct value {
    DataType type;
    bool is_null;
    union {
        int32_t int32_val;
        uint32_t uint32_val;
        numeric_t numeric_val;
        float float_val;
        double double_val;
        char_n_t char_val;
        varchar_n_t varchar_val;
        char *text_val;
        uint64_t date_val;
        uint64_t timestamp_val;
        blob_t blob_val;
        bool bool_val;
        jsonb_t jsonb_val;
    } value;
} Value;

#endif