#ifndef TABLE_H_
#define TABLE_H_

#include <stdint.h>
#include "index.h"
#include "pager.h"
#include "schema.h"

#define MAX_INDEXES 8

/* Table structure that contains:
 * total_rows: total rows of table
 * table_schema: table's schema/column and its rules
 * primary_index: main BTree for storing table based on primary key
 * root: root node of table's Btree
 * offset: table offset in database file (e.g. users.db)
 * data_types: column data types. */
typedef struct table {
    char name[64];
    Schema table_schema;
    Index primary_index;
    Index secondary_indexes[MAX_INDEXES];
    uint32_t total_secondary_indexes;
    uint32_t row_count;
} Table;

#endif