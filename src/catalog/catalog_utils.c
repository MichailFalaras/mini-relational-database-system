#include <stdio.h>
#include <stdlib.h>
#include "../../include/catalog.h"
#include "../../include/btree.h"
#include "../../include/data_types.h"
#include "../../include/pager.h"
#include "../../include/page.h"

/* Create Catalog Key from CatalogRecordInfo.
 *
 * table_name: CHAR(64)
 * object_type: UNSIGNED_INTEGER
 * object_name: CHAR(64). */
bool create_catalog_key(CatalogRecordInfo *record_info, Value ***key) {
    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num || !key) {
        return false;
    }

    *key = (Value **) calloc(CATALOG_KEY_COUNT, sizeof(Value *));
    if (!(*key)) {
        *key = NULL;
        return false;
    }

    char_n_t table_name = { .n = 64, .string = strdup(record_info->table_name) };
    if (!table_name.string) {
        value_free_array(*key, CATALOG_KEY_COUNT);
        *key = NULL;
        return CATALOG_ERROR;
    }

    *key[0] = value_create(CHAR, &table_name);
    free(table_name.string);
    if (!*key[0]) {
        value_free_array(*key, CATALOG_KEY_COUNT);
        *key = NULL;
        return CATALOG_ERROR;
    }

    uint32_t object_type = (uint32_t) record_info->type;
    *key[1] = value_create(UNSIGNED_INTEGER, &object_type);
    if (!*key[1]) {
        value_free_array(*key, CATALOG_KEY_COUNT);
        *key = NULL;
        return CATALOG_ERROR;
    }

    char_n_t object_name = { .n = 64, .string = strdup(record_info->object_name) };
    if (!object_name.string) {
        value_free_array(*key, CATALOG_KEY_COUNT);
        *key = NULL;
        return CATALOG_ERROR;
    }

    *key[2] = value_create(CHAR, &object_name);
    free(object_name.string);
    if (!*key[2]) {
        value_free_array(*key, CATALOG_KEY_COUNT);
        *key = NULL;
        return CATALOG_ERROR;
    }

    return true;
}

/* Returns Table's/Index's serialized metadata size. */
size_t get_catalog_payload_serialized_size(CatalogRecordInfo *record_info) {
    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return 0;
    }

    switch (record_info->type) {
        case CATALOG_TABLE:
            return serialized_table_metadata_size(record_info->object.table);
        case CATALOG_INDEX:
            return serialized_index_metadata_size(record_info->object.index);
        default:
            fprintf(stderr, "catalog_create_metadata_page: Catalog Record Info does not match.\n");
            return 0;
    }
}

/* Persist Catalog payload. (Table/Index metadata). */
bool persist_catalog_payload(CatalogRecordInfo *record_info, uint8_t **write_offset) {
    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return false;
    }

    if (!write_offset || !(*write_offset)) {
        return false;
    }

    switch (record_info->type) {
        case CATALOG_TABLE:
            if (!serialize_table_metadata(write_offset, record_info->object.table)) {
                return false;
            }
            break;
        case CATALOG_INDEX:
            if (!serialize_index_metadata(write_offset, record_info->object.index)) {
                return false;
            }
            break;
        default:
            fprintf(stderr, "catalog_persist_metadata_page: Catalog Entry Type does not match.\n");
            return false;
    }

    return true;
}

/* Deserialize Catalog Payload. (Table/Index metadata). */
bool read_catalog_payload(CatalogRecordInfo *record_info, uint8_t **read_offset) {
    if (!record_info || record_info->type > CATALOG_INDEX
        || !record_info->table_name || record_info->table_name[0] == '\0'
        || !record_info->object_name || !record_info->root_page_num) {
        return false;
    }

    if (!read_offset || !(*read_offset)) {
        return false;
    }

    switch (record_info->type) {
        case CATALOG_TABLE:
            if (!deserialize_table_metadata(read_offset, record_info->object.table)) {
                return false;
            }
            break;
        case CATALOG_INDEX:
            if (!deserialize_index_metadata(read_offset, record_info->object.index)) {
                return false;
            }
            break;
        default:
            fprintf(stderr, "catalog_persist_metadata_page: Catalog Entry Type does not match.\n");
            return false;
    }

    return true;
}

/* BTree → Catalog Status. */
CatalogStatus btree_to_catalog_status(BTreeStatus status) {
    switch (status) {
        case BTREE_SUCCESS:
            return CATALOG_SUCCESS;

        case BTREE_DUPLICATE_KEY:
            return CATALOG_DUPLICATE_KEY;

        case BTREE_NOT_FOUND:
            return CATALOG_NOT_FOUND;

        case BTREE_INVALID_ARGUMENTS:
            return CATALOG_INVALID_ARGUMENTS;

        case BTREE_ERROR:
        case BTREE_CORRUPT_PAGE:
        case BTREE_INVALID_PAGE:
        case BTREE_FREE_PAGE:
        case BTREE_NEEDS_SPLIT:
        case BTREE_NODE_UNDERFLOW:
        case BTREE_UNDERFLOW_UNRESOLVED:
        case BTREE_NEEDS_MERGE:
            return CATALOG_ERROR;

        default:
            return CATALOG_ERROR;
    }
}

/* BTree → Catalog Lookup Status. */
CatalogLookupStatus btree_to_catalog_lookup_status(BTreeStatus status) {
    switch (status) {
        case BTREE_SUCCESS:
            return CATALOG_LOOKUP_SUCCESS;

        case BTREE_NOT_FOUND:
            return CATALOG_LOOKUP_NOT_FOUND;

        case BTREE_INVALID_ARGUMENTS:
            return CATALOG_LOOKUP_INVALID_ARGUMENTS;

        default:
            return CATALOG_LOOKUP_ERROR;
    }
}

/* Check if page number is contained in CatalogMetadataPages. */
bool is_page_in_metadata_pages(CatalogMetadataPages *metadata_pages, uint32_t page_num) {
    if (!metadata_pages || !metadata_pages->pages 
        || !metadata_pages->num_pages || page_num >= MAX_PAGES) {
        return false;
    }

    for (uint32_t i = 0; i < metadata_pages->num_pages; i++) {
        if (!metadata_pages->pages[i]) {
            return false;
        }

        if (metadata_pages->pages[i]->page_num == page_num) {
            return true;
        } 
    }

    return false;
}

/* Visit a list of metadata pages and store them all in a CatalogMetadataPages struct. 
 *
 * (Infinite loops or out of bound page numbers are invalid and thrown away). */
bool visit_metadata_pages(Pager *pager, uint32_t metadata_page_num, CatalogMetadataPages *metadata_pages) {
    if (!metadata_page_num || !metadata_pages
        || !metadata_pages->pages) {
        return false;
    }

    Page *curr = pager_get_page(pager, metadata_page_num);
    if (!curr) {
        return false;
    }

    metadata_pages->pages[metadata_pages->num_pages] = curr;
    metadata_pages->num_pages++;

    uint32_t next_page_num = 0;
    memcpy(&next_page_num, curr->page_data, sizeof(uint32_t));

    while (next_page_num != 0) {
        if (next_page_num >= pager->num_pages
            || next_page_num >= MAX_PAGES
            || metadata_pages->num_pages >= MAX_PAGES) {
            goto page_cleanup;
        } 

        if (is_page_in_metadata_pages(metadata_pages, next_page_num)) {
            goto page_cleanup;
        }

        curr = pager_get_page(pager, next_page_num);
        if (!curr) {
            goto page_cleanup;
        }

        memcpy(&next_page_num, curr->page_data, sizeof(uint32_t));

        metadata_pages->pages[metadata_pages->num_pages] = curr;
        metadata_pages->num_pages++;
    }

    return true;

page_cleanup:
    for (uint32_t i = 0; i < metadata_pages->num_pages; i++) {
        metadata_pages->pages[i] = NULL;
    }

    return false;
}

/* Copy metadata pages and connect them together.
 * (Since copied pages are going to have different page numbers)
 *
 * Store them in CatalogMetadataPages struct. */
bool copy_metadata_pages(Pager *pager, CatalogMetadataPages *metadata_pages, uint32_t *new_page_num) {
    if (!pager || !metadata_pages || !metadata_pages->pages
        || !metadata_pages->num_pages || !new_page_num) {
        return false;
    }
    CatalogMetadataPages copies = {0};

    uint32_t current_page_idx = 0;
    Page *copy = page_copy(pager, metadata_pages->pages[current_page_idx]->page_num);
    if (!copy) {
        return false;
    }
    copies.pages[copies.num_pages] = copy;
    copies.num_pages++;

    uint8_t *prev_write = copy->page_data; 
    *new_page_num = copy->page_num; // New metadata page number

    current_page_idx++;
    while (current_page_idx < metadata_pages->num_pages) {
        copy = page_copy(pager, metadata_pages->pages[current_page_idx]->page_num);
        if (!copy) {
            goto copies_cleanup;
        }
        memcpy(prev_write, &copy->page_num, sizeof(uint32_t));

        prev_write = copy->page_data;
        current_page_idx++;
        
        copies.pages[copies.num_pages] = copy;
        copies.num_pages++;
    }

    return true;
copies_cleanup:
    for (uint32_t i = 0; i < copies.num_pages; i++) {
        if (copies.pages[i]) {
            bool res = pager_release_page(pager, copies.pages[i]->page_num);
            // Free as many as possible
        }
    }

    return false;
}

/* CatalogPayload metadata copy. */
CatalogPayload *catalog_payload_copy(Pager *pager, const CatalogPayload *payload) {
    if (!pager || !payload || !payload->metadata_page_num
        || !payload->root_page_num || payload->type > CATALOG_INDEX) {
        return NULL;
    }

    CatalogPayload *copy = (CatalogPayload *) calloc(1, sizeof(CatalogPayload));
    if (!copy) {
        return NULL;
    }

    copy->root_page_num = payload->root_page_num;
    copy->type = payload->type;
    copy->metadata_page_num = payload->metadata_page_num;

    return copy;
}
