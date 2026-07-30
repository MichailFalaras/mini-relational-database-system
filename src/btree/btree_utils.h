#ifndef BTREE_UTILS_H_
#define BTREE_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

uint16_t btree_get_key_size(const void *keys, void *context);

Value **btree_extract_row_keys(void *payload, void *context);

uint16_t btree_get_available_capacity(void *page_data);

bool btree_has_enough_space(void *page_data, uint16_t payload_size);

bool btree_compare(Value **values, const void *key, void *context, int *result);

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

bool get_leaf_sibling_pointers(void *page_data, uint32_t *previous, uint32_t *next);
bool set_leaf_sibling_pointers(void *page_data, uint32_t previous, uint32_t next);

#endif