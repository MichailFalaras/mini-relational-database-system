#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../include/btree.h"
#include "../../include/page.h"
#include "../../include/data_types.h"
#include "../../include/row.h"
#include "../../include/index.h"
#include "../../include/execution_engine.h"
#include "../src/data_types/data_types_utils.h"

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

/* Comparing BTree Internal/Leaf Node Keys. */
bool btree_compare(Value **values, const void *key, void *context, int *result) {
    if (!values || !key || !context || !result) {
        return false;
    }

    KeyExtractionContext *key_extraction_ctx = (KeyExtractionContext *) context;
    const Value **key_val = (Value **) key;

    /* Compare both same index columns. (With upper limit the search keys used, not
    the amount of keys the BTree is organized with) */
    for (uint32_t i = 0; i < key_extraction_ctx->num_search_keys; i++) {
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

/* Extract data from either Internal/Leaf node.*/
Value **btree_extract_data(uint16_t node_type, const void *data_offset, void *context) {
    if (!data_offset || !context) {
        return NULL;
    }

    KeyExtractionContext *key_extraction_ctx = (KeyExtractionContext *) context;
    Value **values = NULL;
    uint8_t *data = (uint8_t *) data_offset;

    /* For internal nodes: */
    if (node_type == 1) {
         values = (Value **) malloc(key_extraction_ctx->index_key->num_columns*sizeof(Value *));
        if (!values) {
            return NULL;
        }

        // Skip the child pointer
        data += (uint8_t) 4;
        for (uint32_t i = 0; i < key_extraction_ctx->index_key->num_columns; i++) {
            // According to the column's DataType, create a value and store it.
            values[i] = value_create(key_extraction_ctx->data_types[i], data);
            if (!values[i]) {
                for (uint32_t j = 0; j < i; j++) {
                    value_free(values[j]);
                }
                free(values);
                return NULL;
            }

            // Skip the exact size of the column's DataType.
            data += (uint8_t) get_data_type_size(key_extraction_ctx->data_types[i]);
        }

    /* For leaf nodes: */
    } else {
        Row *row = (Row *) data;

        /* Extract row's exact values. */
        values = row_get_values(row, key_extraction_ctx->index_key->column_index_array,
                                        key_extraction_ctx->index_key->num_columns);
        if (!values) {
            return NULL;
        }
    }

    return values;
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

    KeyExtractionContext *key_extraction_ctx = (KeyExtractionContext *) context;

    /* Initialize from and to and mid.*/
    BTreeCommonHeader *common_header = (BTreeCommonHeader *) page_data;
    uint16_t from = 0;
    if (common_header->cell_count == 0) {
        return UINT16_MAX;
    }
    uint16_t to = common_header->cell_count - 1;
    uint16_t mid = 0;

    /* Initialize cell pointer local variable to hold cell's offset in page.*/
    uint16_t cell_pointer = 0;

    /* Calculate whole header offset for internal/leaf nodes. */
    uint16_t header_offset = sizeof(BTreeCommonHeader) +
             (common_header->node_type ? BTREE_INTERNAL_NODE_SIZE : BTREE_LEAF_NODE_SIZE);

    /* Initialize pointer showing to that offset in page data. */
    uint16_t *offset = (uint16_t *) ((uint8_t *)page_data + header_offset);
    
    /* Result local var to hold btree_compare results. */
    int result;

    /* result_index to hold found key index position of page. */
    uint16_t result_index = UINT16_MAX;
    uint8_t *data_offset; // Active moving pointer throughout the loop.
    Value **values = NULL;

    while (from <= to) {
        mid = (from + to)/2;

        memcpy(&cell_pointer, offset + mid, sizeof(uint16_t));
        data_offset = (uint8_t *)page_data + cell_pointer;
        
        /* Extract data from either internal/leaf node. */
        values = btree_extract_data(common_header->node_type, data_offset,
                                            context);
        if (!values) {
            result_index = UINT16_MAX;
            break;
        }

        /* Compare columns lexicographically. */
        if (!btree_compare(values, key, context, &result)) {
            result_index = UINT16_MAX;
            break;
        }

        // Looking to keep any columns that are >= key.
    
        /* Meaning all columns were equal. Then update result index.
         * But continue the loop just in case you find the LOWER BOUND. */
        if (result >= 0) {
            result_index = mid;

            if (mid == 0) {
                break;
            }
            to = mid - 1;
        } else {
            from = mid + 1;
        } 

        /* Free temporary values array in each loop. */
        if (values) {
            for (uint32_t i = 0; i < key_extraction_ctx->index_key->num_columns; i++) {
                value_free(values[i]);
            }
            free(values);
            values = NULL;
        }
    }
    
    /* Same if loop ended from error. */
    if (values) {
        for (uint32_t i = 0; i < key_extraction_ctx->index_key->num_columns; i++) {
            value_free(values[i]);
        }
        free(values);
        values = NULL;
    }

    return result_index;
}
