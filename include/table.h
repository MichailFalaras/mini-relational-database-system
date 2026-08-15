#ifndef TABLE_H_
#define TABLE_H_

#include <stdint.h>
#include <stdbool.h>

/* Forward Declarations. */
typedef struct schema Schema;
typedef struct index Index;
typedef enum index_type IndexType;
typedef struct index_key IndexKey;
typedef struct column Column;
typedef struct constraint Constraint;
typedef struct pager Pager;
typedef struct database Database;

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
    bool is_materialized;
    bool is_deleted;
    Schema* table_schema;
    Index *primary_index;
    Index **secondary_indexes;
    uint32_t total_secondary_indexes;
    uint32_t row_count;
} Table;


/* Search-related structures */
typedef enum table_lookup_status {
    TABLE_LOOKUP_SUCCESS,
    TABLE_LOOKUP_NOT_FOUND,
    TABLE_LOOKUP_INVALID_ARGUMENTS,
    TABLE_LOOKUP_NO_USABLE_INDEX,
    TABLE_LOOKUP_ERROR
} TableLookupStatus;

#define TABLE_RANGE_INITIAL_CAPACITY 16
typedef struct row_result {
    Row **rows;
    uint32_t count;
    uint32_t capacity;
} TableRowResult;



/* Table metadata operations */
extern Table *table_metadata_create(const char *table_name, const Schema *schema);

extern void table_free(Table *table);

extern bool table_alter_rename(Table *table, const char *new_name);

extern bool table_alter_add_col(Table *table, Column *new_col);

extern bool table_alter_drop_col(Table *table, Database *db, const char *col_name);

extern bool table_alter_modify_col(Table *table, const Database *db, const char *old_col_name, const Column *new_column_def);

extern bool table_alter_rename_col(Table *table, const char *old_col_name, const char *new_col_name);

extern bool table_alter_add_constraint(Table *table, const Constraint *constraint);

extern bool table_alter_drop_constraint(Table *table, const char *constraint_name);

extern Index *table_find_index(const Table *table, const char *index_name);

extern bool table_has_column(const Table *table, const char *col_name);

extern Column *table_find_column(const Table *table, const char *col_name);


/* Table disk operations */
extern Table *table_create(const char *table_name, const Schema *schema, Pager *pager);

extern bool table_materialize(Table *table, Pager *pager);

extern bool table_drop(Table *table, Pager *pager);

extern bool table_truncate(Table *table, Pager *pager);

extern bool table_create_index(Table *table, const char *index_name, IndexType type, 
    const IndexKey *key, Pager *page, bool is_unique);

extern bool table_drop_index(Table *table, const char *index_name, Pager *page);

// Exact key search
extern TableLookupStatus table_find_exact(const Table *table, Pager *pager, Value **key_values,
    const uint32_t *column_ids, uint32_t num_columns, TableRowResult *result);

// Prefix key search
extern TableLookupStatus table_find_prefix(const Table *table, Pager *pager, Value **key_values,
    const uint32_t *column_ids, uint32_t num_columns, TableRowResult *result);

// Range key search
extern TableLookupStatus table_find_range(const Table *table, Pager *pager, 
    Value **start_key_values, const uint32_t *start_column_ids, uint32_t start_num_columns, 
    bool include_start, Value **end_key_values, const uint32_t *end_column_ids, uint32_t end_num_columns, 
    bool include_end, TableRowResult *result);

// Full table scan
extern TableLookupStatus table_scan(const Table *table, Pager *pager, TableRowResult *result);

#endif