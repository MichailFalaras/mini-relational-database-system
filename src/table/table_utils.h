#ifndef TABLE_UTILS_H_
#define TABLE_UTILS_H_

#include <stdbool.h>

typedef struct table Table;
typedef struct index Index;
typedef enum index_type IndexType;
typedef struct row Row;

/* Helper that validates a table's logical index metadata */
extern bool table_validate_logical_index(const Table *table, const Index *index, IndexType expected_type);

/* ---------- TableResult helpers ---------- */

extern bool table_row_result_init(TableRowResult *result);

extern bool table_row_result_append(TableRowResult *result, Row *row);

extern void table_row_result_free(TableRowResult *result);

#endif