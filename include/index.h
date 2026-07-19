#ifndef INDEX_H_
#define INDEX_H_

#include <stdint.h>
#include <stdbool.h>

/* Index type. */
typedef enum index_type {
    PRIMARY_INDEX,
    SECONDARY_INDEX,
} IndexType;

/* Index key contains:
 * column_index_keys: indexes to the columns that are the keys
 * amount_keys: amount of keys for index*/
typedef struct index_key {
    uint32_t *column_index_keys;
    uint32_t amount_keys;
} IndexKey;

/* Logical BTree node struct that contains:
 * name: name of index
 * type: type of index
 * root_page_num: page id that corresponds to
 * the root node of the index. */
typedef struct index {
    char name[64];
    IndexType type;
    IndexKey *key;
    uint32_t root_page_num;
} Index;

extern Index *index_create(const char *index_name, IndexType type, IndexKey *key, uint32_t root_page_num);

extern bool index_drop(const char *index_name);

extern bool index_key_has_column(const Index *index, uint32_t index_key);

extern bool index_key_matches_key(const Index *index, const uint32_t *column_ids, uint32_t amount_columns);

extern void index_free(Index *index);

#endif