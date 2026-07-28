#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../include/index.h"
#include "../../include/row.h"
#include "../../include/btree.h"
#include "../data_types/data_types_utils.h"
#include "btree_utils.h"
#include "../../include/page.h"

/* Temporary function prototype soon to be implemented. */
void *value_serialize(Value *val, uint32_t *buf_size);

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
        void *buf = value_serialize(values[i], &buf_size);

        uint32_t current_offset = (uint32_t) (cell_content - (uint8_t *)page_data);
        if (current_offset + buf_size > PAGE_SIZE) {
            free(buf);
            return false;
        }
        memcpy(cell_content, buf, buf_size);

        cell_content += buf_size;
        free(buf);
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