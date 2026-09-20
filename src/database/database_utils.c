#include <stdio.h>
#include <stdlib.h>
#include "../../include/database.h"
#include "../../include/catalog.h"
#include "../src/catalog/catalog_utils.h"
#include "../../include/pager.h"
#include "../../include/page.h"
#include "../../include/table.h"
#include "../../include/index.h"
#include "../src/btree/btree_utils.h"
#include "../../include/serialize.h"

/* Superblock Page Validation. */
bool validate_superblock_page(Pager *pager, PageZeroMetadata *page_zero_metadata) {
    if (!pager || !pager->num_pages || !page_zero_metadata) {
        return false;
    }

    Page *superblock_page = pager_get_page(pager, SUPERBLOCK_PAGE_NUM);
    if (!superblock_page) {
        return false;
    }
    uint8_t *read_offset = superblock_page->page_data;

    if (!deserialize_page_zero_metadata(&read_offset, page_zero_metadata)) {
        return false;
    }

    if (strcmp(page_zero_metadata->magic, DB_MAGIC_STRING)) {
        return false;
    }

    if (page_zero_metadata->version != RDBMS_VERSION) {
        return false;
    }

    if (page_zero_metadata->page_size != PAGE_SIZE) {
        return false;
    }

    return true;
}

/* Reconstruct Database from System Catalog B+Tree & metadata pages.*/
bool reconstruct_system_catalog(Database *db, CatalogLookupResult *lookup_result) {
    if (!db || !db->pager || !db->tables) {
        return false;
    }

    if (!db->catalog || !db->catalog->btree 
        || !db->catalog->btree->pager || !db->catalog->btree->root_page_num
        || !db->catalog->spec.schema || db->catalog->spec.payload_type != BTREE_CATALOG_PAYLOAD
        || db->catalog->spec.key_size != CATALOG_KEY_SIZE || !db->catalog->spec.is_unique 
        || !db->catalog->spec.index_key || !db->catalog->spec.index_key->column_index_array
        || !db->catalog->spec.index_key->num_columns || !db->catalog->spec.column_types) {
        return false;
    }

    if (!lookup_result || !lookup_result->num_records || !lookup_result->records) {
        return false;
    }

    uint32_t num_index_records = 0;
    CatalogRecord **index_records = (CatalogRecord **) calloc(lookup_result->num_records, sizeof(CatalogRecord *));
    if (!index_records) {
        return false;
    }

    uint32_t old_table_count = db->table_count; // For cleanup purposes
    for (uint32_t i = 0; i < lookup_result->num_records; i++) {
        if (!lookup_result->records[i].cell
            || !lookup_result->records[i].cell->BTreePayload.catalog
            || !lookup_result->records[i].cell->BTreePayload.catalog->metadata_page_num
            || !lookup_result->records[i].cell->BTreePayload.catalog->root_page_num
            || lookup_result->records[i].cell->BTreePayload.catalog->type > CATALOG_INDEX) {
            goto cleanup;
        }

        if (lookup_result->records[i].cell->BTreePayload.catalog->type == CATALOG_INDEX) {
            index_records[num_index_records] = (CatalogRecord *) calloc(1, sizeof(CatalogRecord));
            if (!index_records[num_index_records]) {
                goto cleanup;
            }

            index_records[num_index_records]->cell = lookup_result->records[i].cell;
            num_index_records++;
            continue;
        }

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, lookup_result->records[i].cell->keys[0]->value.char_val.string, 64);
        record_info.table_name[63] = '\0';
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, lookup_result->records[i].cell->keys[2]->value.char_val.string, 64);
        record_info.object_name[63] = '\0';
        record_info.root_page_num = lookup_result->records[i].cell->BTreePayload.catalog->root_page_num;

        uint32_t metadata_page_num = lookup_result->records[i].cell->BTreePayload.catalog->metadata_page_num;
        CatalogStatus status = catalog_read_metadata_pages(db->catalog, lookup_result->records[i].cell, &record_info, metadata_page_num);
        if (status != CATALOG_SUCCESS) {
            goto cleanup;
        }
        
        if (!database_add_table(db, record_info.object.table)) {
            goto cleanup;
        }
    }

    for (uint32_t i = 0; i < num_index_records; i++) {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, index_records[i]->cell->keys[0]->value.char_val.string, 64);
        record_info.table_name[63] = '\0';
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, index_records[i]->cell->keys[2]->value.char_val.string, 64);
        record_info.object_name[63] = '\0';
        record_info.root_page_num = index_records[i]->cell->BTreePayload.catalog->root_page_num;

        uint32_t metadata_page_num = index_records[i]->cell->BTreePayload.catalog->metadata_page_num;
        CatalogStatus status = catalog_read_metadata_pages(db->catalog, index_records[i]->cell, &record_info, metadata_page_num);
        if (status != CATALOG_SUCCESS) {
            goto cleanup;
        }

        Index *index = record_info.object.index;
        char *table_name = index_records[i]->cell->keys[0]->value.char_val.string;

        Table *table = database_find_table(db, table_name);
        if (!table) {
            goto cleanup; // this shouldnt happen
        }

        // Appends both Primary (only one allowed) and Secondary Indexes
        if (!table_append_index(table, index)) {
            goto cleanup;
        }
    }


    for (uint32_t j = 0; j < num_index_records; j++) {
        if (index_records[j]) {
            if (index_records[j]->cell) {
                btree_cell_contents_free(index_records[j]->cell, &db->catalog->spec);
            }
            free(index_records[j]);
        }
    }
    free(index_records);


    return true;

cleanup:
    for (uint32_t j = 0; j < num_index_records; j++) {
        if (index_records[j]) {
            if (index_records[j]->cell) {
                btree_cell_contents_free(index_records[j]->cell, &db->catalog->spec);
            }
            free(index_records[j]);
        }
        index_records[j] = NULL;
    }
    free(index_records);

    for (uint32_t j = old_table_count; j < db->table_count; j++) {
        table_free(db->tables[j]);
        db->tables[j] = NULL;
    }
    db->table_count = old_table_count;

    return false;
}

/* Update metadata pages. Used in database_close(). */
bool update_metadata_pages(Database *db) {
    if (!db || !db->pager || !db->catalog || !db->tables) {
        return false;
    }

    CatalogMetadataPages visited = {0};
    CatalogMetadataPages table_metadata = {0};
    CatalogMetadataPages index_metadata = {0};
    CatalogLookupResult lookup_result = {0};

    // No tables at all => return true
    uint32_t table_idx = 0;
    for (; table_idx < db->table_count; table_idx++) {
        if (!db->tables[table_idx]) {
            continue;
        }

        CatalogRecordInfo record_info = {0};
        memcpy(record_info.table_name, db->tables[table_idx]->name, 64);
        record_info.type = CATALOG_TABLE;
        memset(record_info.object_name, 0, 64);
        record_info.object.table = db->tables[table_idx];

        CatalogLookupStatus lookup_status = catalog_lookup_record(db->catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) {
            goto restore_old_metadata;
        }

        if (lookup_result.num_records != 1) {
            goto restore_old_metadata;
        }
        
        record_info.root_page_num = lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;
        uint32_t metadata_page_num = lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;
        
        // Visit all metadata pages
        memset(&visited, 0, sizeof(visited));
        if (!visit_metadata_pages(db->pager, metadata_page_num, &visited)) {
            goto restore_old_metadata;
        }

        // Copy all of them and connect them together
        uint32_t new_metadata_page_num = 0;
        if (!copy_metadata_pages(db->pager, &visited, &new_metadata_page_num)) {
            goto restore_old_metadata;
        }

        // Keep only the first in table_metadata to restore later
        table_metadata.pages[table_metadata.num_pages] = db->pager->pages[new_metadata_page_num];
        table_metadata.num_pages++;

        CatalogStatus status = catalog_persist_metadata_pages(db->catalog, &record_info, metadata_page_num);
        if (status != CATALOG_SUCCESS) {
            goto restore_old_metadata;
        }

        if (!page_mark_dirty(db->pager->pages[metadata_page_num])) {
            goto restore_old_metadata;
        }
        
        if (db->tables[table_idx]->primary_index != NULL) {

            record_info.type = CATALOG_INDEX;
            strncpy(record_info.object_name, db->tables[table_idx]->primary_index->name, 64);
            record_info.object.index = db->tables[table_idx]->primary_index;

            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
            lookup_status = catalog_lookup_record(db->catalog, &record_info, &lookup_result);
            if (lookup_status != CATALOG_LOOKUP_SUCCESS) {
                goto restore_old_metadata;
            }

            if (lookup_result.num_records != 1) {
                goto restore_old_metadata;
            }

            record_info.root_page_num = lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;
            metadata_page_num = lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;

            // Visit all metadata pages
            memset(&visited, 0, sizeof(visited));
            if (!visit_metadata_pages(db->pager, metadata_page_num, &visited)) {
                goto restore_old_metadata;
            }

            // Copy all of them and connect them together
            new_metadata_page_num = 0;
            if (!copy_metadata_pages(db->pager, &visited, &new_metadata_page_num)) {
                goto restore_old_metadata;
            }

            // Keep only the first in index_metadata to restore later
            index_metadata.pages[index_metadata.num_pages] = db->pager->pages[new_metadata_page_num];
            index_metadata.num_pages++;

            status = catalog_persist_metadata_pages(db->catalog, &record_info, metadata_page_num);
            if (status != CATALOG_SUCCESS) {
                goto restore_old_metadata;
            }

            if (!page_mark_dirty(db->pager->pages[metadata_page_num])) {
                goto restore_old_metadata;
            }

        }

        for (uint32_t j = 0; j < db->tables[table_idx]->total_secondary_indexes; j++) {

            if (!db->tables[table_idx]->secondary_indexes[j]) {
                continue;
            }

            memcpy(record_info.object_name, db->tables[table_idx]->secondary_indexes[j]->name, 64);
            record_info.type = CATALOG_INDEX;
            record_info.object.index = db->tables[table_idx]->secondary_indexes[j];

            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
            lookup_status = catalog_lookup_record(db->catalog, &record_info, &lookup_result);
            if (lookup_status != CATALOG_LOOKUP_SUCCESS) {
                goto restore_old_metadata;
            }

            if (lookup_result.num_records != 1) {
                goto restore_old_metadata;
            }

            record_info.root_page_num = lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;
            metadata_page_num = lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;

            // Visit all metadata pages
            memset(&visited, 0, sizeof(visited));
            if (!visit_metadata_pages(db->pager, metadata_page_num, &visited)) {
                goto restore_old_metadata;
            }

            // Copy all of them and connect them together
            new_metadata_page_num = 0;
            if (!copy_metadata_pages(db->pager, &visited, &new_metadata_page_num)) {
                goto restore_old_metadata;
            }

            // Keep only the first in index_metadata to restore later
            index_metadata.pages[index_metadata.num_pages] = db->pager->pages[new_metadata_page_num];
            index_metadata.num_pages++;

            status = catalog_persist_metadata_pages(db->catalog, &record_info, metadata_page_num);
            if (status != CATALOG_SUCCESS) {
                goto restore_old_metadata;
            }

            if (!page_mark_dirty(db->pager->pages[metadata_page_num])) {
                goto restore_old_metadata;
            }
        }

        btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
    }

    if (lookup_result.records) {
        if (lookup_result.records[0].cell) {
            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
        }
    }

    // Free copies of all metadata pages
    for (uint32_t j = 0; j < table_metadata.num_pages; j++) {
        catalog_release_metadata_pages(db->catalog, table_metadata.pages[j]->page_num);
        // Free as many as possible, don't check status
    }

    for (uint32_t j = 0; j < index_metadata.num_pages; j++) {
        catalog_release_metadata_pages(db->catalog, index_metadata.pages[j]->page_num);
        // Free as many as possible, don't check status
    }

    return true;

restore_old_metadata:

    uint32_t table_metadata_idx = 0;
    uint32_t index_metadata_idx = 0;
    for (uint32_t j = 0; j <= table_idx; j++) {
        if (!db->tables[j]) {
            continue;
        }

        if (!table_metadata.pages[table_metadata_idx]) {
            return false;
        }

        CatalogRecordInfo record_info = {0};
        memcpy(record_info.table_name, db->tables[j]->name, 64);
        record_info.type = CATALOG_TABLE;
        memset(record_info.object_name, 0, 64);
        record_info.object.table = db->tables[j];

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(db->catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) {
            if (lookup_result.records && lookup_result.records[0].cell) {
                btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec); 
            }
            return false;
        }

        if (lookup_result.num_records != 1) {
            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
            return false;
        }

        // Copy over new metadata page num which copies the original version of those metadata pages
        // to restore old data
        memcpy(
            &lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num,
            &table_metadata.pages[table_metadata_idx]->page_num,
            sizeof(uint32_t)
        );
        table_metadata_idx++;

        CatalogStatus status = catalog_replace_record(db->catalog, &record_info, lookup_result.records[0].cell);
        if (status != CATALOG_SUCCESS) {
            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
            return false;
        }
        btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);

        record_info.type = CATALOG_INDEX;
        memcpy(record_info.object_name, db->tables[j]->primary_index->name, 64);
        record_info.object.index = db->tables[j]->primary_index;

        if (!index_metadata.pages[index_metadata_idx]) {
            return false;
        }

        lookup_status = catalog_lookup_record(db->catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) {
            if (lookup_result.records && lookup_result.records[0].cell) {
                btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec); 
            }
            return false;
        }

        if (lookup_result.num_records != 1) {
            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
            return false;
        }

        memcpy(
            &lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num,
            &index_metadata.pages[index_metadata_idx]->page_num,
            sizeof(uint32_t)
        );
        index_metadata_idx++;

        status = catalog_replace_record(db->catalog, &record_info, lookup_result.records[0].cell);
        if (status != CATALOG_SUCCESS) {
            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
            return false;
        }
        btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);

        for (uint32_t k = 0; k < db->tables[j]->total_secondary_indexes; k++) {
            if (!db->tables[j]->secondary_indexes[k]) {
                continue;
            }

            if (!index_metadata.pages[index_metadata_idx]) {
                return false;
            }

            memcpy(record_info.object_name, db->tables[j]->secondary_indexes[k]->name, 64);
            record_info.object.index = db->tables[j]->secondary_indexes[k];

            lookup_status = catalog_lookup_record(db->catalog, &record_info, &lookup_result);
            if (lookup_status != CATALOG_LOOKUP_SUCCESS) {
                if (lookup_result.records && lookup_result.records[0].cell) {
                    btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec); 
                }
                return false;
            }

            if (lookup_result.num_records != 1) {
                btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
                return false;
            }

            memcpy(
                &lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num,
                &index_metadata.pages[index_metadata_idx]->page_num,
                sizeof(uint32_t)
            );
            index_metadata_idx++;

            status = catalog_replace_record(db->catalog, &record_info, lookup_result.records[0].cell);
            if (status != CATALOG_SUCCESS) {
                btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
                return false;
            }
            btree_cell_contents_free(lookup_result.records[0].cell, &db->catalog->spec);
        }
    }

    return false;
}