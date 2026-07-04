#ifndef SCHEMA_H_
#define SCHEMA_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"
#include "expressions.h"
#include "constraints.h"

/* Columns struct that contains:
 * name: name of column
 * type: data type of column
 * non_null_rows: rows without NULL value
 * null_rows: rows with NULL value
 * not_null: flag if column has constrain NOT NULL. */
typedef struct column {
    char name[64];
    DataType type;
    uint32_t non_null_rows;
    uint32_t null_rows;
    bool not_null;
} Column;

/* Table's schema. Contains:
 * columns: all columns of table
 * amount_columns: amount of columns
 * constraints: constraints array of table schema. */
typedef struct schema {
    Column *columns;
    uint32_t amount_columns;
    Constraint *constraints;    
} Schema;

#endif 