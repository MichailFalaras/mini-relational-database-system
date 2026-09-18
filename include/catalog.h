#ifndef CATALOG_H_
#define CATALOG_H_

#include <stdint.h>
#include <stdbool.h>
#include "btree.h"

#define METADATA_PAGE_PAYLOAD_SIZE (PAGE_SIZE - sizeof(uint32_t))
#define CATALOG_KEY_COUNT 3
#define CATALOG_KEY_BITMAP_SIZE ((CATALOG_KEY_COUNT + 7) / 8)

// 133 bytes
#define CATALOG_KEY_SIZE (CATALOG_KEY_BITMAP_SIZE + 64 * sizeof(uint8_t) + sizeof(uint32_t) + 64 * sizeof(uint8_t))
// 142 bytes
#define CATALOG_CELL_SIZE (CATALOG_KEY_SIZE + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t))

typedef struct btree Btree;
typedef struct table Table;
typedef struct index Index;

/* Catalog entry type --> Table or Index */
typedef enum catalog_entry_type {
    CATALOG_TABLE = 0,
    CATALOG_INDEX = 1
} CatalogEntryType;

typedef enum catalog_status {
    CATALOG_SUCCESS,
    CATALOG_DUPLICATE_KEY,
    CATALOG_NOT_FOUND,
    CATALOG_INVALID_ARGUMENTS,
    CATALOG_ERROR
} CatalogStatus;

typedef enum catalog_lookup_status {
    CATALOG_LOOKUP_SUCCESS,
    CATALOG_LOOKUP_NOT_FOUND,
    CATALOG_LOOKUP_INVALID_ARGUMENTS,
    CATALOG_LOOKUP_ERROR
} CatalogLookupStatus;

typedef struct catalog {
    BTree *btree;
    BTreeIndexSpec spec; // Catalog-Specific BTreeIndexSpec
} Catalog;

typedef struct catalog_record_info {
    char table_name[64];
    CatalogEntryType type;
    char object_name[64];

    uint32_t root_page_num;
    union {
        Table *table;
        Index *index;
    } object;
} CatalogRecordInfo;

/* Catalog leaf page payload structure */
typedef struct catalog_payload {
    CatalogEntryType type;
    uint32_t root_page_num;

    // Either Table/Schema/Column/Constraint metadata
    // or Index metadata.
    uint32_t metadata_page_num;
} CatalogPayload;

typedef struct catalog_record {
    BTreeCellContents *cell;
    uint32_t cell_index;
    uint32_t page_num;
} CatalogRecord;

typedef struct catalog_lookup_result {
    CatalogRecord *records;
    uint32_t num_records;
} CatalogLookupResult;

typedef struct catalog_metadata_pages {
    Page *pages[MAX_PAGES];
    uint32_t num_pages;
} CatalogMetadataPages;

/* Create Catalog struct and create System Catalog Root Page. */
extern Catalog *catalog_create(Pager *pager, uint32_t *root_page_num);

/* Create empty System Catalog leaf Root Page and update 
 * Catalog's BTree root_page_num metadata. */
extern CatalogStatus catalog_initialize(const Catalog *catalog, uint32_t *root_page_num);

/* Create Catalog Leaf Node Cell. */
extern CatalogStatus catalog_create_record(const Catalog *catalog, CatalogRecordInfo *record_info, BTreeCellContents *cell);

/* Allocate as many metadata pages as needed to store all
 * Table's/Index's metadata. */
extern CatalogStatus catalog_create_metadata_pages(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t *metadata_page_num);

/* Store all Table's/Index's metadata throughout all pages created. */
extern CatalogStatus catalog_persist_metadata_pages(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t metadata_page_num);

/* Reconstruct Table's/Index's as a complete struct from Metadata Pages
 * & corresponding Catalog Cell. */
extern CatalogStatus catalog_read_metadata_pages(const Catalog *catalog, BTreeCellContents *catalog_cell, CatalogRecordInfo *record_info,
    uint32_t metadata_page_num);

/* Release all Metadata Pages from last to first. */
extern CatalogStatus catalog_release_metadata_pages(const Catalog *catalog, uint32_t metadata_page_num);

/* Catalog scan and return all Leaf Node Catalog Cells in CatalogLookupResult. */
extern CatalogLookupStatus catalog_scan(const Catalog *catalog, CatalogLookupResult *lookup_result);

/* Catalog Free. */
extern void catalog_free(Catalog *catalog);

/* ---------- CATALOG ORCHESTRATION ---------- */

/* Insert record into System Catalog BTree. */
extern CatalogStatus catalog_insert_record(const Catalog *catalog, BTreeCellContents *cell);

/* Delete record from System Catalog BTree. */
extern CatalogStatus catalog_delete_record(const Catalog *catalog, BTreeCellContents *cell);

/* Lookup record in System Catalog BTree. */
extern CatalogLookupStatus catalog_lookup_record(const Catalog *catalog, CatalogRecordInfo *record_info, CatalogLookupResult *lookup_result);

/* Update record in System Catalog BTree. */
extern CatalogStatus catalog_update_record(const Catalog *catalog, CatalogRecordInfo *record_info, uint32_t new_root_page_num);

#endif