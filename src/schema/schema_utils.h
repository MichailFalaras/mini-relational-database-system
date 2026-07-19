#include <stdint.h>

extern void shift_indexes(Schema *schema, Database *db, uint32_t index_threshold);

extern void close_array_gap(void **array, uint32_t count, uint32_t index_of_deletion);

extern void **resize_array(void **array, uint32_t new_size);