#ifndef BTREE_UTILS_H_
#define BTREE_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct value Value;
typedef struct btree_page_collection BTreePageCollection;
typedef struct pager Pager;
typedef struct page Page;

/* ---------- Node Split Helpers ---------- */

SplitResult *split_result_create(Page *new_page, void *context);

bool btree_split_cells(Page *original_page, Page *new_page, void *context);

Page *btree_compact_page(Pager *pager, Page *old_page, void *context);

bool connect_sibling_leaf_nodes(Pager *pager, Page *original_page, Page *new_page);

/* ---------- Node Insert Helpers ---------- */

bool update_page_cell_and_payload(Pager *pager, Page *page, uint16_t result_index,
     uint16_t write_offset, uint16_t cell_count, void *payload, void *keys, void *context);

bool swap_internal_rightmost_child_pointer(Page *page, void *payload);

bool check_leaf_duplicate(Page *page, void *keys, uint16_t result_index, void *context, bool *duplicate);

/* ---------- Page Data Comparison Helpers ---------- */

bool btree_compare(Value **values, const void *key, void *context, int *result);

Value **btree_extract_payload_keys(uint8_t node_type, void *payload, void *context);

/* ---------- Page Data Capacity Helpers ---------- */

uint16_t btree_get_available_capacity(void *page_data);

bool btree_has_enough_space(void *page_data, uint16_t metadata_size);

uint32_t btree_get_cell_content_size(uint8_t node_type, void *keys, void *context);

uint16_t btree_get_key_size(const void *keys, void *context);

/* ---------- Shifting Page Data Helpers ---------- */

bool shift_metadata(void *page_data, uint16_t cell_pointer, void *context);

bool shift_cell_pointers(void *page_data, uint16_t index);

/* ---------- Page Data Access Helpers ----------*/

bool get_node_type(void *page_data, uint8_t *node_type);
bool set_node_type(void *page_data, uint8_t node_type);

bool get_root_status(void *page_data, uint8_t *root_status);
bool set_root_status(void *page_data, uint8_t root_status);

bool get_parent_pointer(void *page_data, uint32_t *parent_pointer);
bool set_parent_pointer(void *page_data, uint32_t parent_pointer);

bool get_cell_count(void *page_data, uint16_t *cell_count);
bool set_cell_count(void *page_data, uint16_t cell_count);

bool get_free_space_offset(void *page_data, uint16_t *free_space_offset);
bool set_free_space_offset(void *page_data, uint16_t free_space_offset);

bool get_cell_pointer(void *page_data, uint16_t cell_index, uint16_t *cell_pointer);
bool set_cell_pointer(void *page_data, uint16_t cell_index, uint16_t cell_pointer);

bool get_cell_child_pointer(void *page_data, uint16_t cell_pointer, uint32_t *child_pointer);
bool set_cell_child_pointer(void *page_data, uint16_t cell_pointer, uint32_t child_pointer);

bool get_cell_id(void *page_data, uint16_t cell_pointer, void *context, void **id);
bool set_cell_id(void *page_data, uint16_t cell_pointer, void *context, void *id);

bool get_cell_payload(void *page_data, uint16_t cell_pointer, void *context, void **payload);
bool set_cell_payload(void *page_data, uint16_t cell_pointer, void *context, void *payload);

bool get_rightmost_child_pointer(void *page_data, uint32_t *rightmost_child_pointer);
bool set_rightmost_child_pointer(void *page_data, uint32_t rightmost_child_pointer);

bool get_leaf_previous_pointer(void *page_data, uint32_t *previous);
bool set_leaf_previous_pointer(void *page_data, uint32_t previous);

bool get_leaf_next_pointer(void *page_data, uint32_t *next);
bool set_leaf_next_pointer(void *page_data, uint32_t next);

/* ---------- BTree/Index Traverse Helpers ---------- */

/* Helper that checks the page collection for pages that have already been visited */
extern bool btree_collection_contains(const BTreePageCollection *visited_pages, uint32_t page_num);

/* Helper that recursively traverses internal nodes, and backtracking at leaf nodes */
extern bool btree_traverse_page_recursive(uint32_t page_num, Pager *pager, BTreePageCollection *visited_pages);

#endif