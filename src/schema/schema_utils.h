#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/schema.h"

extern void shift_indexes(void **array, uint32_t index_threshold);

extern void close_array_gap(void **array, uint32_t count, uint32_t index_of_deletion);

extern void **resize_array(void **array, uint32_t new_size);