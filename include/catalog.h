#ifndef CATALOG_H_
#define CATALOG_H_

#include <stdint.h>

/* Catalog entry type --> Table or Index */
typedef enum catalog_entry_type {
    CATALOG_TABLE = 0,
    CATALOG_INDEX = 1
} CatalogEntryType;

/* Catalog leaf page payload structure */
typedef struct catalog_payload {
    CatalogEntryType type;
    uint32_t root_page_num;

    // Either Table/Schema/Column/Constraint metadata
    // or Index metadata.
    uint32_t metadata_page_num;
} CatalogPayload;

#endif