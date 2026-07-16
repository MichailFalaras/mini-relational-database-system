#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/constraints.h"

extern uint32_t *copy_uint32_array(const uint32_t *source, uint32_t amount);

extern void constraint_shift_indexes(Constraint *constraint, uint32_t index_threshold);