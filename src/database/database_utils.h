#ifndef DATABASE_UTILS_H
#define DATABASE_UTILS_H

#include <stdbool.h>

typedef struct database Database;
typedef struct catalog_lookup_result CatalogLookupResult;
typedef struct pager Pager;
typedef struct page_zero_metadata PageZeroMetadata;

/* Superblock Page Validation. */
bool validate_superblock_page(Pager *pager, PageZeroMetadata *page_zero_metadata);

/* Reconstruct Database from System Catalog B+Tree & metadata pages.*/
bool reconstruct_system_catalog(Database *db, CatalogLookupResult *lookup_result);

/* Update metadata pages. Used in database_close(). */
bool update_metadata_pages(Database *db);

#endif