#include <stdio.h>
#include <stdlib.h>
#include "../../include/catalog.h"
#include "../../include/btree.h"
#include "../../include/data_types.h"
#include "../src/data_types/data_types_utils.h"
#include "../../include/pager.h"
#include "../../include/serialize.h"
#include "../../include/page.h"
#include "../src/btree/btree_utils.h"
#include "../../include/schema.h"
#include "../../include/index.h"
#include "catalog_utils.h"

/* Create Catalog struct and create System Catalog Root Page
 * through catalog_initialize().
 *
 * Initialize Catalog-specific BTreeIndexSpec with pseudo-contents.
 * (Pseudo-index key, pseudo-column_types and pseudo-schema) */
Catalog *catalog_create(Pager *pager, uint32_t *root_page_num) {
    if (!pager || !root_page_num) {
        return NULL;
    }

    Catalog *catalog = (Catalog *) calloc(1, sizeof(Catalog));
    if (!catalog) {
        goto cleanup;
    }

    catalog->btree = (BTree *) calloc(1, sizeof(BTree));
    if (!catalog->btree) {
        goto cleanup;
    }

    catalog->btree->pager = pager; 
    catalog->btree->root_page_num = UINT32_MAX;

    // Pseudo-index key referencing schema below to allow for B+Tree Operations
    uint32_t num_columns = 3;
    uint32_t column_indexes[3] = {0, 1, 2};
    catalog->spec.index_key = index_key_create(column_indexes, num_columns);
    if (!catalog->spec.index_key) {
        goto cleanup;
    }

    Column *columns[3] = {0};
    columns[0] = column_alloc("table_name", CHAR, 64, 0, 0);
    if (!columns[0]) { goto cleanup; }
    columns[1] = column_alloc("object_type", UNSIGNED_INTEGER, 0, 0, 0);
    if (!columns[1]) { goto cleanup; }
    columns[2] = column_alloc("object_name", CHAR, 64, 0, 0);
    if (!columns[2]) { goto cleanup; }

    // Pseudo-schema to allow for B+Tree Operations
    catalog->spec.schema = schema_create(columns, NULL, 3, 0);
    if (!catalog->spec.schema) {
        goto cleanup;
    }
    for (uint32_t i = 0; i < 3; i++) { free(columns[i]); }

    catalog->spec.is_unique = true;
    catalog->spec.key_size = CATALOG_KEY_SIZE;
    catalog->spec.payload_type = BTREE_CATALOG_PAYLOAD;
    catalog->spec.column_types = (DataType *) calloc(3, sizeof(DataType));
    if (!catalog->spec.column_types) {
        goto cleanup;
    }
    catalog->spec.column_types[0] = CHAR;
    catalog->spec.column_types[1] = UNSIGNED_INTEGER;
    catalog->spec.column_types[2] = CHAR;

    CatalogStatus status = catalog_initialize(catalog, root_page_num);
    if (status != CATALOG_SUCCESS) {
        goto cleanup;
    }

    return catalog;

cleanup:
    for (uint32_t i = 0; i < 3; i++) {
        if (columns[i]) { free(columns[i]); }
    }
    if (catalog->spec.index_key) { index_key_free(catalog->spec.index_key); }
    if (catalog->spec.column_types) { free(catalog->spec.column_types); }
    if (catalog->spec.schema) { schema_free(catalog->spec.schema); }

    free(catalog->btree);
    free(catalog);
    catalog->spec.column_types = NULL;
    catalog->spec.index_key = NULL;
    catalog->spec.schema = NULL;
    catalog->spec.is_unique = false;
    catalog->spec.key_size = 0;
    catalog->spec.payload_type = 0;
    catalog->btree = NULL;
    catalog = NULL;


    if (*root_page_num < MAX_PAGES
        || *root_page_num < pager->num_pages
        || *root_page_num != SUPERBLOCK_PAGE_NUM 
        || pager->pages[*root_page_num]) {
        bool res = pager_release_page(pager, *root_page_num);
    }
    return NULL;
}

/* Create empty System Catalog leaf Root Page and update 
 * Catalog's BTree root_page_num metadata. */
CatalogStatus catalog_initialize(const Catalog *catalog, uint32_t *root_page_num) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!pager_allocate_page(catalog->btree->pager, root_page_num)) {
        return CATALOG_ERROR;
    }

    Page *catalog_root_page = pager_get_page(catalog->btree->pager, *root_page_num);
    if (!catalog_root_page) {
        return BTREE_ERROR;
    }

    BTreePage btree_catalog_page = {0};
    btree_page_attach(&btree_catalog_page, catalog_root_page);
    BTreeStatus status = btree_page_init_empty_leaf(&btree_catalog_page);
    if (status != BTREE_SUCCESS) {
        return btree_to_catalog_status(status);
    }

    catalog->btree->root_page_num = *root_page_num;
    return CATALOG_SUCCESS;
}

/* Create Catalog Leaf Node Cell.
 *
 * Initialize metadata, allocate CatalogPayload, create
 * and persist all metadata pages needed to store Table's/Index's
 * metadata. */
CatalogStatus catalog_create_record(const Catalog *catalog, CatalogRecordInfo *record_info, BTreeCellContents *cell) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!cell) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    cell->type = BTREE_LEAF_NODE;
    cell->num_keys = CATALOG_KEY_COUNT;
    cell->key_size = CATALOG_KEY_SIZE;
    cell->cell_size = CATALOG_CELL_SIZE;

    if (!create_catalog_key(record_info, &cell->keys)) {
        btree_cell_contents_free(cell, &(catalog->spec));
        cell = NULL;
        return CATALOG_ERROR;
    }
    
    cell->BTreePayload.catalog = (Catalog *) calloc(1, sizeof(CatalogPayload));
    if (!cell->BTreePayload.catalog) {
        btree_cell_contents_free(cell, &(catalog->spec));
        cell = NULL;
        return CATALOG_ERROR;
    }

    uint32_t object_type = (CatalogEntryType) record_info->type;
    cell->BTreePayload.catalog->type = object_type;
    cell->BTreePayload.catalog->root_page_num = record_info->root_page_num;
    
    uint32_t metadata_page_num = 0;
    CatalogStatus status = catalog_create_metadata_pages(catalog, record_info, &metadata_page_num);
    if (status != CATALOG_SUCCESS) {
        btree_cell_contents_free(cell, &(catalog->spec));
        cell = NULL;
        return status;
    }

    cell->BTreePayload.catalog->metadata_page_num = metadata_page_num;
    status = catalog_persist_metadata_pages(catalog, record_info, metadata_page_num);
    if (status != CATALOG_SUCCESS) {
        status = catalog_release_metadata_pages(catalog, cell->BTreePayload.catalog->metadata_page_num);
        if (status == CATALOG_SUCCESS) {
            status = CATALOG_ERROR;
        }
        
        btree_cell_contents_free(cell, &(catalog->spec));
        cell = NULL;
        return status;
    }

    return CATALOG_SUCCESS;
}

/* Allocate as many metadata pages as needed to store all
 * Table's/Index's metadata.
 *
 * Metadata Pages are linked together in a linked-list fashion.
 * Last metadata page has a next_page_num equal to zero. */
CatalogStatus catalog_create_metadata_pages(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t *metadata_page_num) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!metadata_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    size_t serialized_size = get_catalog_payload_serialized_size(record_info);
    if (!serialized_size) {
        return CATALOG_ERROR;
    }
    
    if (!pager_allocate_page(catalog->btree->pager, metadata_page_num)) {
        return CATALOG_ERROR;
    }

    CatalogMetadataPages metadata_pages = {0};

    Page *curr = pager_get_page(catalog->btree->pager, *metadata_page_num);
    if (!curr) {
        return CATALOG_ERROR;
    }
    metadata_pages.pages[metadata_pages.num_pages] = curr;
    metadata_pages.num_pages++;

    // Ceiling rounding up of pages needed
    uint32_t pages_needed = (serialized_size + METADATA_PAGE_PAYLOAD_SIZE - 1) / METADATA_PAGE_PAYLOAD_SIZE;
    Page *prev = curr;
    uint8_t *write = prev->page_data;
    uint32_t page_num = 0;

    // Allocate new pages and connect them to the previous
    while (pages_needed > 1) {
        
        if (!pager_allocate_page(catalog->btree->pager, &page_num)) {
            goto page_cleanup;
        }

        curr = pager_get_page(catalog->btree->pager, page_num);
        if (!curr) {
            goto page_cleanup;
        }
        metadata_pages.pages[metadata_pages.num_pages] = curr;
        metadata_pages.num_pages++;

        write = prev->page_data;
        memcpy(write, &page_num, sizeof(uint32_t));

        prev = curr;
        pages_needed--;
    }

    page_num = 0;
    write = curr->page_data;
    memcpy(write, &page_num, sizeof(uint32_t));
    
    return CATALOG_SUCCESS;

page_cleanup: // Safe cleanup without calling catalog_release_metadata_pages
    for (uint32_t i = 0; i < metadata_pages.num_pages; i++) {
        if (metadata_pages.pages[i]) {
            bool res = pager_release_page(catalog->btree->pager, metadata_pages.pages[i]->page_num);
            // Don't check result, release as many pages as possible
        }
    }

    return CATALOG_ERROR;
}

/* Store all Table's/Index's metadata throughout all pages created. */
CatalogStatus catalog_persist_metadata_pages(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t metadata_page_num) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!metadata_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    Page *metadata_page = pager_get_page(catalog->btree->pager, metadata_page_num);
    if (!metadata_page) {
        return CATALOG_ERROR;
    }

    size_t serialized_size = get_catalog_payload_serialized_size(record_info);
    if (!serialized_size) {
        return CATALOG_ERROR;
    }
    
    /* Write all of the payload onto a buffer. */
    uint8_t *buffer = calloc(serialized_size, sizeof(uint8_t));
    if (!buffer) {
        return CATALOG_ERROR;
    }
    uint8_t *buffer_write = buffer;
    uint8_t *buffer_read = buffer;

    if (!persist_catalog_payload(record_info, &buffer_write)) {
        free(buffer);
        return CATALOG_ERROR;
    }

    uint32_t remaining_size = serialized_size;
    uint32_t bytes_to_write = remaining_size > METADATA_PAGE_PAYLOAD_SIZE ? METADATA_PAGE_PAYLOAD_SIZE : remaining_size;

    Page *curr = metadata_page;
    uint8_t *curr_write = curr->page_data + sizeof(uint32_t);

    memcpy(curr_write, buffer_read, bytes_to_write);
    buffer_read += bytes_to_write;
    remaining_size -= bytes_to_write;

    /* And then distribute buffer contents to all metadata pages. */
    while (remaining_size > 0) {
        if (remaining_size < METADATA_PAGE_PAYLOAD_SIZE) {
            bytes_to_write = remaining_size;
        }

        uint32_t next_page_num = 0;
        memcpy(&next_page_num, curr->page_data, sizeof(uint32_t));

        if (next_page_num == SUPERBLOCK_PAGE_NUM) {
            free(buffer);
            return CATALOG_ERROR;
        }

        if (next_page_num >= catalog->btree->pager->num_pages
            || next_page_num >= MAX_PAGES) {
            free(buffer);
            return CATALOG_INVALID_ARGUMENTS;
        }

        curr = pager_get_page(catalog->btree->pager, next_page_num);
        if (!curr) {
            free(buffer);
            return CATALOG_ERROR;
        }
        curr_write = curr->page_data + sizeof(uint32_t);

        if (!remaining_size) {
            break;
        }

        memcpy(curr_write, buffer_read, bytes_to_write);
        buffer_read += bytes_to_write;
        remaining_size -= bytes_to_write;
    }

    free(buffer);
    return CATALOG_SUCCESS;
}

/* Reconstruct Table's/Index's metadata from Metadata Pages. */
CatalogStatus catalog_read_metadata_pages(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t metadata_page_num) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!metadata_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    // Create a buffer to store all metadata at once
    uint32_t serialized_size = get_catalog_payload_serialized_size(record_info);
    uint8_t *buffer = (uint8_t *) calloc(serialized_size, sizeof(uint8_t));
    if (!buffer) {
        return CATALOG_ERROR;
    }
    uint8_t *buffer_write = buffer;
    uint8_t *buffer_read = buffer;

    uint32_t page_num = metadata_page_num;
    uint32_t remaining_size = serialized_size;
    uint32_t bytes_to_write = (remaining_size < METADATA_PAGE_PAYLOAD_SIZE) ? 
                                    remaining_size : METADATA_PAGE_PAYLOAD_SIZE;

    CatalogMetadataPages metadata_pages = {0};
    do {
        if (page_num == SUPERBLOCK_PAGE_NUM) {
            free(buffer);
            return CATALOG_ERROR;
        }

        if (page_num >= catalog->btree->pager->num_pages
            || page_num >= MAX_PAGES
            || metadata_pages.num_pages >= MAX_PAGES) {
            free(buffer);
            return CATALOG_INVALID_ARGUMENTS;
        }

        // Check for circle connections
        if (is_page_in_metadata_pages(&metadata_pages, page_num)) {
            free(buffer);
            return CATALOG_ERROR;
        }

        if (remaining_size < METADATA_PAGE_PAYLOAD_SIZE) {
            bytes_to_write = remaining_size;
        }

        Page *page = pager_get_page(catalog->btree->pager, page_num);
        if (!page) {
            free(buffer);
            return CATALOG_ERROR;
        }
        metadata_pages.pages[metadata_pages.num_pages] = page;
        metadata_pages.num_pages++;

        memcpy(&page_num, page->page_data, sizeof(uint32_t));

        memcpy(buffer_write, page->page_data + sizeof(uint32_t), bytes_to_write);
        buffer_write += bytes_to_write;
        remaining_size -= bytes_to_write;
    } while (remaining_size > 0);

    // Then reconstruct Table/Index stored in CatalogRecordInfo
    // from buffer
    if (!read_catalog_payload(record_info, &buffer_read)) {
        free(buffer);
        return CATALOG_ERROR;
    }

    free(buffer);
    return CATALOG_SUCCESS;
}

/* Release all Metadata Pages from last to first. */
CatalogStatus catalog_release_metadata_pages(const Catalog *catalog, uint32_t metadata_page_num) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!metadata_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    CatalogMetadataPages metadata_pages = {0}; // Rollback safe method, LIFO
    uint32_t page_num = metadata_page_num;
    while (page_num != 0 && metadata_pages.num_pages < MAX_PAGES) {

        if (page_num >= catalog->btree->pager->num_pages
            || page_num >= MAX_PAGES
            || metadata_pages.num_pages >= MAX_PAGES) {
            return CATALOG_INVALID_ARGUMENTS;
        }

        if (is_page_in_metadata_pages(&metadata_pages, page_num)) {
            return CATALOG_ERROR;
        }

        Page *page = pager_get_page(catalog->btree->pager, page_num);
        if (!page) {
            return CATALOG_ERROR;
        }
        memcpy(&page_num, page->page_data, sizeof(uint32_t));

        metadata_pages.pages[metadata_pages.num_pages] = page;
        metadata_pages.num_pages++;
    }

    while (metadata_pages.num_pages > 0) {
        if (!pager_release_page(catalog->btree->pager, metadata_pages.pages[metadata_pages.num_pages-1]->page_num)) {
            return CATALOG_ERROR;
        }

        metadata_pages.num_pages--;
    }

    return CATALOG_SUCCESS;
}

/* ---------- CATALOG ORCHESTRATION ---------- */

/* Insert record into System Catalog BTree.
 *
 * (NOTE: Non-atomic behavior) */
CatalogStatus catalog_insert_record(const Catalog *catalog, BTreeCellContents *cell) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!cell || cell->type != BTREE_LEAF_NODE
        || cell->num_keys != CATALOG_KEY_COUNT || !cell->keys
        || cell->key_size != CATALOG_KEY_SIZE || cell->cell_size != CATALOG_CELL_SIZE
        || !cell->BTreePayload.catalog || cell->BTreePayload.catalog->type > CATALOG_INDEX
        || !cell->BTreePayload.catalog->root_page_num || !cell->BTreePayload.catalog->metadata_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    BTreeInsertionResult insertion_res = {0};
    BTreeStatus status = btree_insert(catalog->btree, cell, &insertion_res, &(catalog->spec));

    return btree_to_catalog_status(status);
}

/* Delete record from System Catalog BTree.
 *
 * (NOTE: Non-atomic behavior) */
CatalogStatus catalog_delete_record(const Catalog *catalog, BTreeCellContents *cell) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!cell || cell->type != BTREE_LEAF_NODE
        || cell->num_keys != CATALOG_KEY_COUNT || !cell->keys
        || cell->key_size != CATALOG_KEY_SIZE || cell->cell_size != CATALOG_CELL_SIZE
        || !cell->BTreePayload.catalog || cell->BTreePayload.catalog->type > CATALOG_INDEX
        || !cell->BTreePayload.catalog->root_page_num || !cell->BTreePayload.catalog->metadata_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    // Store before deleting record
    uint32_t metadata_page_num = cell->BTreePayload.catalog->metadata_page_num;

    // Underflow handling will not occur since underflow checking operations are nested
    // inside an if that checks BTree Payload Type
    BTreeDeletionResult deletion_res = {0};
    BTreeStatus btree_status = btree_delete(catalog->btree, cell, &deletion_res, &(catalog->spec));
    if (btree_status != BTREE_SUCCESS) {
        return btree_to_catalog_status(btree_status);
    }

    CatalogStatus status = catalog_release_metadata_pages(catalog, metadata_page_num);
    if (status != CATALOG_SUCCESS) {
        return status;
    }

    return BTREE_SUCCESS;
}

/* Lookup record in System Catalog BTree. */
CatalogLookupStatus catalog_lookup_record(const Catalog *catalog, CatalogRecordInfo *record_info, CatalogLookupResult *lookup_result) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return CATALOG_LOOKUP_INVALID_ARGUMENTS;
    }

    if (!lookup_result) {
        return CATALOG_LOOKUP_INVALID_ARGUMENTS;
    }

    Value **key = NULL;
    if (!create_catalog_key(record_info, &key)) {
        return CATALOG_LOOKUP_ERROR;
    }

    BTreeSearchEntries search_entries = {0};
    BTreeSearchResult search_result = {0};
    BTreeSearchKey search_key = {0};
    search_key.index = &(catalog->spec);
    search_key.num_target_keys = CATALOG_KEY_COUNT;
    search_key.target_key = values_to_serialized_key(key, CATALOG_KEY_COUNT, &(catalog->spec));
    if (!search_key.target_key) {
        value_free_array(key, CATALOG_KEY_COUNT);
        key = NULL;
        return CATALOG_LOOKUP_ERROR;
    }

    BTreeStatus btree_status = btree_find_exact_key(catalog->btree, &search_key, &search_result, &search_entries, &(catalog->spec));
    value_free_array(key, CATALOG_KEY_COUNT);
    key = NULL;
    free(search_key.target_key);
    search_key.target_key = NULL;

    CatalogLookupStatus status = btree_to_catalog_lookup_status(btree_status);
    if (status != CATALOG_LOOKUP_SUCCESS) {
        btree_search_entries_free(&search_entries, &(catalog->spec));
        return status;
    }

    if (search_entries.count == 0) {
        btree_search_entries_free(&search_entries, &(catalog->spec));
        return CATALOG_LOOKUP_NOT_FOUND;
    }

    // Catalog B+Tree is unique by default
    if (search_entries.count != 1) {
        btree_search_entries_free(&search_entries, &(catalog->spec));
        return CATALOG_LOOKUP_ERROR;
    }

    lookup_result->cell = btree_cell_contents_copy(catalog->btree->pager, &(search_entries.entries[0].cell), &(catalog->spec));
    if (!lookup_result->cell) {
        btree_search_entries_free(&search_entries, &(catalog->spec));
        return CATALOG_LOOKUP_ERROR;
    }

    lookup_result->page_num = search_entries.entries->page_num;
    btree_search_entries_free(&search_entries, &(catalog->spec));
    return CATALOG_LOOKUP_SUCCESS;
}

/* Update record in System Catalog BTree.
 *
 * (NOTE: Non-atomic behavior) */
CatalogStatus catalog_update_record(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t new_root_page_num) {
    if (!catalog || !catalog->btree 
        || !catalog->btree->pager || !catalog->btree->root_page_num
        || !catalog->spec.schema || catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || catalog->spec.key_size != CATALOG_KEY_SIZE || !catalog->spec.is_unique 
        || !catalog->spec.index_key || !catalog->spec.index_key->column_index_array
        || !catalog->spec.index_key->num_columns || !catalog->spec.column_types) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    if (!new_root_page_num) {
        return CATALOG_INVALID_ARGUMENTS;
    }

    CatalogLookupResult lookup_result = {0};
    CatalogLookupStatus status = catalog_lookup_record(catalog, record_info, &lookup_result);
    if (status != CATALOG_LOOKUP_SUCCESS) {
        if (lookup_result.cell) { btree_cell_contents_free(lookup_result.cell, &catalog->spec); }
        return status;
    }

    // Copy cell since its going to be deleted with catalog_delete_record()
    BTreeCellContents *cell = btree_cell_contents_copy(catalog->btree->pager, lookup_result.cell, &(catalog->spec));
    if (!cell) {
        btree_cell_contents_free(lookup_result.cell, &catalog->spec);
        return CATALOG_ERROR;
    }

    CatalogMetadataPages metadata_pages = {0};

    // Visit all metadata pages
    if (!visit_metadata_pages(catalog->btree->pager, cell->BTreePayload.catalog->metadata_page_num, &metadata_pages)) {
        btree_cell_contents_free(lookup_result.cell, &catalog->spec);
        btree_cell_contents_free(cell, &(catalog->spec));
        return CATALOG_ERROR;
    }

    // Copy all metadata pages since they are going to be released with catalog_delete_record
    if (!copy_metadata_pages(catalog->btree->pager, &metadata_pages, &(cell->BTreePayload.catalog->metadata_page_num))) {
        btree_cell_contents_free(lookup_result.cell, &catalog->spec);
        btree_cell_contents_free(cell, &(catalog->spec));
        return CATALOG_ERROR;
    }
    cell->BTreePayload.catalog->root_page_num = new_root_page_num; // Update root_page_num

    status = catalog_delete_record(catalog, lookup_result.cell);
    if (status != CATALOG_SUCCESS) {
        if (lookup_result.cell) { btree_cell_contents_free(lookup_result.cell, &catalog->spec); }
        btree_cell_contents_free(cell, &(catalog->spec));
        return status;
    }
    btree_cell_contents_free(lookup_result.cell, &catalog->spec);


    status = catalog_insert_record(catalog, cell);
    if (status != CATALOG_SUCCESS) {
        btree_cell_contents_free(cell, &(catalog->spec));
        return status;
    }

    btree_cell_contents_free(cell, &(catalog->spec));
    return CATALOG_SUCCESS;
}