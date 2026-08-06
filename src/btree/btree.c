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
BTreeStatus btree_lower_bound_search(BTreePage *btree_page, BTreeSearchKey *search_key,
    BTreeSearchResult *search_result) {
    if (!btree_page || !btree_page->page || !btree_page->data) {
        return BTREE_INVALID_ARGUMENTS;
    }
    BTreeStatus status = BTREE_ERROR;

    /* Initialize BTreeSearchResult. */
    search_result->found = true; // Only errors change this to false
    search_result->exact_match = false; // Only if exact match was found
    search_result->page = NULL; // This should be updated in root-to-leaf search
    search_result->result_index = btree_page->cell_count; // Initialize to cell count

    /* If there are no cells in the page, return index 0 as insertion position. */
    if (btree_page->cell_count == 0) {
        fprintf(stderr, "btree_lower_bound_search: Empty node.\n");

        search_result->found = true;
        search_result->exact_match = false;
        search_result->result_index = 0;

        return BTREE_SUCCESS;
    }

    /* Initialize binary search vars. */
    uint16_t from = 0;
    uint16_t to = btree_page->cell_count - 1;
    uint16_t mid = 0;

    BTreeKeyView btree_key = {0};
    Value **values = NULL;
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
        values = (Value **) calloc(search_key->num_target_keys, sizeof(Value *));
        uint8_t *key_offset = (uint8_t *) btree_key.key;
        for (uint16_t i = 0; i < search_key->num_target_keys; i++) {
            values[i] = value_create(search_key->index->column_types[i], key_offset);

            key_offset += get_data_type_size(search_key->index->column_types[i]);
        }

        /* Compare values. */
        status = btree_compare(values, &btree_key, search_key, &result);
        if (status != BTREE_SUCCESS) {
            search_result->found = false;
            search_result->exact_match = false;
            search_result->result_index = UINT16_MAX;
            value_free_array(values, search_key->num_target_keys);
            return status;
        }

        /* Evalute result: */
        if (result >= 0) {
            search_result->found = true;

            /* If result is 0, it means we have an exact match.
             * (Used to detect duplicates)*/
            if (!result) {
                search_result->exact_match = true;
            }

            search_result->result_index = mid;
            if (mid == 0) {
                value_free_array(values, search_key->num_target_keys);
                break;
            }
            to = mid - 1;
        } else {
            from = mid + 1;
        } 

        /* Free temporary values array in each loop. */
        value_free_array(values, search_key->num_target_keys);
    }

    return BTREE_SUCCESS;
}

/* Root to leaf traversal using a specific key to ultimately reach
 * a cell position to store data.
 * Also returns important information in BTreeSearchResult. */
BTreeStatus btree_root_to_leaf(BTree *btree, BTreeSearchKey *search_key, BTreeSearchResult *search_result) {
    if (!btree || !btree->pager
        || btree->root_page_num >= btree->pager->num_pages
        || btree->root_page_num == 0 || btree->root_page_num == 1
        || !search_key || !search_result) {
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
        return status;
    }

    uint32_t child_pointer = 0;
    /* While node isn't a Leaf Node. */
    while (btree_page.type != BTREE_LEAF_NODE) {

        /* Lower bound binary search to traverse throughout internal nodes. */
        status = btree_lower_bound_search(&btree_page, search_key, search_result);
        if (status != BTREE_SUCCESS) {
            pager_evict_page(btree->pager, page->page_num);
            return status;
        }

        /* Choose correct cell pointer index handling. */
        if (search_result->result_index == btree_page.cell_count) {
            child_pointer = btree_page.type_specific_data.rightmost_child_pointer;
        } else {
            uint32_t cell_pointer = get_cell_pointer(btree_page.data, search_result->result_index);
            child_pointer = get_cell_child_pointer(btree_page.data, cell_pointer);
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
    status = btree_lower_bound_search(&btree_page, search_key, search_result);
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

    /* Check if there's enough space, then update flag and return BTREE_NEEDS_SPLIT. */
    if (!btree_page_has_enough_space(btree_page, cell_contents)) {
        split_result->split = true;
        return BTREE_NEEDS_SPLIT;
    }

    /* Initialize temporary BTreeSearchKey & BtreeSearchResult structs. */
    BTreeSearchKey search_key = {0};
    search_key.num_target_keys = cell_contents->num_keys;
    search_key.target_key = (void *) cell_contents->keys;
    search_key.index = index;

    BTreeSearchResult search_result = {0};

    /* Find correct position to insert cell by searching with cell's keys.*/
    BTreeStatus status = btree_lower_bound_search(btree_page, &search_key, &search_result);
    if (status != BTREE_SUCCESS) {
        fprintf(stderr, "btree_leaf_node_insert: Something went wrong.\n");
        return status;
    }

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
    
    /* INTERNAL NODE SPECIFIC CHECK: 
     * If its an internal node and result index is equal to the cell count
     * then it means its the rightmost child pointer.
     * We swap the rightmost child pointer with the payload's child pointer. */
    } else if (btree_page->type == BTREE_INTERNAL_NODE && search_result.result_index == btree_page->cell_count) {
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
    btree_page_sync(pager, btree_page);
    return BTREE_SUCCESS;
}

/* BTree Leaf Node Split.
 * Returns important split information in BTreeSplitResult. */
BTreeStatus btree_leaf_node_split(Pager *pager, BTreePage *original_page, BTreeIndexSpec *index,
                                BTreeSplitResult *split_result) {
    if (!pager || !original_page || !original_page->page 
        || !original_page->data || !index || !split_result) {
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
        return BTREE_ERROR;
    }

    /* Initialize right child of split. */
    BTreePage btree_right_page = {0};
    btree_page_attach(&btree_right_page, new_page);
    BTreeStatus status = btree_page_init_empty_leaf(&btree_right_page);
    if (status != BTREE_SUCCESS) {
        return status;
    }
    btree_page_sync(pager, &btree_right_page);


    /* Split original page's cells in half and transfers them to the right child of the split. */
    status = btree_split_cells(original_page, &btree_right_page, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    /* Compact original page's cells to remove any garbage value left behind. */
    status = btree_compact_page(pager, original_page, index);
    if (status != BTREE_SUCCESS) {
        return status;
    }

    /* Connect original page's sibling pointers with the right child. */
    status = connect_sibling_leaf_nodes(pager, original_page, &btree_right_page);
    if (status != BTREE_SUCCESS) {
        return BTREE_ERROR;
    }
    
    // Sync with pages' page_data
    btree_page_sync(pager, original_page);
    btree_page_sync(pager, &btree_right_page);

    /* Extract separator key from right child and store it in BTreeSplitResult. */
    BTreeKeyView key = {0};
    status = get_key(&btree_right_page, 0, &key, index);
    if (status != BTREE_SUCCESS) {
        return BTREE_ERROR;
    }

    split_result->right_page = btree_right_page.page->page_num;
    split_result->separator_key = key.key;
    split_result->separator_size = key.key_size;
    split_result->split = false;
    return BTREE_SUCCESS;
}

/* BTree Root Node Split.
 * Split has already happened, this just creates new root, moves separator key
 * and updates pages' metadata.
 * Returns split information in BTreeSplitResult. */
BTreeStatus btree_root_split(BTree *btree, BTreePage *btree_old_root, BTreeSplitResult *split_result, BTreeIndexSpec *index) {
    if (!btree || !btree->pager || !btree_old_root || !btree_old_root->page
        || !btree_old_root->data || !split_result || !index) {
        return BTREE_INVALID_ARGUMENTS;
    }

    /* Allocate new root page. */
    uint32_t new_root_page_num = 0;
    if (!pager_allocate_page(btree->pager, &new_root_page_num)) {
        return false;
    }

    /* Get both pages in cache. */
    Page *new_root = pager_get_page(btree->pager, new_root_page_num);
    Page *right_child = pager_get_page(btree->pager, split_result->right_page);
    if (!new_root || !right_child) {
        return false;
    }

    /* Initialize temporary BTreePage structs. */
    // Create new root page
    BTreePage btree_new_root = {0};
    btree_page_attach(&btree_new_root, new_root);
    btree_page_init_internal(&btree_new_root, UINT32_MAX);
    btree_page_sync(btree->pager, &btree_new_root);

    // Create right child page
    BTreePage btree_right_child = {0};
    btree_page_attach_load_validate(btree->pager, &btree_right_child, right_child, index);

    /* Create cell that's going to be moved to the new root. */
    BTreeCellContents cell = {0};
    cell.type = BTREE_INTERNAL_NODE;
    cell.keys = split_result->separator_key;
    cell.key_size = split_result->separator_size;
    cell.num_keys = index->index_key->num_columns;
    cell.BTreePayload.child_pointer = btree_old_root->page->page_num;
    cell.cell_size = split_result->separator_size + sizeof(uint32_t);

    /* Insert separator cell into new root. */
    BTreeStatus status = btree_node_insert(btree->pager, &btree_new_root, &cell, split_result, index);
    if (status != BTREE_SUCCESS) {
        page_free(new_root);
        return status;
    }

    /* Remove separator cell from right child. */
    status = btree_remove_cell(&btree_right_child, 0, index);
    if (status != BTREE_SUCCESS) {
        page_free(new_root);
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
    btree_page_sync(btree->pager, btree_old_root);
    btree_page_sync(btree->pager, &btree_new_root);
    btree_page_sync(btree->pager, &btree_right_child);

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