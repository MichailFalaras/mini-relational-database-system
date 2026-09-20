#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "../include/database.h"
#include "../include/catalog.h"
#include "../include/table.h"
#include "../include/schema.h"
#include "../include/data_types.h"
#include "../include/pager.h"
#include "../include/page.h"
#include "../include/btree.h"
#include "../src/btree/btree_utils.h"
#include "../src/data_types/data_types_utils.h"
#include "../src/catalog/catalog_utils.h"
#include "../include/index.h"
#include "../include/serialize.h"

#define ASSERT(condition) { \
    if (!(condition)) { \
        return 1; \
    } \
}

/* ---------- Helpers (Borrowed from catalog_tests.c) ---------- */

static bool allocate_mock_root_page(Pager *pager, uint32_t *page_num) {
    if (!pager || !page_num) {
        return false;
    }

    return pager_allocate_page(
        pager,
        page_num
    );
}

static Schema *create_mock_schema(void) {
    Schema *schema = NULL;
    Column **columns = calloc(3, sizeof(Column *));
    if (!columns) {
        return NULL;
    }

    columns[0] = column_alloc(
        "id",
        UNSIGNED_INTEGER,
        0,
        0,
        0
    );
    if (!columns[0]) {
        goto cleanup;
    }

    columns[1] = column_alloc(
        "name",
        VARCHAR,
        64,
        0,
        0
    );
    if (!columns[1]) {
        goto cleanup;
    }

    columns[2] = column_alloc(
        "age",
        UNSIGNED_INTEGER,
        0,
        0,
        0
    );
    if (!columns[2]) {
        goto cleanup;
    }

    schema = schema_create(columns, NULL, 3, 0);
    if (!schema) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < 3; i++) {
        free(columns[i]);
    }

    free(columns);
    return schema;

cleanup:
    for (uint32_t i = 0; i < 3; i++) {
        if (columns[i]) {
            free(columns[i]);
        }
    }

    free(columns);
    return NULL;
}

static Table *create_mock_table(Pager *pager, const char *table_name) {
    if (!pager || !table_name) {
        return NULL;
    }

    Schema *schema = create_mock_schema();
    if (!schema) {
        return NULL;
    }

    Table *table = table_create(
        table_name,
        schema,
        pager
    );

    schema_free(schema);

    return table;
}

static int fill_database(Database *db, uint32_t start_id, uint32_t end_id) {
    if (!db) {
        return -1;
    }

    for (uint32_t i = start_id; i < end_id; i++) {
        Table *table = NULL;
        Index *index = NULL;

        char table_name[64] = {0};
        char index_name[64] = {0};

        snprintf(table_name, 64, "table%02d", i);
        snprintf(index_name, 64, "index%02d", i);

        /* ----- CATALOG TABLE ----- */
        {
            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_name, 64);
            record_info.type = CATALOG_TABLE;
            strncpy(record_info.object_name, "", 64);

            if (!allocate_mock_root_page(db->catalog->btree->pager, &record_info.root_page_num)) {
                return -1;
            }

            table = create_mock_table(db->catalog->btree->pager, table_name);
            if (!table) { return -1; }
            record_info.object.table = table;

            BTreeCellContents cell = {0};
            CatalogStatus status = catalog_create_record(db->catalog, &record_info, &cell);
            ASSERT(status == CATALOG_SUCCESS);

            status = catalog_insert_record(db->catalog, &cell);
            ASSERT(status == CATALOG_SUCCESS);

            db->tables[db->table_count] = table;
            db->table_count++;

            btree_cell_contents_free(&cell, &db->catalog->spec);
        }

        /* ----- CATALOG INDEX ----- */
        {
            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_name, 64);
            record_info.type = CATALOG_INDEX;
            strncpy(record_info.object_name, index_name, 64);

            uint32_t column_refs[2] = {0, 1};
            IndexKey *index_key = index_key_create(column_refs, 2);
            if (!index_key) { return -1; }
            index = index_create(index_name, (IndexType) (i % 2), index_key, db->pager, (bool) ((i+1) % 2));
            if (!index) { return -1; }
            record_info.object.index = index;
            record_info.root_page_num = index->root_page_num;

            BTreeCellContents cell = {0};

            CatalogStatus status = catalog_create_record(db->catalog, &record_info, &cell);
            ASSERT(status == CATALOG_SUCCESS);

            status = catalog_insert_record(db->catalog, &cell);
            ASSERT(status == CATALOG_SUCCESS);

            if (!table_append_index(db->tables[db->table_count-1], index)) { return -1; }

            btree_cell_contents_free(&cell, &db->catalog->spec);
        }
    }

    return 0;
}

/* ---------- Tests ---------- */

static int test_empty_database_open() {
    const char *pathname = "build/database_db1_1.db";
    Database *db = database_open(pathname);

    ASSERT(db != NULL);
    ASSERT(!strcmp(db->pathname, pathname));

    ASSERT(db->tables != NULL);
    ASSERT(db->table_count == 0);

    ASSERT(db->catalog != NULL);
    ASSERT(db->catalog->btree != NULL);
    ASSERT(db->catalog->btree->pager == db->pager);
    ASSERT(db->catalog->btree->root_page_num == db->pager->num_pages-1);
    // Already tested in tests/catalog_tests.c

    ASSERT(db->pager != NULL);
    ASSERT(db->pager->num_pages == 2);
    ASSERT(db->pager->file_length == 0); // Nothing in disk

    database_free(db);
    return 0;
}

static int test_invalid_database_open() {

    /* ----- EMPTY PATHNAME ----- */
    {
        const char *pathname = "";
        Database *db = database_open(pathname);
        ASSERT(db == NULL);
    }

    /* ----- INVALID SUPERBLOCK PAGE (Superblock Page validation test) ----- */
    {
        const char *pathname = "build/database_db1_2.db";
        Pager *pager = pager_open(pathname);
        if (!pager) { return -1; }

        PageZeroMetadata superblock_page = {0};
        uint8_t *read_offset = pager->pages[SUPERBLOCK_PAGE_NUM]->page_data;
        if (!deserialize_page_zero_metadata(&read_offset, &superblock_page)) {
            return -1;
        }

        uint8_t *write_offset = pager->pages[SUPERBLOCK_PAGE_NUM]->page_data;
        strncpy(superblock_page.magic, "error", 10);
        if (!serialize_page_zero_metadata(&write_offset, &superblock_page)) {
            return -1;
        }

        pager_close(pager);

        Database *db = database_open(pathname);
        ASSERT(db == NULL);
    }

    /* ----- INVALID CATALOG ROOT ----- */
    {
        const char *pathname = "build/database_db1_2.db";
        Pager *pager = pager_open(pathname);
        if (!pager) { return -1; }

        /* More than 1 pages allocated. */
        uint32_t page_num = 0;
        if (!pager_allocate_page(pager, &page_num)
            || !pager_get_page(pager, page_num)) {
            return -1;
        }

        PageZeroMetadata superblock_page = {0};
        uint8_t *read_offset = pager->pages[SUPERBLOCK_PAGE_NUM]->page_data;
        if (!deserialize_page_zero_metadata(&read_offset, &superblock_page)) {
            return -1;
        }

        /* But, THERE'S NO CATALOG ROOT. */
        superblock_page.catalog_root = UINT32_MAX;
        pager_close(pager);

        Database *db = database_open(pathname);
        ASSERT(db == NULL);
    }

    return 0;
}

static int test_empty_database_close() {
    const char *pathname = "build/database_db2_1.db";
    Database *db = database_open(pathname);
    ASSERT(db != NULL);

    bool res = database_close(db);
    ASSERT(res == true);

    return 0;
}

static int test_database_close() {
    const char *pathname = "build/database_db2_2.db";
    Database *db = database_open(pathname);

    /* Fill database to have some actual data to close with. */
    int res = fill_database(db, 0, 5);
    if (res == -1) { return -1; }

    /* Validate database correctly made and stored metadata. */
    ASSERT(db->table_count == 5);

    uint32_t index_count = 0;
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(db->tables[i] != NULL);

        if (i%2 == 0) {
            ASSERT(db->tables[i]->primary_index != NULL);
            index_count++;
        } else {
            ASSERT(db->tables[i]->secondary_indexes[0] != NULL);
            index_count++;
        }
    }
    ASSERT(index_count == 5);

    ASSERT(database_close(db));

    return 0;
}

static int test_invalid_database_close() {
    const char *pathname = "build/database_db2_3.db";
    Database *db = database_open(pathname);

    int res = fill_database(db, 0, 5);
    if (res == -1) { return -1; }

    ASSERT(!database_close(NULL));
    return 0;
}

static int test_metadata_reconstruction_and_metadata_page_update() {
    const char *pathname = "build/database_db3_1.db";
    Database *db = database_open(pathname);
    if (!db) { return -1; }

    uint32_t num_catalog_records = 5;
    int res = fill_database(db, 0, num_catalog_records);
    if (res == -1) { return -1; }

    char *table_names[] = {"table00", "table01", "table02", "table03", "table04"};
    char *index_names[] = {"index00", "index01", "index02", "index03", "index04"};

    uint32_t total_metadata_records = num_catalog_records * 2;

    uint8_t *buffer_before[MAX_PAGES] = {0};
    uint8_t *buffer_after_modifications[MAX_PAGES] = {0};
    uint8_t *buffer_after_reopen[MAX_PAGES] = {0};

    size_t before_sizes[MAX_PAGES] = {0};
    size_t after_sizes[MAX_PAGES] = {0};

    uint32_t buffer_idx = 0;

    /* ----- STORE SERIALIZED METADATA BEFORE MODIFICATIONS ----- */
    for (uint32_t i = 0; i < num_catalog_records; i++) {
        Table *table = database_find_table(db, table_names[i]);
        if (!table) { return -1; }

        /* ----- TABLE METADATA BEFORE MODIFICATIONS ----- */
        {
            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_names[i], 64);
            record_info.type = CATALOG_TABLE;
            memset(record_info.object_name, 0, 64);
            record_info.root_page_num = UINT32_MAX;

            CatalogLookupResult lookup_result = {0};
            CatalogLookupStatus lookup_status =
                catalog_lookup_record(db->catalog, &record_info, &lookup_result);

            if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }
            if (lookup_result.num_records != 1) { return -1; }

            uint32_t metadata_page_num =
                lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;

            CatalogMetadataPages metadata_pages = {0};
            if (!visit_metadata_pages(db->pager, metadata_page_num, &metadata_pages)) {
                return -1;
            }

            before_sizes[buffer_idx] = serialized_table_metadata_size(table);
            if (!before_sizes[buffer_idx]) { return -1; }

            buffer_before[buffer_idx] =
                calloc(before_sizes[buffer_idx], sizeof(uint8_t));
            if (!buffer_before[buffer_idx]) { return -1; }

            size_t remaining_size = before_sizes[buffer_idx];
            uint8_t *write_offset = buffer_before[buffer_idx];

            for (uint32_t j = 0; j < metadata_pages.num_pages; j++) {
                size_t bytes_to_copy =
                    remaining_size > METADATA_PAGE_PAYLOAD_SIZE
                        ? METADATA_PAGE_PAYLOAD_SIZE
                        : remaining_size;

                memcpy(
                    write_offset,
                    metadata_pages.pages[j]->page_data + sizeof(uint32_t),
                    bytes_to_copy
                );

                write_offset += bytes_to_copy;
                remaining_size -= bytes_to_copy;

                if (!remaining_size) {
                    break;
                }
            }

            ASSERT(remaining_size == 0);

            btree_cell_contents_free(
                lookup_result.records[0].cell,
                &db->catalog->spec
            );
            free(lookup_result.records);

            buffer_idx++;
        }

        /* ----- INDEX METADATA BEFORE MODIFICATIONS ----- */
        {
            Index *index = NULL;

            if (table->primary_index
                && !strcmp(table->primary_index->name, index_names[i])) {
                index = table->primary_index;
            } else {
                for (uint32_t j = 0; j < table->total_secondary_indexes; j++) {
                    if (table->secondary_indexes[j]
                        && !strcmp(table->secondary_indexes[j]->name, index_names[i])) {
                        index = table->secondary_indexes[j];
                        break;
                    }
                }
            }

            if (!index) { return -1; }

            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_names[i], 64);
            record_info.type = CATALOG_INDEX;
            strncpy(record_info.object_name, index_names[i], 64);
            record_info.root_page_num = UINT32_MAX;

            CatalogLookupResult lookup_result = {0};
            CatalogLookupStatus lookup_status =
                catalog_lookup_record(db->catalog, &record_info, &lookup_result);

            if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }
            if (lookup_result.num_records != 1) { return -1; }

            uint32_t metadata_page_num =
                lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;

            CatalogMetadataPages metadata_pages = {0};
            if (!visit_metadata_pages(db->pager, metadata_page_num, &metadata_pages)) {
                return -1;
            }

            before_sizes[buffer_idx] = serialized_index_metadata_size(index);
            if (!before_sizes[buffer_idx]) { return -1; }

            buffer_before[buffer_idx] =
                calloc(before_sizes[buffer_idx], sizeof(uint8_t));
            if (!buffer_before[buffer_idx]) { return -1; }

            size_t remaining_size = before_sizes[buffer_idx];
            uint8_t *write_offset = buffer_before[buffer_idx];

            for (uint32_t j = 0; j < metadata_pages.num_pages; j++) {
                size_t bytes_to_copy =
                    remaining_size > METADATA_PAGE_PAYLOAD_SIZE
                        ? METADATA_PAGE_PAYLOAD_SIZE
                        : remaining_size;

                memcpy(
                    write_offset,
                    metadata_pages.pages[j]->page_data + sizeof(uint32_t),
                    bytes_to_copy
                );

                write_offset += bytes_to_copy;
                remaining_size -= bytes_to_copy;

                if (!remaining_size) {
                    break;
                }
            }

            ASSERT(remaining_size == 0);

            btree_cell_contents_free(
                lookup_result.records[0].cell,
                &db->catalog->spec
            );
            free(lookup_result.records);

            buffer_idx++;
        }
    }

    ASSERT(buffer_idx == total_metadata_records);

    /* ----- MODIFY TABLE/INDEX OBJECTS IN MEMORY ----- */
    for (uint32_t i = 0; i < db->table_count; i++) {
        Column col = {
            .name = "new_col",
            .type = i%12,
            .type_parameter = 0,
            .null_rows = 0,
            .non_null_rows = 0
        };

        if (!schema_add_column(db->tables[i]->table_schema, &col)) {
            return -1;
        }

        if (i%2 == 0) {
            for (uint32_t j = 0;
                 j < db->tables[i]->primary_index->key->num_columns;
                 j++) {

                db->tables[i]->primary_index->key->column_index_array[j]++;
            }
        } else {
            for (uint32_t j = 0;
                 j < db->tables[i]->total_secondary_indexes;
                 j++) {

                for (uint32_t k = 0;
                     k < db->tables[i]->secondary_indexes[j]->key->num_columns;
                     k++) {

                    db->tables[i]
                        ->secondary_indexes[j]
                        ->key
                        ->column_index_array[k]++;
                }
            }
        }
    }

    /* ----- CREATE EXPECTED SERIALIZED METADATA AFTER MODIFICATIONS ----- */
    buffer_idx = 0;

    for (uint32_t i = 0; i < num_catalog_records; i++) {
        Table *table = database_find_table(db, table_names[i]);
        if (!table) { return -1; }

        /* ----- EXPECTED TABLE METADATA ----- */
        {
            after_sizes[buffer_idx] =
                serialized_table_metadata_size(table);

            if (!after_sizes[buffer_idx]) { return -1; }

            buffer_after_modifications[buffer_idx] =
                calloc(after_sizes[buffer_idx], sizeof(uint8_t));

            if (!buffer_after_modifications[buffer_idx]) {
                return -1;
            }

            uint8_t *write_offset =
                buffer_after_modifications[buffer_idx];

            if (!serialize_table_metadata(&write_offset, table)) {
                return -1;
            }

            buffer_idx++;
        }

        /* ----- EXPECTED INDEX METADATA ----- */
        {
            Index *index = NULL;

            if (table->primary_index
                && !strcmp(table->primary_index->name, index_names[i])) {
                index = table->primary_index;
            } else {
                for (uint32_t j = 0; j < table->total_secondary_indexes; j++) {
                    if (table->secondary_indexes[j]
                        && !strcmp(table->secondary_indexes[j]->name, index_names[i])) {
                        index = table->secondary_indexes[j];
                        break;
                    }
                }
            }

            if (!index) { return -1; }

            after_sizes[buffer_idx] =
                serialized_index_metadata_size(index);

            if (!after_sizes[buffer_idx]) { return -1; }

            buffer_after_modifications[buffer_idx] =
                calloc(after_sizes[buffer_idx], sizeof(uint8_t));

            if (!buffer_after_modifications[buffer_idx]) {
                return -1;
            }

            uint8_t *write_offset =
                buffer_after_modifications[buffer_idx];

            if (!serialize_index_metadata(&write_offset, index)) {
                return -1;
            }

            buffer_idx++;
        }
    }

    ASSERT(buffer_idx == total_metadata_records);

    /*
     * BEFORE must differ from AFTER MODIFICATIONS.
     *
     * Table metadata can also change serialized size after adding a column,
     * so a size difference is already sufficient to establish inequality.
     */
    for (uint32_t i = 0; i < total_metadata_records; i++) {
        if (before_sizes[i] == after_sizes[i]) {
            ASSERT(memcmp(
                buffer_before[i],
                buffer_after_modifications[i],
                after_sizes[i]
            ));
        } else {
            ASSERT(before_sizes[i] != after_sizes[i]);
        }
    }

    /* database_close() must persist the modified metadata pages. */
    ASSERT(database_close(db));

    db = database_open(pathname);
    ASSERT(db != NULL);

    /* ----- STORE SERIALIZED METADATA AFTER REOPEN ----- */
    buffer_idx = 0;

    for (uint32_t i = 0; i < num_catalog_records; i++) {
        Table *table = database_find_table(db, table_names[i]);
        if (!table) { return -1; }

        /* ----- TABLE METADATA AFTER REOPEN ----- */
        {
            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_names[i], 64);
            record_info.type = CATALOG_TABLE;
            memset(record_info.object_name, 0, 64);
            record_info.root_page_num = UINT32_MAX;

            CatalogLookupResult lookup_result = {0};
            CatalogLookupStatus lookup_status =
                catalog_lookup_record(db->catalog, &record_info, &lookup_result);

            if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }
            if (lookup_result.num_records != 1) { return -1; }

            size_t reopened_size =
                serialized_table_metadata_size(table);

            ASSERT(reopened_size == after_sizes[buffer_idx]);

            uint32_t metadata_page_num =
                lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;

            CatalogMetadataPages metadata_pages = {0};
            if (!visit_metadata_pages(db->pager, metadata_page_num, &metadata_pages)) {
                return -1;
            }

            buffer_after_reopen[buffer_idx] =
                calloc(reopened_size, sizeof(uint8_t));

            if (!buffer_after_reopen[buffer_idx]) {
                return -1;
            }

            size_t remaining_size = reopened_size;
            uint8_t *write_offset = buffer_after_reopen[buffer_idx];

            for (uint32_t j = 0; j < metadata_pages.num_pages; j++) {
                size_t bytes_to_copy =
                    remaining_size > METADATA_PAGE_PAYLOAD_SIZE
                        ? METADATA_PAGE_PAYLOAD_SIZE
                        : remaining_size;

                memcpy(
                    write_offset,
                    metadata_pages.pages[j]->page_data + sizeof(uint32_t),
                    bytes_to_copy
                );

                write_offset += bytes_to_copy;
                remaining_size -= bytes_to_copy;

                if (!remaining_size) {
                    break;
                }
            }

            ASSERT(remaining_size == 0);

            btree_cell_contents_free(
                lookup_result.records[0].cell,
                &db->catalog->spec
            );
            free(lookup_result.records);

            buffer_idx++;
        }

        /* ----- INDEX METADATA AFTER REOPEN ----- */
        {
            Index *index = NULL;

            if (table->primary_index
                && !strcmp(table->primary_index->name, index_names[i])) {
                index = table->primary_index;
            } else {
                for (uint32_t j = 0; j < table->total_secondary_indexes; j++) {
                    if (table->secondary_indexes[j]
                        && !strcmp(table->secondary_indexes[j]->name, index_names[i])) {
                        index = table->secondary_indexes[j];
                        break;
                    }
                }
            }

            if (!index) { return -1; }

            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_names[i], 64);
            record_info.type = CATALOG_INDEX;
            strncpy(record_info.object_name, index_names[i], 64);
            record_info.root_page_num = UINT32_MAX;

            CatalogLookupResult lookup_result = {0};
            CatalogLookupStatus lookup_status =
                catalog_lookup_record(db->catalog, &record_info, &lookup_result);

            if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }
            if (lookup_result.num_records != 1) { return -1; }

            size_t reopened_size =
                serialized_index_metadata_size(index);

            ASSERT(reopened_size == after_sizes[buffer_idx]);

            uint32_t metadata_page_num =
                lookup_result.records[0].cell->BTreePayload.catalog->metadata_page_num;

            CatalogMetadataPages metadata_pages = {0};
            if (!visit_metadata_pages(db->pager, metadata_page_num, &metadata_pages)) {
                return -1;
            }

            buffer_after_reopen[buffer_idx] =
                calloc(reopened_size, sizeof(uint8_t));

            if (!buffer_after_reopen[buffer_idx]) {
                return -1;
            }

            size_t remaining_size = reopened_size;
            uint8_t *write_offset = buffer_after_reopen[buffer_idx];

            for (uint32_t j = 0; j < metadata_pages.num_pages; j++) {
                size_t bytes_to_copy =
                    remaining_size > METADATA_PAGE_PAYLOAD_SIZE
                        ? METADATA_PAGE_PAYLOAD_SIZE
                        : remaining_size;

                memcpy(
                    write_offset,
                    metadata_pages.pages[j]->page_data + sizeof(uint32_t),
                    bytes_to_copy
                );

                write_offset += bytes_to_copy;
                remaining_size -= bytes_to_copy;

                if (!remaining_size) {
                    break;
                }
            }

            ASSERT(remaining_size == 0);

            btree_cell_contents_free(
                lookup_result.records[0].cell,
                &db->catalog->spec
            );
            free(lookup_result.records);

            buffer_idx++;
        }
    }

    ASSERT(buffer_idx == total_metadata_records);

    /*
     * Expected serialized metadata after the in-memory modifications must be
     * exactly equal to the serialized metadata persisted by database_close()
     * and read from the metadata page chains after database_open().
     */
    for (uint32_t i = 0; i < total_metadata_records; i++) {
        ASSERT(!memcmp(
            buffer_after_modifications[i],
            buffer_after_reopen[i],
            after_sizes[i]
        ));
    }

    for (uint32_t i = 0; i < total_metadata_records; i++) {
        free(buffer_before[i]);
        free(buffer_after_modifications[i]);
        free(buffer_after_reopen[i]);
    }

    ASSERT(database_close(db));

    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;

    result = unlink("build/database_db1_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database_db1_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database_db2_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database_db2_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database_db2_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database_db3_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    result = test_empty_database_open();
    generate_output(result, 0, "test_empty_database_open");
    result = test_invalid_database_open();
    generate_output(result, 1, "test_invalid_database_open");
    result = test_empty_database_close();
    generate_output(result, 2, "test_empty_database_close");
    result = test_database_close();
    generate_output(result, 3, "test_database_close");
    result = test_invalid_database_close();
    generate_output(result, 4, "test_invalid_database_close");
    result = test_metadata_reconstruction_and_metadata_page_update();
    generate_output(result, 5, "test_metadata_reconstruction_and_metadata_page_update");
    
    return 0;
}
