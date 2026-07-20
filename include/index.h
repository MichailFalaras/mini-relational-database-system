#ifndef INDEX_H_
#define INDEX_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct pager Pager;

/* Index type. */
typedef enum index_type {
    PRIMARY_INDEX,
    SECONDARY_INDEX,
} IndexType;

/* Index key contains:
 * column_index_array: index array to the columns that comprise the index key
 * num_columns: amount of columns for index key */
typedef struct index_key {
    uint32_t *column_index_array;
    uint32_t num_columns;
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


/* Index metadata operations */
extern Index *index_metadata_create(const char *index_name, IndexType type, IndexKey *key, uint32_t root_page_num);

extern void index_free(Index *index);

extern IndexKey *index_key_create(const uint32_t *column_indexes, uint32_t num_columns);

extern void index_key_free(IndexKey *key);

extern bool index_key_has_column(const Index *index, uint32_t index_key);

extern bool index_key_matches_key(const Index *index, const uint32_t *column_ids, uint32_t num_columns);

extern bool index_key_matches_prefix( const Index *index, const uint32_t *column_ids, uint32_t num_columns);


/* Index disk operations */
extern Index *index_create(const char *index_name, IndexType type, const IndexKey *key, Pager *pager);

extern bool index_drop(Index *index, Pager *pager);

#endif