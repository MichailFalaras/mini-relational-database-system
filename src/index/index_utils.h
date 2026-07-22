#ifndef INDEX_UTILS_H_
#define INDEX_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct index Index;

/* Decrement Index Key columns' position index after removing a column */
extern bool index_key_shift_after_column_drop(Index *index, uint32_t col_pos);

#endif