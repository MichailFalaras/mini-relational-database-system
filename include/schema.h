#ifndef SCHEMA_H_
#define SCHEMA_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"

typedef struct column_constraints ColumnConstraints;
typedef struct columns Columns;

/* Column constraints that contain: */
typedef struct column_constraints {
    bool primary_key;
    bool foreign_key;
    char foreign_table_name[64];
    char foreign_column_name[64];
    uint32_t foreign_table_index;
    uint32_t foreign_column_index;
    bool unique;
    bool not_null;
    bool has_index;
    /* TODO: CHECK */
} ColumnConstraints;

/* Columns struct that contains:
 * name: name of column
 * has_index: flag if column has index
 * amount_rows: rows without NULL value
 * amount_null: rows with NULL value*/
typedef struct columns {
    char name[64];
    DataType type;
    uint32_t amount_rows;
    uint32_t amount_null;
    ColumnConstraints constraints;
} Columns;

/* Table's schema. Contains:
 * columns: all columns of table
 * amount_columns: amount of columns*/
typedef struct schema {
    Columns *columns;
    uint32_t *primary_key_columns;
    uint32_t amount_columns;
} Schema;

#endif 