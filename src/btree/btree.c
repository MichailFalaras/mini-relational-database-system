#include <stdio.h>
#include <stdlib.h>
#include "../../include/btree.h"
#include "../../include/page.h"

/* After btree_init the pages should be marked dirty via Pager. */

/* Initialize empty leaf node. */
bool btree_init_empty_leaf(void *page_data) {
    if (!page_data) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->node_type = 0;
    common_header->is_root = 1;
    common_header->parent_pointer = UINT32_MAX;
    common_header->cell_count = 0;
    common_header->free_space_offset = PAGE_SIZE;

    BTreeLeafNode *leaf_specific = (BTreeLeafNode *) ((uint8_t *)page_data + sizeof(BTreeCommonHeader));
    leaf_specific->previous_leaf_pointer = UINT32_MAX;
    leaf_specific->next_leaf_pointer = UINT32_MAX;

    return true;
}

/* Initialize internal node. */
bool btree_init_internal(void *page_data, uint32_t rightmost_child_pointer) {
    if (!page_data || rightmost_child_pointer == 0
        || rightmost_child_pointer == 1) {
        return false;
    }

    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    common_header->node_type = 1;
    common_header->is_root = 1;
    common_header->parent_pointer = UINT32_MAX;
    common_header->cell_count = 0;
    common_header->free_space_offset = PAGE_SIZE;

    BTreeInternalNode *internal_specific = (BTreeInternalNode *) ((uint8_t *)page_data + sizeof(BTreeCommonHeader));
    internal_specific->rightmost_child_pointer = rightmost_child_pointer;

    return true;
}

/* Return available capacity to store actual data or (for internal nodes) storing
 * pointers to other pages. */
uint16_t btree_get_available_capacity(void *page_data) {
    if (!page_data) {
        return 0;
    }

    uint16_t reserved_space = 0;
    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;

    if (common_header->node_type == 1) {
        reserved_space += BTREE_INTERNAL_NODE_SIZE;
    } else {
        reserved_space += BTREE_LEAF_NODE_SIZE;
    }

    reserved_space += common_header->cell_count * 2;

    return common_header->free_space_offset - reserved_space;
}

/* Check if there's enough space to store payload + its pointer/offset
 * in the available space. */
bool btree_has_enough_space(void *page_data, uint16_t payload_size) {
    uint16_t available_space = btree_get_available_capacity(page_data);
    uint16_t needed_space = payload_size + 2;

    if (available_space >= needed_space) {
        return true;
    }

    return false;
}