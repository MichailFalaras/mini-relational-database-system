#ifndef TABLE_H_
#define TABLE_H_

#include <stdint.h>
#include "index.h"
#include "pager.h"
#include "schema.h"

#define MAX_INDEXES 8

/* Table structure that contains:
 * name: table name
 * table_schema: table's schema/columns and its rules
 * primary_index: main BTree for storing table based on primary key
 * secondary_indexes: secondary B+ Tree index based on the ordering
 * of specific column combinations
 * total_secondary_indexes: amount of secondary indexes
 * row_count: amount of rows. */
typedef struct table {
    char name[64];
    Schema table_schema;
    Index *primary_index;
    Index *secondary_indexes[MAX_INDEXES];
    uint32_t total_secondary_indexes;
    uint32_t row_count;
} Table;

/* Basic Table Operations. */
extern Table *table_create(const char *table_name, const Schema *schema);

extern bool table_drop(Table *table);

extern bool table_alter_rename(Table *table, const char *new_name);

extern bool table_truncate(Table *table);

/* ALTER TABLE Operations. */
extern bool table_alter_add_col(Table *table, const Column *new_col);

extern bool table_alter_drop_col(Table *table, const char *col_name);

extern bool table_alter_modify_col(Table *table, const char *old_col_name, const Column *new_column_def);

extern bool table_alter_rename_col(Table *table, const char *old_col_name, const char *new_col_name);

extern bool table_alter_add_constraint(Table *table, const Constraint *constraint);

extern bool table_alter_drop_constraint(Table *table, const char *constraint_name);

/* Basic Index Operations. */
extern bool table_create_index(Table *table, const Index *new_index);

extern bool table_drop_index(Table *table, const char *index_name);

/* Searching Operations. */
extern Index *table_find_index(const Table *table, const char *index_name);

extern bool table_has_column(const Table *table, const char *col_name);

extern Column *table_find_column(const Table *table, const char *col_name);

/* Deallocation. */
extern void table_free(Table *table);

#endif