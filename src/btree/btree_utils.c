#include <stdio.h>
#include <stdlib.h>
#include "btree_utils.h"
#include "../../include/pager.h"
#include "../../include/page.h"
#include "../../include/data_types.h"
#include "../data_types/data_types_utils.h"
#include "../src/pager/pager_utils.h"
#include "../../include/serialize.h"

/* Pass address of BTreePage in Stack and then logically connect it with a page. */
void btree_page_attach(BTreePage *btree_page, Page *page) {
    if (!btree_page || !page) {
        return;
    }

    btree_page->page = page;
    btree_page->data = page->page_data;

    return btree_page;
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
        && ((btree_page->type_specific_data.previous_leaf_pointer != UINT32_MAX
            && btree_page->type_specific_data.previous_leaf_pointer >= pager->num_pages)
        || (btree_page->type_specific_data.next_leaf_pointer != UINT32_MAX
            && btree_page->type_specific_data.next_leaf_pointer >= pager->num_pages)
            )) {
        return BTREE_CORRUPT_PAGE;
    }

    if (btree_page->type == BTREE_LEAF_NODE
        && (btree_page->type_specific_data.previous_leaf_pointer == 0
        || btree_page->type_specific_data.previous_leaf_pointer == 1
        || btree_page->type_specific_data.next_leaf_pointer == 0
        || btree_page->type_specific_data.next_leaf_pointer == 1)) {
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
        btree_page->type_specific_data.previous_leaf_pointer = get_leaf_previous(btree_page->data);
        btree_page->type_specific_data.next_leaf_pointer = get_leaf_next(btree_page->data);
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
        set_leaf_previous(btree_page->data, btree_page->type_specific_data.previous_leaf_pointer);
        set_leaf_next(btree_page->data, btree_page->type_specific_data.next_leaf_pointer);
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

/* Check if leaf is duplicate by comparing the keys. */
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
BTreeStatus btree_compact_page(BTree *btree, BTreePage *btree_page, BTreeIndexSpec *index) {
    if (!btree || !btree->pager || !btree_page
        || !btree_page->page || !btree_page->data || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Create a page through page.h interface so that I can give it
     * the same page number as the old page without interefering with the Pager.*/
    Page *new_page = page_create(btree->pager, btree_page->page->page_num);
    if (!new_page) {
        return NULL;
    }

    /* Create BTreePage and initialize with the same header metadata. */
    BTreePage replacement = {0};
    btree_page_attach(&replacement, new_page);
    btree_page_init_empty_leaf(btree, &replacement);

    /* Transfer all cell pointers and contents onto new page. */
    BTreeStatus status = BTREE_SUCCESS;
    for (uint16_t i = 0; i < replacement.cell_count; i++) {
        status = btree_transfer_cells(btree_page, i, &replacement, i, index);
        if (status != BTREE_SUCCESS) {
            return status;
        }
    }

    /* Cache new page in position of the old page. */
    btree->pager->pages[btree_page->page->page_num] = new_page;
    /* Free old page and update pointer to NULL for safety. */
    page_free(btree_page->page);
    btree_page->page = NULL;

    btree_page_attach(&btree_page, new_page);
    return BTREE_SUCCESS;
}

/* Connect sibling leaf nodes right after splitting. */
BTreeStatus connect_sibling_leaf_nodes(BTree *btree, BTreePage *btree_page1, BTreePage *btree_page2) {
    if (!btree || !btree->pager || !btree_page1 || !btree_page2) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* If page1 was already connected with previous and next, update them too. */
    if (btree_page1->type_specific_data.previous_leaf_pointer != UINT32_MAX) {
        Page *left_page = pager_get_page(btree->pager, btree_page1->type_specific_data.previous_leaf_pointer);
        if (!left_page) {
            return BTREE_ERROR;
        }

        set_leaf_next(left_page->page_data, btree_page2->page->page_num);
    }

    if (btree_page1->type_specific_data.next_leaf_pointer != UINT32_MAX) {
        Page *right_page = pager_get_page(btree->pager, btree_page1->type_specific_data.next_leaf_pointer);
        if (!right_page) {
            return BTREE_ERROR;
        }

        set_leaf_previous(right_page->page_data, btree_page2->page->page_num);
    }

    /* Page1 and Page2 connection. */
    btree_page2->type_specific_data.next_leaf_pointer = btree_page1->type_specific_data.next_leaf_pointer;
    btree_page1->type_specific_data.next_leaf_pointer = btree_page2->page->page_num;
    btree_page2->type_specific_data.previous_leaf_pointer = btree_page1->page->page_num;

    return BTREE_SUCCESS;
}

/* BTreeCellView RAM Component Interface for easy cell access with offset and its length.
 * DOESNT update the Page Data, just for cell viewing and accesibility. */
BTreeStatus get_cell(BTreePage *btree_page, uint16_t cell_index, BTreeCellView *cell, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data
        || !cell || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint8_t node_type = get_node_type(btree_page->data);
    uint16_t header_size = get_header_size(node_type);
    uint32_t cell_pointer = get_cell_pointer(btree_page->data, cell_index);

    uint16_t offset = get_cell_offset(cell_pointer);
    uint16_t size = get_cell_size(cell_pointer);
    const void *data = (void *) (btree_page->data + offset);

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
BTreeStatus insert_cell(BTree *btree, BTreePage *btree_page, BTreeIndexSpec *index, BTreeSearchResult *search_result,
                     BTreeCellContents *cell_contents) {
    if (!btree || !btree_page || !btree_page->page || !btree_page->data
        || !index || !search_result || !cell_contents) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Shift cell pointers downwards to create empty space. */
    BTreeStatus status = shift_cell_pointers(btree_page, search_result->result_index, BTREE_SHIFT_INSERT);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    uint16_t write_offset = btree_page->free_space_offset - cell_contents->cell_size;
    uint16_t size = cell_contents->cell_size;
    /* Fill that empty space with the new cell pointer. */
    set_cell_pointer(
        btree_page->data,
        search_result->result_index,
        make_cell_pointer(write_offset, size)
    );

    /* Serialize and write payload onto page. */
    if (!serialize_cell_contents(write_offset, btree_page, cell_contents)) {
        return BTREE_ERROR;
    }
    
    /* Update metadata. */
    btree_page->cell_count++;
    btree_page->free_space_offset = write_offset;

    if (!page_mark_dirty(btree_page->page) 
        || !page_touch(btree->pager, btree_page->page)) {
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

    uint32_t dest_cell_pointer = get_cell_pointer(dest->data, dest_idx);

    BTreeCellView src_cell = {0};
    BTreeStatus status = get_cell(src, src_idx, &src_cell, index);
    
    /* Update dest with new cell pointer & free_space_offset. */
    dest->free_space_offset -= src_cell.payload_size;
    set_cell_pointer(dest->data, dest_idx, make_cell_pointer(dest->free_space_offset, src_cell.payload_size));
    
    if (!serialize_cell_contents(dest->free_space_offset, dest, &src_cell)) {
        return false;
    }

    status = shift_cell_pointers(src, src_idx, BTREE_SHIFT_DELETE);
    if (status != BTREE_SUCCESS) {
        return status;
    }
}

/* Remove cell by SHIFTING DOWN cell pointers and contents. */
BTreeStatus btree_remove_cell(BTreePage *btree_page, uint32_t cell_pointer_index, BTreeIndexSpec *index) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint32_t cell_pointer = get_cell_pointer(btree_page->data, 0);

    // Close separator key gap without needing to compact whole page
    BTreeStatus status = shift_cell_pointers(btree_page, 0, BTREE_SHIFT_DELETE);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    status = shift_cells(btree_page, cell_pointer, BTREE_SHIFT_DELETE);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    return BTREE_SUCCESS;
}

/* Shift cell pointers forward/backwards to insert/delete specific cell pointer. */
BTreeStatus shift_cell_pointers(BTreePage *btree_page, uint16_t index, BTreeShiftDirection shift_direction) {
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

/* Shift cells forward/backwards to insert/delete specific cell and its contents. */
BTreeStatus shift_cells(BTreePage *btree_page, uint32_t cell_pointer, BTreeShiftDirection shift_direction) {
    if (!btree_page || !btree_page->type || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    uint16_t cell_offset = get_cell_offset(cell_pointer);
    uint16_t cell_size = get_cell_size(cell_pointer);

    uint16_t bytes_to_be_moved = cell_offset - btree_page->free_space_offset;
    uint8_t *cell_offset = btree_page->data + btree_page->free_space_offset; 

    uint8_t *src, *dest;
    if (shift_direction == BTREE_SHIFT_DELETE) {
        dest = cell_offset;
        src = cell_offset + cell_size;
        btree_page->free_space_offset += cell_size;
    } else {
        dest = cell_offset + cell_size;
        src = cell_offset;
        btree_page->free_space_offset -= cell_size;
    }

    memmove(
        dest,
        src,
        bytes_to_be_moved
    );

    /* Don't forget to update the offset of cell pointers that 
     * their contents were moved due to the shifting above. */
    uint32_t temp = 0;
    uint16_t offset = 0, size = 0;
    for (uint32_t i = 0; i < btree_page->cell_count; i++) {
        temp = get_cell_pointer(btree_page->data, i);
        offset = get_cell_offset(temp);
        size = get_cell_size(temp);

        if (get_cell_offset(cell_pointer) < offset) {
            continue;
        }
        
        if (shift_direction == BTREE_SHIFT_DELETE) {
            offset += cell_size;
            temp = make_cell_pointer(offset, size);
            set_cell_pointer(btree_page->data, i, temp);
        } else {
            offset -= cell_size;
            temp = make_cell_pointer(offset, size);
            set_cell_pointer(btree_page->data, i, temp);
        }
    }

    return BTREE_SUCCESS;
}
