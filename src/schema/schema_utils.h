#ifndef SCHEMA_UTILS_H_
#define SCHEMA_UTILS_H_

#include <stdint.h>

typedef struct schema Schema;
typedef struct database Database;

extern void shift_column_refs_after_drop(Schema *schema, Database *db, uint32_t index_threshold);

extern void close_array_gap(void **array, uint32_t count, uint32_t index_of_deletion);

extern void **resize_array(void **array, uint32_t new_size);

#endif