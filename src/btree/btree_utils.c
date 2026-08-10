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
    uint16_t max_cells = (PAGE_SIZE - header_size) / MIN_CELL_SIZE;
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

        if (cell.offset < btree_page->free_space_offset || cell.offset >= PAGE_SIZE
            || cell.offset > PAGE_SIZE - cell.payload_size) {
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

    uint16_t cell_pointer = get_cell_pointer(btree_page->data, btree_page->cell_count-1);
    if (cell_pointer > btree_page->free_space_offset) {
        return BTREE_CORRUPT_PAGE;
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
void btree_page_sync(Pager *pager, BTreePage *btree_page) {
    if (!btree_page || !btree_page->data) {
        return;
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
        return;
    }
}

/* Wrapper function that initializes and validates BTreePage fully. */
BTreeStatus btree_page_attach_load_validate(Pager *pager, BTreePage *btree_page, Page *page, BTreeIndexSpec *index) {
    if (!pager || !btree_page || !btree_page->page 
        || !btree_page->data || !page) {
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

    BTreeCellView cell = {0};
    BTreeStatus status = get_cell(btree_page, cell_index, &cell, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    key = &cell.key;
    
    return BTREE_SUCCESS;
}

/* Comparing BTree Internal/Leaf Node Keys. */
BTreeStatus btree_compare(Value **values, BTreeKeyView *btree_key, BTreeSearchKey *search_key, int *result) {
    if (!values || !search_key || !result) {
        return false;
    }

    Value **key_val = (Value **) btree_key->key;

    /* Compare both same index columns. (With upper limit the search keys used, not
    the amount of keys the BTree is organized with) */
    for (uint32_t i = 0; i < search_key->num_target_keys; i++) {
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

/* Check if there's enough space to store a Cell and its Cell Pointer. */
BTreeStatus btree_page_has_enough_space(BTreePage *btree_page, BTreeCellContents *cell_contents) {
    if (!btree_page || !btree_page->page
         || !btree_page->data || !cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }
    uint16_t reserved_space = 0;
    uint16_t header_size = get_header_size(btree_page->type);

    reserved_space += header_size;
    reserved_space += btree_page->cell_count * 2;
    
    uint16_t available_space = btree_page->free_space_offset - reserved_space;

    // +2 for the cell pointer.
    uint16_t needed_space = cell_contents->key_size + cell_contents->cell_size + 2;

    if (available_space >= needed_space) {
        return true;
    }

    return false;
}

/* (NOT USED ANYWHERE, just extra helper func) Check if leaf is duplicate by comparing the keys. */
BTreeStatus check_leaf_duplicate(BTreePage *btree_page, BTreeSearchKey *search_key,
     BTreeSearchResult *search_result, bool *duplicate) {
    if (!btree_page || !btree_page->page || !btree_page->data
        || !search_key || !search_result || !duplicate) {
        return BTREE_INVALID_ARGUMENTS;
    }
    *duplicate = false;

    /* Get BTreeKeyView pointer to where key begins. */
    BTreeKeyView key = {0};
    BTreeStatus status = get_key(btree_page, search_result->result_index, &key, search_key->index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    /* And reconstruct array of values from that BTreeKeyView pointer. */
    Value **values = NULL;
    void *key_offset = key.key;
    for (uint32_t i = 0; i < search_key->num_target_keys; i++) {
        values[i] = value_create(search_key->index->column_types[i], key_offset);

        key_offset += get_data_type_size(search_key->index->column_types[i]);
    }

    /* Then compare them. */
    int result;
    status = btree_compare(values, &key, search_key, &result);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    /* If they are the same, its a duplicate. */
    if (result == 0) {
        *duplicate = true;
        fprintf(stderr, "btree_leaf_node_insert: Duplicate key found.\n");
    }

    value_free_array(values, search_key->num_target_keys);
    return true;
}

/* Inserting into an internal node and the binary search's returned
 * index is equal to the page's cell count then:
 * swap rightmost child pointer with the child pointer contained in
 * the payload*/
BTreeStatus swap_internal_rightmost_child_pointer(BTreePage *btree_page, BTreeCellContents *cell_contents) {
    if (!btree_page || !btree_page->type 
        || !btree_page->data || !cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }
    
    uint32_t old_rightmost_child_pointer = btree_page->type_specific_data.rightmost_child_pointer;
    btree_page->type_specific_data.rightmost_child_pointer = cell_contents->BTreePayload.child_pointer;
    cell_contents->BTreePayload.child_pointer = old_rightmost_child_pointer;

    return BTREE_SUCCESS;
}

/* Remove garbage cell pointers and contents by creating a new page and transfering
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
    btree_page_init_empty_leaf(&replacement);
    btree_page_sync(pager, &replacement);

    /* Transfer all cell pointers and contents onto new page. */
    BTreeStatus status = BTREE_SUCCESS;
    for (uint16_t i = 0; i < replacement.cell_count; i++) {
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

    btree_page_attach(btree_page, new_page);
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

        set_leaf_next(left_page->page_data, btree_page2->page->page_num);
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

    uint32_t cell_pointer = get_cell_pointer(btree_page->data, cell_index);

    uint16_t offset = get_cell_offset(cell_pointer);
    uint16_t size = get_cell_size(cell_pointer);
    void *data = (void *) (btree_page->data + offset);

    /* Create BTreeKeyView struct too. */
    cell->key.key = data;
    cell->key.offset = offset;
    if (btree_page->type == BTREE_INTERNAL_NODE) {
        cell->key.key += 4;
        cell->key.offset += 4;
    }

    cell->key.key_size = index->key_size;
    cell->payload = data;
    cell->offset = offset;
    cell->payload_size = size;

    return BTREE_SUCCESS;
}

/* Insert new cell's Cell Pointer and its contents onto a BTreePage AFTER BINARY SEARCH. */
BTreeStatus insert_cell(Pager *pager, BTreePage *btree_page, BTreeIndexSpec *index, BTreeSearchResult *search_result,
                     BTreeCellContents *cell_contents) {
    if (!pager || !btree_page || !btree_page->page || !btree_page->data
        || !index || !search_result || !cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Shift cell pointers downwards to create empty space. */
    BTreeStatus status = shift_cell_pointer(btree_page, search_result->result_index, BTREE_SHIFT_INSERT);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    uint8_t write_offset = btree_page->free_space_offset - cell_contents->cell_size;
    uint16_t size = cell_contents->cell_size;
    /* Fill that empty space with the new cell pointer. */
    set_cell_pointer(
        btree_page->data,
        search_result->result_index,
        make_cell_pointer(write_offset, size)
    );

    /* Serialize and write payload onto page. */
    if (!serialize_cell_contents(&write_offset, btree_page, cell_contents)) {
        return BTREE_ERROR;
    }
    
    /* Update metadata. */
    btree_page->cell_count++;
    btree_page->free_space_offset = write_offset;

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
    set_cell_count(src->data, cell_index);
    set_cell_count(dest->data, src->cell_count - cell_index);

    return BTREE_SUCCESS;
}

/* Function that actually does that transferring of cell pointers and contents. */
BTreeStatus btree_transfer_cells(BTreePage *src, uint16_t src_idx, BTreePage *dest, uint16_t dest_idx, BTreeIndexSpec *index) {
    if (!src || !src->page || !src->data
        || !dest || !dest->page || !dest->data
        || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeCellView src_cell_view = {0};
    BTreeStatus status = get_cell(src, src_idx, &src_cell_view, index);
    BTreeCellContents src_cell = {0};
    
    if (!deserialize_cell_contents(index->schema, src->data, src, &src_cell_view, &src_cell, index)) {
        return false;
    }
    
    /* Update dest with new cell pointer & free_space_offset. */
    dest->free_space_offset -= src_cell_view.payload_size;
    set_cell_pointer(dest->data, dest_idx, make_cell_pointer(dest->free_space_offset, src_cell_view.payload_size));
    
    if (!serialize_cell_contents((uint8_t *) &dest->free_space_offset, dest, &src_cell)) {
        return BTREE_ERROR;
    }

    status = shift_cell_pointer(src, src_idx, BTREE_SHIFT_DELETE);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    return BTREE_SUCCESS;
}


/* Remove cell by SHIFTING DOWN cell pointers and contents. */
BTreeStatus btree_remove_cell(BTreePage *btree_page, uint32_t cell_pointer_index, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint32_t cell_pointer = get_cell_pointer(btree_page->data, 0);

    // Close separator key gap without needing to compact whole page
    BTreeStatus status = shift_cell_pointer(btree_page, 0, BTREE_SHIFT_DELETE);
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
    if (index >= btree_page->cell_count) {
        return BTREE_SUCCESS;
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
    if (!btree_page || !btree_page->type || !btree_page->data) {
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
        uint16_t cell_offset = get_cell_offset(cell_pointer);
        uint32_t child_page_num = get_cell_child_pointer(btree_page.data, cell_offset);

        if (btree_traverse_page_recursive(btree, child_page_num, visited_pages) != BTREE_SUCCESS) {
            return BTREE_ERROR;
        }
    }

    // Visit the rightmost child page (which is a standard internal node field)
    uint32_t rightmost_child_page_num = get_rightmost_child(btree_page.data);
    

    return btree_traverse_page_recursive(btree, rightmost_child_page_num, visited_pages);
}


/* ---------- BTreeRangeResult Helpers ---------- */

/* The caller owns the BTreeRangeResult struct */
BTreeStatus btree_range_result_init(BTreeRangeResult *result) {
    if (!result) {
        return BTREE_INVALID_ARGUMENTS;
    }

    result->cells = (BTreeCellContents *) calloc(BTREE_RANGE_INITIAL_CAPACITY, sizeof(BTreeCellContents));
    
    if(!result->cells) {
        result->count = 0;
        result->capacity = 0;
        return BTREE_ERROR;
    }
    
    result->count = 0;
    result->capacity = BTREE_RANGE_INITIAL_CAPACITY;
    return BTREE_SUCCESS;
}


BTreeStatus btree_range_result_append(BTreeRangeResult *result, BTreeCellContents *new_cell) {
    if (!result || !new_cell) {
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
        BTreeCellContents *new_result_cells = 
            (BTreeCellContents *) realloc(result->cells, new_capacity * sizeof(BTreeCellContents));

        if (!new_result_cells) {
            return BTREE_ERROR;
        }

        result->cells = new_result_cells;
        result->capacity = new_capacity;
    }

    // Appending the new cell content struct
    result->cells[result->count] = *new_cell;
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

    if (cell->BTreePayload.row) {
        row_free(cell->BTreePayload.row);
        cell->BTreePayload.row = NULL;
    }
}

void btree_range_result_free(BTreeRangeResult *result) {
    if (!result) {
        return;
    }

    // Free the allocate Key and Rows for each cell contents entry in the ragne result
    for (uint32_t i = 0; i < result->count; i++) {
        btree_cell_contents_free(&result->cells[i]);
    }

    free(result->cells);

    result->cells = NULL;
    result->count = 0;
    result->capacity = 0;
}