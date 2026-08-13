#ifndef INDEX_UTILS_H_
#define INDEX_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct index Index;
typedef struct row Row;

/* Decrement Index Key columns' position index after removing a column */
extern bool index_key_shift_after_column_drop(Index *index, uint32_t col_pos);

/* ---------- IndexRangeResult helpers ---------- */

extern bool index_range_result_init(IndexRangeResult *result);

extern bool index_range_result_append(IndexRangeResult *result, Row *row);

extern bool index_entry_free(IndexEntry *entry);

extern bool index_range_result_free(IndexRangeResult *result);

#endif