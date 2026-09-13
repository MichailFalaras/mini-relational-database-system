#ifndef CATALOG_UTILS_H_
#define CATALOG_UTILS_H_

/* Create Catalog Key from CatalogRecordInfo. */
bool create_catalog_key(CatalogRecordInfo *record_info, Value ***key);

/* Returns Table's/Index's serialized metadata size. */
size_t get_catalog_payload_serialized_size(CatalogRecordInfo *record_info);

/* Persist Catalog payload. (Table/Index metadata). */
bool persist_catalog_payload(CatalogRecordInfo *record_info, uint8_t **write_offset);

/* Deserialize Catalog Payload. (Table/Index metadata). */
bool read_catalog_payload(CatalogRecordInfo *record_info, uint8_t **read_offset);

/* BTree → Catalog Status. */
CatalogStatus btree_to_catalog_status(BTreeStatus status);

/* BTree → Catalog Lookup Status. */
CatalogLookupStatus btree_to_catalog_lookup_status(BTreeStatus status);

/* Check if page number is contained in CatalogMetadataPages. */
bool is_page_in_metadata_pages(CatalogMetadataPages *metadata_pages, uint32_t page_num);

/* Visit a list of metadata pages and store them all in a CatalogMetadataPages struct. */
bool visit_metadata_pages(Pager *pager, uint32_t metadata_page_num, CatalogMetadataPages *metadata_pages);

/* Copy metadata pages and connect them together. */
bool copy_metadata_pages(Pager *pager, CatalogMetadataPages *metadata_pages, uint32_t *new_page_num);

/* CatalogPayload metadata copy. */
CatalogPayload *catalog_payload_copy(Pager *pager, const CatalogPayload *payload);

#endif