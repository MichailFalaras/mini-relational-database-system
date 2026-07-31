#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../include/index.h"
#include "../../include/row.h"
#include "../../include/btree.h"
#include "../data_types/data_types_utils.h"
#include "btree_utils.h"
#include "../../include/page.h"
#include "../../include/pager.h"
#include "../../include/serialize.h"

/* Create SplitResult struct with keys and page number of new page.*/
SplitResult *split_result_create(Page *new_page, void *context) {
    if (!new_page || !context) {
        return NULL;
    }

    uint16_t cell_pointer = 0;
    if (!get_cell_pointer((void *) new_page->page_data, 0, &cell_pointer)) {
        return NULL;
    }

    void *id;
    if (!get_cell_id((void *) new_page->page_data, cell_pointer, context, &id)) {
        return NULL;
    }

    SplitResult *split_result = (SplitResult *) calloc(1, sizeof(SplitResult));
    if (!split_result) {
        return NULL;
    }
    split_result->separator_key = id;
    split_result->new_page_num = new_page->page_num;

    return split_result;
}

/* BTree splitting cells in half and transfering them. */
bool btree_split_cells(Page *original_page, Page *new_page, void *context) {
    if (!original_page || !new_page || !context) {
        return false;
    }

    /* Get cell count and split it. */
    uint16_t cell_count = 0;
    if (!get_cell_count((void *)original_page->page_data, &cell_count)) {
        return false;
    }
    uint16_t cell_index = cell_count / 2;

    /* Get free space offset of NEW PAGE. (Should be PAGE_SIZE) */
    uint16_t free_space_offset = 0;
    if (!get_free_space_offset((void *) new_page->page_data, &free_space_offset)) {
        return false;
    }
    uint16_t write_offset = free_space_offset; 

    /* Get node type of ORIGINAL PAGE. */
    uint8_t node_type = 0;
    if (!get_node_type((void *) original_page->page_data, &node_type)) {
        return false;
    }

    /* Transfer cells. */
    for (uint16_t i = cell_index; i < cell_count; i++) {
        if (!btree_transfer_cell(
            new_page, i - cell_index,
            original_page, i,
            node_type, 
            &write_offset,
             context
            )) {
            return false;
        }
    }

    /* Update cell count on both pages. */
    if (!set_cell_count((void *) original_page->page_data, cell_index)
        || !set_cell_count((void *) new_page->page_data, cell_count - cell_index)) {
        return false;
    }

    if (!set_free_space_offset((void *) new_page->page_data, write_offset)) {
        return false;
    }

    return true;
}

/* Transfers cells from src to dest. */
bool btree_transfer_cell(Page *dest, uint16_t dest_cell_index, Page *src, uint16_t src_cell_index,
    uint8_t node_type, uint16_t *write_offset, void *context) {
    if (!dest || !src || !write_offset || !context) {
        return false;
    }
    bool success = false;
    
    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    void *ids = NULL;
    void *payload = NULL;

    uint16_t cell_pointer;
    /* Get src's cell pointer in order to get [ Keys + Row ].*/
    if (!get_cell_pointer((void *) src->page_data, src_cell_index, &cell_pointer)
        || !get_cell_id((void *) src->page_data, cell_pointer, context, &ids)) {
        goto cleanup;
    }

    /* Calculate key size. */
    uint16_t key_size = btree_get_key_size(ids, context);
    if (!key_size) {
        goto cleanup;
    }

    /* Depending on node type, move back the write offset as many bytes as needed
     * and write [ Child Pointer + Keys] or [ Keys + Row ]. */
    if (node_type) {
        *write_offset -= 4 + key_size;

        uint32_t child_pointer = 0;
        if (!set_cell_id((void *) dest->page_data, *write_offset, context, ids) 
            || !get_cell_child_pointer((void *) src->page_data, cell_pointer, &child_pointer)
            || !set_cell_child_pointer((void *) dest->page_data, *write_offset, child_pointer)) {
            goto cleanup;
        }

    } else {
        *write_offset -= sizeof(Row) + key_size;

        if (!set_cell_id((void *) dest->page_data, *write_offset, context, ids) 
            || !get_cell_payload((void *) src->page_data, cell_pointer, context, &payload)
            || !set_cell_payload((void *) dest->page_data, *write_offset, context, payload)) {
            goto cleanup;
        }   
        
    }

    /* Update dest's cell pointer with the write offset (beg of cell content). */
    if (!set_cell_pointer((void *) dest->page_data, dest_cell_index, *write_offset)) {
        goto cleanup;
    }

    success = true;

    cleanup:
    /* Free temporary structs. */
    Row *row = (Row *) payload;
    row_free(payload);
    Value **values = (Value **) ids;
    value_free_array(values, ctx->index_key->num_columns);

    return success;
}

/* Compact page that has been splitted and has garbage values
 * spread within the page.*/
Page *btree_compact_page(Pager *pager, Page *old_page, void *context) {
    if (!pager || !old_page || !context) {
        return NULL;
    }

    /* Create a page through page.h interface so that I can give it
     * the same page number as the old page without interefering with the Pager.*/
    Page *new_page = page_create(pager, old_page->page_num);
    if (!new_page) {
        return NULL;
    }

    /* Get node type of old page. */
    uint8_t node_type = 0;
    if (!get_node_type((void *) old_page->page_data, &node_type)) {
        page_free(new_page);
        return NULL;
    }

    /* Copy node specific header onto new page. */
    uint16_t node_specific_offset = node_type ? BTREE_INTERNAL_NODE_SIZE : BTREE_LEAF_NODE_SIZE;
    memcpy(new_page->page_data, old_page->page_data, node_specific_offset);

    /* Get amount of cells from old page. */
    uint16_t cell_count = 0;
    if (!get_cell_count((void *) old_page->page_data, &cell_count)) {
        page_free(new_page);
        return NULL;
    }
    
    /* Get free space offset of new page. (Should be PAGE_SIZE)*/
    uint16_t free_space_offset = 0;
    if (!get_free_space_offset((void *) new_page->page_data, &free_space_offset)) {
        page_free(new_page);
        return NULL;
    }
    uint16_t write_offset = free_space_offset;

    /* Transfer all the cells. */
    for (uint16_t i = 0; i < cell_count; i++) {
        if (!btree_transfer_cell(
            new_page, i,
            old_page, i,
            node_type, 
            &write_offset,
             context
            )) {
            page_free(new_page);
            return NULL;
        }
    }

    /* Update cell count and free space offset on new page. */
    if (!set_cell_count((void *) new_page->page_data, cell_count)
        || !set_free_space_offset((void *) new_page->page_data, write_offset)) {
        page_free(new_page);
        return NULL;
    }

    /* Cache new page in position of the old page. */
    pager->pages[old_page->page_num] = new_page;
    
    /* Free old page and update pointer to NULL for safety. */
    page_free(old_page);
    old_page = NULL;

    return new_page;
}

/* Get full cell content size. (btree_get_key_size wrapper) */
uint32_t btree_get_cell_content_size(void *page_data, uint16_t cell_pointer, void *context) {
    if (!page_data || !context) {
        return 0;
    }
    uint32_t cell_content_size = 0;

    void *id;
    if (!get_cell_id(page_data, cell_pointer, context, &id)) {
        return 0;
    }

    Value **keys = (Value **) id;

    uint16_t key_size = btree_get_key_size(keys, context);
    if (!key_size) {
        return 0;
    }
    cell_content_size += key_size;

    uint32_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return 0;
    }

    if (node_type) {
        cell_content_size += 4;
    } else {
        cell_content_size += sizeof(Row);
    }

    return cell_content_size;
}

/* Get key size. */
uint16_t btree_get_key_size(const void *keys, void *context) {
    if (!keys || !context) {
        return 0;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    Value **key_vals = (Value **) keys;

    uint16_t key_size = 0;
    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        key_size += get_data_type_size(key_vals[i]->type);
    }

    return key_size;
}

/* Extract row's keys ACCORDING TO THE CONTEXT of the BTree. */
Value **btree_extract_row_keys(void *payload, void *context) {
    if (!payload || !context) {
        return NULL;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    Row *row = (Row *) payload;

    Value **row_keys = (Value **) malloc(ctx->index_key->num_columns*sizeof(Value *));
    if (!row_keys) {
        return NULL;
    }

    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        row_keys[i] = value_copy(row->values[ctx->index_key->column_index_array[i]]);
        if (!row_keys[i]) {
            value_free_array(row_keys, i);
            free(row_keys);
            return NULL;
        }
    }

    return row_keys;
}

/* Return available capacity to store actual data or (for internal nodes) storing
 * pointers to other pages. */
uint16_t btree_get_available_capacity(void *page_data) {
    if (!page_data) {
        return UINT16_MAX;
    }

    uint16_t reserved_space = 0;
    uint8_t node_type;
    uint16_t cell_count;
    uint16_t free_space_offset;
    if (!get_node_type(page_data, &node_type)){
        return UINT16_MAX;
    }

    if (!get_cell_count(page_data, &cell_count)) {
        return UINT16_MAX;
    }

    if (!get_free_space_offset(page_data, &free_space_offset)) {
        return UINT16_MAX;
    }

    if (node_type) {
        reserved_space += BTREE_INTERNAL_NODE_SIZE;
    } else {
        reserved_space += BTREE_LEAF_NODE_SIZE;
    }

    reserved_space += cell_count * 2;

    return free_space_offset - reserved_space;
}

/* Check if there's enough space to store the metadata [Keys + Payload]
 * and the new cell pointer in the available space. */
bool btree_has_enough_space(void *page_data, uint16_t metadata_size) {
    uint16_t available_space = btree_get_available_capacity(page_data);
    if (available_space == UINT16_MAX) {
        return false;
    }

    // +2 for the cell pointer.
    uint16_t needed_space = metadata_size + 2;

    if (available_space >= needed_space) {
        return true;
    }

    return false;
}

/* Comparing BTree Internal/Leaf Node Keys. */
bool btree_compare(Value **values, const void *key, void *context, int *result) {
    if (!values || !key || !context || !result) {
        return false;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    Value **key_val = (Value **) key;

    /* Compare both same index columns. (With upper limit the search keys used, not
    the amount of keys the BTree is organized with) */
    for (uint32_t i = 0; i < ctx->num_search_keys; i++) {
        if (!value_compare(values[i], key_val[i], result)) {
            return false;
        }

        // result == -1 for values[i] < key_val
        // result == 1 for values[i] > key_val

        /* If it isn't equal, don't bother comparing other columns. */
        if (*result != 0) {
            break;
        } 
    }

    return true;
}

bool shift_metadata(void *page_data, uint16_t cell_pointer, void *context) {
    if (!page_data) {
        return false;
    }

    uint16_t free_space_offset = PAGE_SIZE;
    if (!get_free_space_offset(page_data, &free_space_offset)) {
        return false;
    }

    if (cell_pointer <= free_space_offset) {
        return false;
    }

    uint32_t cell_content_size = btree_get_cell_content_size(page_data, cell_pointer, context);
    if (!cell_content_size) {
        return false;
    }

    uint16_t bytes_to_be_moved = cell_pointer - free_space_offset;
    uint8_t *offset = (uint8_t *) page_data + free_space_offset; 
    memmove(
        offset + cell_content_size,
        offset,
        bytes_to_be_moved
    );

    free_space_offset += cell_content_size;
    if (!set_free_space_offset(page_data, free_space_offset)) {
        return false;
    }

    return true;
}

bool shift_cell_pointers(void *page_data, uint16_t index) {

    if (!page_data) {
        return false;
    }

    uint8_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    uint16_t cell_count = 0;
    if (!get_cell_count(page_data, &cell_count)) {
        return false;
    }

    /* No shifting needed for it to be added in the end. */
    if (index >= cell_count) {
        return true;
    }

    /* Else needs shifting. */
    uint16_t bytes_to_be_shifted = (cell_count - index)*sizeof(uint16_t);

    uint16_t static_header_offset = node_type ? BTREE_INTERNAL_NODE_SIZE : BTREE_LEAF_NODE_SIZE;
    uint8_t *cell_specific_offset = (uint8_t *)page_data + static_header_offset;
    
    memmove(
        cell_specific_offset + (index+1)*sizeof(uint16_t),
        cell_specific_offset + index*sizeof(uint16_t),
        bytes_to_be_shifted
    );

    return true;
}

/* Read/write node type. */
bool get_node_type(void *page_data, uint8_t *node_type) {
    if (!page_data || !node_type) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    *node_type = common_header->node_type;
    
    return true;
}

bool set_node_type(void *page_data, uint8_t node_type) {
    if (!page_data) {
        return false;
    }

    if (node_type != 0 && node_type != 1) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->node_type = node_type;
    
    return true;
}

/* Read/write is_root. */
bool get_root_status(void *page_data, uint8_t *root_status) {
    if (!page_data || !root_status) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    *root_status = common_header->is_root;
    
    return true;
}

bool set_root_status(void *page_data, uint8_t root_status) {
    if (!page_data) {
        return false;
    }

    if (root_status != 0 && root_status != 1) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->is_root = root_status;
    
    return true;
}

/* Read/write parent pointer/page number. */
bool get_parent_pointer(void *page_data, uint32_t *parent_pointer) {
    if (!page_data || !parent_pointer) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    *parent_pointer = common_header->parent_pointer;
    
    return true;
}

bool set_parent_pointer(void *page_data, uint32_t parent_pointer) {
    if (!page_data) {
        return false;
    }

    /* Function should be simple with zero dependecies.
    So do these checks in higher-level functions before calling this.
    if (parent_pointer >= MAX_PAGES) {
        return false;
    }*/

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->parent_pointer = parent_pointer;
    
    return true;
}

/* Read/write cell count. */
bool get_cell_count(void *page_data, uint16_t *cell_count) {
    if (!page_data || !cell_count) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    *cell_count = common_header->cell_count;
    
    return true;
}

bool set_cell_count(void *page_data, uint16_t cell_count) {
    if (!page_data) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->cell_count = cell_count;
    
    return true;
}

/* Read/write free space offset. */
bool get_free_space_offset(void *page_data, uint16_t *free_space_offset) {
    if (!page_data || !free_space_offset) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    *free_space_offset = common_header->free_space_offset;
    
    return true;
}

bool set_free_space_offset(void *page_data, uint16_t free_space_offset) {
    if (!page_data) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->free_space_offset = free_space_offset;
    
    return true;
}

/* Read/write cell pointer offset. */
bool get_cell_pointer(void *page_data, uint16_t cell_index, uint16_t *cell_pointer) {
    if (!page_data || !cell_pointer) {
        return false;
    }

    uint16_t cell_count = 0;
    if (!get_cell_count(page_data, &cell_count)) {
        return false;
    }

    if (cell_index >= cell_count) {
        return false;
    }

    uint8_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    uint32_t offset = node_type ? BTREE_INTERNAL_NODE_SIZE : BTREE_LEAF_NODE_SIZE;
    uint16_t *cell_pointer_offset = (uint16_t *) ((uint8_t *) page_data + offset);
    
    if (offset + (cell_index * sizeof(uint16_t) + sizeof(uint16_t)) > PAGE_SIZE) {
        return false;
    }

    *cell_pointer = cell_pointer_offset[cell_index];

    return true;
}

bool set_cell_pointer(void *page_data, uint16_t cell_index, uint16_t cell_pointer) {
    if (!page_data) {
        return false;
    }

    uint16_t cell_count = 0;
    if (!get_cell_count(page_data, &cell_count)) {
        return false;
    }

    if (cell_index >= cell_count) {
        return false;
    }

    uint8_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    uint32_t offset = node_type ? BTREE_INTERNAL_NODE_SIZE : BTREE_LEAF_NODE_SIZE;
    uint16_t *cell_pointer_offset = (uint16_t *) ((uint8_t *) page_data + offset);
    
    if (offset + (cell_index * sizeof(uint16_t) + sizeof(uint16_t)) > PAGE_SIZE) {
        return false;
    }

    cell_pointer_offset[cell_index] = cell_pointer;

    return true;
}

/* Read/write child pointer/page number. (InternalNode only) */
bool get_cell_child_pointer(void *page_data, uint16_t cell_pointer, uint32_t *child_pointer) {
    if (!page_data || !child_pointer) {
        return false;
    }

    // Prevent access beyond PAGE_SIZE
    if (cell_pointer + sizeof(uint32_t) > PAGE_SIZE) {
        return false; 
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No leaf nodes. */
    if (!node_type) {
        return false;
    }

    uint8_t *cell_pointer_offset = (uint8_t *) page_data + cell_pointer;

    memcpy(child_pointer, cell_pointer_offset, sizeof(uint32_t));
    
    return true;
}

bool set_cell_child_pointer(void *page_data, uint16_t cell_pointer, uint32_t child_pointer) {
    if (!page_data) {
        return false;
    }

    if (cell_pointer + sizeof(uint32_t) > PAGE_SIZE) {
        return false; 
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No leaf nodes. */
    if (!node_type) {
        return false;
    }

    uint8_t *cell_pointer_offset = (uint8_t *) page_data + cell_pointer;

    memcpy(cell_pointer_offset, &child_pointer, sizeof(uint32_t));
    
    return true;
}

/* Read/write cell id/key for Internal/Leaf. */
bool get_cell_id(void *page_data, uint16_t cell_pointer, void *context, void **id) {
    if (!page_data || !context || !id) {
        return false;
    }

    if (cell_pointer + sizeof(uint32_t) > PAGE_SIZE) {
        return false; 
    }

    uint8_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    uint8_t *cell_content =  (uint8_t *) page_data + cell_pointer;
    Value **values = NULL;
    
    // Skip the child pointer only for internal nodes
    if (node_type) {
        cell_content += 4;
    } 

    values = (Value **) malloc(ctx->index_key->num_columns*sizeof(Value *));
    if (!values) {
        return false;
    }

    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        // According to the column's DataType, create a value and store it.
        values[i] = value_create(ctx->data_types[i], cell_content);
        if (!values[i]) {
            for (uint32_t j = 0; j < i; j++) {
                value_free(values[j]);
            }
            free(values);
            return false;
        }

        // Skip the exact size of the column's DataType.
        cell_content += get_data_type_size(ctx->data_types[i]);
    }
    
    *id = (void *) values;
    return true;
}

bool set_cell_id(void *page_data, uint16_t cell_pointer, void *context, void *id) {
    if (!page_data || !context || !id) {
        return false;
    }

    if (cell_pointer + sizeof(uint32_t) > PAGE_SIZE) {
        return false; 
    }

    uint8_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    uint8_t *cell_content =  (uint8_t *) page_data + cell_pointer;
    Value **values = (Value **) id;

    if (node_type) {
        cell_content += 4;
    } 

    uint32_t buf_size;
    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        buf_size = get_data_type_size(values[i]->type); 
        
        uint32_t current_offset = (uint32_t) (cell_content - (uint8_t *)page_data);
        if (current_offset + buf_size > PAGE_SIZE) {
            return false;
        }

        if (!serialize_value_data(values[i], cell_content)) {
            return false;
        }

        cell_content += buf_size;
    }
    
    return true;
}

/* Read/write payload. (LeafNode only)*/
bool get_cell_payload(void *page_data, uint16_t cell_pointer, void *context, void **payload) {
    if (!page_data || !context || !payload) {
        return false;
    }

    if (cell_pointer + sizeof(uint32_t) > PAGE_SIZE) {
        return false; 
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No internal nodes. */
    if (node_type) {
        return false;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    uint8_t *cell_content =  (uint8_t *) page_data + cell_pointer;
    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        cell_content += get_data_type_size(ctx->data_types[i]);
    }    

    Row *row = (Row *) malloc(sizeof(Row));
    if (!row) {
        return false;
    }

    uint32_t current_offset = (uint32_t) (cell_content - (uint8_t *)page_data);
    if (current_offset + sizeof(Row) > PAGE_SIZE) {
        free(row);
        return false;
    }

    memcpy(row, cell_content, sizeof(Row));
    *payload = (void *) row;

    return true;
}

bool set_cell_payload(void *page_data, uint16_t cell_pointer, void *context, void *payload) {
    if (!page_data || !context || !payload) {
        return false;
    }

    if (cell_pointer + sizeof(uint32_t) > PAGE_SIZE) {
        return false; 
    }
    
    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No internal nodes. */
    if (node_type) {
        return false;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    uint8_t *cell_content =  (uint8_t *) page_data + cell_pointer;
    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        cell_content += get_data_type_size(ctx->data_types[i]);
    }    

    Row row = *((Row *) payload);

    uint32_t current_offset = (uint32_t) (cell_content - (uint8_t *)page_data);
    if (current_offset + sizeof(Row) > PAGE_SIZE) {
        return false;
    }

    memcpy(cell_content, &row, sizeof(Row));

    return true;
}

/* Read/write rightmost child pointer. (InternalNode only) */
bool get_rightmost_child_pointer(void *page_data, uint32_t *rightmost_child_pointer) {
    if (!page_data || !rightmost_child_pointer) {
        return false;
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No leaf nodes. */
    if (!node_type) {
        return false;
    }

    uint32_t *internal_node = (uint32_t *) ((uint8_t *)page_data + sizeof(BTreeCommonHeader));

    memcpy(rightmost_child_pointer, internal_node, sizeof(uint32_t));
    
    return true;
}

bool set_rightmost_child_pointer(void *page_data, uint32_t rightmost_child_pointer) {
    if (!page_data) {
        return false;
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No leaf nodes. */
    if (!node_type) {
        return false;
    }

    uint32_t *internal_node = (uint32_t *) ((uint8_t *)page_data + sizeof(BTreeCommonHeader));

    memcpy(internal_node, &rightmost_child_pointer, sizeof(uint32_t));
    
    return true;
}

/* Read/write sibling pointers. (LeafNode only) */
bool get_leaf_sibling_pointers(void *page_data, uint32_t *previous, uint32_t *next) {
    if (!page_data || !previous || !next) {
        return false;
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No internal nodes. */
    if (node_type) {
        return false;
    }

    uint8_t *leaf_node = (uint8_t *)page_data + sizeof(BTreeCommonHeader);

    memcpy(previous, leaf_node, sizeof(uint32_t));
    memcpy(next, leaf_node + sizeof(uint32_t), sizeof(uint32_t));
    
    return true;
}

bool set_leaf_sibling_pointers(void *page_data, uint32_t previous, uint32_t next) {
    if (!page_data) {
        return false;
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }

    /* No internal nodes. */
    if (node_type) {
        return false;
    }

    uint8_t *leaf_node = (uint8_t *)page_data + sizeof(BTreeCommonHeader);

    memcpy(leaf_node, &previous, sizeof(uint32_t));
    memcpy(leaf_node + sizeof(uint32_t), &next, sizeof(uint32_t));
    
    return true;
}


/* Helper that checks the page collection for pages that have already been visited */
bool btree_collection_contains(const BTreePageCollection *visited_pages, uint32_t page_num) {
    if (!visited_pages) {
        return false;
    }

    for (uint32_t i = 0; i < visited_pages->count; i++) {
        if (visited_pages->page_numbers[i] == page_num) {
            return true;
        }
    }

    return false;
}

// Helper that recursively traverses internal nodes, and backtracking at leaf nodes
bool btree_traverse_page_recursive(uint32_t page_num, Pager *pager, BTreePageCollection *visited_pages) {
    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("btree_traverse_page_recursive: Invalid Pager.\n");
        return false;
    }

    if (!visited_pages) {
        printf("btree_traverse_page_recursive: Invalid page collection.\n");
        return false;
    }

    if (page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        page_num >= pager->num_pages ||
        page_num >= MAX_PAGES) {
        printf("btree_traverse_page_recursive: Invalid root page number.\n");
        return false;
    }

    // Checking for duplicate pages or cyclic page connections 
    if (btree_collection_contains(visited_pages, page_num)) {
        printf("btree_traverse_page_recursive: Cycle or duplicate page reference detected.\n");
        return false;
    }

    if (visited_pages->count >= MAX_PAGES) {
        printf("btree_traverse_page_recursive: Page collection is full.\n");
        return false;
    }

    // Retrieve current page
    Page *page = pager_get_page(pager, page_num);
    if (!page) {
        printf("btree_traverse_page_recursive: Invalid page.\n");
        return false;
    }

    // Find node type
    uint8_t node_type;
    if (!get_node_type(page->page_data, &node_type)) {
        printf("btree_traverse_page_recursive: Couldn't get node/page type (internal/leaf).\n");
        return false;
    }

    if (node_type != 0 && node_type != 1) {
        printf("btree_traverse_page_recursive: Invalid node type.\n");
        return false;
    }

    // Record current page as visited
    visited_pages->page_numbers[visited_pages->count] = page_num;
    visited_pages->count++;

    // Leaf node
    if (node_type == 0) {
        return true;
    }

    // Internal node
    uint16_t cell_count;
    if(!get_cell_count(page->page_data, &cell_count)) {
        printf("btree_traverse_page_recursive: Couldn't get node's number of cells.\n");
        return false;
    }
    
    // Traverse all cells, and:
    // Extract cell pointer toward the cell data (pointer/child-page-num + key)
    // Visit cell data and call the recursive function for the corresponding child page
    for (uint16_t i = 0; i < cell_count; i++) {
        uint16_t cell_pointer;

        if(!get_cell_pointer(page->page_data, i, &cell_pointer)) {
            printf("btree_traverse_page_recursive: Couldn't get cell pointer for cell %u.\n", (unsigned) i);
            return false;
        }

        uint32_t child_page_num;
        if (!get_cell_child_pointer(page->page_data, cell_pointer, &child_page_num)) {
            printf("btree_traverse_page_recursive: Couldn't get child pointer for cell %u.\n", (unsigned) i);
            return false;
        }

        if (!btree_traverse_page_recursive(child_page_num, pager, visited_pages)) {
            return false;
        }
    }

    // Visit the rightmost child page (which is a standard internal node field)
    uint32_t rightmost_child_page_num;
    if (!get_rightmost_child_pointer(page->page_data, &rightmost_child_page_num)) {
        printf("btree_traverse_page_recursive: Couldn't read rightmost child pointer.\n");
        return false;
    }

    return btree_traverse_page_recursive(rightmost_child_page_num, pager, visited_pages);
}