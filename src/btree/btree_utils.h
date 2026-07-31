#ifndef BTREE_UTILS_H_
#define BTREE_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct value Value;
typedef struct btree_page_collection BTreePageCollection;
typedef struct pager Pager;
typedef struct page Page;

SplitResult *split_result_create(Page *new_page, void *context);

bool btree_split_cells(Page *original_page, Page *new_page, void *context);

bool btree_transfer_cell(Page *dest, uint16_t dest_cell_index, Page *src, uint16_t src_cell_index,
                        uint8_t node_type, uint16_t *write_offset, void *context);

Page *btree_compact_page(Pager *pager, Page *old_page, void *context);

uint32_t btree_get_cell_content_size(void *page_data, uint16_t cell_pointer, void *context);

uint16_t btree_get_key_size(const void *keys, void *context);

Value **btree_extract_row_keys(void *payload, void *context);

uint16_t btree_get_available_capacity(void *page_data);

bool btree_has_enough_space(void *page_data, uint16_t payload_size);

bool btree_compare(Value **values, const void *key, void *context, int *result);

bool shift_metadata(void *page_data, uint16_t cell_pointer, void *context);

bool shift_cell_pointers(void *page_data, uint16_t index);

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


/* Helper that checks the page collection for pages that have already been visited */
extern bool btree_collection_contains(const BTreePageCollection *visited_pages, uint32_t page_num);

/* Helper that recursively traverses internal nodes, and backtracking at leaf nodes */
extern bool btree_traverse_page_recursive(uint32_t page_num, Pager *pager, BTreePageCollection *visited_pages);

#endif