#ifndef BTREE_H_
#define BTREE_H_

#include <stdint.h>
#include <stdbool.h>
#include "pager.h"

typedef struct index_key IndexKey;
typedef struct row Row;
typedef enum data_types DataType;
typedef struct value Value;
typedef struct schema Schema;

typedef enum btree_node_type {
    BTREE_LEAF_NODE,
    BTREE_INTERNAL_NODE
} BTreeNodeType;

typedef enum btree_shift_direction {
    BTREE_SHIFT_INSERT,
    BTREE_SHIFT_DELETE
} BTreeShiftDirection;

/* Enum for all types of needed offsets. */
typedef enum btree_offsets {
    BTREE_NODE_TYPE_OFFSET = 0,
    BTREE_ROOT_FLAG_OFFSET = 1,
    BTREE_PARENT_OFFSET = 2,
    BTREE_CELL_COUNT_OFFSET = 6,
    BTREE_FREE_SPACE_OFFSET = 8,

    BTREE_COMMON_HEADER_SIZE = 10,

    BTREE_INTERNAL_RIGHTMOST_OFFSET = 10,
    BTREE_INTERNAL_HEADER_SIZE = 14,

    BTREE_LEAF_PREVIOUS_OFFSET = 10,
    BTREE_LEAF_NEXT_OFFSET = 14,
    BTREE_LEAF_HEADER_SIZE = 18
} BTreeOffsets;

/* BTree return status enum. */
typedef enum btree_status {
    BTREE_ERROR,
    BTREE_INVALID_ARGUMENTS,
    BTREE_CORRUPT_PAGE,
    BTREE_INVALID_PAGE,
    BTREE_FREE_PAGE,
    BTREE_NEEDS_SPLIT,
    BTREE_DUPLICATE_KEY,
    BTREE_SUCCESS
} BTreeStatus;

/* BTree Component with pager for traversal. */
typedef struct btree {
    Pager *pager;
    uint32_t root_page_num;
} BTree;

/* Lower Bound Binary Search Result. */
typedef struct btree_search_result {
    // If at least the position the key should have been in was found
    bool found; 
    // If exact same key was found
    bool exact_match;

    Page *page; // Refers to root-to-leaf traversal
    uint16_t result_index;
} BTreeSearchResult;

/* BTree Split Result after failed insertion. */
typedef struct btree_split_result {
    bool split;
    uint32_t right_page;
    uint16_t separator_size;
    void *separator_key;
} BTreeSplitResult;

typedef struct btree_key_view {
    void *key;
    uint16_t offset;
    uint16_t key_size;
} BTreeKeyView;

/* Direct access to cell with pointers and offset. */
typedef struct btree_cell_view {
    uint16_t offset;
    BTreeKeyView key;
    const uint8_t *payload;
    uint16_t payload_size;
} BTreeCellView;

/* Insert Struct. */
typedef struct btree_cell_contents {
    BTreeNodeType type;
    Value **keys;
    uint16_t num_keys;
    uint16_t key_size;
    union {
        Row *row;
        uint32_t child_pointer;
    } BTreePayload;
    uint16_t cell_size;
} BTreeCellContents;

/* Helper struct for BTreePage Union type_specific data. */
typedef struct btree_sibling_pointers {
    uint32_t previous_leaf_pointer;
    uint32_t next_leaf_pointer;
} BTreeSiblingPointers;

/* BTree RAM struct with all needed already extracted
 * metadata, ready for immediate use. */
typedef struct btree_page {
    Page *page;
    BTreeNodeType type;
    uint8_t is_root;
    uint32_t parent_pointer;
    uint16_t cell_count;
    uint16_t free_space_offset; 
    uint8_t *data;
    union {
        uint32_t rightmost_child_pointer;
        BTreeSiblingPointers siblings;
    } type_specific_data;
} BTreePage;

// Traversal-related structure that keeps track of the visited pages 
// during the Index traversal
typedef struct btree_page_collection {
    uint32_t page_numbers[MAX_PAGES];
    uint32_t count;
} BTreePageCollection;

/* Structs replacing: KeyExtractionContext */
typedef struct btree_index_spec {
    IndexKey *index_key; // Containts columns that comprise the index key
    Schema *schema; // Schema needed for row extraction
    DataType *column_types; // Containts their data type
    bool is_unique; // If Index is unique, for duplicate key checks
    uint16_t key_size; // Key size for specific BTree
} BTreeIndexSpec;

/* Active Search Key containing index specifications and information
 * about the target key itself. */
typedef struct btree_search_key {
    BTreeIndexSpec *index;
    const void *target_key;

    /* Amount of keys used to traverse BTree.
     * Can be less than keys organizing the BTree. */
    uint16_t num_target_keys; 
} BTreeSearchKey;



/* Initialize btree_page as an Empty Leaf Node. */
extern BTreeStatus btree_page_init_empty_leaf(BTreePage *btree_page);

extern BTreeStatus btree_page_init_internal(BTreePage *btree_page, uint32_t rightmost_child_pointer);

/* Lower Bound Binary Search with a target key.
 * Used to traverse through the B+Tree and find correct cell position in a page.
 * Return important information in BTreeSearchResult. */
extern BTreeStatus btree_lower_bound_search(BTreePage *btree_page, BTreeSearchKey *search_key,
    BTreeSearchResult *search_result);

/* Root to leaf traversal using a specific key to ultimately reach
 * a cell position to store data.
 * Also returns important information in BTreeSearchResult. */
extern BTreeStatus btree_root_to_leaf(BTree *btree, BTreeSearchKey *search_key, BTreeSearchResult *search_result);

/* BTree Node insert type agnostic function.
 * Content being inserted is stored in BTreeCellContents.
 * Returns BTREE_SUCCESS or BTREE_NEEDS_SPLIT/BTreeSplitResult with split boolean value equal to true. */
extern BTreeStatus btree_node_insert(Pager *pager, BTreePage *btree_page, BTreeCellContents *cell_contents,
                            BTreeSplitResult *split_result, BTreeIndexSpec *index);

/* BTree Leaf Node Split.
 * Returns important split information in BTreeSplitResult. */
extern BTreeStatus btree_leaf_node_split(Pager *pager, BTreePage *original_page, BTreeIndexSpec *index,
                                BTreeSplitResult *split_result);

/* BTree Root Node Split.
 * Split has already happened, this just creates new root, moves separator key
 * and updates pages' metadata.
 * Returns split information in BTreeSplitResult. */
extern BTreeStatus btree_root_split(BTree *btree, BTreePage *btree_old_root, BTreeSplitResult *split_result, BTreeIndexSpec *index);

// Traverse B+Tree
extern BTreeStatus btree_traverse_reachable_pages(BTree *btree, BTreePageCollection *visited_pages);

#endif