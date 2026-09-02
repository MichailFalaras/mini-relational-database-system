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
typedef struct catalog_payload CatalogPayload;

typedef enum btree_node_type {
    BTREE_LEAF_NODE,
    BTREE_INTERNAL_NODE
} BTreeNodeType;

typedef enum btree_binary_search_type {
    BTREE_LOWER_BOUND,
    BTREE_UPPER_BOUND
} BTreeBinarySearchType;

typedef enum btree_shift_direction {
    BTREE_SHIFT_INSERT,
    BTREE_SHIFT_DELETE
} BTreeShiftDirection;

typedef enum btree_borrow_direction {
    BTREE_BORROW_FROM_LEFT,
    BTREE_BORROW_FROM_RIGHT
} BTreeBorrowDirection;

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
    BTREE_NODE_UNDERFLOW,
    BTREE_UNDERFLOW_UNRESOLVED,
    BTREE_NEEDS_MERGE,
    BTREE_SUCCESS,
    BTREE_NOT_FOUND
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

/* BTree Split Result after insertion. */
typedef struct btree_split_result {
    bool split;
    uint32_t left_page;
    uint32_t right_page;
    void *separator_key;
    uint16_t separator_size;
} BTreeSplitResult;

/* Direct access to cell keys with pointers and offset. */
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
        CatalogPayload *catalog;
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
    void *target_key;

    /* Amount of keys used to traverse BTree.
     * Can be less than keys organizing the BTree. */
    uint16_t num_target_keys; 
} BTreeSearchKey;

/* Result structure storing the matching cell contents for a range query */
#define BTREE_RANGE_INITIAL_CAPACITY 16

// Update BTree search result entries to store the page & cell number of each matching entry
typedef struct btree_entry {
    BTreeCellContents cell;
    uint32_t page_num;
    uint16_t cell_index;
} BTreeEntry;
typedef struct btree_search_entries {
    BTreeEntry *entries;
    uint32_t count;
    uint32_t capacity;
} BTreeSearchEntries;

/* BTree Deletion Result after deletion. */
typedef struct btree_deletion_result {
    bool deleted;
    bool underflow;
    uint32_t page_num;

    bool first_key_changed;
    BTreeCellContents first_cell;
} BTreeDeletionResult;

/* BTree Insertion Result for insertion orchestration. */
typedef struct btree_insertion_result {
    // Refers to leaf node
    bool inserted;
    uint32_t insertion_page_num;

    // Refers to split propagation and separator
    // cell insertions to the parents above
    bool splitted;
    uint32_t split_levels;
} BTreeInsertionResult;

/* BTree Merge Result for merging underflowing pages that
 * cannot have their cells redistributed. */
typedef struct btree_merge_result {
    bool needs_merge; // Flag to make sure merge is needed

    // Merging always right to left
    uint32_t underflowing_page_num;
    uint32_t sibling_page_num;

    // Parent page num and parent cell index references
    uint32_t parent_page_num;
    uint32_t parent_underflowing_cell_index;
    uint32_t parent_sibling_cell_index;
} BTreeMergeResult;

/* Initialize btree_page as an Empty Leaf Node. */
extern BTreeStatus btree_page_init_empty_leaf(BTreePage *btree_page);

extern BTreeStatus btree_page_init_internal(BTreePage *btree_page, uint32_t rightmost_child_pointer);

/* Lower Bound Binary Search with a target key.
 * Used to traverse through the B+Tree and find correct cell position in a page.
 * Return important information in BTreeSearchResult. */
extern BTreeStatus btree_binary_search(BTreePage *btree_page, BTreeSearchKey *search_key,
    BTreeSearchResult *search_result, BTreeBinarySearchType mode);

/* Root to leaf traversal using a specific key to ultimately reach
 * a cell position to store data.
 * Also returns important information in BTreeSearchResult. */
extern BTreeStatus btree_root_to_leaf(BTree *btree, BTreeSearchKey *search_key, BTreeSearchResult *search_result, 
    BTreeBinarySearchType mode);
    
/* BTree Node insert type agnostic function.
 * Content being inserted is stored in BTreeCellContents.
 * Returns BTREE_SUCCESS or BTREE_NEEDS_SPLIT/BTreeSplitResult with split boolean value equal to true. */
extern BTreeStatus btree_node_insert(Pager *pager, BTreePage *btree_page, BTreeCellContents *cell_contents,
                            BTreeSplitResult *split_result, BTreeIndexSpec *index);

/* BTree type agnostic deletion of cell through search key. */
extern BTreeStatus btree_node_delete(BTree *btree, BTreeCellContents *target_cell, BTreeDeletionResult *deletion_result,
    BTreeIndexSpec *index, BTreePage *target_leaf);
    
/* BTree Leaf Node Split.
 * Returns important split information in BTreeSplitResult. */
extern BTreeStatus btree_leaf_node_split(Pager *pager, BTreePage *original_page, BTreeIndexSpec *index,
                                BTreeSplitResult *split_result);
/* BTree Internal Node Split. 
 * Returns split information BTreeSplitResult. */
extern BTreeStatus btree_internal_node_split(Pager *pager, BTreePage *original_page, BTreeIndexSpec *index,
                                BTreeSplitResult *split_result);

/* BTree Root Node Split.
 * Split has already happened, this just creates new root, moves separator key
 * and updates pages' metadata.
 * Returns split information in BTreeSplitResult. */
extern BTreeStatus btree_root_split(BTree *btree, BTreePage *btree_old_root, BTreeSplitResult *split_result, BTreeIndexSpec *index);

// Traverse B+Tree
extern BTreeStatus btree_traverse_reachable_pages(BTree *btree, BTreePageCollection *visited_pages);

// Find B+ Tree Key 
extern BTreeStatus btree_find_exact_key(BTree *btree, BTreeSearchKey *search_key, BTreeSearchResult *search_result,
    BTreeSearchEntries *result);

// Find B+ Tree Key 
extern BTreeStatus btree_find_prefix_keys(BTree *btree, BTreeIndexSpec *index, BTreeSearchKey *prefix_key,
    BTreeSearchEntries *result);

// Find B+ Tree Range of Keys
extern BTreeStatus btree_find_range_keys(BTree *btree, BTreeIndexSpec *index, BTreeSearchKey *start_search_key, 
    bool includes_start, BTreeSearchKey *end_search_key, bool includes_end, BTreeSearchEntries *result);

/* Borrow a leaf cell from a sibling leaf node and
 * update parent internal node key. */
extern BTreeStatus btree_leaf_borrow(Pager *pager, uint32_t parent_cell_pointer_index, BTreePage *underflowing_page, BTreePage *parent, BTreePage *lender,
    BTreeBorrowDirection borrow_dir, BTreeIndexSpec *index);
  
/* Borrow an internal cell from a parent and promote
 * lender's internal cell upwards. */
extern BTreeStatus btree_internal_borrow(Pager *pager, uint32_t parent_cell_pointer_index, BTreePage *underflowing_page, BTreePage *parent, BTreePage *lender,
    BTreeBorrowDirection borrow_dir, BTreeIndexSpec *index);

/* B+Tree borrow wrapper function.  */
extern BTreeStatus btree_node_borrow(Pager *pager, uint32_t parent_cell_pointer_index, BTreePage *underflowing_page, BTreePage *parent, 
    BTreePage *lender, BTreeBorrowDirection borrow_dir, BTreeIndexSpec *index);

/* Merge 2 leaf nodes. */
extern BTreeStatus btree_leaf_merge(Pager *pager, BTreePage *left, BTreePage *right, BTreePage *parent_page,
    uint32_t separator_cell_index, BTreeIndexSpec *index);

/* Merge 2 internal nodes. */
extern BTreeStatus btree_internal_merge(Pager *pager, BTreePage *left, BTreePage *right, BTreePage *parent_page,
    uint32_t separator_cell_index, BTreeIndexSpec *index);
    
/* B+Tree merge wrapper function. */
extern BTreeStatus btree_node_merge(Pager *pager, BTreeMergeResult *merge_result, BTreeIndexSpec *index);

/* B+Tree root *internal* node collapse after merging children. 
 * (NOTE: Update Index's new root page num). */
extern BTreeStatus btree_root_collapse(BTree *btree, BTreePage *old_root, BTreeIndexSpec *index);

/* ---- B+Tree orchestration ---- */

/* BTree cell redistribution in case of underflowing nodes. */
extern BTreeStatus btree_node_redistribution(Pager *pager, BTreePage *underflowing_page, BTreeIndexSpec *index,
    BTreeMergeResult *merge_result);

/* BTree Insertion Orchestration function. */
extern BTreeStatus btree_insert(BTree *btree, BTreeCellContents *cell_contents, BTreeInsertionResult *insertion_res,
    BTreeIndexSpec *index);

/* BTree deletion & underflow orchestration. */
extern BTreeStatus btree_delete(BTree *btree, BTreeCellContents *cell_contents, BTreeDeletionResult *deletion_res, BTreeIndexSpec *index);

/* BTree Split Propagation function. */
extern BTreeStatus btree_split_propagation(BTree *btree, BTreePage *leaf_node, BTreeCellContents *pending_leaf_cell,
    BTreeSplitResult *split_result, BTreeIndexSpec *index, BTreeInsertionResult *insertion_res);

#endif