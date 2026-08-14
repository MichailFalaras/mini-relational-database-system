#ifndef BTREE_UTILS_H_
#define BTREE_UTILS_H_

#include "../../include/btree.h"
#include "../../include/page.h"

typedef struct index Index;
typedef struct value Value;
typedef struct page Page;
typedef struct pager Pager;
typedef enum btree_shift_direction BTreeShiftDirection;
typedef struct btree BTree;
typedef struct btree_page BTreePage;
typedef struct btree_key_view BTreeKeyView;
typedef struct btree_index_spec BTreeIndexSpec;
typedef struct btree_search_key BTreeSearchKey;
typedef struct btree_cell_contents BTreeCellContents;
typedef struct btree_search_result BTreeSearchResult;

#define MIN_CELL_SIZE 5 // lets suppose key is a bool (uint8_t) and a child pointer (uint32_t)

/* ---------- BTreePage Component Interface ---------- */

/* Attach specific page to a BTreePage component. */
void btree_page_attach(BTreePage *btree_page, Page *page);

/* Validate BtreePage's header metadata. */
BTreeStatus btree_page_validate(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index);

/* Load already existing header metadata from page_data onto BTreePage.*/
void btree_page_load(BTreePage *btree_page);

/* Sync BTreePage header metadata with page's page_data in order to be
 * written in the disk later. */
void btree_page_sync(Pager *pager, BTreePage *btree_page);

/* Wrapper function that initializes and validates BTreePage fully. */
BTreeStatus btree_page_attach_load_validate(Pager *pager, BTreePage *btree_page, Page *page, BTreeIndexSpec *index);


/* ---------- BTreeRangeResult Helpers ---------- */

// Initialize BTreeRangeResult fields
extern BTreeStatus btree_range_result_init(BTreeRangeResult *result);

// Append cell content structure to the result
extern BTreeStatus btree_range_result_append(BTreeRangeResult *result, BTreeCellContents *new_cell);

// Free individual cell content structure
extern void btree_cell_contents_free(BTreeCellContents *cell);

// Free BTreeRangeResult
extern void btree_range_result_free(BTreeRangeResult *result);


/* ---------- BTreeIndexSpec Helpers ---------- */

// Initialize BTreeIndexSpec fields
extern bool btree_index_spec_init(const Index *index, Schema *schema, BTreeIndexSpec *spec);

// Free BTreeIndexSpec fields
extern void index_btree_spec_free(BTreeIndexSpec *spec);

/* ---------- Miscellaneous  ---------- */

/* Allocate memory for serialized separator key from key_view. */
void *separator_key_alloc(BTreeKeyView *key_view);

/* Update internal node's connected leaf nodes' parent pointers right after split. */
BTreeStatus update_parent_pointers(Pager *pager, BTreePage *right_page, BTreeIndexSpec *index);

/* Get exact offset and size of key and store it in BTreeKeyView. */
BTreeStatus get_key(BTreePage *btree_page, uint16_t cell_index, BTreeKeyView *key, BTreeIndexSpec *index);

/* Comparing BTree Internal/Leaf Node Keys. */
BTreeStatus btree_compare(Value **values, BTreeKeyView *btree_key, BTreeSearchKey *search_key, int *result);

/* Check if there's enough space to store a Cell and its Cell Pointer. */
BTreeStatus btree_page_has_enough_space(BTreePage *btree_page, BTreeCellContents *cell_contents);

/* Check if leaf is duplicate by comparing the keys. */
BTreeStatus check_leaf_duplicate(BTreePage *btree_page, BTreeSearchKey *search_key,
    BTreeSearchResult *search_result, bool *duplicate);

/* Inserting into an internal node and the binary search's returned
 * index is equal to the page's cell count then:
 * swap rightmost child pointer with the child pointer contained in
 * the payload*/
BTreeStatus swap_internal_rightmost_child_pointer(BTreePage *btree_page, BTreeCellContents *cell_contents);

/* Remove garbage cell pointers and contents by creating a new page and transfering
 * ONLY the valid metadata there. */
BTreeStatus btree_compact_page(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index);

/* Connect sibling leaf nodes right after splitting. */
BTreeStatus connect_sibling_leaf_nodes(Pager *pager, BTreePage *btree_page1, BTreePage *btree_page2);

/* Find the leftmost leaf page */
BTreeStatus btree_find_leftmost_page(BTree *btree, BTreeIndexSpec *index, Page **res_page);


/* ---------- Cell Operations/Interface ---------- */

/* BTreeCellView RAM Component Interface for easy cell access with offset and its length.
 * DOESNT update the Page Data, just for cell viewing and accesibility. */
BTreeStatus get_cell(BTreePage *btree_page, uint16_t cell_index, BTreeCellView *cell, BTreeIndexSpec *index);

// Updates Cell Pointer with new [ offset + size ]
BTreeStatus set_cell(BTree *btree, BTreePage *btree_page, BTreeIndexSpec *index, BTreeSearchResult *search_result,
                    BTreeCellContents *cell_contents);

/* Insert new cell's Cell Pointer and its contents onto a BTreePage AFTER BINARY SEARCH. */
BTreeStatus insert_cell(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index, BTreeSearchResult *search_result,
    BTreeCellContents *cell_contents);

/* Transfer half of src's cells to dest and update cell count. */
BTreeStatus btree_split_cells(BTreePage *src, BTreePage *dest, BTreeIndexSpec *index);

/* Function that actually does that transferring of cell pointers and contents. */
BTreeStatus btree_transfer_cells(BTreePage *src, uint16_t src_idx, BTreePage *dest, uint16_t dest_idx, BTreeIndexSpec *index);

/* Remove cell by SHIFTING DOWN cell pointers and contents. */
BTreeStatus btree_remove_cell(BTreePage *btree_page, uint32_t cell_pointer_index);


/* ---------- Shift Cell Pointers & Cell Contents ---------- */

/* Shift cell pointer forward/backwards to insert/delete specific cell pointer. */
BTreeStatus shift_cell_pointer(BTreePage *btree_page, uint16_t index, BTreeShiftDirection shift_direction);

/* Shift cell forward/backwards to insert/delete specific cell and its contents. */
BTreeStatus shift_cell(BTreePage *btree_page, uint32_t cell_pointer, BTreeShiftDirection shift_direction);

/* Update cell pointers offset that was affected because of shift_cell. */
BTreeStatus update_cell_pointers_offset(BTreePage *btree_page, uint32_t boundary_cell_pointer, uint16_t cell_size, 
    BTreeShiftDirection shift_direction);

/* ---------- Inline Helpers for immediate Page Metadata Access ---------- */

/* Cell Pointer now containts both the cell's offset and its length. */
static inline uint32_t make_cell_pointer(uint16_t offset, uint16_t size) {
    return ((uint32_t)offset << 16) | (uint32_t) size;
}

static inline uint16_t get_cell_offset(uint32_t cell_pointer) {
    return (uint16_t)(cell_pointer >> 16);
}

static inline uint16_t get_cell_size(uint32_t cell_pointer) {
    return (uint16_t)(cell_pointer & 0xFFFF);
}

static inline uint16_t get_header_size(BTreeNodeType node_type) {
    return (node_type == BTREE_INTERNAL_NODE) ? BTREE_INTERNAL_HEADER_SIZE : BTREE_LEAF_HEADER_SIZE;
}

// Solving Endianness/Portability and Unaligned Access
static inline uint16_t load_u16_le(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline void store_u16_le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

static inline uint32_t load_u32_le(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | 
           ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static inline void store_u32_le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static inline BTreeNodeType get_node_type(const uint8_t *page_data) {
    return (BTreeNodeType)(page_data)[BTREE_NODE_TYPE_OFFSET];
}

static inline void set_node_type(uint8_t *page_data, BTreeNodeType type) {
    page_data[BTREE_NODE_TYPE_OFFSET] = (uint8_t )type;
}

static inline uint8_t get_root_status(const uint8_t *page_data) {
    return page_data[BTREE_ROOT_FLAG_OFFSET];
}

static inline void set_root_status(uint8_t *page_data, uint8_t is_root) {
    page_data[BTREE_ROOT_FLAG_OFFSET] = is_root ? 1 : 0;
}

static inline uint32_t get_parent_pointer(const uint8_t *page_data) {
    return load_u32_le(page_data + BTREE_PARENT_OFFSET);
}

static inline void set_parent_pointer(uint8_t *page_data, uint32_t parent) {
    store_u32_le(page_data + BTREE_PARENT_OFFSET, parent);
}

static inline uint16_t get_cell_count(const uint8_t *page_data) {
    return load_u16_le(page_data + BTREE_CELL_COUNT_OFFSET);
}

static inline void set_cell_count(uint8_t *page_data, uint16_t count) {
    store_u16_le(page_data + BTREE_CELL_COUNT_OFFSET, count);
}

static inline uint16_t get_free_space_offset(const uint8_t *page_data) {
    return load_u16_le(page_data + BTREE_FREE_SPACE_OFFSET);
}

static inline void set_free_space_offset(uint8_t *page_data, uint16_t offset) {
    store_u16_le(page_data + BTREE_FREE_SPACE_OFFSET, offset);
}

static inline uint32_t get_cell_pointer(const uint8_t *page_data, uint16_t cell_index) {
    BTreeNodeType node_type = get_node_type(page_data);
    uint16_t header_size = get_header_size(node_type);

    return load_u32_le(page_data + header_size + (cell_index * 4));
}

static inline void set_cell_pointer(uint8_t *page_data, uint16_t cell_index, uint32_t cell_pointer) {
    BTreeNodeType node_type = get_node_type(page_data);
    uint16_t header_size = get_header_size(node_type);

    store_u32_le(page_data + header_size + (cell_index * 4), cell_pointer);
}

static inline uint32_t get_cell_child_pointer(const uint8_t *page_data, uint16_t cell_offset) {
    return load_u32_le(page_data + cell_offset);
}

static inline void set_cell_child_pointer(uint8_t *page_data, uint16_t cell_offset, uint32_t child_page) {
    store_u32_le(page_data + cell_offset, child_page);
}

static inline uint32_t get_rightmost_child(const uint8_t *page_data) {
    return load_u32_le(page_data + BTREE_INTERNAL_RIGHTMOST_OFFSET);
}

static inline void set_rightmost_child(uint8_t *page_data, uint32_t child) {
    store_u32_le(page_data + BTREE_INTERNAL_RIGHTMOST_OFFSET, child);
}

static inline uint32_t get_leaf_previous(const uint8_t *page_data) {
    return load_u32_le(page_data + BTREE_LEAF_PREVIOUS_OFFSET);
}

static inline void set_leaf_previous(uint8_t *page_data, uint32_t previous) {
    store_u32_le(page_data + BTREE_LEAF_PREVIOUS_OFFSET, previous);
}

static inline uint32_t get_leaf_next(const uint8_t *page_data) {
    return load_u32_le(page_data + BTREE_LEAF_NEXT_OFFSET);
}

static inline void set_leaf_next(uint8_t *page_data, uint32_t next) {
    store_u32_le(page_data + BTREE_LEAF_NEXT_OFFSET, next);
}

/* ---------- BTree/Index Traverse Helpers ---------- */

/* Helper that checks the page collection for pages that have already been visited */
extern bool btree_collection_contains(const BTreePageCollection *visited_pages, uint32_t page_num);

/* Helper that recursively traverses internal nodes, and backtracking at leaf nodes */
extern BTreeStatus btree_traverse_page_recursive(BTree *btree, uint32_t page_num, BTreePageCollection *visited_pages);

#endif