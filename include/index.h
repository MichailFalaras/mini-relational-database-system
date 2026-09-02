#ifndef INDEX_H_
#define INDEX_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"

// Used for creation of a temporary page placeholder for the newly created indexes
#define INVALID_ROOT_PAGE UINT32_MAX

#define SUPERBLOCK_PAGE_NUM 0
#define SYSTEM_CATALOG_PAGE_NUM  1
typedef struct pager Pager;
typedef struct schema Schema;
typedef enum data_types DataType;
typedef struct row Row;

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
    bool is_unique;
} Index;

/* Index search-related metadata */
typedef enum index_lookup_status {
    INDEX_LOOKUP_SUCCESS,
    INDEX_LOOKUP_NOT_FOUND,
    INDEX_LOOKUP_INVALID_ARGUMENTS,
    INDEX_LOOKUP_ERROR
} IndexLookupStatus;

typedef struct index_entry {
    Row *row;
} IndexEntry;

#define INDEX_RANGE_INITIAL_CAPACITY 16
typedef struct index_range_result {
    IndexEntry *entries;
    uint32_t count;
    uint32_t capacity;
} IndexRangeResult;

/* Index mutation-related metadata (insertion/update/deletion of entries) */
typedef enum index_mutation_status {
    INDEX_MUTATION_SUCCESS,
    INDEX_MUTATION_DUPLICATE_KEY,
    INDEX_MUTATION_NOT_FOUND,
    INDEX_MUTATION_INVALID_ARGUMENTS,
    INDEX_MUTATION_ERROR
} IndexMutationStatus;


/* Index metadata operations */
extern Index *index_metadata_create(const char *index_name, IndexType type, const IndexKey *key, 
    uint32_t root_page_num, bool is_unique);

extern void index_free(Index *index);

extern IndexKey *index_key_create(const uint32_t *column_indexes, uint32_t num_columns);

extern void index_key_free(IndexKey *key);

extern bool index_key_has_column(const Index *index, uint32_t index_key);

extern bool index_key_matches_key(const Index *index, const uint32_t *column_ids, uint32_t num_columns);

extern bool index_key_matches_prefix(const Index *index, const uint32_t *column_ids, uint32_t num_columns);


/* Index disk operations */
extern Index *index_create(const char *index_name, IndexType type, const IndexKey *key, Pager *pager, bool is_unique);

extern bool index_truncate(Index *index, Pager *pager);

extern bool index_drop(Index *index, Pager *pager);

// Find exact-match index entry
extern IndexLookupStatus index_find_exact(const Index *index, Pager *pager, Schema *schema,
    Value **key_values, const uint32_t *column_ids, uint32_t num_columns, IndexRangeResult *result);

// Find prefix-key matching entries
extern IndexLookupStatus index_find_prefix(const Index *index, Pager *pager, Schema *schema, 
    Value **prefix_key_values, const uint32_t *prefix_column_ids, uint32_t prefix_num_columns,
    IndexRangeResult *result);

// Find range of index entries that match the range query bounds
extern IndexLookupStatus index_find_range(const Index *index, Pager *pager, Schema *schema,
    Value **start_key_values, const uint32_t *start_column_ids, uint32_t start_num_columns, bool include_start, 
    Value **end_key_values, const uint32_t *end_column_ids, uint32_t end_num_columns, bool include_end, 
    IndexRangeResult *result);

// Full Index scan
extern IndexLookupStatus index_scan(const Index *index, Pager *pager, Schema *schema, IndexRangeResult *result);

// Insert Index entry
extern IndexMutationStatus index_insert_entry(Index *index, Pager *pager, Schema *schema, Row *row);

// Delete Index entry
extern IndexMutationStatus index_delete_entry(Index *index, Pager *pager, Schema *schema, Row *row);

#endif