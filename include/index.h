#ifndef INDEX_H_
#define INDEX_H_

#include "pager.h"
#include "data_types.h"

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
    uint32_t num_keys;
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

/* Physical creation of Index on disk */
extern Index *index_create(const char *index_name, IndexType type, const IndexKey *key, Pager *pager);

/* Physical deletion of Index on disk */
extern bool index_drop(Index *index, Pager *pager);

/* Creation of logical Index struct */
extern Index *index_metadata_create(const char *index_name, IndexType type, IndexKey *key, uint32_t root_page_num);

/* Deallocation of logical Index struct */
extern void index_free(Index *index);

/* Create Index key */
extern IndexKey *index_key_create(const uint32_t *column_indexes, uint32_t num_columns);

/* Free Index key */
extern void index_key_free(IndexKey *key);

/* Check for columns in Index */
extern bool index_key_has_column(const Index *index, uint32_t index_key);

extern bool index_key_matches_key(const Index *index, const uint32_t *column_ids, uint32_t num_columns);


#endif