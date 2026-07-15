#ifndef SCHEMA_H_
#define SCHEMA_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"
#include "expressions.h"
#include "constraints.h"
#include "row.h"
#include "database.h"

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
} Column;

/* Table's schema. Contains:
 * columns: all columns of table
 * amount_columns: amount of columns
 * constraints: constraints array of table schema. */
typedef struct schema {
    Column **columns;
    uint32_t num_columns;
    Constraint **constraints;
    uint32_t num_constraints;
} Schema;

extern Schema *schema_create(const Column **columns, const Constraint **constraints, uint32_t num_columns,
                            uint32_t num_constraints);

extern bool schema_add_column(Schema *schema, const Column *new_column);

extern bool schema_drop_column(Schema *schema, Database *db, const char *col_name);

extern bool schema_rename_column(Schema *schema, const char *old_col_name, const char *new_col_name);

extern bool schema_modify_column(Schema *schema, const char *old_col_name, const Column *new_column);

extern bool schema_add_constraint(Schema *schema, const Constraint *new_constraint);

extern bool schema_drop_constraint(Schema *schema, const char *constraint_name);

extern Column *schema_find_column(const Schema *schema, const char *col_name);

extern int32_t schema_find_column_index(const Schema *schema, const char *col_name);

extern uint32_t schema_find_constraint_index(const Schema *schema, const char *constraint_name);

extern bool schema_validate_row(const Schema *schema, const Row *row);

extern bool schema_drop(Schema *schema, Database *db); 

extern Column *column_alloc(char *column_name, DataType type, uint32_t not_null_rows,
    uint32_t null_rows);

extern void schema_free(Schema *schema);

#endif 