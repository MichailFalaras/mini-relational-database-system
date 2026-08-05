#ifndef TABLE_UTILS_H_
#define TABLE_UTILS_H_

#include <stdbool.h>

typedef struct table Table;
typedef struct index Index;
typedef enum index_type IndexType;

/* Helper that validates a table's logical index metadata */
extern bool table_validate_logical_index(const Table *table, const Index *index, IndexType expected_type);

#endif