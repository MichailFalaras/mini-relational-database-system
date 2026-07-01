#ifndef TABLE_H_
#define TABLE_H_

#include "btree.h"

/* Table structure that contains:
 * total_rows: total rows of table
 * n_pages: amount of pages of the table
 * root: root node of table's Btree
 * offset: table offset in database file (e.g. users.db)
 * data_types: column data types. */
typedef struct table {
    int total_rows;
    int n_pages;
    BtreeNode *root;
    unsigned int offset;
    char **data_types;
} Table;

#endif