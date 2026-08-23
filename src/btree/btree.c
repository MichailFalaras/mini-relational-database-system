#include <stdio.h>
#include <stdlib.h>
#include "../../include/btree.h"
#include "btree_utils.h"
#include "../../include/page.h"
#include "../../include/data_types.h"
#include "../../include/serialize.h"
#include "../src/data_types/data_types_utils.h"
#include "../../include/index.h"
#include "../../include/schema.h"

/* Initialize btree_page as an Empty Leaf Node. */
BTreeStatus btree_page_init_empty_leaf(BTreePage *btree_page) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Update in memory data. */
    btree_page->type = BTREE_LEAF_NODE;
    /* New leaf node temporarily initialized as a root node, and if it isn't its updated later */
    btree_page->is_root = 1;
    btree_page->parent_pointer = UINT32_MAX;
    btree_page->cell_count = 0;
    btree_page->free_space_offset = PAGE_SIZE;
    btree_page->type_specific_data.siblings.previous_leaf_pointer = UINT32_MAX;
    btree_page->type_specific_data.siblings.next_leaf_pointer = UINT32_MAX;

    return BTREE_SUCCESS;
}

/* Initialize btree_page as an Internal Node and set its rightmost child pointer. */
BTreeStatus btree_page_init_internal(BTreePage *btree_page, uint32_t rightmost_child_pointer) {
    if (!btree_page || !btree_page->page || !btree_page->data
        || rightmost_child_pointer == 0 || rightmost_child_pointer == 1) {
        return BTREE_INVALID_ARGUMENTS;
    }

    btree_page->type = BTREE_INTERNAL_NODE;
    /* New internal node temporarily initialized as a root node, and if it isn't its updated later */
    btree_page->is_root = 1; 
    btree_page->parent_pointer = UINT32_MAX;
    btree_page->cell_count = 0;
    btree_page->free_space_offset = PAGE_SIZE;
    btree_page->type_specific_data.rightmost_child_pointer = rightmost_child_pointer;

    return BTREE_SUCCESS;
}

/* Lower Bound Binary Search with a target key.
 * Used to traverse through the B+Tree and find correct cell position in a page.
 * Return important information in BTreeSearchResult. */
BTreeStatus btree_binary_search(BTreePage *btree_page, BTreeSearchKey *search_key,
    BTreeSearchResult *search_result, BTreeBinarySearchType mode) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }
    
    if (mode != BTREE_LOWER_BOUND && mode != BTREE_UPPER_BOUND) {
        return BTREE_INVALID_ARGUMENTS;
    }

    BTreeStatus status = BTREE_ERROR;

    /* Initialize BTreeSearchResult. */
    search_result->found = true; // Only errors change this to false
    search_result->exact_match = false; // Only if exact match was found
    search_result->page = btree_page->page; // This should be updated in root-to-leaf search
    search_result->result_index = btree_page->cell_count; // Initialize to cell count

    /* If there are no cells in the page, return index 0 as insertion position. */
    if (btree_page->cell_count == 0) {
        fprintf(stderr, "btree_binary_search: Empty node.\n");
        search_result->result_index = 0;

        return BTREE_SUCCESS;
    }

    /* Initialize binary search vars. */
    uint16_t from = 0;
    uint16_t to = btree_page->cell_count - 1;
    uint16_t mid = 0;

    BTreeKeyView btree_key = {0};
    Value **target_key_vals = NULL, **cell_key_vals = NULL;
    int result = 0;
    while (from <= to) {
        mid = from + (to - from) / 2;

        /* Get key and store it in a temporary BTreeKeyView struct. */
        status = get_key(btree_page, mid, &btree_key, search_key->index);
        if (status != BTREE_SUCCESS) {
            search_result->found = false;
            search_result->exact_match = false;
            search_result->result_index = UINT16_MAX;
            search_result->page = NULL;
            return status;
        }

        /* Temporary version of comparison between keys. */
        /* Allocate as much needed space to extract values from the key. */
        cell_key_vals = serialized_key_to_values(btree_key.key, search_key->num_target_keys, search_key->index);
        if (!cell_key_vals) {
            return BTREE_ERROR;
        }

        target_key_vals = serialized_key_to_values(search_key->target_key, search_key->num_target_keys, search_key->index);
        if (!target_key_vals) {
            value_free_array(cell_key_vals, search_key->num_target_keys);
            return BTREE_ERROR;
        }

        /* Compare values. */
        status = btree_compare(cell_key_vals, target_key_vals, search_key->num_target_keys, &result);
        if (status != BTREE_SUCCESS) {
            search_result->found = false;
            search_result->exact_match = false;
            search_result->result_index = UINT16_MAX;
            value_free_array(cell_key_vals, search_key->num_target_keys);
            value_free_array(target_key_vals, search_key->num_target_keys);
            return status;
        }

        bool candidate = false;
        if (mode == BTREE_LOWER_BOUND) {
            candidate = (result >= 0);
        } else if (mode == BTREE_UPPER_BOUND) {
            candidate = (result > 0);
        }

        /* Evalute result: */
        if (candidate) {
            search_result->found = true;

            /* If result is 0, it means we have an exact match.
             * (Used to detect duplicates)*/
            search_result->exact_match = (result == 0);

            search_result->result_index = mid;
            if (mid == 0) {
                value_free_array(cell_key_vals, search_key->num_target_keys);
                value_free_array(target_key_vals, search_key->num_target_keys);
                break;
            }
            to = mid - 1;
        } else {
            from = mid + 1;
        } 

        /* Free temporary values array in each loop. */
        value_free_array(cell_key_vals, search_key->num_target_keys);
        value_free_array(target_key_vals, search_key->num_target_keys);
    }

    return BTREE_SUCCESS;
}

/* Root to leaf traversal using a specific key to ultimately reach
 * a cell position to store data.
 * Also returns important information in BTreeSearchResult.
 * 
 * BTreeBinarySearchType flexibility.
 * - If we are searching a range/prefix of key then you want the first
 * appearance of that prefix you need to search with BTREE_LOWER_BOUND.
 * - If you want a full key exact root-to-leaf traversal, choosing the correct
 * child pointers, you need to search with BTREE_UPPER_BOUND.  */
BTreeStatus btree_root_to_leaf(BTree *btree, BTreeSearchKey *search_key, BTreeSearchResult *search_result, 
    BTreeBinarySearchType mode) {
    if (!btree || !btree->pager
        || btree->root_page_num >= btree->pager->num_pages
        || btree->root_page_num == 0 || btree->root_page_num == 1
        || !search_key || !search_result) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (mode != BTREE_LOWER_BOUND && mode != BTREE_UPPER_BOUND) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Get root page in cache. */
    Page *page = pager_get_page(btree->pager, btree->root_page_num);
    if (!page) {
        return BTREE_ERROR;
    }

    /* Initialize BTreePage and validate it.
     * Page cannot be free. Should have been removed from the BTree
     * if it was freed. */
    BTreePage btree_page = {0};
    BTreeStatus status = btree_page_attach_load_validate(btree->pager, &btree_page, page, search_key->index);
    if (status != BTREE_SUCCESS) {
        pager_evict_page(btree->pager, page->page_num);
        return status;
    }

    uint32_t child_pointer = 0;
    /* While node isn't a Leaf Node. */
    while (btree_page.type != BTREE_LEAF_NODE) {

        /* Lower bound binary search to traverse throughout internal nodes. */
        status = btree_binary_search(&btree_page, search_key, search_result, mode);
        if (status != BTREE_SUCCESS) {
            pager_evict_page(btree->pager, page->page_num);
            return status;
        }

        /* Choose correct cell pointer index handling. */
        if (search_result->result_index == btree_page.cell_count) {
            child_pointer = btree_page.type_specific_data.rightmost_child_pointer;
        } else {
            uint32_t cell_pointer = get_cell_pointer(btree_page.data, search_result->result_index);
            child_pointer = get_cell_child_pointer(btree_page.data, get_cell_offset(cell_pointer));
        }

        /* Child pointer gets validated below in btree_page_attach_load_validate. */

        uint32_t parent_page_num = page->page_num; // Keep page num to evict at the end

        /* Get new page in cache and initialize BTreePage again. */
        page = pager_get_page(btree->pager, child_pointer);
        if (!page) {
            return BTREE_ERROR;
        }

        status = btree_page_attach_load_validate(btree->pager, &btree_page, page, search_key->index);
        if (status != BTREE_SUCCESS) {
            pager_evict_page(btree->pager, page->page_num);
            return status;
        }
        
        // Evict parent page here
        if (!pager_evict_page(btree->pager, parent_page_num)) {
            return BTREE_ERROR;
        }
    }

    /* One last lower bound binary search to find correct cell pointer index
     * within the leaf node. */
    status = btree_binary_search(&btree_page, search_key, search_result, BTREE_LOWER_BOUND);
    if (status != BTREE_SUCCESS) {
        pager_evict_page(btree->pager, page->page_num);
        return status;
    }

    search_result->page = btree_page.page;    
    return BTREE_SUCCESS;
}

/* BTree Node insert type agnostic function.
 * Content being inserted is stored in BTreeCellContents.
 * Returns BTREE_SUCCESS or BTREE_NEEDS_SPLIT/BTreeSplitResult with split boolean value equal to true.
 * 
 * Also used to insert the separator key onto the parent page. */
BTreeStatus btree_node_insert(Pager *pager, BTreePage *btree_page, BTreeCellContents *cell_contents,
                                BTreeSplitResult *split_result, BTreeIndexSpec *index) {
    if (!pager || !btree_page || !btree_page->page || !btree_page->data
        || !cell_contents || !split_result || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Initialize temporary BTreeSearchKey & BtreeSearchResult structs. */
    BTreeSearchKey search_key = {0};
    search_key.num_target_keys = cell_contents->num_keys;
    search_key.target_key = values_to_serialized_key(cell_contents->keys, search_key.num_target_keys, index);
    if (!search_key.target_key) {
        return BTREE_ERROR;
    }
    search_key.index = index;

    BTreeSearchResult search_result = {0};

    /* Find correct position to insert cell by searching with cell's keys.*/
    BTreeStatus status = btree_binary_search(btree_page, &search_key, &search_result, BTREE_LOWER_BOUND);
    if (status != BTREE_SUCCESS) {
        fprintf(stderr, "btree_leaf_node_insert: Something went wrong.\n");
        free(search_key.target_key);
        return status;
    }
    free(search_key.target_key);
    
    /* LEAF NODE SPECIFIC CHECK:
     * If at least one column is PRIMARY_KEY or UNIQUE (Composite Index)
     * then the whole key is unique. Therefore, if the key of the
     * row we want to add to the page has the same key as the row
     * already in its position then we have a duplicate. */
    if (btree_page->type == BTREE_LEAF_NODE && index->is_unique == true
         && search_result.result_index < btree_page->cell_count) {
    
        if (search_result.exact_match == true) {
            return BTREE_DUPLICATE_KEY;
        }
    
    }

    /* First check if what's going to be inserted is a duplicate,
     * then check if there's enough space, update split flag and return BTREE_NEEDS_SPLIT. */
    status = btree_page_has_enough_space(btree_page, cell_contents);
    if (status != BTREE_SUCCESS) {
        if (status == BTREE_NEEDS_SPLIT) {
            split_result->split = true;
            split_result->left_page = btree_page->page->page_num;
        }

        return status;
    }

    /* INTERNAL NODE SPECIFIC CHECK: 
     * If its an internal node and result index is equal to the cell count
     * then it means its the rightmost child pointer.
     * We swap the rightmost child pointer with the payload's child pointer. */
    if (btree_page->type == BTREE_INTERNAL_NODE && search_result.result_index == btree_page->cell_count) {
        status = swap_internal_rightmost_child_pointer(btree_page, cell_contents);
        if (status != BTREE_SUCCESS) {
            return status;
        }
    }

    /* Creates space for new cell pointer and actually serializes cell contents onto
     * page's page_data. */
    status = insert_cell(pager, btree_page, index, &search_result, cell_contents);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    // Sync everything that happened in RAM with the page's page_data. 
    if (!btree_page_sync(pager, btree_page)){
        return BTREE_ERROR;
    }
    return BTREE_SUCCESS;
}

BTreeStatus btree_node_delete(Pager *pager, BTreePage *btree_page, BTreeSearchKey *search_key, BTreeDeletionResult *deletion_result) {
    if (!pager || !btree_page || !btree_page->page
        || !btree_page->data || !search_key 
        || !search_key->index || !deletion_result) {
        return BTREE_INVALID_ARGUMENTS;
    }
    BTreeStatus deletion_status = BTREE_SUCCESS;

    /* Initialize deletion result metadata. */
    deletion_result->underflow = false; // To change only if there's underflow
    deletion_result->page_num = btree_page->page->page_num;
    deletion_result->deleted = false;
    deletion_result->first_key_changed = false;
    deletion_result->new_first_key = NULL;

    /* Find index of exact cell we want to delete.*/
    BTreeSearchResult search_result = {0};
    BTreeStatus status = btree_binary_search(btree_page, search_key, &search_result, BTREE_LOWER_BOUND);
    if (status != BTREE_SUCCESS) {
        fprintf(stderr, "btree_node_delete: Something went wrong.\n");
        return status;
    }
    
    /* We want an exact match of keys in order to delete. */
    if (search_result.exact_match == false) {
        return BTREE_NOT_FOUND;
    }

    /* Acquire serialized first key before removing the cell. */
    BTreeKeyView key_view = {0};
    void *first_key = NULL;
    if (search_result.result_index == 0 && btree_page->cell_count > 1) {
        /* index 1 because we are choosing the key that is GOING TO BE the first key
        * after deletion of the first one. */
        status = get_key(btree_page, 1, &key_view, search_key->index);
        if (status != BTREE_SUCCESS) {
            return status;
        }
        
        first_key = serialized_key_alloc(&key_view);
        if (!first_key) {
            return BTREE_ERROR;
        }
    }

    /* Remove specific cell. */
    status = btree_remove_cell(btree_page, search_result.result_index);
    if (status != BTREE_SUCCESS) {
        free(first_key);
        return status;
    }

    /* Check if page is underflowing. */
    status = btree_check_underflow(btree_page);
    if (status == BTREE_NODE_UNDERFLOW) {
        deletion_result->underflow = true;
        deletion_status = BTREE_NODE_UNDERFLOW;

    } else if (status != BTREE_SUCCESS) {
        free(first_key);
        return status;
    }

    if (!btree_page_sync(pager, btree_page)) {
        free(first_key);
        return BTREE_ERROR;
    }

    /* Pass serialized first key only if everything went ok. */
    if (search_result.result_index == 0) {
        deletion_result->first_key_changed = true;
        deletion_result->new_first_key = first_key;
    }
    deletion_result->deleted = true;

    return deletion_status;
}

/* BTree Leaf Node Split.
 * Returns important split information in BTreeSplitResult. */
BTreeStatus btree_leaf_node_split(Pager *pager, BTreePage *original_page, BTreeIndexSpec *index,
                                BTreeSplitResult *split_result) {
    if (!pager || !original_page || !original_page->page 
        || !original_page->data || !index || !split_result) {
        return BTREE_INVALID_ARGUMENTS;
    }
    
    if (original_page->type != BTREE_LEAF_NODE) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Logically allocate new page. */
    uint32_t new_page_num = 0;
    if (!pager_allocate_page(pager, &new_page_num)) {
        return BTREE_ERROR;
    }

    /* Get it in cache. */
    Page *new_page = pager_get_page(pager, new_page_num);
    if (!new_page) {
        pager_release_page(pager, new_page->page_num);
        return BTREE_ERROR;
    }

    /* Initialize right child of split. */
    BTreePage btree_right_page = {0};
    btree_page_attach(&btree_right_page, new_page);
    BTreeStatus status = btree_page_init_empty_leaf(&btree_right_page);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page->page_num);
        return status;
    }
    
    /* Split original page's cells in half and transfers them to the right child of the split. */
    status = btree_split_cells(original_page, &btree_right_page, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page->page_num);
        return status;
    }

    if (!btree_page_sync(pager, original_page) 
        || !btree_page_sync(pager, &btree_right_page)) {
        pager_release_page(pager, new_page->page_num);
        return BTREE_ERROR;
    }

    /* Compact original page's cells to remove any garbage value left behind. */
    status = btree_compact_page(pager, original_page, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page->page_num);
        return status;
    }

    /* Connect original page's sibling pointers with the right child. */
    status = connect_sibling_leaf_nodes(pager, original_page, &btree_right_page);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page->page_num);
        return BTREE_ERROR;
    }
    
    // Sync with pages' page_data
    if (!btree_page_sync(pager, original_page) 
        || !btree_page_sync(pager, &btree_right_page)) {
        pager_release_page(pager, new_page->page_num);
        return BTREE_ERROR;
    }

    /* Extract separator key from right child and store it in BTreeSplitResult. */
    BTreeKeyView key_view = {0};
    status = get_key(&btree_right_page, 0, &key_view, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page->page_num);
        return BTREE_ERROR;
    }

    split_result->left_page = original_page->page->page_num;
    split_result->right_page = btree_right_page.page->page_num;
    split_result->separator_key = serialized_key_alloc(&key_view);
    if (!split_result->separator_key) {
        pager_release_page(pager, new_page->page_num);
        split_result->right_page = UINT32_MAX;
        return BTREE_ERROR;
    }
    split_result->separator_size = key_view.key_size;
    split_result->split = true;
    return BTREE_SUCCESS;
}

/* BTree Internal Node Split. 
 * Returns split information BTreeSplitResult. */
BTreeStatus btree_internal_node_split(Pager *pager, BTreePage *original_page, BTreeIndexSpec *index,
                                BTreeSplitResult *split_result) {
    if (!pager || !original_page || !original_page->page 
        || !original_page->data || !index || !split_result) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (original_page->type != BTREE_INTERNAL_NODE) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Logically allocate new page. */
    uint32_t new_page_num = 0;
    if (!pager_allocate_page(pager, &new_page_num)) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }

    /* Get it in cache. */
    Page *new_page = pager_get_page(pager, new_page_num);
    if (!new_page) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }

    /* Initialize right child of split. */
    BTreePage btree_right_page = {0};
    btree_page_attach(&btree_right_page, new_page);
    BTreeStatus status = btree_page_init_internal(&btree_right_page, UINT32_MAX);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page_num);
        return status;
    }
    
    if (!btree_page_sync(pager, &btree_right_page)) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }
    
    /* Split original page's cells in half and transfers them to the right child of the split. */
    status = btree_split_cells(original_page, &btree_right_page, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page_num);
        return status;
    }

    /* Pass original page's rightmost to the right child because the original page's
     * rightmost child pointer is bigger than all of the keys. */
    btree_right_page.type_specific_data.rightmost_child_pointer = original_page->
                                                                type_specific_data.rightmost_child_pointer;

    /* Update right page's connected child nodes' parent pointer metadata. */
    status = update_children_parent_metadata(pager, &btree_right_page, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page_num);
        return status;
    }

    if (!btree_page_sync(pager, original_page) 
        || !btree_page_sync(pager, &btree_right_page)) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }

    /* Compact original page's cells to remove any garbage value left behind. */
    status = btree_compact_page(pager, original_page, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page_num);
        return status;
    }
    
    // Sync with pages' page_data
    if (!btree_page_sync(pager, original_page) 
        || !btree_page_sync(pager, &btree_right_page)) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }

    BTreeCellView cell_view = {0};
    status = get_cell(&btree_right_page, 0, &cell_view, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page_num);
        return status;
    }

    /* Get separator cell's child pointer and pass it to the rightmost child pointer of
     * the original page. */
    memcpy(&(original_page->type_specific_data.rightmost_child_pointer), cell_view.payload, sizeof(uint32_t));
    status = update_children_parent_metadata(pager, original_page, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(pager, new_page_num);
        return status;
    }
    if (!btree_page_sync(pager, original_page)) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }

    split_result->separator_key = serialized_key_alloc(&cell_view.key);
    if (!split_result->separator_key) {
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }
    
    status = btree_remove_cell(&btree_right_page, 0);
    if (status != BTREE_SUCCESS) {
        free(split_result->separator_key);
        split_result->separator_key = NULL;
        pager_release_page(pager, new_page_num);
        return status;
    }

    if (!btree_page_sync(pager, &btree_right_page)) {
        free(split_result->separator_key);
        split_result->separator_key = NULL;
        pager_release_page(pager, new_page_num);
        return BTREE_ERROR;
    }

    split_result->left_page = original_page->page->page_num;
    split_result->right_page = btree_right_page.page->page_num;
    split_result->separator_size = cell_view.key.key_size;
    split_result->split = true;

    return BTREE_SUCCESS;
}

/* BTree Root Node Split Handling.
 * Creates new root, moves separator key
 * and updates pages' metadata. */
BTreeStatus btree_root_split(BTree *btree, BTreePage *btree_old_root, BTreeSplitResult *split_result, BTreeIndexSpec *index) {
    if (!btree || !btree->pager || !btree_old_root || !btree_old_root->page
        || !btree_old_root->data || !split_result || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Allocate new root page. */
    uint32_t new_root_page_num = 0;
    if (!pager_allocate_page(btree->pager, &new_root_page_num)) {
        pager_release_page(btree->pager, new_root_page_num);
        return BTREE_ERROR;
    }

    /* Get both pages in cache. */
    Page *new_root = pager_get_page(btree->pager, new_root_page_num);
    Page *right_child = pager_get_page(btree->pager, split_result->right_page);
    if (!new_root || !right_child) {
        pager_release_page(btree->pager, new_root_page_num);
        return BTREE_ERROR;
    }

    /* Initialize temporary BTreePage structs. */
    // Create new root page
    BTreePage btree_new_root = {0};
    btree_page_attach(&btree_new_root, new_root);
    BTreeStatus status = btree_page_init_internal(&btree_new_root, UINT32_MAX);
    if (status != BTREE_SUCCESS) {
        pager_release_page(btree->pager, new_root_page_num);
        return status;
    }
    if (!btree_page_sync(btree->pager, &btree_new_root)) {
        pager_release_page(btree->pager, new_root_page_num);
        return BTREE_ERROR;
    }
    btree_new_root.type_specific_data.rightmost_child_pointer = btree_old_root->page->page_num;

    // Create right child page
    BTreePage btree_right_child = {0};
    status = btree_page_attach_load_validate(btree->pager, &btree_right_child, right_child, index);
    if (status != BTREE_SUCCESS) {
        pager_release_page(btree->pager, new_root_page_num);
        return status;
    }

    /* Create cell that's going to be moved to the new root. */
    BTreeCellContents cell = {0};
    cell.type = BTREE_INTERNAL_NODE;
    cell.num_keys = index->index_key->num_columns;
    cell.keys = serialized_key_to_values(split_result->separator_key, cell.num_keys, index);
    if (!cell.keys) {
        page_free(new_root);
        pager_release_page(btree->pager, new_root_page_num);
        return BTREE_ERROR;
    }

    cell.key_size = split_result->separator_size;
    cell.BTreePayload.child_pointer = btree_old_root->page->page_num;
    cell.cell_size = split_result->separator_size + sizeof(uint32_t);

    /* Insert separator cell into new root. */
    status = btree_node_insert(btree->pager, &btree_new_root, &cell, split_result, index);
    if (status != BTREE_SUCCESS) {
        value_free_array(cell.keys, cell.num_keys);
        pager_release_page(btree->pager, new_root_page_num);
        return status;
    }

    /* Update BTree's root page num*/
    btree->root_page_num = new_root->page_num;

    /* Update old root's metadata. */
    btree_old_root->is_root = false;
    btree_old_root->parent_pointer = btree_new_root.page->page_num;

    /* Update new root's metadata. */
    btree_new_root.is_root = true;
    btree_new_root.parent_pointer = UINT32_MAX;
    btree_new_root.type_specific_data.rightmost_child_pointer = btree_right_child.page->page_num;

    /* Update right child's metadata. */
    btree_right_child.is_root = false;
    btree_right_child.parent_pointer = btree_new_root.page->page_num;

    // Sync with pages' page_data.
    if (!btree_page_sync(btree->pager, btree_old_root)
        || !btree_page_sync(btree->pager, &btree_new_root)
        || !btree_page_sync(btree->pager, &btree_right_child)) {
        value_free_array(cell.keys, cell.num_keys);
        pager_release_page(btree->pager, new_root_page_num);
        return BTREE_ERROR;
    }

    value_free_array(cell.keys, cell.num_keys);
    split_result_reset(split_result);
    return BTREE_SUCCESS;
}

// Traverse B+ Tree and store the numbers of the visited pages
BTreeStatus btree_traverse_reachable_pages(BTree *btree, BTreePageCollection *visited_pages) {
    if (!btree->pager || btree->pager->num_pages <= SYSTEM_CATALOG_PAGE_NUM) {
        printf("btree_traverse_reachable_pages: Invalid Pager.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!visited_pages) {
        printf("btree_traverse_reachable_pages: Invalid visited-pages structure.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    if (btree->root_page_num <= SYSTEM_CATALOG_PAGE_NUM ||
        btree->root_page_num >= btree->pager->num_pages ||
        btree->root_page_num >= MAX_PAGES) {
        printf("btree_traverse_reachable_pages: Invalid root page number.\n");
        return BTREE_CORRUPT_PAGE;
    }
    visited_pages->count = 0;
   
    // Helper that recursively traverses internal nodes, and backtracking at leaf nodes
    BTreeStatus status = btree_traverse_page_recursive(btree, btree->root_page_num, visited_pages);
    if (status != BTREE_SUCCESS) {
        printf("btree_traverse_reachable_pages: Recursive Index B+ Tree traversal failed.\n");
        return status;
    }
    
    return BTREE_SUCCESS;
}

/* Find a unique B+ Tree key */
BTreeStatus btree_find_exact_key(BTree *btree, BTreeSearchKey *search_key, BTreeSearchResult *search_result,
                                 BTreeCellContents *cell_contents) {
    // Validate inputs
    if (!btree || !search_key || !search_result || !cell_contents) {
        printf("btree_find_exact_key: Invalid inputs.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!btree->pager || 
        btree->root_page_num == INVALID_ROOT_PAGE ||
        btree->root_page_num >= btree->pager->num_pages) {
        printf("btree_find_exact_key: Invalid input B+ Tree.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    if (!search_key->index || 
        !search_key->index->index_key || 
        !search_key->index->index_key->column_index_array ||
        search_key->index->index_key->num_columns == 0) {
        printf("btree_find_exact_key: Invalid index Key.\n");
        return BTREE_INVALID_ARGUMENTS;        
    }

    if (!search_key->target_key ||
        search_key->num_target_keys == 0 ||
        search_key->num_target_keys != search_key->index->index_key->num_columns) {
        printf("btree_find_exact_key: Invalid target search Key.\n");
        return BTREE_INVALID_ARGUMENTS;
    }

    // Initializing the search result structure to default values,
    // in cases the search function call fails
    *search_result = (BTreeSearchResult) {
        .found = false,
        .exact_match = false,
        .page = NULL,
        .result_index = UINT16_MAX
    };

    BTreeStatus status = btree_root_to_leaf(btree, search_key, search_result, BTREE_UPPER_BOUND);
    
    // Search operation failed
    if (status != BTREE_SUCCESS) {
        return status;
    }

    // Search did not find an exact match of the requested search key
    if (!search_result->exact_match) {
        search_result->found = false;
        search_result->page = NULL;
        search_result->result_index = UINT16_MAX;
        return BTREE_NOT_FOUND;
    }

    // Search result must point to an existing (the leaf) page 
    if (!search_result->page) {
        return BTREE_ERROR;
    }

    // Load the leaf page to a BTreePage structure
    BTreePage leaf = {0};

    status = btree_page_attach_load_validate(btree->pager, &leaf, search_result->page, search_key->index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    // Validating that the result page is actually a leaf,
    // and that the exact match result index is within the available cell count
    if (leaf.type != BTREE_LEAF_NODE || search_result->result_index >= leaf.cell_count) {
        return BTREE_CORRUPT_PAGE;
    }

    BTreeCellView cell_view = {0};

    // Get cell view 
    status = get_cell(&leaf, search_result->result_index, &cell_view, search_key->index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    // Deserialize the exact-matching cell contents. 
    // If the process fails, the page is corrupt
    if (!deserialize_cell_contents(
        search_key->index->schema, 
        leaf.data,
        &leaf,
        &cell_view,
        cell_contents,
        search_key->index)) {
        
        return BTREE_CORRUPT_PAGE;
    }

    return BTREE_SUCCESS;
}

// Find all B+ Tree Key prefixes
BTreeStatus btree_find_prefix_keys(BTree *btree, BTreeIndexSpec *index, BTreeSearchKey *prefix_key,
    BTreeRangeResult *result) {

    if (!btree || !index || !index->index_key ||
        !index->index_key->column_index_array ||
        index->index_key->num_columns == 0) {
        return BTREE_INVALID_ARGUMENTS;        
    }
    
    if (!prefix_key || !prefix_key->target_key ||
        prefix_key->index != index ||
        prefix_key->num_target_keys == 0 ||
        prefix_key->num_target_keys > index->index_key->num_columns || !result) {
        return BTREE_INVALID_ARGUMENTS;
    }

    return btree_find_range_keys(btree, index, prefix_key, true, prefix_key, true, result);
}

/* Find B+ Tree Range of Keys 
 *
 * start_search_key == NULL -> unbounded lower search
 * end_search_key == NULL -> unbounded upper search
 * 
*/
BTreeStatus btree_find_range_keys(BTree *btree, BTreeIndexSpec *index, BTreeSearchKey *start_search_key, 
    bool includes_start, BTreeSearchKey *end_search_key, bool includes_end, BTreeRangeResult *result) {

    // Validate inputs
    if (!btree || !btree->pager || !index || !result) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (btree->root_page_num == INVALID_ROOT_PAGE ||
        btree->root_page_num >= btree->pager->num_pages) {
        return BTREE_INVALID_ARGUMENTS;
    }

    if (start_search_key) {
        if (!start_search_key->index || start_search_key->index != index ||
            !start_search_key->target_key) {
            return BTREE_INVALID_ARGUMENTS;
        }
    }
    

    if (end_search_key) {
        if (!end_search_key->index || end_search_key->index != index ||
            !end_search_key->target_key) {
            return BTREE_INVALID_ARGUMENTS;
        }
    }


    // Allocating Range Result structure using a helper
    BTreeStatus status = btree_range_result_init(result);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    Page *starting_page = NULL;
    uint16_t starting_index_pos = 0;

    // Search for the starting leaf page where the range result entries should begin
    if (start_search_key) {
        BTreeSearchResult start_result = {0};

        BTreeStatus status = btree_root_to_leaf(btree, start_search_key, &start_result, BTREE_LOWER_BOUND);
        if (status != BTREE_SUCCESS) {
            btree_range_result_free(result);
            return status;
        }

        // Found the starting page, and starting cell's index position
        starting_page = start_result.page;
        starting_index_pos = start_result.result_index;
    }
    else {
        // If there's no lower bound in the range query, start from the leftmost leaf page
        // and the first cell contents entry there
        status = btree_find_leftmost_page(btree, index, &starting_page);
        if (status != BTREE_SUCCESS) {
            btree_range_result_free(result);
            return status;
        }

        starting_index_pos = 0;
    }

    
    Page *page = starting_page;
    uint16_t index_pos = starting_index_pos;


    BTreePageCollection visited_pages = {0};

    // Main loop where matching entries are appended in the BTreeRangeResult structure
    while (true) {

        // Detect sibling cycles (e.g., leaf 8 -> leaf 9 -> leaf 8) or duplicate leaf references
        if (btree_collection_contains(&visited_pages, page->page_num)) {
            btree_range_result_free(result);
            return BTREE_CORRUPT_PAGE;
        }

        if (visited_pages.count >= MAX_PAGES) {
            btree_range_result_free(result);
            return BTREE_CORRUPT_PAGE;
        }

        visited_pages.page_numbers[visited_pages.count] = page->page_num;
        visited_pages.count++;


        // Loading current page to a BTreePage structure
        BTreePage btree_page = {0};
        
        status = btree_page_attach_load_validate(btree->pager, &btree_page, page, index);
        if (status != BTREE_SUCCESS) {
            btree_range_result_free(result);
            return status;
        }

        if (btree_page.type != BTREE_LEAF_NODE) {
            btree_range_result_free(result);
            return BTREE_CORRUPT_PAGE;
        }
        
        // Traversing the rest of the cells for the current page
        while (index_pos < btree_page.cell_count) {
            BTreeCellView cell_view = {0};

            status = get_cell(&btree_page, index_pos, &cell_view, index);
            if (status != BTREE_SUCCESS) {
                btree_range_result_free(result);
                return status;
            }

            // Creating a Value ** structure from the cell view's key 
            Value **values = serialized_key_to_values(cell_view.key.key, index->index_key->num_columns, index);
            if (!values) {
                btree_range_result_free(result);
                return BTREE_ERROR;
            }

            // Compare current Key with the lower bound search Key
            if (start_search_key) {
                int cmp_start = 0;

                Value **start_search_key_vals = serialized_key_to_values(start_search_key->target_key,
                                            start_search_key->num_target_keys, start_search_key->index);
                if (!start_search_key_vals) {
                    value_free_array(values, index->index_key->num_columns);
                    btree_range_result_free(result);
                    return BTREE_ERROR;
                }

                status = btree_compare(values, start_search_key_vals, start_search_key->num_target_keys, &cmp_start);
                if (status != BTREE_SUCCESS) {
                    value_free_array(start_search_key_vals, start_search_key->num_target_keys);
                    value_free_array(values, index->index_key->num_columns);
                    btree_range_result_free(result);
                    return status;
                }
                value_free_array(start_search_key_vals, start_search_key->num_target_keys);

                // If current key is less than the lower bound key, 
                // or, equal to lower bound key but lower bound is not included, skip it
                if (cmp_start < 0 || (cmp_start == 0 && !includes_start)) {
                    value_free_array(values, index->index_key->num_columns);
                    index_pos++;
                    continue;
                }
            }

            // Compare current Key with the upper bound search Key
            if (end_search_key) {
                int cmp_end = 0;

                Value **end_search_key_vals = serialized_key_to_values(end_search_key->target_key,
                                            end_search_key->num_target_keys, end_search_key->index);
                if (!end_search_key_vals) {
                    value_free_array(values, index->index_key->num_columns);
                    btree_range_result_free(result);
                    return BTREE_ERROR;
                }

                status = btree_compare(values, end_search_key_vals, end_search_key->num_target_keys, &cmp_end);
                if (status != BTREE_SUCCESS) {
                    value_free_array(end_search_key_vals, end_search_key->num_target_keys);
                    value_free_array(values, index->index_key->num_columns);
                    btree_range_result_free(result);
                    return status;
                }
                value_free_array(end_search_key_vals, end_search_key->num_target_keys);

                // If current key is greater than the upper bound key, 
                // or, equal to upper bound key but upper bound is not included, the range query is complete
                if (cmp_end > 0 || (cmp_end == 0 && !includes_end)) {
                    value_free_array(values, index->index_key->num_columns);
                    return BTREE_SUCCESS;
                }
            }

            value_free_array(values, index->index_key->num_columns);

            // Deserialize cell contents entry
            BTreeCellContents cell = {0};

            if (!deserialize_cell_contents(
                    index->schema, 
                    btree_page.data, 
                    &btree_page, 
                    &cell_view, 
                    &cell, 
                    index)) {
                btree_range_result_free(result);
                return BTREE_CORRUPT_PAGE;                  
            }
            
            // Add current matching entry to the range results' structure
            status = btree_range_result_append(result, &cell);

            if (status != BTREE_SUCCESS) {
                btree_cell_contents_free(&cell);
                btree_range_result_free(result);
                return status;
            }

            index_pos++;
        }
        
        uint32_t next_page_num = btree_page.type_specific_data.siblings.next_leaf_pointer;

        // The current page was the rightmost leaf page, and the range query is successful
        if (next_page_num == UINT32_MAX) {
            return BTREE_SUCCESS;
        }

        // Invalid next leaf node/page
        if (next_page_num <= SYSTEM_CATALOG_PAGE_NUM 
            || next_page_num >= btree->pager->num_pages
            || next_page_num >= MAX_PAGES) {
            btree_range_result_free(result);
            return BTREE_CORRUPT_PAGE;
        }

        // Retrieving the next leaf page
        page = pager_get_page(btree->pager, next_page_num);

        if (!page) {
            btree_range_result_free(result);
            return BTREE_ERROR;
        }

        // Resetting cell index position
        index_pos = 0;
    }

}

/* ---- B+Tree handling/orchestration ---- */

/* BTree Insertion Orchestration function.
 * Calls split propagation if inserting into leaf node
 * returns BTREE_NEEDS_SPLIT.
 *
 * Returns BTreeInsertionResult which contains valid modifications
 * only if insertion and splitted flags are true. Otherwise, ignore.
 * (NOTE: Rollback need to be implemented in case of failure)*/
BTreeStatus btree_insert(BTree *btree, BTreeCellContents *cell_contents, BTreeInsertionResult *insertion_res, BTreeIndexSpec *index) {
    if (!btree || btree->root_page_num <= SYSTEM_CATALOG_PAGE_NUM 
        || btree->root_page_num >= MAX_PAGES || !btree->pager
        || !cell_contents  || !insertion_res || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }
    insertion_result_reset(insertion_res);

    /* Create search key and search result for root-to-leaf traversal. */
    BTreeSearchResult search_result = {0};
    BTreeSearchKey search_key = {0};
    search_key.index = index;
    search_key.num_target_keys = cell_contents->num_keys;
    search_key.target_key = values_to_serialized_key(cell_contents->keys, search_key.num_target_keys, index);
    if (!search_key.target_key) {
        return BTREE_ERROR;
    }

    BTreeStatus status = btree_root_to_leaf(btree, &search_key, &search_result, BTREE_UPPER_BOUND); 
    if (status != BTREE_SUCCESS) {
        free(search_key.target_key);
        return status;
    }
    free(search_key.target_key);
    search_key.target_key = NULL;

    BTreePage leaf_node = {0};
    status = btree_page_attach_load_validate(btree->pager, &leaf_node, search_result.page, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    BTreeSplitResult split_result = {0};
    status = btree_node_insert(btree->pager, &leaf_node, cell_contents, &split_result, index);
    if (status != BTREE_SUCCESS && status != BTREE_NEEDS_SPLIT) {
        return status;
    }

    if (status == BTREE_NEEDS_SPLIT) {
        status = btree_split_propagation(btree, &leaf_node, cell_contents, &split_result, index, insertion_res);
        if (status != BTREE_SUCCESS) {
            insertion_result_reset(insertion_res);
            return status;
        }

        insertion_res->splitted = true; // Only if split propagation returned successfully
    } else {
        insertion_res->insertion_page_num = leaf_node.page->page_num; // If it was inserted immediately
    }

    insertion_res->inserted = true; // Only if insertion/split propagation returned successfully
    return BTREE_SUCCESS;
}

/* BTree Split Propagation function.
 *
 * Begins split propagation from when trying to insert
 * on a leaf node that's full and returns BTREE_NEEDS_SPLIT.
 * Receive BTreeSplitResult from leaf node split and update
 * parent nodes upwards.*/
BTreeStatus btree_split_propagation(BTree *btree, BTreePage *leaf_node, BTreeCellContents *pending_leaf_cell,
    BTreeSplitResult *split_result, BTreeIndexSpec *index, BTreeInsertionResult *insertion_res) {
    if (!btree || btree->root_page_num <= SYSTEM_CATALOG_PAGE_NUM 
        || btree->root_page_num >= MAX_PAGES || !btree->pager
        || !leaf_node || !leaf_node->page || !leaf_node->data 
        || !pending_leaf_cell ||!split_result || !index || !insertion_res) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Check if we actually intended on inserting a node to an already
     * full LEAF NODE page and it needed to split. */
    if (leaf_node->type != BTREE_LEAF_NODE || !split_result->split) {
        return BTREE_INVALID_ARGUMENTS;
    }

    // Actual variable continuing the loop since split_result->split
    // resets/changes in every insertion/split. 
    bool keep_propagating = split_result->split; 

    split_result_reset(split_result);
    BTreeStatus status = btree_leaf_node_split(btree->pager, leaf_node, index, split_result);
    if (status != BTREE_SUCCESS) {
        return status;
    }
    insertion_res->split_levels++;

    Page *insertion_page = NULL;
    status = choose_split_insertion_page(btree->pager, &insertion_page, pending_leaf_cell, split_result, index);
    if (status != BTREE_SUCCESS) {
        split_result_reset(split_result);
        return status;
    }

    BTreePage btree_insertion_page = {0};
    status = btree_page_attach_load_validate(btree->pager, &btree_insertion_page, insertion_page, index);
    if (status != BTREE_SUCCESS) {
        split_result_reset(split_result);
        return status;
    }
    
    status = btree_node_insert(btree->pager, &btree_insertion_page, pending_leaf_cell, split_result, index);
    if (status != BTREE_SUCCESS) {
        split_result_reset(split_result);
        return status;
    }
    insertion_res->insertion_page_num = insertion_page->page_num;

    if (leaf_node->is_root) {
        status = btree_root_split(btree, leaf_node, split_result, index);

        if (status != BTREE_SUCCESS) {
            split_result_reset(split_result);
        }
        
        return status;
    }

    BTreePage btree_parent = {0};

    /* While nodes keep splitting. */
    while (keep_propagating) {
        
        /* Use new split result fields each time split continues. */
        Page *left_page = pager_get_page(btree->pager, split_result->left_page);
        if (!left_page) {
            split_result_reset(split_result);
            return BTREE_ERROR;
        }

        BTreePage btree_left_page = {0};
        status = btree_page_attach_load_validate(btree->pager, &btree_left_page, left_page, index);
        if (status != BTREE_SUCCESS) {
            split_result_reset(split_result);
            return status;
        }

        Page *parent = pager_get_page(btree->pager, btree_left_page.parent_pointer);
        if (!parent) {
            split_result_reset(split_result);
            return BTREE_ERROR;
        }

        status = btree_page_attach_load_validate(btree->pager, &btree_parent, parent, index);
        if (status != BTREE_SUCCESS) {
            split_result_reset(split_result);
            return status;
        }

        if (btree_parent.type != BTREE_INTERNAL_NODE) {
            split_result_reset(split_result);
            return BTREE_CORRUPT_PAGE;
        }

        /* Same structure of cell contents for internal/root node splitting. */
        BTreeCellContents cell_contents = {0};
        status = build_internal_separator_cell(&cell_contents, split_result, index);
        if (status != BTREE_SUCCESS) {
            split_result_reset(split_result);
            return status;
        }

        status = btree_page_has_enough_space(&btree_parent, &cell_contents);
        if (status != BTREE_SUCCESS && status != BTREE_NEEDS_SPLIT) {
            split_result_reset(split_result);
            value_free_array(cell_contents.keys, index->index_key->num_columns);
            return status;
        }
        
        if (status == BTREE_SUCCESS) {
            status = prepare_propagated_separator_cell(
                &btree_parent, &cell_contents, split_result->left_page,
                split_result->right_page, index);

            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);  
                return status;
            }
        }

        /* Store left and right page nums if parent splits again. */
        uint32_t pending_left_page = split_result->left_page;
        uint32_t pending_right_page = split_result->right_page;

        /* Insert split cell contents to the parent above. */
        split_result_reset(split_result); // Reset split result before re-using it
        status = btree_node_insert(btree->pager, &btree_parent, &cell_contents, split_result, index);
        if (status != BTREE_SUCCESS && status != BTREE_NEEDS_SPLIT) {
            split_result_reset(split_result);
            value_free_array(cell_contents.keys, index->index_key->num_columns);
            return status;
        }
        
        if (status == BTREE_SUCCESS) {
            keep_propagating = false;
        } 

        /* If parent doesn't have enough space and needs to split:
         * Else parent received pending separator from leaf node and loop stops. */
        if (keep_propagating) {
            status = btree_internal_node_split(btree->pager, &btree_parent, index, split_result);        
            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return status;
            }
            insertion_res->split_levels++;
            
            Page *insertion_page = NULL;
            status = choose_split_insertion_page(btree->pager, &insertion_page, &cell_contents,
                                                split_result, index);
            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return status;
            }
        
            BTreePage btree_insertion_page = {0};
            status = btree_page_attach_load_validate(btree->pager, &btree_insertion_page, insertion_page, index);
            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return status;
            }

            status = btree_page_has_enough_space(&btree_insertion_page, &cell_contents);
            if (status == BTREE_NEEDS_SPLIT) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return BTREE_ERROR;
            }

            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return status;
            }

            status = prepare_propagated_separator_cell(&btree_insertion_page, &cell_contents, pending_left_page,
                                                        pending_right_page, index);
            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return status;
            }

            status = btree_node_insert(btree->pager, &btree_insertion_page, &cell_contents, split_result, index);
            if (status != BTREE_SUCCESS) {
                split_result_reset(split_result);
                value_free_array(cell_contents.keys, index->index_key->num_columns);
                return status;
            }

            if (btree_parent.is_root) {
                status = btree_root_split(btree, &btree_parent, split_result, index);
                value_free_array(cell_contents.keys, index->index_key->num_columns);

                if (status != BTREE_SUCCESS) {
                    split_result_reset(split_result);
                    return status;
                }

                break;
            }
        }
        
        value_free_array(cell_contents.keys, index->index_key->num_columns);
    } 

    return BTREE_SUCCESS;
}