#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../include/btree.h"
#include "../../include/pager.h"
#include "../../include/page.h"
#include "../../include/data_types.h"
#include "../../include/row.h"
#include "../../include/index.h"
#include "../src/data_types/data_types_utils.h"
#include "btree_utils.h"
#include "../../include/serialize.h"

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

        /* Extract keys from either internal/leaf node. */
        void *ids;
        if (!get_cell_id(page_data, cell_pointer, context, &ids)) {
            result_index = UINT16_MAX;
            break;
        }
        values = (Value **) ids;

        /* Compare columns lexicographically. */
        if (!btree_compare(values, key, context, &result)) {
            result_index = UINT16_MAX;
            value_free_array(values, ctx->index_key->num_columns);
            break;
        }
       
        /* Continue the loop just in case you find the LOWER BOUND. */
        if (result >= 0) {
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

/* Find leaf node position according to your key. */
Page *btree_find_leaf_node(Pager *pager, uint32_t root_page_num, const void *key, void *context) {
    if (!pager || !key || !context) {
        return NULL;
    }

    // dont know if this is needed yet
    if (root_page_num == 0 || root_page_num == 1) {
        return NULL;
    }

    /* Hasn't been allocated yet. */
    if (root_page_num >= pager->num_pages) {
        return NULL;
    }

    Page *page = pager_get_page(pager, root_page_num);
    if (!page) {
        return NULL;
    }

    /* Page cannot be free. Should have been removed from the BTree
     * if it was freed. */
    if (is_page_free(pager, root_page_num) != PAGE_NOT_FREE) {
        pager_evict_page(pager, page->page_num);
        return NULL;
    }

    uint8_t node_type = 1;
    uint16_t result_index = UINT16_MAX;
    uint32_t child_pointer = 0;
    uint16_t cell_count = 0;

    /* Continue searching in BTree until you find a leaf node. */
    while (1) {
        if (!get_node_type(page->page_data, &node_type)) {
            pager_evict_page(pager, page->page_num);
            return NULL;
        }

        /* If its a leaf node, return page. */
        if (!node_type) {
            return page;
        }

        if (!get_cell_count(page->page_data, &cell_count)) {
            pager_evict_page(pager, page->page_num);
            return NULL;
        }

        result_index = btree_lower_bound(page->page_data, key, context);
        if (result_index == UINT16_MAX) {
            fprintf(stderr, "btree_find_leaf_node: Something went wrong.\n");
            pager_evict_page(pager, page->page_num);
            return NULL;
        }

        if (result_index == cell_count) {
            if (!get_rightmost_child_pointer(page->page_data, &child_pointer)) {
                pager_evict_page(pager, page->page_num);
                return NULL;
            }
        } else {
            uint16_t cell_pointer;

            if (!get_cell_pointer(page->page_data, result_index, &cell_pointer)) {
                pager_evict_page(pager, page->page_num);
                return NULL;
            }

            if (!get_cell_child_pointer(page->page_data, cell_pointer, &child_pointer)){
                pager_evict_page(pager, page->page_num);
                return NULL;
            }
        }

        /* Page doesn't exist in cache/disk. */
        if (child_pointer >= pager->num_pages) {
            pager_evict_page(pager, page->page_num);
            return NULL;
        }
        
        if (!pager_evict_page(pager, page->page_num)) {
            return NULL;
        }

        page = pager_get_page(pager, child_pointer);
        if (!page) {
            return NULL;
        }

        if (is_page_free(pager, page->page_num) != PAGE_NOT_FREE) {
            pager_evict_page(pager, page->page_num);
            return NULL;
        }
    }
}

/* Insert metadata into leaf node.
 * Create new Cell Pointer, store [Keys + Row] into page_data,
 * update cell_count, free_space_offset and mark page dirty. */
bool btree_leaf_node_insert(Pager *pager, Page *page, void *payload, void *context) {
    if (!page || !payload || !context) {
        return false;
    }
    bool success = false;

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;

    /* Extract Row's keys according to the IndexKey. */
    Value **row_keys = btree_extract_row_keys(payload, context);
    if (!row_keys) {
        return false;
    }
    /* Get its size. */
    uint16_t key_size = btree_get_key_size((void *) row_keys, context);

    /* Check if there's enough space to add [ Cell Pointer ] in the header,
     * and [ Keys + Row ]. */
    if (!btree_has_enough_space((void *) page->page_data, sizeof(Row)+key_size)) {
        // split if only if there's no other space in page
        goto cleanup;
    }
    
    uint16_t free_space_offset = PAGE_SIZE;
    if (!get_free_space_offset((void *) page->page_data, &free_space_offset)) {
        goto cleanup;
    }
    /* Get write position. */
    uint16_t write_offset = free_space_offset - (sizeof(Row)+key_size);

    /* Get cell count. */
    uint16_t cell_count;
    if (!get_cell_count(page->page_data, &cell_count)) {
        goto cleanup;
    }

    /* Find correct Cell Pointer position by comparing Row's keys with already
     * existing keys in the page. */
    uint16_t result_index = btree_lower_bound((void *) page->page_data, row_keys, context);
    if (result_index == UINT16_MAX) {
        fprintf(stderr, "btree_leaf_node_insert: Something went wrong.\n");
        goto cleanup;
    }
    
    Value **values = NULL;
    /* If at least one column is PRIMARY_KEY or UNIQUE (Composite Index)
     * then the whole key is unique. Therefore, if the key of the
     * row we want to add to the page has the same key as the row
     * already in its position then we have a duplicate. */
    if (ctx->is_unique == true && result_index < cell_count) {
        /* Get already existing Cell Pointer's value in the position we want to
        * add a new cell pointer..*/
        uint16_t cell_pointer;
        if (!get_cell_pointer(page->page_data, result_index, &cell_pointer)) {
            goto cleanup;
        }

        /* Follow Cell Pointer to its Cell Contents and retrieve keys. */
        void *ids;
        if (!get_cell_id(page->page_data, cell_pointer, context, &ids)) {
            goto cleanup;
        }
        values = (Value **) ids;
    
        /* Check if they are the same. */
        int result;
        if (!btree_compare(values, (void **) row_keys, context, &result)) {
            goto cleanup;
        }

        if (result == 0) {
            fprintf(stderr, "btree_leaf_node_insert: Duplicate key found.\n");
            goto cleanup;
        }
    }

    /* Shifting all cell pointers from result_index and over. */
    if (!shift_cell_pointers((void *) page->page_data, result_index)) {
        goto cleanup;
    }

    /* Set result index as cell pointer in that free space we just created by
     * shifting all cell pointers one position over. */
    if (!set_cell_pointer((void *) page->page_data, result_index, write_offset)) {
        goto cleanup;
    }

    /* Serialize and write payload onto page. */
    if (!serialize_cell_data((void *) page->page_data, write_offset, payload, row_keys, context)) {
        goto cleanup;
    }

    /* Increment cell count. */
    if (!set_cell_count((void *) page->page_data, cell_count+1)) {
        goto cleanup;
    }

    /* Update free space offset. */
    if (!set_free_space_offset((void *) page->page_data, write_offset)) {
        goto cleanup;
    }

    if (!page_mark_dirty(page) || !page_touch(pager, page)) {
        goto cleanup;
    }

    success = true;
    cleanup:
    if (values) {
        value_free_array(values, ctx->index_key->num_columns);
    }

    if (row_keys) {
        value_free_array(row_keys, ctx->index_key->num_columns);
    }

    return success;
}