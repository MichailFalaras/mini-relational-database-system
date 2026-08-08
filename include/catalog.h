#ifndef CATALOG_H_
#define CATALOG_H_

#include <stdint.h>

/* Catalog entry type --> Table or Index */
typedef struct catalog_entry_type {
    CATALOG_TABLE,
    CATALOG_INDEX
} CatalogEntryType;

/* Catalog leaf page payload structure */
typedef struct catalog_payload {
    CatalogEntryType type;
    uint32_t root_page_num;
    uint32_t ddl_size;
    char *ddl;
} CatalogPayload;

#endif