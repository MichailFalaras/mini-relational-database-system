#include <stdio.h>
#include <stdlib.h>
#include "btree_utils.h"
#include "../../include/pager.h"
#include "../../include/page.h"
#include "../../include/data_types.h"
#include "../data_types/data_types_utils.h"
#include "../src/pager/pager_utils.h"
#include "../../include/serialize.h"
#include "../../include/index.h"
#include "../../include/row.h"
#include "../../include/schema.h"

/* Pass address of BTreePage in Stack and then logically connect it with a page. */
void btree_page_attach(BTreePage *btree_page, Page *page) {
    if (!btree_page || !page) {
        return;
    }

    btree_page->page = page;
    btree_page->data = page->page_data;
}

/* Validate BTreePage's metadata.
 * Also checks if page is free in the beginning just in case its
 * about to read garbage. */
BTreeStatus btree_page_validate(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page 
        || !btree_page->data || !pager) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Extra check if page is free. */
    if (is_page_free(pager, btree_page->page->page_num) != PAGE_NOT_FREE) {    
        return BTREE_FREE_PAGE;
    }

    /* Node Type checks. */
    if (btree_page->type != BTREE_LEAF_NODE
        && btree_page->type != BTREE_INTERNAL_NODE) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Root status checks. */
    if (btree_page->is_root != 0
        && btree_page->is_root != 1) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Parent pointer checks. */
    if (btree_page->is_root == 1
        && btree_page->parent_pointer != UINT32_MAX) {
        return BTREE_CORRUPT_PAGE;
    } 

    if (btree_page->is_root == 0
        && btree_page->parent_pointer == UINT32_MAX) {
        return BTREE_CORRUPT_PAGE;
    }

    if (btree_page->parent_pointer != UINT32_MAX
        && btree_page->parent_pointer >= pager->num_pages) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Cell Count checks. */
    uint16_t header_size = get_header_size(btree_page->type);
    uint16_t max_cells = (PAGE_SIZE - header_size) / MIN_INTERNAL_CELL_SIZE;
    if (btree_page->cell_count > max_cells) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Free Space Offset checks */
    if (btree_page->free_space_offset > PAGE_SIZE) {
        return BTREE_CORRUPT_PAGE;
    }

    if  (btree_page->free_space_offset < header_size) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Rightmost child pointer checks. */
    if (btree_page->type == BTREE_INTERNAL_NODE
        && btree_page->type_specific_data.rightmost_child_pointer != UINT32_MAX
        && btree_page->type_specific_data.rightmost_child_pointer >= pager->num_pages) {
        return BTREE_CORRUPT_PAGE;
    }

    if (btree_page->type == BTREE_INTERNAL_NODE
        && (btree_page->type_specific_data.rightmost_child_pointer == 0
        || btree_page->type_specific_data.rightmost_child_pointer == 1)) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Previous and next pointer checks. */
    if (btree_page->type == BTREE_LEAF_NODE
        && ((btree_page->type_specific_data.siblings.previous_leaf_pointer != UINT32_MAX
            && btree_page->type_specific_data.siblings.previous_leaf_pointer >= pager->num_pages)
        || (btree_page->type_specific_data.siblings.next_leaf_pointer != UINT32_MAX
            && btree_page->type_specific_data.siblings.next_leaf_pointer >= pager->num_pages)
            )) {
        return BTREE_CORRUPT_PAGE;
    }

    if (btree_page->type == BTREE_LEAF_NODE
        && (btree_page->type_specific_data.siblings.previous_leaf_pointer == 0
        || btree_page->type_specific_data.siblings.previous_leaf_pointer == 1
        || btree_page->type_specific_data.siblings.next_leaf_pointer == 0
        || btree_page->type_specific_data.siblings.next_leaf_pointer == 1)) {
        return BTREE_CORRUPT_PAGE;
    }

    /* Cell Contents checks */
    BTreeCellView cell = {0};
    BTreeStatus status = 0;
    for (uint16_t i = 0; i < btree_page->cell_count; i++) {
        status = get_cell(btree_page, i, &cell, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }

        if (cell.offset == 0 || cell.payload_size == 0) {
            return BTREE_CORRUPT_PAGE;
        }

        if (cell.offset < btree_page->free_space_offset 
            || cell.offset >= PAGE_SIZE
            || cell.offset > PAGE_SIZE - cell.payload_size - cell.key.key_size) {
            return BTREE_CORRUPT_PAGE;
        }

        if (btree_page->type == BTREE_INTERNAL_NODE) {
            uint32_t child_pointer = get_cell_child_pointer(btree_page->data, cell.offset);

            if (child_pointer >= pager->num_pages 
            || child_pointer == 0 || child_pointer == 1) {
                return BTREE_CORRUPT_PAGE;
            }
        }
    }
    
    // No actual payload checks here, just metadata. 

    return BTREE_SUCCESS;
}

/* BTree load with valid data if only if page had already
 * initialized page data*/
void btree_page_load(BTreePage *btree_page) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return;
    }

    /* Extract metadata from page->page_data. */
    btree_page->type = get_node_type(btree_page->data);
    btree_page->is_root = get_root_status(btree_page->data);
    btree_page->parent_pointer = get_parent_pointer(btree_page->data);
    btree_page->cell_count = get_cell_count(btree_page->data);
    btree_page->free_space_offset = get_free_space_offset(btree_page->data);

    if (btree_page->type == BTREE_INTERNAL_NODE) {
        btree_page->type_specific_data.rightmost_child_pointer = get_rightmost_child(btree_page->data);
    } else {
        btree_page->type_specific_data.siblings.previous_leaf_pointer = get_leaf_previous(btree_page->data);
        btree_page->type_specific_data.siblings.next_leaf_pointer = get_leaf_next(btree_page->data);
    }
}

/* Sync BTreePage header metadata with page's page_data. */
bool btree_page_sync(Pager *pager, BTreePage *btree_page) {
    if (!btree_page || !btree_page->page 
        ||!btree_page->data || !pager) {
        return false;
    }

    /* Write back all the metadata onto page->page_data. */
    set_node_type(btree_page->data, btree_page->type);
    set_root_status(btree_page->data, btree_page->is_root);
    set_parent_pointer(btree_page->data, btree_page->parent_pointer);
    set_cell_count(btree_page->data, btree_page->cell_count);
    set_free_space_offset(btree_page->data, btree_page->free_space_offset);

    if (btree_page->type == BTREE_INTERNAL_NODE) {
        set_rightmost_child(btree_page->data, btree_page->type_specific_data.rightmost_child_pointer);
    } else {
        set_leaf_previous(btree_page->data, btree_page->type_specific_data.siblings.previous_leaf_pointer);
        set_leaf_next(btree_page->data, btree_page->type_specific_data.siblings.next_leaf_pointer);
    }

    if (!page_mark_dirty(btree_page->page) || !page_touch(pager, btree_page->page)) {
        return false;
    }

    return true;
}

/* Wrapper function that initializes and validates BTreePage fully. */
BTreeStatus btree_page_attach_load_validate(Pager *pager, BTreePage *btree_page, Page *page, BTreeIndexSpec *index) {
    if (!pager || !btree_page || !page) {
        return BTREE_INVALID_ARGUMENTS;
    }

    btree_page_attach(btree_page, page);
    btree_page_load(btree_page);
    BTreeStatus status = btree_page_validate(pager, btree_page, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    return BTREE_SUCCESS;
}

/* Get exact offset and size of key and store it in BTreeKeyView. */
BTreeStatus get_key(BTreePage *btree_page, uint16_t cell_index, BTreeKeyView *key, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data
        || !key || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (cell_index >= btree_page->cell_count) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeCellView cell = {0};
    BTreeStatus status = get_cell(btree_page, cell_index, &cell, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    key->key = cell.key.key;
    key->key_size = cell.key.key_size;
    key->offset = cell.key.offset;
    
    return BTREE_SUCCESS;
}

/* Comparing BTree Internal/Leaf Node Keys. */
BTreeStatus btree_compare(Value **left, Value **right, uint32_t num_vals, int *result) {
    if (!left || !right || !result || num_vals == 0) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Compare both same index columns. (With upper limit the search keys used, not
    the amount of keys the BTree is organized with) */
    for (uint32_t i = 0; i < num_vals; i++) {
        if (!left[i] || !right[i]) {
            return BTREE_INVALID_ARGUMENTS;
        }

        if (left[i]->null_val && right[i]->null_val) {
            *result = 0;
        } else if (left[i]->null_val) {
            *result = 1;
        } else if (right[i]->null_val) {
            *result = -1;
        } else if (!value_compare(left[i], right[i], result)) {
            return BTREE_ERROR;
        }

        // result == -1 for left < right
        // result == 1 for left > right
        /* If it isn't equal, don't bother comparing other columns. */
        if (*result != 0) {
            break;
        } 
    }

    return BTREE_SUCCESS;
}

/* Check if there's enough space to store a Cell and its Cell Pointer. */
BTreeStatus btree_page_has_enough_space(BTreePage *btree_page, uint32_t cell_size) {
    if (!btree_page || !btree_page->page
         || !btree_page->data || cell_size == 0) {
        return BTREE_INVALID_ARGUMENTS;
    }
    uint16_t reserved_space = 0;
    uint16_t header_size = get_header_size(btree_page->type);

    reserved_space += header_size;
    reserved_space += btree_page->cell_count * sizeof(uint32_t);
    
    uint16_t available_space = btree_page->free_space_offset - reserved_space;

    // +2 for the cell pointer.
    uint16_t needed_space = cell_size + sizeof(uint32_t);

    if (available_space < needed_space) {
        return BTREE_NEEDS_SPLIT;
    }

    return BTREE_SUCCESS;
}

/* Check if less than 25% of usable space is being taken up.
 *
 * Roots are allowed to contain less than underflow. */
BTreeStatus btree_check_underflow(BTreePage *btree_page, uint32_t used_space) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    // Return success regardless because root are exceptions to this rule
    if (btree_page->is_root == true) {
        return BTREE_SUCCESS;
    }

    uint16_t header_size = get_header_size(btree_page->type);
    uint16_t usable_bytes = PAGE_SIZE - header_size;
    uint16_t used_bytes = 0;

    /* If used_space is given UINT32_MAX, it means that it will
     * check for actual page's used bytes.
     *
     * If used_space has a normal value, then just use it.
     * Used to find out if a page IS GOING TO BE UNDERFLOWING
     * with a removal of cell. */
    if (used_space == UINT32_MAX) {
        used_bytes = (btree_page->cell_count * sizeof(uint32_t)) +
                            (PAGE_SIZE - btree_page->free_space_offset);
    } else {
        used_bytes = used_space;
    }
    
    /* If used bytes are less than 25% of usable page space
     * it means the page is underflowing. */
    if (used_bytes < (usable_bytes / 4)) {
        return BTREE_NODE_UNDERFLOW;
    }

    return BTREE_SUCCESS;
}

/* Inserting into an internal node and the binary search's returned
 * index is equal to the page's cell count then:
 * swap rightmost child pointer with the child pointer contained in
 * the payload*/
BTreeStatus swap_rightmost_pointer_with_inserting_cell(BTreePage *btree_page, BTreeCellContents *cell_contents) {
    if (!btree_page || !btree_page->type 
        || !btree_page->data || !cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }
    
    uint32_t old_rightmost_child_pointer = btree_page->type_specific_data.rightmost_child_pointer;
    btree_page->type_specific_data.rightmost_child_pointer = cell_contents->BTreePayload.child_pointer;
    cell_contents->BTreePayload.child_pointer = old_rightmost_child_pointer;

    return BTREE_SUCCESS;
}

/* Swap rightmost child pointers with already existing cell's
 * child pointer and serialize that change back onto the page. */
BTreeStatus swap_rightmost_pointer_with_existing_cell(BTreePage *btree_page, uint32_t cell_pointer_index, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data 
        || cell_pointer_index >= btree_page->cell_count || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }
    
    BTreeCellContents cell_contents = {0};
    BTreeStatus status = get_cell_contents(btree_page, cell_pointer_index, &cell_contents, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    uint32_t old_rightmost_child_pointer = btree_page->type_specific_data.rightmost_child_pointer;
    btree_page->type_specific_data.rightmost_child_pointer = cell_contents.BTreePayload.child_pointer;
    cell_contents.BTreePayload.child_pointer = old_rightmost_child_pointer;

    uint32_t cell_pointer = get_cell_pointer(btree_page->data, cell_pointer_index);
    uint8_t *write_offset = btree_page->data + get_cell_offset(cell_pointer);
    if (!serialize_cell_contents(write_offset, btree_page, &cell_contents, index)) {
        value_free_array(cell_contents.keys, cell_contents.num_keys);
        return BTREE_ERROR;
    }

    value_free_array(cell_contents.keys, cell_contents.num_keys);
    return BTREE_SUCCESS;
}

/* (Type-agnostic) Remove garbage cell pointers and contents by creating a new page and transfering
 * ONLY the valid metadata there. */
BTreeStatus btree_compact_page(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index) {
    if (!pager || !btree_page || !btree_page->page 
        || !btree_page->data || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Create a page through page.h interface so that I can give it
     * the same page number as the old page without interefering with the Pager.*/
    Page *new_page = page_create(pager, btree_page->page->page_num);
    if (!new_page) {
        return BTREE_ERROR;
    }

    /* Create BTreePage and initialize with the same header metadata. */
    BTreePage replacement = {0};
    btree_page_attach(&replacement, new_page);
    if (btree_page->type == BTREE_LEAF_NODE) {
        btree_page_init_empty_leaf(&replacement);
        replacement.type_specific_data.siblings.previous_leaf_pointer = btree_page->type_specific_data.siblings.previous_leaf_pointer;
        replacement.type_specific_data.siblings.next_leaf_pointer = btree_page->type_specific_data.siblings.next_leaf_pointer;
    } else {
        btree_page_init_internal(&replacement, btree_page->type_specific_data.rightmost_child_pointer);
    }

    if (!btree_page_sync(pager, &replacement)) {
        return BTREE_ERROR;
    }

    /* Transfer all cell pointers and contents onto new page. */
    BTreeStatus status = BTREE_SUCCESS;
    for (uint16_t i = 0; i < btree_page->cell_count; i++) {
        status = btree_transfer_cells(btree_page, i, &replacement, i, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }
    }

    /* Cache new page in position of the old page. */
    pager->pages[btree_page->page->page_num] = new_page;
    /* Free old page and update pointer to NULL for safety. */
    page_free(btree_page->page);
    btree_page->page = NULL;

    replacement.is_root = btree_page->is_root;
    replacement.parent_pointer = btree_page->parent_pointer;
    replacement.cell_count = btree_page->cell_count;
    if (!btree_page_sync(pager, &replacement)) {
        return BTREE_ERROR;
    }

    btree_page_attach(btree_page, new_page);
    btree_page_load(btree_page);
    return BTREE_SUCCESS;
}

/* Connect sibling leaf nodes right after splitting. */
BTreeStatus connect_sibling_leaf_nodes(Pager *pager, BTreePage *btree_page1, BTreePage *btree_page2) {
    if (!pager || !btree_page1 || !btree_page2) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* If page1 was already connected with previous and next, update them too. */
    if (btree_page1->type_specific_data.siblings.previous_leaf_pointer != UINT32_MAX) {
        Page *left_page = pager_get_page(pager, btree_page1->type_specific_data.siblings.previous_leaf_pointer);
        if (!left_page) {
            return BTREE_ERROR;
        }

        set_leaf_next(left_page->page_data, btree_page1->page->page_num);
    }

    if (btree_page1->type_specific_data.siblings.next_leaf_pointer != UINT32_MAX) {
        Page *right_page = pager_get_page(pager, btree_page1->type_specific_data.siblings.next_leaf_pointer);
        if (!right_page) {
            return BTREE_ERROR;
        }

        set_leaf_previous(right_page->page_data, btree_page2->page->page_num);
    }

    /* Page1 and Page2 connection. */
    btree_page2->type_specific_data.siblings.next_leaf_pointer = btree_page1->type_specific_data.siblings.next_leaf_pointer;
    btree_page1->type_specific_data.siblings.next_leaf_pointer = btree_page2->page->page_num;
    btree_page2->type_specific_data.siblings.previous_leaf_pointer = btree_page1->page->page_num;

    return BTREE_SUCCESS;
}

/* Find the leftmost leaf page */
BTreeStatus btree_find_leftmost_page(BTree *btree, BTreeIndexSpec *index, Page **res_page) {
    if (!btree || !btree->pager || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (btree->root_page_num == INVALID_ROOT_PAGE ||
        btree->root_page_num >= btree->pager->num_pages) {
        return BTREE_INVALID_ARGUMENTS;
    }

    // Get root page
    Page *page = pager_get_page(btree->pager, btree->root_page_num);

    if (!page) {
        return BTREE_ERROR;
    }

    while (true) {
        BTreePage btree_page = {0};

        // In iteration, attach the current page to a BTreePage stucture
        BTreeStatus status = btree_page_attach_load_validate(btree->pager, &btree_page, page, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }

        // Check if the leftmost leaf has been found
        if (btree_page.type == BTREE_LEAF_NODE) {
            *res_page = page;
            return BTREE_SUCCESS;
        }

        // Empty-cell invalid page
        if (btree_page.cell_count == 0) {
            return BTREE_CORRUPT_PAGE;
        }

        // Find the leftmost child of the current internal node/page
        uint32_t cell_pointer = get_cell_pointer(btree_page.data, 0);
        uint16_t cell_offset = get_cell_offset(cell_pointer);
        uint32_t child_page_num = get_cell_child_pointer(btree_page.data, cell_offset);

        // Validate page number
        if (child_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
            child_page_num >= btree->pager->num_pages) {
            return BTREE_CORRUPT_PAGE;
        }

        // Load leftmost child page
        page = pager_get_page(btree->pager, child_page_num);

        if (!page) {
            return BTREE_ERROR;
        }

    }

    
}

/* BTreeCellView RAM Component Interface for easy cell access with offset and its length.
 * DOESNT update the Page Data, just for cell viewing and accesibility. */
BTreeStatus get_cell(BTreePage *btree_page, uint16_t cell_index, BTreeCellView *cell, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data
        || !cell || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (cell_index >= btree_page->cell_count) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint32_t cell_pointer = get_cell_pointer(btree_page->data, cell_index);

    uint16_t offset = get_cell_offset(cell_pointer);
    uint16_t size = get_cell_size(cell_pointer);
    void *data = (void *) (btree_page->data + offset);


    if (btree_page->type == BTREE_INTERNAL_NODE) {
        cell->key.key = (uint8_t *) data + sizeof(uint32_t);
        cell->key.key_size = index->key_size;
        cell->key.offset = offset + sizeof(uint32_t);

        cell->payload = (uint8_t *) data;
        cell->payload_size = sizeof(uint32_t);
    } else {
        cell->key.key = (uint8_t *) data;
        cell->key.key_size = index->key_size;
        cell->key.offset = offset;

        cell->payload = (uint8_t *) data + cell->key.key_size;
        cell->payload_size = size - cell->key.key_size;
    }
    cell->offset = offset;


#if 0
    /* Create BTreeKeyView struct too. */
    cell->key.key = data;
    cell->key.offset = offset;
    if (btree_page->type == BTREE_INTERNAL_NODE) {
        cell->key.key += 4;
        cell->key.offset += 4;

        cell->payload = 
    }

    cell->key.key_size = index->key_size;
    cell->payload = cell->key.key + cell->key.key_size;
    cell->offset = offset;
    cell->payload_size = size - cell->key.key_size;
#endif
    return BTREE_SUCCESS;
}

/* Insert new cell's Cell Pointer and its contents onto a BTreePage AFTER BINARY SEARCH. */
BTreeStatus insert_cell(Pager *pager, BTreePage *btree_page, BTreeCellContents *cell_contents, uint32_t cell_index,
    BTreeIndexSpec *index) {
    if (!pager || !btree_page || !btree_page->page || !btree_page->data
        || !index || cell_index > btree_page->cell_count || !cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Shift cell pointers downwards to create empty space. */
    BTreeStatus status = shift_cell_pointer(btree_page, cell_index, BTREE_SHIFT_INSERT);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    uint16_t offset = btree_page->free_space_offset - cell_contents->cell_size;
    uint8_t *write_offset = btree_page->data + offset;
    uint16_t size = cell_contents->cell_size;
    /* Fill that empty space with the new cell pointer. */
    set_cell_pointer(
        btree_page->data,
        cell_index,
        make_cell_pointer(offset, size)
    );

    /* Serialize and write payload onto page. */
    if (!serialize_cell_contents(write_offset, btree_page, cell_contents, index)) {
        return BTREE_ERROR;
    }
    
    /* Update metadata. */
    btree_page->free_space_offset = offset;

    if (btree_page->type == BTREE_INTERNAL_NODE) {
        status = update_children_parent_metadata(pager, btree_page, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }
    }

    if (!page_mark_dirty(btree_page->page) 
        || !page_touch(pager, btree_page->page)) {
        return BTREE_ERROR;
    }

    return BTREE_SUCCESS;
}

/* Transfer half of src's cells to dest and update cell count. */
BTreeStatus btree_split_cells(BTreePage *src, BTreePage *dest, BTreeIndexSpec *index) {
    if (!src || !src->page || !src->data
        || !dest || !dest->page || !dest->data
        || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }
    BTreeStatus status = BTREE_SUCCESS;
    uint16_t cell_index = src->cell_count / 2;

    /* Transfer half of cells. */
    for (uint16_t i = cell_index; i < src->cell_count; i++) {
        status = btree_transfer_cells(src, i, dest, i - cell_index, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }
    }

    /* Update cell count on both pages. */
    dest->cell_count = src->cell_count - cell_index;
    src->cell_count = cell_index;

    return BTREE_SUCCESS;
}

/* Function that actually does type-agnostic transferring of cell pointers and contents. */
BTreeStatus btree_transfer_cells(BTreePage *src, uint16_t src_idx, BTreePage *dest, uint16_t dest_idx, BTreeIndexSpec *index) {
    if (!src || !src->page || !src->data
        || !dest || !dest->page || !dest->data
        || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeCellView src_cell_view = {0};
    BTreeStatus status = get_cell(src, src_idx, &src_cell_view, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }
    BTreeCellContents src_cell = {0};
    
    if (!deserialize_cell_contents(src->data + src_cell_view.offset, src, &src_cell_view, &src_cell, index)) {
        return BTREE_ERROR;
    }

    /* Update dest with new cell pointer & free_space_offset. */
    dest->free_space_offset -= src_cell.cell_size;
    set_cell_pointer(dest->data, dest_idx, make_cell_pointer(dest->free_space_offset, src_cell.cell_size));
    
    if (!serialize_cell_contents(dest->data + dest->free_space_offset, dest, &src_cell, index)) {
        return BTREE_ERROR;
    }

    value_free_array(src_cell.keys, index->index_key->num_columns);
    if (src->type == BTREE_LEAF_NODE) {
        value_free_array(src_cell.BTreePayload.row->values, index->index_key->num_columns);
    }
    return BTREE_SUCCESS;
}

/* Remove cell by SHIFTING DOWN cell pointers and contents. */
BTreeStatus btree_remove_cell(BTreePage *btree_page, uint32_t cell_pointer_index) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (cell_pointer_index >= btree_page->cell_count) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint32_t cell_pointer = get_cell_pointer(btree_page->data, cell_pointer_index);

    // Close separator key gap without needing to compact whole page
    BTreeStatus status = shift_cell_pointer(btree_page, cell_pointer_index, BTREE_SHIFT_DELETE);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    status = shift_cell(btree_page, cell_pointer, BTREE_SHIFT_DELETE);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    return BTREE_SUCCESS;
}

/* Shift cell pointer forward/backwards to insert/delete specific cell pointer. */
BTreeStatus shift_cell_pointer(BTreePage *btree_page, uint16_t index, BTreeShiftDirection shift_direction) {
    if (!btree_page) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* No shifting needed for it to be added in the end. */
    if (index >= btree_page->cell_count && shift_direction == BTREE_SHIFT_INSERT) {
        btree_page->cell_count++;
        return BTREE_SUCCESS;
    } else if (index >= btree_page->cell_count && shift_direction == BTREE_SHIFT_DELETE) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Else needs shifting. */
    uint16_t bytes_to_be_shifted = 0;

    uint16_t header_size = get_header_size(btree_page->type);
    uint8_t *cell_specific_offset = btree_page->data + header_size;
    
    uint8_t *src, *dest;
    if (shift_direction == BTREE_SHIFT_INSERT) {
        dest = cell_specific_offset + (index+1)*sizeof(uint32_t);
        src = cell_specific_offset + index*sizeof(uint32_t);
        bytes_to_be_shifted = (btree_page->cell_count - index)*sizeof(uint32_t);
        btree_page->cell_count++;
    } else {
        dest = cell_specific_offset + index*sizeof(uint32_t);
        src = cell_specific_offset + (index+1)*sizeof(uint32_t);
        bytes_to_be_shifted = (btree_page->cell_count - index-1)*sizeof(uint32_t);
        btree_page->cell_count--;
    }

    memmove(
        dest,
        src,
        bytes_to_be_shifted
    );

    return BTREE_SUCCESS;
}

/* Shift cell forward/backwards to insert/delete specific cell and its contents. */
BTreeStatus shift_cell(BTreePage *btree_page, uint32_t cell_pointer, BTreeShiftDirection shift_direction) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint16_t cell_offset = get_cell_offset(cell_pointer);
    uint16_t cell_size = get_cell_size(cell_pointer);

    uint16_t bytes_to_be_moved = cell_offset - btree_page->free_space_offset;
    uint8_t *offset = btree_page->data + btree_page->free_space_offset; 

    uint8_t *src, *dest;
    if (shift_direction == BTREE_SHIFT_DELETE) {
        dest = offset + cell_size;
        src = offset;
        btree_page->free_space_offset += cell_size;
    } else {
        dest = offset;
        src = offset + cell_size;
        btree_page->free_space_offset -= cell_size;
    }

    memmove(
        dest,
        src,
        bytes_to_be_moved
    );

    BTreeStatus status = update_cell_pointers_offset(btree_page, cell_pointer, cell_size, shift_direction);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    return BTREE_SUCCESS;
}

/* Update cell pointers offset that was affected because of shift_cell. */
BTreeStatus update_cell_pointers_offset(BTreePage *btree_page, uint32_t boundary_cell_pointer, uint16_t cell_size,
    BTreeShiftDirection shift_direction) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint16_t boundary_offset = get_cell_offset(boundary_cell_pointer);

    uint32_t cell_pointer = 0;
    uint16_t offset = 0, size = 0;
    for (uint32_t i = 0; i < btree_page->cell_count; i++) {
        /* Retrieve cell pointer. */
        cell_pointer = get_cell_pointer(btree_page->data, i);
        offset = get_cell_offset(cell_pointer);
        size = get_cell_size(cell_pointer);

        /* Check if its actually affected by the shift. */
        if (boundary_offset < offset) {
            continue;
        }
        
        /* Update its offset according to shift direction. */
        if (shift_direction == BTREE_SHIFT_DELETE) {
            offset += cell_size;
            cell_pointer = make_cell_pointer(offset, size);
            set_cell_pointer(btree_page->data, i, cell_pointer);
        } else {
            offset -= cell_size;
            cell_pointer = make_cell_pointer(offset, size);
            set_cell_pointer(btree_page->data, i, cell_pointer);
        }
    }

    return BTREE_SUCCESS;
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
// Create temporary index spec if this is called in index.c or similar files. 
BTreeStatus btree_traverse_page_recursive(BTree *btree, uint32_t page_num, BTreePageCollection *visited_pages) {
    if (!btree || !btree->pager || btree->pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("btree_traverse_page_recursive: Invalid Pager.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!visited_pages) {
        printf("btree_traverse_page_recursive: Invalid page collection.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    if (page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        page_num >= btree->pager->num_pages ||
        page_num >= MAX_PAGES) {
        printf("btree_traverse_page_recursive: Invalid root page number.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    // Checking for duplicate pages or cyclic page connections 
    if (btree_collection_contains(visited_pages, page_num)) {
        printf("btree_traverse_page_recursive: Cycle or duplicate page reference detected.\n");
        return BTREE_ERROR;
    }

    if (visited_pages->count >= MAX_PAGES) {
        printf("btree_traverse_page_recursive: Page collection is full.\n");
        return BTREE_ERROR;
    }

    // Retrieve current page
    Page *page = pager_get_page(btree->pager, page_num);
    if (!page) {
        printf("btree_traverse_page_recursive: Invalid page.\n");
        return BTREE_ERROR;
    }

    BTreePage btree_page = {0};
    btree_page_attach(&btree_page, page);
    btree_page_load(&btree_page);
    // No validation needed because pages were already validated before enterring the B+Tree

    if (btree_page.type != 0 && btree_page.type != 1) {
        printf("btree_traverse_page_recursive: Invalid node type.\n");
        return BTREE_ERROR;
    }

    // Record current page as visited
    visited_pages->page_numbers[visited_pages->count] = page_num;
    visited_pages->count++;

    // Leaf node
    if (btree_page.type == BTREE_LEAF_NODE) {
        return BTREE_SUCCESS;
    }

    // Internal node >> already got cell count
    
    // Traverse all cells, and:
    // Extract cell pointer toward the cell data (pointer/child-page-num + key)
    // Visit cell data and call the recursive function for the corresponding child page
    for (uint16_t i = 0; i < btree_page.cell_count; i++) {
        uint32_t cell_pointer = get_cell_pointer(btree_page.data, i);
        uint32_t child_page_num = get_cell_child_pointer(btree_page.data, get_cell_offset(cell_pointer));

        BTreeStatus status = btree_traverse_page_recursive(btree, child_page_num, visited_pages);
        if (status != BTREE_SUCCESS) {
            return status;
        }
    }

    // Visit the rightmost child page (which is a standard internal node field)
    uint32_t rightmost_child_page_num = get_rightmost_child(btree_page.data);
    
    return btree_traverse_page_recursive(btree, rightmost_child_page_num, visited_pages);
}

/* Update parent pointers of child pages & root status right after split. */
BTreeStatus update_children_parent_metadata(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index) {
    if (!pager || !btree_page || !btree_page->page 
        || !btree_page->data || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    Page *page = pager_get_page(pager, btree_page->type_specific_data.rightmost_child_pointer);
    if (!page) {
        return BTREE_ERROR;
    }

    BTreePage rightmost_child = {0};
    BTreeStatus status = btree_page_attach_load_validate(pager, &rightmost_child, page, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    rightmost_child.parent_pointer = btree_page->page->page_num;
    rightmost_child.is_root = false;
    if (!btree_page_sync(pager, &rightmost_child)) {
        return BTREE_ERROR;
    }

    for (uint32_t i = 0; i < btree_page->cell_count; i++) {
        uint32_t cell_pointer = get_cell_pointer(btree_page->data, i);
        uint32_t child_pointer = get_cell_child_pointer(btree_page->data, get_cell_offset(cell_pointer));

        page = pager_get_page(pager, child_pointer);
        if (!page) {
            return BTREE_ERROR;
        }

        if (get_parent_pointer(page->page_data) == btree_page->page->page_num
            && get_root_status(page->page_data) == false) {
            continue;
        }

        BTreePage child = {0};
        status = btree_page_attach_load_validate(pager, &child, page, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }

        child.parent_pointer = btree_page->page->page_num;
        child.is_root = false;
        if (!btree_page_sync(pager, &child)) {
            return BTREE_ERROR;
        }
    }

    return BTREE_SUCCESS;
}

/* Allocate memory for serialized separator key from key_view. */
void *serialized_key_alloc(BTreeKeyView *key_view) {
    if (!key_view || !key_view->key
        || key_view->key_size == 0
        || key_view->offset == 0) {
        return NULL;
    }

    void *separator_key = malloc(key_view->key_size);
    if (!separator_key) {
        return NULL;
    }

    memcpy(separator_key, key_view->key, key_view->key_size);
    return separator_key;
}

/* Result BTreeSplitResult metadata. */
void split_result_reset(BTreeSplitResult *split_result) {
    if (!split_result) {
        return;
    }

    if (split_result->separator_key) {
        free(split_result->separator_key);
    }

    split_result->left_page = UINT32_MAX;
    split_result->right_page = UINT32_MAX;
    split_result->separator_key = NULL;
    split_result->separator_size = 0;
    split_result->split = false;
}

/* Reset insertion orchestration result. */
void insertion_result_reset(BTreeInsertionResult *insertion_res) {
    if (!insertion_res) {
        return;
    }

    insertion_res->inserted = false;
    insertion_res->splitted = false;
    insertion_res->insertion_page_num = UINT32_MAX;
    insertion_res->split_levels = 0;
}

/* Reset deletion result. */
void deletion_result_reset(BTreeDeletionResult *deletion_res) {
    if (!deletion_res) {
        return;
    }

    deletion_res->deleted = false;
    deletion_res->underflow = false;
    deletion_res->page_num = UINT32_MAX;
    deletion_res->first_key_changed = false;

    if (deletion_res->first_cell.keys) {
        value_free_array(deletion_res->first_cell.keys, deletion_res->first_cell.num_keys);
        deletion_res->first_cell.keys = NULL;
    }

    if (deletion_res->first_cell.BTreePayload.row) {
        row_free(deletion_res->first_cell.BTreePayload.row);
        deletion_res->first_cell.BTreePayload.row = NULL;
    }
    
    deletion_res->first_cell.num_keys = 0;
    deletion_res->first_cell.key_size = 0;
    deletion_res->first_cell.cell_size = 0;
}

/* Reset merge result metadata. */
void merge_result_reset(BTreeMergeResult *merge_result) {
    if (!merge_result) {
        return;
    }

    merge_result->needs_merge = false;
    merge_result->underflowing_page_num = UINT32_MAX;
    merge_result->sibling_page_num = UINT32_MAX;
    merge_result->parent_page_num = UINT32_MAX;
    merge_result->parent_underflowing_cell_index = UINT32_MAX;
    merge_result->parent_sibling_cell_index = UINT32_MAX;
}

/* Serialized key to Value array conversion. */
Value **serialized_key_to_values(void *separator_key, uint32_t num_keys, BTreeIndexSpec *index) {
    if (!separator_key || !index
        || !num_keys || !index->index_key
        || !index->index_key->num_columns
        || num_keys > index->index_key->num_columns) {
        return NULL;
    }
    uint8_t *key_offset = (uint8_t *) separator_key;

    uint8_t *bitmap;
    if (!deserialize_null_bitmap(&key_offset, &bitmap, index->index_key->num_columns)) {
        return NULL;
    }

    Value **key_vals = (Value **) calloc(num_keys, sizeof(Value *));
    if (!key_vals) {
        free(bitmap);
        return NULL;
    }
        
    for (uint32_t i = 0; i < num_keys; i++) {
        uint32_t bitmap_index = i / 8; 
        uint32_t bitmap_shift = i % 8;

        bool is_null = ((bitmap[bitmap_index] & (1 << bitmap_shift)) != 0);

        key_vals[i] = deserialize_value_data(get_key_column(index, i), is_null, key_offset);
        if (!key_vals[i]) {
            free(bitmap);
            value_free_array(key_vals, num_keys);
            return NULL;
        }

        key_offset += get_serialized_key_size(index, i);
    }

    free(bitmap);
    return key_vals;
}

/* Value array to serialized key stored in heap. */
void *values_to_serialized_key(Value **key_vals, uint32_t num_keys, BTreeIndexSpec *index) {
    if (!key_vals || !index
        || !num_keys || !index->index_key
        || !index->index_key->num_columns
        || num_keys > index->index_key->num_columns) {
        return NULL;
    }

    void *serialized_key = malloc(index->key_size);
    if (!serialized_key) {
        return NULL;
    }

    uint8_t *offset = (uint8_t *) serialized_key;
    if (!serialize_null_bitmap(&offset, key_vals, num_keys, index->index_key->num_columns)) {
        free(serialized_key);
        return NULL;
    }

    for (uint32_t i = 0; i < num_keys; i++) {
        if (!serialize_value_data(key_vals[i], get_key_column(index, i), offset)) {
            free(serialized_key);
            return NULL;
        }

        offset += get_serialized_key_size(index, i);
    }

    return serialized_key;
}

/* Build internal node separator cell to promote in split propagation. */
BTreeStatus build_internal_separator_cell(BTreeCellContents *cell_contents, BTreeSplitResult *split_result, BTreeIndexSpec *index) {
    if (!cell_contents || !split_result || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    cell_contents->type = BTREE_INTERNAL_NODE;
    cell_contents->num_keys = index->index_key->num_columns;
    cell_contents->keys = serialized_key_to_values(split_result->separator_key, cell_contents->num_keys, index);
    if (!cell_contents->keys) {
        return BTREE_ERROR;
    }

    cell_contents->key_size = split_result->separator_size;
    cell_contents->cell_size = cell_contents->key_size + sizeof(uint32_t);
    cell_contents->BTreePayload.child_pointer = split_result->left_page;

    return BTREE_SUCCESS;
}

/* Handle propagated insertion before actual cell insertion onto page.
 *
 * btree_node_insert is not built to handle direct insertion with propagation.
 * Even from leaf splits, the separator key is the first key of the right child.
 * Then that separator key points to the LEFT CHILD while having its rightmost
 * child pointer as the RIGHT CHILD.
 * 
 * Therefore, it requires a swap between the splitted pages during insertion
 * for correct alignment of pages. (Swap for rightmost pointers is already handled) */
BTreeStatus prepare_propagated_separator_cell(BTreePage *insertion_page, BTreeCellContents *cell_contents, uint32_t pending_left_page,
    uint32_t pending_right_page, BTreeIndexSpec *index) {
    if (!insertion_page || !insertion_page->page || !insertion_page->data
        || !cell_contents || !index
        || pending_left_page <= SYSTEM_CATALOG_PAGE_NUM || pending_right_page <= SYSTEM_CATALOG_PAGE_NUM
        || pending_left_page >= MAX_PAGES || pending_right_page >= MAX_PAGES) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (insertion_page->type != BTREE_INTERNAL_NODE ||
        cell_contents->type != BTREE_INTERNAL_NODE) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeSearchKey search_key = {0};
    search_key.index = index;
    search_key.num_target_keys = index->index_key->num_columns;
    search_key.target_key = values_to_serialized_key(cell_contents->keys, search_key.num_target_keys, index);
    if (!search_key.target_key) {
        return BTREE_ERROR;
    }

    BTreeSearchResult search_result = {0};
    BTreeStatus status = btree_binary_search(insertion_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    if (status != BTREE_SUCCESS) {
        free(search_key.target_key);
        return status;
    }
    free(search_key.target_key);

    cell_contents->BTreePayload.child_pointer = pending_right_page;
    if (search_result.result_index != insertion_page->cell_count) {
        cell_contents->BTreePayload.child_pointer = pending_left_page;

        uint32_t cell_pointer = get_cell_pointer(insertion_page->data, search_result.result_index);
        uint32_t child_pointer = get_cell_child_pointer(insertion_page->data, get_cell_offset(cell_pointer));

        if (child_pointer != pending_left_page) {
            return BTREE_CORRUPT_PAGE;
        }

        set_cell_child_pointer(insertion_page->data, get_cell_offset(cell_pointer), pending_right_page);

    } else if (insertion_page->type_specific_data.rightmost_child_pointer != pending_left_page) {
        return BTREE_CORRUPT_PAGE;
    }

    return BTREE_SUCCESS;
}

/* Handling for when you want to insert a separator cell into a page
 * that's already full and needs to split. 
 *
 * It chooses in which of the new 2 splitted pages the separator key/cell
 * should be inserted in. */
BTreeStatus choose_split_insertion_page(Pager *pager, Page **insertion_page, BTreeCellContents *cell_contents,
    BTreeSplitResult *split_result, BTreeIndexSpec *index) {
    if (!pager || !insertion_page || !cell_contents
        || !split_result || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Create Values of new separator key. */
    Value **new_separator_key = serialized_key_to_values(split_result->separator_key, index->index_key->num_columns, index);
    if (!new_separator_key) {
        return BTREE_ERROR;
    }

    int result = 0;
    BTreeStatus status = btree_compare(cell_contents->keys, new_separator_key, index->index_key->num_columns, &result);
    if (status != BTREE_SUCCESS) {
        value_free_array(new_separator_key, index->index_key->num_columns);
        return status;
    }
    value_free_array(new_separator_key, index->index_key->num_columns);

    uint32_t chosen_page_num = (result == -1) ? split_result->left_page : split_result->right_page;
    *insertion_page = pager_get_page(pager, chosen_page_num);
    if (!(*insertion_page)) {
        return BTREE_ERROR;
    }

    return BTREE_SUCCESS;
}

/* Get cell contents by firstly getting the cell view 
 * and deserializing needed information. */
BTreeStatus get_cell_contents(BTreePage *btree_page, uint32_t cell_pointer_index, BTreeCellContents *dest, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data
        || cell_pointer_index >= btree_page->cell_count
        || !dest || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeCellView cell_view = {0};
    BTreeStatus status = get_cell(btree_page, cell_pointer_index, &cell_view, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    dest->type = btree_page->type;
    dest->num_keys = index->index_key->num_columns;
    dest->key_size = cell_view.key.key_size;
    dest->cell_size = dest->key_size + cell_view.payload_size;

    if (!deserialize_cell_contents(btree_page->data + cell_view.offset, btree_page, &cell_view, dest, index)) {
        return BTREE_ERROR;
    }

    return BTREE_SUCCESS;
}

/* Check if node can lend a cell to a close by underflowing node
 * without underflowing itself. */
BTreeStatus btree_node_can_lend(BTreePage *lender, uint32_t cell_pointer_index, BTreeIndexSpec *index) {
    if (!lender || cell_pointer_index >= lender->cell_count || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeCellContents cell_to_be_lended = {0};
    BTreeStatus status = get_cell_contents(lender, cell_pointer_index, &cell_to_be_lended, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    uint32_t used_space = (lender->cell_count-1) * sizeof(uint32_t) +
                        (PAGE_SIZE - lender->free_space_offset - cell_to_be_lended.cell_size);

    status = btree_check_underflow(lender, used_space);

    value_free_array(cell_to_be_lended.keys, cell_to_be_lended.num_keys);
    if (cell_to_be_lended.type == BTREE_LEAF_NODE) {
        row_free(cell_to_be_lended.BTreePayload.row);
    }
    return status;
}

/* Replace cell by removing what was in its position and inserting another one. */
BTreeStatus btree_replace_cell(Pager *pager, BTreePage *btree_page, uint32_t cell_index, BTreeCellContents *replacement,
    BTreeIndexSpec *index) {
    if (!pager || !btree_page || !btree_page->page || !btree_page->data
        || cell_index >= btree_page->cell_count
        || !replacement || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeCellContents to_be_deleted = {0};
    BTreeStatus status = get_cell_contents(btree_page, cell_index, &to_be_deleted, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    /* Simulate cell deletion in a temporary stack struct and check if replacement
     * would fit inside the page without overflow. */
    BTreePage temp = *btree_page;
    temp.cell_count--;
    temp.free_space_offset -= to_be_deleted.cell_size + sizeof(uint32_t);

    status = btree_page_has_enough_space(&temp, replacement->cell_size);
    if (status != BTREE_SUCCESS && status != BTREE_NEEDS_SPLIT) {
        btree_cell_contents_free(&to_be_deleted);
        return status;
    }

    if (status == BTREE_NEEDS_SPLIT) {
        btree_cell_contents_free(&to_be_deleted);
        return BTREE_ERROR;
    }
    btree_cell_contents_free(&to_be_deleted);

    status = btree_remove_cell(btree_page, cell_index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    status = insert_cell(pager, btree_page, replacement, cell_index, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    if (!btree_page_sync(pager, btree_page)) {
        return BTREE_ERROR;
    }

    return status;
}

/* Propagate first key change to parents. */
BTreeStatus propagate_first_key_to_parents(Pager *pager, BTreePage *btree_page, BTreeCellContents *first_cell, BTreeIndexSpec *index) {
    if (!pager || !btree_page || !first_cell || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreePage current = *btree_page;
    while (!current.is_root) {
        /* Get parent in memory. */
        Page *parent = pager_get_page(pager, current.parent_pointer);
        if (!parent) {
            return BTREE_ERROR;
        }

        BTreePage btree_parent = {0};
        BTreeStatus status = btree_page_attach_load_validate(pager, &btree_parent, parent, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }

        /* Find child's cell index. */
        bool found = false;
        uint32_t cell_index = 0;
        for (uint32_t i = 0; i < btree_parent.cell_count; i++) {
            uint32_t cell_pointer = get_cell_pointer(btree_parent.data, i);
            uint32_t child_pointer = get_cell_child_pointer(btree_parent.data, get_cell_offset(cell_pointer));

            if (child_pointer == current.page->page_num) {
                found = true;
                cell_index = i;
                break;
            }
        }

        if (btree_parent.type_specific_data.rightmost_child_pointer == current.page->page_num) {
            found = true;
            cell_index = btree_parent.cell_count;
        }

        if (!found) {
            return BTREE_ERROR;
        }
        
        /* If cell index isn't 0, update cell[cell_index-1]'s keys with child's. */
        if (cell_index > 0) {
            BTreeCellContents parent_first_cell = {0};
            status = get_cell_contents(&btree_parent, cell_index-1, &parent_first_cell, index);
            if (status != BTREE_SUCCESS) {
                return status;
            }
            value_free_array(parent_first_cell.keys, parent_first_cell.num_keys);

            parent_first_cell.num_keys = first_cell->num_keys;
            parent_first_cell.key_size = first_cell->key_size;
            parent_first_cell.keys = first_cell->keys;
            parent_first_cell.cell_size = parent_first_cell.key_size + sizeof(uint32_t);

            status = btree_replace_cell(pager, &btree_parent, cell_index-1, &parent_first_cell, index);
            if (status != BTREE_SUCCESS) {
                return status;
            }

            break;
        }

        /* If cell index is 0, continue propagating to the parent. */
        current = btree_parent;
    }
    
    return BTREE_SUCCESS;
}

/* Check if NULL is contained in a key. */
bool key_contains_null_val(Value **key, uint32_t num_keys) {
    if (!key || !num_keys) {
        return false;
    }

    for (uint32_t i = 0; i < num_keys; i++) {
        if (!key[i]) {
            return false;
        }

        if (key[i]->null_val) {
            return true;
        }
    }

    return false;
} 

/* ---------- BTreeSearchEntries Helpers ---------- */

/* The caller owns the BTreeSearchEntries struct */
BTreeStatus btree_search_entries_init(BTreeSearchEntries *result) {
    if (!result) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (result->entries || result->count != 0 || result->capacity != 0) {
        return BTREE_INVALID_ARGUMENTS;
    }

    result->entries = (BTreeEntry *) calloc(BTREE_RANGE_INITIAL_CAPACITY, sizeof(BTreeEntry));
    
    if(!result->entries) {
        result->count = 0;
        result->capacity = 0;
        return BTREE_ERROR;
    }
    
    result->count = 0;
    result->capacity = BTREE_RANGE_INITIAL_CAPACITY;
    return BTREE_SUCCESS;
}

BTreeStatus btree_search_entries_append(BTreeSearchEntries *result, BTreeEntry *new_entry) {
    if (!result || !new_entry) {
        return BTREE_INVALID_ARGUMENTS;
    }

    // Checking if range result struct is full
    if (result->count == result->capacity) {
        uint32_t new_capacity = 
            (result->capacity == 0) ? BTREE_RANGE_INITIAL_CAPACITY : result->capacity * 2;

        if (new_capacity < result->capacity) {
            return BTREE_ERROR;
        }

        // Increasing the size of the cell contents array
        BTreeEntry *new_entries = 
            (BTreeEntry *) realloc(result->entries, new_capacity * sizeof(BTreeEntry));

        if (!new_entries) {
            return BTREE_ERROR;
        }

        result->entries = new_entries;
        result->capacity = new_capacity;
    }

    // Appending the new cell content struct
    result->entries[result->count] = *new_entry;
    result->count++;

    return BTREE_SUCCESS;
}

void btree_cell_contents_free(BTreeCellContents *cell) {
    if (!cell) {
        return;
    }

    if (cell->keys) {
        value_free_array(cell->keys, cell->num_keys);
        cell->keys = NULL;
    }

    if (cell->type == BTREE_LEAF_NODE && cell->BTreePayload.row) {
        row_free(cell->BTreePayload.row);
        cell->BTreePayload.row = NULL;
    }
}

void btree_entry_free(BTreeEntry *entry) {
    if (!entry) {
        return;
    }

    btree_cell_contents_free(&entry->cell);

    entry->page_num = UINT32_MAX;
    entry->cell_index = UINT16_MAX;
}


void btree_search_entries_free(BTreeSearchEntries *entries) {
    if (!entries) {
        return;
    }

    // Free the allocate Key and Rows for each cell contents entry in the ragne result
    for (uint32_t i = 0; i < entries->count; i++) {
        btree_entry_free(&entries->entries[i]);
    }

    free(entries->entries);

    entries->entries = NULL;
    entries->count = 0;
    entries->capacity = 0;
}


/* ---------- BTreeIndexSpec Helpers ---------- */

// Initialize BTreeIndexSpec fields
bool btree_index_spec_init(const Index *index, Schema *schema, BTreeIndexSpec *spec) {
    // Validate inputs
    if (!index || !index->key ||
        !index->key->column_index_array ||
        index->key->num_columns == 0) {
        return false;
    }

    if (!schema || !schema->columns || !spec) {
        return false;
    }

    memset(spec, 0, sizeof(BTreeIndexSpec));

    spec->schema = schema;
    spec->index_key = index->key;
    
    spec->column_types = calloc(index->key->num_columns, sizeof(DataType));
    if (!spec->column_types) {
        return false;
    }

    uint32_t key_size = 0;
    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        uint32_t column_index = index->key->column_index_array[i];

        if (column_index >= schema->num_columns || !schema->columns[column_index]) {
            free(spec->column_types);
            spec->column_types = NULL;
            return false;
        }

        DataType type = schema->columns[column_index]->type;
        spec->column_types[i] = type;

        key_size += get_serialized_key_size(spec, i);

        if (key_size > UINT16_MAX) {
            free(spec->column_types);
            spec->column_types = NULL;
            return false;
        }
    }

    // Needs bitmap and key size for correct cell operations despite
    // bitmap existing only in disk. 
    uint32_t bitmap_size = (index->key->num_columns + 7) / 8;
    if (key_size > UINT16_MAX - bitmap_size) {
        free(spec->column_types);
        spec->column_types = NULL;
        return false;
    }

    spec->key_size = bitmap_size + key_size; 
    spec->is_unique = index->is_unique;

    return true;
}

/* Get serialized column/key size or Column pointer. */
uint32_t get_serialized_column_size(BTreeIndexSpec *spec, uint32_t col_idx) {
    return spec->schema->columns[col_idx]->serialized_size;
}

uint32_t get_serialized_key_size(BTreeIndexSpec *spec, uint32_t key_idx) {
    uint32_t column_index = spec->index_key->column_index_array[key_idx];
    return get_serialized_column_size(spec, column_index);
}

Column *get_column(BTreeIndexSpec *spec, uint32_t col_idx) {
    return spec->schema->columns[col_idx];
}

Column *get_key_column(BTreeIndexSpec *spec, uint32_t key_idx) {
    uint32_t column_index = spec->index_key->column_index_array[key_idx];
    return spec->schema->columns[column_index];
}

void index_btree_spec_free(BTreeIndexSpec *spec) {
    if (!spec) {
        return;
    }

    free(spec->column_types);
    spec->column_types = NULL;
}


/* Get cell contents from a Row */
BTreeStatus get_cell_contents_from_row(BTreeCellContents *cell_contents, BTreeIndexSpec *spec, Row *row) {
    if (!cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!spec ||
        !spec->schema ||
        !spec->schema->columns ||
        !spec->index_key ||
        !spec->index_key->column_index_array ||
        spec->index_key->num_columns == 0 ||
        spec->key_size == 0) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!row ||
        !row->values ||
        row->n_columns == 0 ||
        row->n_columns != spec->schema->num_columns) {
        return BTREE_INVALID_ARGUMENTS;
    }

    cell_contents->type = BTREE_LEAF_NODE;
    cell_contents->num_keys = spec->index_key->num_columns;
    cell_contents->key_size = spec->key_size;

    cell_contents->keys = (Value **) calloc(cell_contents->num_keys, sizeof(Value *));

    if (!cell_contents->keys) {
        return BTREE_ERROR;
    }

    // Extract indexed values from the Row
    for (uint32_t i = 0; i < cell_contents->num_keys; i++) {
        uint32_t col_pos = spec->index_key->column_index_array[i];

        if (col_pos >= row->n_columns) {
            free(cell_contents->keys);
            cell_contents->keys = NULL;
            return BTREE_INVALID_ARGUMENTS;
        }

        Value *value = row_get_value(row, col_pos);

        if (!value) {
            free(cell_contents->keys);
            cell_contents->keys = NULL;
            return BTREE_ERROR;
        }

        if (!spec->schema->columns[col_pos] ||
            value->type != spec->schema->columns[col_pos]->type) {
            free(cell_contents->keys);
            cell_contents->keys = NULL;
            return BTREE_INVALID_ARGUMENTS;
        }

        cell_contents->keys[i] = value;
    }

    // Serialized Row: [is_deleted] [n_columns] [NULL bitmap] [column 0] ...
    
    uint32_t row_size = sizeof(uint8_t) + sizeof(uint32_t) + ((row->n_columns + 7) / 8);

    for (uint32_t i = 0; i < row->n_columns; i++) {
        if (!row->values[i] ||
            !spec->schema->columns[i] ||
            row->values[i]->type != spec->schema->columns[i]->type) {
            free(cell_contents->keys);
            cell_contents->keys = NULL;
            return BTREE_INVALID_ARGUMENTS;
        }

        uint32_t serialized_size = get_serialized_column_size(spec, i);

        if (row_size > UINT16_MAX - serialized_size) {
            free(cell_contents->keys);
            cell_contents->keys = NULL;
            return BTREE_ERROR;
        }

        row_size += serialized_size;
    }

    if (cell_contents->key_size > UINT16_MAX - row_size) {
        free(cell_contents->keys);
        cell_contents->keys = NULL;
        return BTREE_ERROR;
    }

    cell_contents->cell_size = (uint16_t)(cell_contents->key_size + row_size);

    // Borrow the full Row as the leaf payload
    cell_contents->BTreePayload.row = row;

    return BTREE_SUCCESS;
}

// Locate the position (page number and cell index) of a Row
BTreeStatus btree_locate_target_row(BTree *btree, BTreeSearchKey *search_key, const Row *target_row,
    BTreeIndexSpec *index, uint32_t *page_num, uint16_t *cell_index) {
    
    // Validate inputs
    if (!btree ||
        !btree->pager ||
        btree->pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!index || 
        !index->index_key ||
        !index->index_key->column_index_array ||
        index->index_key->num_columns == 0) {
        return BTREE_INVALID_ARGUMENTS;        
    }

    if (!search_key ||
        !search_key->target_key ||
        !search_key->index ||
        search_key->index != index ||
        search_key->num_target_keys != index->index_key->num_columns) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!target_row || !page_num || !cell_index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    *page_num = UINT32_MAX;
    *cell_index = UINT16_MAX;

    BTreeSearchEntries matches = {0};    
    BTreeStatus status;

    // Distinguishing between a Unique and a Non-Unique index
    // 1. Find exact match in unique index
    // 2. Find all matches and then compare them column-by-column with the target row

    // UNIQUE index
    if (index->is_unique) {
        BTreeSearchResult search_result = {0};

        status = btree_find_exact_key(btree, search_key, &search_result, &matches);
        
        if (status != BTREE_SUCCESS) {
            btree_search_entries_free(&matches);
            return status;
        }

        if (matches.count != 1) {
            BTreeStatus return_status = matches.count == 0 ? BTREE_NOT_FOUND : BTREE_CORRUPT_PAGE;

            btree_search_entries_free(&matches);
            return return_status;
        }

        *page_num = matches.entries[0].page_num;
        *cell_index = matches.entries[0].cell_index;

        btree_search_entries_free(&matches);
        return BTREE_SUCCESS;
    }

    // NON-UNIQUE index
    status = btree_find_range_keys(btree, index, search_key, true, search_key, true, &matches);

    if (status != BTREE_SUCCESS) {
        btree_search_entries_free(&matches);
        return status;
    }

    for (uint32_t i = 0; i < matches.count; i++) {
        Row *row = matches.entries[i].cell.BTreePayload.row;

        if (!row) {
            btree_search_entries_free(&matches);
            return BTREE_CORRUPT_PAGE;
        }

        
        // If the current row equals the target row on all Columns, we retrieve its position
        if (row_equals(row, target_row)) {
            *page_num = matches.entries[i].page_num;
            *cell_index = matches.entries[i].cell_index;

            btree_search_entries_free(&matches);
            return BTREE_SUCCESS;
        }
    }

    btree_search_entries_free(&matches);
    return BTREE_NOT_FOUND;

}

// Delete a cell in a particular position (page number and cell index)
BTreeStatus btree_node_delete_at(Pager *pager, BTreePage *btree_page, uint16_t cell_index,
    BTreeDeletionResult *result, BTreeIndexSpec *index) {
    
    // Validate inputs
    if (!pager || pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!btree_page || 
        !btree_page->page || 
        !btree_page->data ||
        btree_page->type != BTREE_LEAF_NODE || 
        btree_page->cell_count == 0 || cell_index >= btree_page->cell_count) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!index || 
        !index->index_key ||
        !index->index_key->column_index_array ||
        index->index_key->num_columns == 0) {
        return BTREE_INVALID_ARGUMENTS;        
    }

    if (!result) {
        return BTREE_INVALID_ARGUMENTS; 
    }

    deletion_result_reset(result);
    result->page_num = btree_page->page->page_num;

    // Extract cell contents of target cell
    BTreeCellContents cell_to_be_removed = {0};
    
    BTreeStatus status = get_cell_contents(btree_page, cell_index, &cell_to_be_removed, index);

    if (status != BTREE_SUCCESS) {
        btree_cell_contents_free(&cell_to_be_removed);
        return status;
    }

    // Verify its deletion doesn't cause underflow
    uint32_t used_space = 
        (btree_page->cell_count - 1) * sizeof(uint32_t) +
        (PAGE_SIZE - btree_page->free_space_offset - cell_to_be_removed.cell_size);

    status = btree_check_underflow(btree_page, used_space);

    btree_cell_contents_free(&cell_to_be_removed);

    if (status == BTREE_NODE_UNDERFLOW) {
        result->underflow = true;
        return BTREE_NODE_UNDERFLOW;
    }

    if (status != BTREE_SUCCESS) {
        return status;
    }

    // Delete cell since it doesn't cause underflow at this point
    status = btree_remove_cell(btree_page, cell_index);

    if (status != BTREE_SUCCESS) {
        return status;
    }

    if (!btree_page_sync(pager, btree_page)) {
        return BTREE_ERROR;
    }

    if (cell_index == 0 && btree_page->cell_count > 0) {
        BTreeCellContents new_first_cell = {0};

        status = get_cell_contents(btree_page, 0, &new_first_cell, index);

        if (status != BTREE_SUCCESS) {
            btree_cell_contents_free(&new_first_cell);
            return status;
        }

        result->first_key_changed = true;
        result->first_cell = new_first_cell;
    }

    result->deleted = true;

    return BTREE_SUCCESS;
}