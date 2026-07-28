#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../include/btree.h"
#include "../../include/page.h"
#include "../../include/data_types.h"
#include "../../include/row.h"
#include "../../include/index.h"
//#include "../../include/execution_engine.h" might need to move to index.h
#include "../src/data_types/data_types_utils.h"
#include "btree_utils.h"

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

/* Check if there's enough space to store payload + its pointer/offset
 * in the available space. */
bool btree_has_enough_space(void *page_data, uint16_t payload_size) {
    uint16_t available_space = btree_get_available_capacity(page_data);
    if (available_space == UINT16_MAX) {
        return false;
    }

    uint16_t needed_space = payload_size + 2;

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
    const Value **key_val = (Value **) key;

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

/* Extract keys from either Internal/Leaf node.*/
Value **btree_extract_data(void *page_data, uint16_t cell_pointer, void *context) {
    if (!page_data || !context) {
        return NULL;
    }

    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return NULL;
    }

    void *ids;
    if (!get_cell_id(page_data, cell_pointer, context, &ids)) {
        return NULL;
    }

    return (Value **) ids;
}

/* Lower Bound Binary Search.
 * ---> Execution Engine calling this function.
 * Key is of type (Values **) to support multiple-column keys.
 * Context is of type (KeyExtractionContext *). 
 * Execution Engine is responsible for creating this temporary struct
 * (KeyExtractionContext) to hold needed information for the execution
 * of this function. */
uint16_t btree_lower_bound(void *page_data, const void *key, void *context) {
    if (!page_data || !key || !context) {
        return UINT16_MAX;
    }

    uint16_t cell_count;
    uint8_t node_type;
    if (!get_node_type(page_data, &node_type)) {
        return UINT16_MAX;
    }
    if (!get_cell_count(page_data, &cell_count)) {
        return UINT16_MAX;
    }

    if (cell_count == 0) {
        fprintf(stderr, "btree_lower_bound: Empty node.\n");
        return 0; // result_index = 0 because there are no cells yet
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;

    /* Initialize from and to and mid.*/
    uint16_t from = 0;
    uint16_t to = cell_count - 1;
    uint16_t mid = 0;

    /* Initialize cell pointer local variable to hold cell's offset in page.*/
    uint16_t cell_pointer = 0;

    /* Result local var to hold btree_compare results. */
    int result;

    /* result_index to hold found key index position of page. */
    // for the edge case of the target key being bigger than any other key
    uint16_t result_index = cell_count; 
    Value **values = NULL;

    while (from <= to) {
        mid = (from + to)/2;

        if (!get_cell_pointer(page_data, mid, &cell_pointer)) {
            result_index = UINT16_MAX;
            break;
        }

        /* Extract data from either internal/leaf node. */
        values = btree_extract_data(page_data, cell_pointer, context);
        if (!values) {
            result_index = UINT16_MAX;
            break;
        }

        /* Compare columns lexicographically. */
        if (!btree_compare(values, key, context, &result)) {
            result_index = UINT16_MAX;
            value_free_array(values, ctx->index_key->num_columns);
            break;
        }
       
        /* Continue the loop just in case you find the LOWER BOUND. */
        if (result >= 0) {
            /* Check if keys are supposed to be UNIQUE.
             * If they are UNIQUE and result came back 0, which means
             * key equal to the target key was found.
             * No duplicates are allowed then. */
            if (ctx->is_unique == true && result == 0) {
                fprintf(stderr, "btree_lower_bound: Duplicate key found.\n");
                result_index = UINT16_MAX; // to signify error
                value_free_array(values, ctx->index_key->num_columns);
                break;
            }

            result_index = mid;
            if (mid == 0) {
                value_free_array(values, ctx->index_key->num_columns);
                break;
            }
            to = mid - 1;
        } else {
            from = mid + 1;
        } 

        /* Free temporary values array in each loop. */
        value_free_array(values, ctx->index_key->num_columns);
    }
    
    return result_index;
}
