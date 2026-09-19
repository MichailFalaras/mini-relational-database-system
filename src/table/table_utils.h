#ifndef TABLE_UTILS_H_
#define TABLE_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct table Table;
typedef struct index Index;
typedef enum index_type IndexType;
typedef struct row Row;
typedef struct table_row_result TableRowResult;
typedef struct catalog Catalog;
typedef enum index_mutation_status IndexMutationStatus;
typedef enum table_mutation_status TableMutationStatus;
typedef enum catalog_status CatalogStatus;


/* Helper that validates a table's logical index metadata */
extern bool table_validate_logical_index(const Table *table, const Index *index, IndexType expected_type);

/* ---------- TableResult helpers ---------- */

extern bool table_row_result_init(TableRowResult *result);

extern bool table_row_result_append(TableRowResult *result, Row *row);

extern void table_row_result_free(TableRowResult *result);

/* Update index's root page number in corresponding catalog record */
extern TableMutationStatus table_sync_index_catalog_root(Table *table, Index *index, Catalog *catalog,
    uint32_t old_root_page_num);

/* Update index's root page number in corresponding catalog record */
extern TableMutationStatus table_sync_table_catalog_root(Table *table, Catalog *catalog, 
    uint32_t old_root_page_num);

/* Conversion from index mutation status to table mutation status */
extern TableMutationStatus index_mutation_to_table_mutation_status(IndexMutationStatus status);

/* Conversion from catalog status to table mutation status */
extern TableMutationStatus catalog_mutation_to_table_mutation_status(CatalogStatus status);

#endif