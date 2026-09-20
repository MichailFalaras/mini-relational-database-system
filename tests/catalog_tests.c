#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

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

#define LARGE_MOCK_TABLE_COLUMNS 54
static Table *create_large_mock_table(void)
{
    Column **columns = calloc(LARGE_MOCK_TABLE_COLUMNS, sizeof(Column *));
    if (!columns) {
        return NULL;
    }

    for (uint32_t i = 0; i < LARGE_MOCK_TABLE_COLUMNS; i++) {
        char column_name[64];

        snprintf(column_name, sizeof(column_name), "mock_column_%u", i);

        columns[i] = column_alloc(
            column_name,
            CHAR,
            64,
            0,
            0
        );

        if (!columns[i]) {
            for (uint32_t j = 0; j < i; j++) {
                free(columns[j]);
            }

            free(columns);
            return NULL;
        }
    }

    Schema *schema = schema_create(
        columns,
        NULL,
        LARGE_MOCK_TABLE_COLUMNS,
        0
    );

    if (!schema) {
        for (uint32_t i = 0; i < LARGE_MOCK_TABLE_COLUMNS; i++) {
            free(columns[i]);
        }

        free(columns);
        return NULL;
    }

    Table *table = table_metadata_create("large_mock_table", schema);

    if (!table) {
        schema_free(schema);
        return NULL;
    }
    
    schema_free(schema);
    for (uint32_t i = 0; i < LARGE_MOCK_TABLE_COLUMNS; i++) {
        free(columns[i]);
    }
    free(columns);

    return table;
}

static bool create_mock_catalog_cell(Catalog *catalog, const char *table_name, uint32_t root_page_num, BTreeCellContents *cell) {
    if (!catalog || !catalog->btree
        || !catalog->btree->pager
        || !table_name || !root_page_num || !cell) {
        return false;
    }

    Table *table = create_mock_table(
        catalog->btree->pager,
        table_name
    );
    if (!table) {
        return false;
    }

    CatalogRecordInfo record_info = {0};

    strncpy(
        record_info.table_name,
        table_name,
        sizeof(record_info.table_name) - 1
    );

    record_info.type = CATALOG_TABLE;
    record_info.root_page_num = root_page_num;
    record_info.object.table = table;

    CatalogStatus status = catalog_create_record(
        catalog,
        &record_info,
        cell
    );

    table_free(table);

    return status == CATALOG_SUCCESS;
}

static bool allocate_mock_root_page(Pager *pager, uint32_t *page_num) {
    if (!pager || !page_num) {
        return false;
    }

    return pager_allocate_page(
        pager,
        page_num
    );
}

static void catalog_lookup_result_free(CatalogLookupResult *lookup_result, BTreeIndexSpec *spec) {
    if (!lookup_result) {
        return;
    }

    if (lookup_result->records) {
        for (uint32_t i = 0;
             i < lookup_result->num_records;
             i++) {

            if (lookup_result->records[i].cell) {
                btree_cell_contents_free(
                    lookup_result->records[i].cell,
                    spec
                );
            }
        }

        free(lookup_result->records);
        lookup_result->records = NULL;
    }

    lookup_result->num_records = 0;
}

/* Start index => to create mock table with name: "table<start>"
 * End index => how many tables to create while increasing the start index. */
static int insert_catalog_cells(Catalog *catalog, uint32_t start, uint32_t end) {
    if (!catalog || !end) {
        return -1;
    }

    for (uint32_t i = start; i < end; i++) {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%02d", i);

        if (!allocate_mock_root_page(catalog->btree->pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        if (status != CATALOG_SUCCESS) { return -1; }

        btree_cell_contents_free(&cell, &catalog->spec);
    }

    return 0;
}

/* ---------- Tests ---------- */

static int test_catalog_create() {

    /* ----- CATALOG CREATE WITH NOT CREATED SYSTEM CATALOG ROOT ----- */
    {
        const char *pathname = "build/catalog_db1_1.db";
        Pager *pager = pager_open(pathname);
        if (!pager) { return -1; }

        uint32_t system_catalog_root_page_num = UINT32_MAX;
        Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);

        ASSERT(catalog != NULL);
        ASSERT(catalog->btree != NULL);
        ASSERT(catalog->btree->pager != NULL);
        ASSERT(system_catalog_root_page_num == pager->pages[pager->num_pages-1]->page_num);
        ASSERT(catalog->btree->root_page_num == pager->pages[pager->num_pages-1]->page_num);

        ASSERT(catalog->spec.column_types != NULL);
        ASSERT(catalog->spec.column_types[0] == CHAR);
        ASSERT(catalog->spec.column_types[1] == UNSIGNED_INTEGER);
        ASSERT(catalog->spec.column_types[2] == CHAR);

        ASSERT(catalog->spec.index_key != NULL);
        ASSERT(catalog->spec.index_key->num_columns == 3);
        ASSERT(catalog->spec.index_key->column_index_array[0] == 0);
        ASSERT(catalog->spec.index_key->column_index_array[1] == 1);
        ASSERT(catalog->spec.index_key->column_index_array[2] == 2);

        ASSERT(catalog->spec.is_unique == true);
        ASSERT(catalog->spec.key_size = CATALOG_KEY_SIZE);
        ASSERT(catalog->spec.payload_type == BTREE_CATALOG_PAYLOAD);

        ASSERT(catalog->spec.schema != NULL);
        ASSERT(catalog->spec.schema->num_columns == 3);
        ASSERT(catalog->spec.schema->num_constraints == 0);

        ASSERT(!strcmp(catalog->spec.schema->columns[0]->name, "table_name"));
        ASSERT(catalog->spec.schema->columns[0]->type == CHAR);
        ASSERT(catalog->spec.schema->columns[0]->type_parameter == 64);
        ASSERT(catalog->spec.schema->columns[0]->serialized_size == get_serialized_value_size(CHAR, 64));
        ASSERT(catalog->spec.schema->columns[0]->null_rows == 0);
        ASSERT(catalog->spec.schema->columns[0]->non_null_rows == 0);

        ASSERT(!strcmp(catalog->spec.schema->columns[1]->name, "object_type"));
        ASSERT(catalog->spec.schema->columns[1]->type == UNSIGNED_INTEGER);
        ASSERT(catalog->spec.schema->columns[1]->type_parameter == 0);
        ASSERT(catalog->spec.schema->columns[1]->serialized_size == get_serialized_value_size(UNSIGNED_INTEGER, 0));
        ASSERT(catalog->spec.schema->columns[1]->null_rows == 0);
        ASSERT(catalog->spec.schema->columns[1]->non_null_rows == 0);

        ASSERT(!strcmp(catalog->spec.schema->columns[2]->name, "object_name"));
        ASSERT(catalog->spec.schema->columns[2]->type == CHAR);
        ASSERT(catalog->spec.schema->columns[2]->type_parameter == 64);
        ASSERT(catalog->spec.schema->columns[2]->serialized_size == get_serialized_value_size(CHAR, 64));
        ASSERT(catalog->spec.schema->columns[2]->null_rows == 0);
        ASSERT(catalog->spec.schema->columns[2]->non_null_rows == 0);

        catalog_free(catalog);
        pager_close(pager);
    }
    

    /* ----- CATALOG CREATE WITH CREATED SYSTEM CATALOG ROOT ----- */
    {
        const char *pathname = "build/catalog_db1_3.db";
        Pager *pager = pager_open(pathname);
        if (!pager) { return -1; }

        uint32_t system_catalog_root_page_num = UINT32_MAX;
        if (!allocate_mock_root_page(pager, &system_catalog_root_page_num)) { return -1; }
        
        Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);

        ASSERT(catalog != NULL);
        ASSERT(catalog->btree != NULL);
        ASSERT(catalog->btree->pager != NULL);

        // It didn't allocate a new system catalog root page
        ASSERT(system_catalog_root_page_num == system_catalog_root_page_num);
        ASSERT(catalog->btree->root_page_num == system_catalog_root_page_num);

        ASSERT(catalog->spec.column_types != NULL);
        ASSERT(catalog->spec.column_types[0] == CHAR);
        ASSERT(catalog->spec.column_types[1] == UNSIGNED_INTEGER);
        ASSERT(catalog->spec.column_types[2] == CHAR);

        ASSERT(catalog->spec.index_key != NULL);
        ASSERT(catalog->spec.index_key->num_columns == 3);
        ASSERT(catalog->spec.index_key->column_index_array[0] == 0);
        ASSERT(catalog->spec.index_key->column_index_array[1] == 1);
        ASSERT(catalog->spec.index_key->column_index_array[2] == 2);

        ASSERT(catalog->spec.is_unique == true);
        ASSERT(catalog->spec.key_size = CATALOG_KEY_SIZE);
        ASSERT(catalog->spec.payload_type == BTREE_CATALOG_PAYLOAD);

        ASSERT(catalog->spec.schema != NULL);
        ASSERT(catalog->spec.schema->num_columns == 3);
        ASSERT(catalog->spec.schema->num_constraints == 0);

        ASSERT(!strcmp(catalog->spec.schema->columns[0]->name, "table_name"));
        ASSERT(catalog->spec.schema->columns[0]->type == CHAR);
        ASSERT(catalog->spec.schema->columns[0]->type_parameter == 64);
        ASSERT(catalog->spec.schema->columns[0]->serialized_size == get_serialized_value_size(CHAR, 64));
        ASSERT(catalog->spec.schema->columns[0]->null_rows == 0);
        ASSERT(catalog->spec.schema->columns[0]->non_null_rows == 0);

        ASSERT(!strcmp(catalog->spec.schema->columns[1]->name, "object_type"));
        ASSERT(catalog->spec.schema->columns[1]->type == UNSIGNED_INTEGER);
        ASSERT(catalog->spec.schema->columns[1]->type_parameter == 0);
        ASSERT(catalog->spec.schema->columns[1]->serialized_size == get_serialized_value_size(UNSIGNED_INTEGER, 0));
        ASSERT(catalog->spec.schema->columns[1]->null_rows == 0);
        ASSERT(catalog->spec.schema->columns[1]->non_null_rows == 0);

        ASSERT(!strcmp(catalog->spec.schema->columns[2]->name, "object_name"));
        ASSERT(catalog->spec.schema->columns[2]->type == CHAR);
        ASSERT(catalog->spec.schema->columns[2]->type_parameter == 64);
        ASSERT(catalog->spec.schema->columns[2]->serialized_size == get_serialized_value_size(CHAR, 64));
        ASSERT(catalog->spec.schema->columns[2]->null_rows == 0);
        ASSERT(catalog->spec.schema->columns[2]->non_null_rows == 0);

        catalog_free(catalog);
        pager_close(pager);
    }

    return 0;
}

static int test_invalid_catalog_creation() {

    /* ----- UNINITIALIZED PAGER ----- */
    {
        Pager *pager = NULL;
        uint32_t system_catalog_root_page_num = UINT32_MAX;
        Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);

        ASSERT(catalog == NULL);
    }
    
    /* ----- UNINITIALIZED ROOT PAGE NUM  ----- */
    {
        const char *pathname = "build/catalog_db1_2.db";
        Pager *pager = pager_open(pathname);
        if (!pager) { return -1; }
        Catalog *catalog = catalog_create(pager, NULL);

        ASSERT(catalog == NULL);
    }
    
    /* ----- SYSTEM CATALOG ROOT PAGE NUM BEING SUPERBLOCK PAGE ----- */
    {
        const char *pathname = "build/catalog_db1_3.db";
        Pager *pager = pager_open(pathname);
        if (!pager) { return -1; }

        uint32_t system_catalog_root_page_num = 0;
        Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);

        ASSERT(catalog == NULL);
    }
    
    return 0;
}

static int test_catalog_create_record() {
    const char *pathname = "build/catalog_db2_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- CATALOG TABLE PAYLOAD ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table1", 64); 
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }

        Schema *schema = create_mock_schema();
        record_info.object.table = table_create("table1", schema, pager);
        if (!record_info.object.table) { return -1; }

        BTreeCellContents cell = {0};

        CatalogStatus status = catalog_create_record(catalog, &record_info, &cell);
        ASSERT(status == CATALOG_SUCCESS);

        ASSERT(cell.type == BTREE_LEAF_NODE);
        ASSERT(cell.num_keys == CATALOG_KEY_COUNT);
        ASSERT(cell.key_size == CATALOG_KEY_SIZE);
        ASSERT(cell.cell_size == CATALOG_CELL_SIZE);

        ASSERT(cell.keys != NULL);
        ASSERT(cell.keys[0]->type == CHAR);
        ASSERT(cell.keys[0]->null_val == false);
        ASSERT(cell.keys[0]->value.char_val.n <= 64);
        ASSERT(cell.keys[0]->value.char_val.string != NULL);
        ASSERT(!strcmp(cell.keys[0]->value.char_val.string, "table1"));

        ASSERT(cell.keys[1]->type == UNSIGNED_INTEGER);
        ASSERT(cell.keys[1]->null_val == false);
        ASSERT(cell.keys[1]->value.uint32_val == (uint32_t) CATALOG_TABLE);

        ASSERT(cell.keys[2]->type == CHAR);
        ASSERT(cell.keys[2]->null_val == false);
        ASSERT(cell.keys[2]->value.char_val.n <= 64);
        ASSERT(cell.keys[2]->value.char_val.string != NULL);
        ASSERT(!strcmp(cell.keys[2]->value.char_val.string, ""));

        ASSERT(cell.BTreePayload.catalog != NULL);
        ASSERT(cell.BTreePayload.catalog->type == CATALOG_TABLE);
        ASSERT(cell.BTreePayload.catalog->root_page_num == pager->num_pages-2);

        /* SUPERBLOCK PAGE NUM: 0
        * SYSTEM CATALOG ROOT PAGE NUM: 1
        * MOCK ALLOCATED B+TREE ROOT PAGE: 2
        * FIRST AND ONLY METADATA PAGE NUM: 3 */
        ASSERT(cell.BTreePayload.catalog->metadata_page_num == pager->num_pages-1);

        CatalogMetadataPages metadata_pages = {0};
        if (!visit_metadata_pages(pager, 2, &metadata_pages)) { return -1; }

        ASSERT(metadata_pages.num_pages == 1);
        ASSERT(metadata_pages.pages[0]->page_num == 2);

        uint32_t next_page_num = UINT32_MAX;
        memcpy(&next_page_num, metadata_pages.pages[0]->page_data, sizeof(uint32_t));
        ASSERT(next_page_num == 0); // No other metadata page

        table_free(record_info.object.table);
    }

    /* ----- CATALOG INDEX PAYLOAD ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table2", 64); 
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, "primary_key_idx", 64);
        
        uint32_t column_refs[2] = {0, 1};
        IndexKey *index_key = index_key_create(column_refs, 2);
        if (!index_key) { return -1; }
        record_info.object.index = index_create("primary_key_idx", PRIMARY_INDEX, index_key, pager, true);
        if (!record_info.object.index) { return -1; }
        record_info.root_page_num = record_info.object.index->root_page_num;

        BTreeCellContents cell = {0};

        CatalogStatus status = catalog_create_record(catalog, &record_info, &cell);
        ASSERT(status == CATALOG_SUCCESS);

        ASSERT(cell.type == BTREE_LEAF_NODE);
        ASSERT(cell.num_keys == CATALOG_KEY_COUNT);
        ASSERT(cell.key_size == CATALOG_KEY_SIZE);
        ASSERT(cell.cell_size == CATALOG_CELL_SIZE);

        ASSERT(cell.keys != NULL);
        ASSERT(cell.keys[0]->type == CHAR);
        ASSERT(cell.keys[0]->null_val == false);
        ASSERT(cell.keys[0]->value.char_val.n <= 64);
        ASSERT(cell.keys[0]->value.char_val.string != NULL);
        ASSERT(!strcmp(cell.keys[0]->value.char_val.string, "table2"));

        ASSERT(cell.keys[1]->type == UNSIGNED_INTEGER);
        ASSERT(cell.keys[1]->null_val == false);
        ASSERT(cell.keys[1]->value.uint32_val == (uint32_t) CATALOG_INDEX);

        ASSERT(cell.keys[2]->type == CHAR);
        ASSERT(cell.keys[2]->null_val == false);
        ASSERT(cell.keys[2]->value.char_val.n <= 64);
        ASSERT(cell.keys[2]->value.char_val.string != NULL);
        ASSERT(!strcmp(cell.keys[2]->value.char_val.string, "primary_key_idx"));

        ASSERT(cell.BTreePayload.catalog != NULL);
        ASSERT(cell.BTreePayload.catalog->type == CATALOG_INDEX);
        ASSERT(cell.BTreePayload.catalog->root_page_num == pager->num_pages-2);

        // /* SUPERBLOCK PAGE NUM: 0
        // * SYSTEM CATALOG ROOT PAGE NUM: 1
        // * MOCK ALLOCATED B+TREE ROOT PAGE: 2
        // * FIRST AND ONLY METADATA PAGE NUM: 3 */
        // ASSERT(cell.BTreePayload.catalog->metadata_page_num == pager->num_pages-1);

        // CatalogMetadataPages metadata_pages = {0};
        // if (!visit_metadata_pages(pager, 2, &metadata_pages)) { return -1; }

        // ASSERT(metadata_pages.num_pages == 1);
        // ASSERT(metadata_pages.pages[0]->page_num == 2);

        // uint32_t next_page_num = UINT32_MAX;
        // memcpy(&next_page_num, metadata_pages.pages[0]->page_data, sizeof(uint32_t));
        // ASSERT(next_page_num == 0); // No other metadata page

        // index_free(record_info.object.index);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_record_creation() {
    const char *pathname = "build/catalog_db2_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- INVALID CATALOG ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table_name", 64); 
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }

        Schema *schema = create_mock_schema();
        record_info.object.table = table_create("table1", schema, pager);
        if (!record_info.object.table) { return -1; }

        BTreeCellContents cell = {0};

        CatalogStatus status = catalog_create_record(NULL, &record_info, &cell);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        table_free(record_info.object.table);
    }

    /* ----- NO TABLE NAME GIVEN ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "", 64); 
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, "pk_id", 64);
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }

        Schema *schema = create_mock_schema();
        record_info.object.table = table_create("table1", schema, pager);
        if (!record_info.object.table) { return -1; }

        BTreeCellContents cell = {0};

        CatalogStatus status = catalog_create_record(catalog, &record_info, &cell);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        table_free(record_info.object.table);
    }

    /* ----- CATALOG INDEX PAYLOAD BUT NO OBJECT NAME GIVEN ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table2", 64); 
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, "", 64);
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }

        Schema *schema = create_mock_schema();
        record_info.object.table = table_create("table2", schema, pager);
        if (!record_info.object.table) { return -1; }

        BTreeCellContents cell = {0};

        CatalogStatus status = catalog_create_record(catalog, &record_info, &cell);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        table_free(record_info.object.table);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_catalog_metadata_pages() {
    const char *pathname = "build/catalog_db3_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    uint32_t table_root_page_num = 0;
    ASSERT(allocate_mock_root_page(pager, &table_root_page_num));

    Table *table = create_mock_table(pager, "metadata_table");
    ASSERT(table != NULL);

    CatalogRecordInfo record_info = {0};
    strncpy(record_info.table_name, "metadata_table", sizeof(record_info.table_name)-1);
    record_info.type = CATALOG_TABLE;
    record_info.root_page_num = table_root_page_num;
    record_info.object.table = table;

    BTreeCellContents catalog_cell = {0};
    CatalogStatus status = catalog_create_record(catalog, &record_info, &catalog_cell);

    ASSERT(status == CATALOG_SUCCESS);
    ASSERT(catalog_cell.keys != NULL);
    ASSERT(catalog_cell.BTreePayload.catalog != NULL);

    uint32_t metadata_page_num = catalog_cell.BTreePayload.catalog->metadata_page_num;

    ASSERT(metadata_page_num != 0);

    Page *metadata_page = pager_get_page(pager, metadata_page_num);
    ASSERT(metadata_page != NULL);

    /* Read the metadata back using the complete catalog cell. */
    CatalogRecordInfo read_record_info = {0};
    strncpy(read_record_info.table_name, "metadata_table", sizeof(read_record_info.table_name) - 1);
    read_record_info.type = CATALOG_TABLE;
    read_record_info.root_page_num = table_root_page_num;

    Table *read_table = create_mock_table(pager, "metadata_table");
    ASSERT(read_table != NULL);

    read_record_info.object.table = read_table;

    status = catalog_read_metadata_pages(catalog, &catalog_cell, &read_record_info, metadata_page_num);
    ASSERT(status == CATALOG_SUCCESS);
    ASSERT(read_record_info.object.table != NULL);

    /*
     * Release the metadata pages independently from the catalog record.
     */
    status = catalog_release_metadata_pages(catalog, metadata_page_num);
    ASSERT(status == CATALOG_SUCCESS);

    btree_cell_contents_free(&catalog_cell, (BTreeIndexSpec *) &(catalog->spec));

    table_free(read_record_info.object.table);
    table_free(table);

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_metadata_pages() {
    const char *pathname = "build/catalog_db3_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    
    /* ----- INFINITE CYCLE TRYING TO READ/RELEASE PAGES ----- */
    {
        uint32_t page_num = 0;
        if (!pager_allocate_page(pager, &page_num)) { return -1; }
        Page *page = pager_get_page(pager, page_num);
        if (!page) { return -1; }
        
        // Connect a page to itself
        memcpy(page->page_data, &page->page_num, sizeof(uint32_t));

        BTreeCellContents cell = {0};
        CatalogRecordInfo record_info = {0};
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table1", record_info.root_page_num, &cell)) { return -1; }
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &cell, &record_info)) { return -1; }
        record_info.object.table = create_large_mock_table();

        CatalogStatus status = catalog_persist_metadata_pages(catalog, &record_info, page_num);
        if (status != CATALOG_SUCCESS) { return -1; }

        status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num);
        ASSERT(status == CATALOG_ERROR);

        status = catalog_release_metadata_pages(catalog, page_num);
        ASSERT(status == CATALOG_ERROR);

        table_free(record_info.object.table);
    }

    /* ----- METADATA PAGE CONNECTED TO INVALID PAGE ----- */
    {
        uint32_t page_num = 0;
        if (!pager_allocate_page(pager, &page_num)) { return -1; }
        Page *page = pager_get_page(pager, page_num);
        if (!page) { return -1; }
        
        uint32_t invalid_page_num = 99;
        memcpy(page->page_data, &invalid_page_num, sizeof(uint32_t));

        BTreeCellContents cell = {0};
        CatalogRecordInfo record_info = {0};
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table1", record_info.root_page_num, &cell)) { return -1; }
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &cell, &record_info)) { return -1; }
        record_info.object.table = create_large_mock_table();

        CatalogStatus status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num);
        ASSERT(status == CATALOG_ERROR);

        status = catalog_release_metadata_pages(catalog, page_num);
        ASSERT(status == CATALOG_ERROR);

        table_free(record_info.object.table);
    }

    /* ----- PAGE CONNECTED TO SUPERBLOCK ----- */
    {
        uint32_t page_num = 0;
        if (!pager_allocate_page(pager, &page_num)) { return -1; }
        Page *page = pager_get_page(pager, page_num);
        if (!page) { return -1; }
        
        uint32_t superblock_page_num = SUPERBLOCK_PAGE_NUM;
        memcpy(page->page_data, &superblock_page_num, sizeof(uint32_t));

        BTreeCellContents cell = {0};
        CatalogRecordInfo record_info = {0};
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table1", record_info.root_page_num, &cell)) { return -1; }
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &cell, &record_info)) { return -1; }
        record_info.object.table = create_large_mock_table();

        CatalogStatus status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num);
        ASSERT(status == CATALOG_ERROR);

        // CATALOG RELEASE METADATA PAGES NOT TESTED SINCE THE MOMENT SUPERBLOCK
        // PAGE IS DETECTED, THE LOOP STOPS

        // status = catalog_release_metadata_pages(catalog, page_num);
        // ASSERT(status == CATALOG_ERROR);

        table_free(record_info.object.table);
    }

    /* ----- MORE PAGES THAN NEEDED TO SERIALIZE METADATA ----- */
    {
        Page *pages[4] = {0};
        uint32_t page_num[4] = {0};
        for (uint32_t i = 0; i < 4; i++) {

            if (!pager_allocate_page(pager, &page_num[i])) { return -1; }
            pages[i] = pager_get_page(pager, page_num[i]);
            if (!pages[i]) { return -1; }
        }

        memcpy(pages[0]->page_data, &page_num[1], sizeof(uint32_t));
        memcpy(pages[1]->page_data, &page_num[2], sizeof(uint32_t));
        memcpy(pages[2]->page_data, &page_num[3], sizeof(uint32_t));
        uint32_t superblock_page_num = SUPERBLOCK_PAGE_NUM;
        memcpy(pages[3]->page_data, &superblock_page_num, sizeof(uint32_t));
    
        BTreeCellContents cell = {0};
        CatalogRecordInfo record_info = {0};
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table1", record_info.root_page_num, &cell)) { return -1; }
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &cell, &record_info)) { return -1; }
        record_info.object.table = create_large_mock_table();

        CatalogStatus status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num[0]);
        ASSERT(status == CATALOG_ERROR);

        // CATALOG RELEASE METADATA NOT TESTED SINCE IT'S GOING TO RELEASE AS
        // MANY CONNECTED VALID PAGES AS IT POSSIBLY CAN

        // status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num[0]);
        // ASSERT(status != CATALOG_ERROR);
    }

    /* ----- LESS PAGES THAN NEEDED TO SERIALIZE OBJECT METADATA ----- */
    {
        uint32_t page_num = 0;
        if (!pager_allocate_page(pager, &page_num)) { return -1; }
        Page *page = pager_get_page(pager, page_num);
        if (!page) { return -1; }
        
        uint32_t superblock_page_num = SUPERBLOCK_PAGE_NUM;
        memcpy(page->page_data, &superblock_page_num, sizeof(uint32_t));

        BTreeCellContents cell = {0};
        CatalogRecordInfo record_info = {0};
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table1", record_info.root_page_num, &cell)) { return -1; }
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &cell, &record_info)) { return -1; }
        record_info.object.table = create_large_mock_table();

        CatalogStatus status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num);
        ASSERT(status == CATALOG_ERROR);

        // CATALOG RELEASE METADATA PAGES NOT TESTED SINCE IT JUST RELEASES
        // VALID CONNECTED PAGES. THERE'S NOTHING WRONG HERE, ITS JUST THAT
        // THERE ARE NOT ENOUGH ALLOCATED PAGES TO READ ALL THE DATA THAT WAS
        // SUPPOSED TO BE READ.

        // status = catalog_release_metadata_pages(catalog, page_num);
        // ASSERT(status == CATALOG_ERROR);

        table_free(record_info.object.table);
    }

    /* ----- NOT AN ACTUAL METADATA PAGE ----- */
    {
        uint32_t page_num = 0;
        if (!pager_allocate_page(pager, &page_num)) { return -1; }
        Page *page = pager_get_page(pager, page_num);
        if (!page) { return -1; }
        memset(page, 1, PAGE_SIZE); // Random data inside
        
        BTreeCellContents cell = {0};
        CatalogRecordInfo record_info = {0};
        if (!allocate_mock_root_page(pager, &record_info.root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table1", record_info.root_page_num, &cell)) { return -1; }
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &cell, &record_info)) { return -1; }
        record_info.object.table = create_large_mock_table();

        CatalogStatus status = catalog_read_metadata_pages(catalog, &cell, &record_info, page_num);
        ASSERT(status == CATALOG_ERROR);

        status = catalog_release_metadata_pages(catalog, page_num);
        ASSERT(status == CATALOG_ERROR);

        table_free(record_info.object.table);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_catalog_insert_record() {
    const char *pathname = "build/catalog_db4_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- INSERT INTO SYSTEM CATALOG EMPTY LEAF NODE ROOT UNTIL FULL -----*/
    for (uint32_t i = 0; i < 27; i++) {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%d", i);

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_SUCCESS);

        btree_cell_contents_free(&cell, &catalog->spec);
    }

    BTreePage leaf_root = {0};
    BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
    if (btree_status != BTREE_SUCCESS) { return -1; }

    ASSERT(leaf_root.is_root == true);
    ASSERT(leaf_root.parent_pointer == UINT32_MAX);
    ASSERT(leaf_root.cell_count == 27);
    ASSERT(leaf_root.free_space_offset == 262); // Right before splitting

    /* ----- SPLIT SYSTEM CATALOG EMPTY LEAF NODE ROOT ----- */
    for (uint32_t i = 0; i < 2; i++) {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%d", i+27);

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_SUCCESS);

        btree_cell_contents_free(&cell, &catalog->spec);
    }


    ASSERT(leaf_root.is_root == false);
    // minus 2 since the page splitted (+1 pages) and also created the new root (+1 pages)
    ASSERT(leaf_root.parent_pointer == pager->pages[pager->num_pages-1]->page_num-2);
    ASSERT(leaf_root.cell_count == 13);

    /* ----- INSERT INTO START AND MID OF PAGE ----- */
    {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = "apple";

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_SUCCESS);

        btree_cell_contents_free(&cell, &catalog->spec);
    }
    {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = "george";

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_SUCCESS);

        btree_cell_contents_free(&cell, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_record_insertion() {
    const char *pathname = "build/catalog_db4_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }


    /* Max out system catalog empty leaf node root. */
    int res = insert_catalog_cells(catalog, 0, 15);
    if (res == -1) { return -1; }

    /* ----- INVALID ARGUMENTS ----- */
    {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = {0};

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table_name", root_page_num, &cell)) { return -1; }

        if (cell.keys[0]->value.char_val.string) { free(cell.keys[0]->value.char_val.string); }
        cell.keys[0]->value.char_val.string = strdup(table_name); // table_name = "\0";
        BTreePage leaf_root = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        btree_cell_contents_free(&cell, &catalog->spec);
    }

    /* ----- DUPLICATE CELL DETECTION ----- */
    {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%02d", 7);

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        BTreePage leaf_root = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_DUPLICATE_KEY);

        btree_cell_contents_free(&cell, &catalog->spec);
    }

    /* ----- MISSING OBJECT NAME ----- */
    {
        BTreeCellContents cell = {0};
        uint32_t root_page_num = 0;
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%02d", 16);

        if (!allocate_mock_root_page(pager, &root_page_num)) { return -1; }
        if (!create_mock_catalog_cell(catalog, table_name, root_page_num, &cell)) { return -1; }

        cell.BTreePayload.catalog->type = CATALOG_INDEX;
        cell.keys[1]->value.uint32_val = 1;
        if (cell.keys[2]->value.char_val.string) { free(cell.keys[2]->value.char_val.string); }
        cell.keys[2]->value.char_val.string = strdup("");;
        BTreePage leaf_root = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_root, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        CatalogStatus status = catalog_insert_record(catalog, &cell);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        btree_cell_contents_free(&cell, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_catalog_lookup_record() {
    const char *pathname = "build/catalog_db5_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* Max out system catalog empty leaf node root and split it. */
    int res = insert_catalog_cells(catalog, 0, 28);
    if (res == -1) { return -1; }

    /* ----- LOOKUP CELL IN BEGINNING OF FIRST LEAF PAGE ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table00", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);

        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);
        ASSERT(lookup_result.num_records == 1);
        ASSERT(lookup_result.records[0].cell_index == 0);
        ASSERT(lookup_result.records[0].page_num == 1);

        ASSERT(lookup_result.records[0].cell->type == BTREE_LEAF_NODE);
        ASSERT(lookup_result.records[0].cell->num_keys == CATALOG_KEY_COUNT);
        ASSERT(lookup_result.records[0].cell->key_size == CATALOG_KEY_SIZE);
        ASSERT(lookup_result.records[0].cell->cell_size == CATALOG_CELL_SIZE);

        ASSERT(lookup_result.records[0].cell->keys != NULL);
        ASSERT(lookup_result.records[0].cell->keys[0]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[0]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[0]->value.char_val.string, "table00"));

        ASSERT(lookup_result.records[0].cell->keys[1]->type == UNSIGNED_INTEGER);
        ASSERT(lookup_result.records[0].cell->keys[1]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[1]->value.uint32_val == (uint32_t) CATALOG_TABLE);

        ASSERT(lookup_result.records[0].cell->keys[2]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[2]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[2]->value.char_val.string, ""));

        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->type == CATALOG_TABLE);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }
    
    /* ----- LOOKUP CELL IN MIDDLE OF FIRST LEAF PAGE ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table05", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);

        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);
        ASSERT(lookup_result.num_records == 1);

        ASSERT(lookup_result.records[0].cell_index == 5);
        ASSERT(lookup_result.records[0].page_num == 1);

        ASSERT(lookup_result.records[0].cell->type == BTREE_LEAF_NODE);
        ASSERT(lookup_result.records[0].cell->num_keys == CATALOG_KEY_COUNT);
        ASSERT(lookup_result.records[0].cell->key_size == CATALOG_KEY_SIZE);
        ASSERT(lookup_result.records[0].cell->cell_size == CATALOG_CELL_SIZE);

        ASSERT(lookup_result.records[0].cell->keys != NULL);
        ASSERT(lookup_result.records[0].cell->keys[0]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[0]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[0]->value.char_val.string, "table05"));

        ASSERT(lookup_result.records[0].cell->keys[1]->type == UNSIGNED_INTEGER);
        ASSERT(lookup_result.records[0].cell->keys[1]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[1]->value.uint32_val == (uint32_t) CATALOG_TABLE);

        ASSERT(lookup_result.records[0].cell->keys[2]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[2]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[2]->value.char_val.string, ""));

        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->type == CATALOG_TABLE);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }
    
    /* ----- LOOKUP CELL IN END OF FIRST LEAF PAGE ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table12", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);

        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);
        ASSERT(lookup_result.num_records == 1);
        ASSERT(lookup_result.records[0].cell_index == 12);
        ASSERT(lookup_result.records[0].page_num == 1);

        ASSERT(lookup_result.records[0].cell->type == BTREE_LEAF_NODE);
        ASSERT(lookup_result.records[0].cell->num_keys == CATALOG_KEY_COUNT);
        ASSERT(lookup_result.records[0].cell->key_size == CATALOG_KEY_SIZE);
        ASSERT(lookup_result.records[0].cell->cell_size == CATALOG_CELL_SIZE);

        ASSERT(lookup_result.records[0].cell->keys != NULL);
        ASSERT(lookup_result.records[0].cell->keys[0]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[0]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[0]->value.char_val.string, "table12"));

        ASSERT(lookup_result.records[0].cell->keys[1]->type == UNSIGNED_INTEGER);
        ASSERT(lookup_result.records[0].cell->keys[1]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[1]->value.uint32_val == (uint32_t) CATALOG_TABLE);

        ASSERT(lookup_result.records[0].cell->keys[2]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[2]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[2]->value.char_val.string, ""));

        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->type == CATALOG_TABLE);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

     /* ----- LOOKUP CELL IN SECOND LEAF PAGE ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table20", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);

        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);
        ASSERT(lookup_result.num_records == 1);

        ASSERT(lookup_result.records[0].cell_index == 7);
        ASSERT(lookup_result.records[0].page_num == 58);

        ASSERT(lookup_result.records[0].cell->type == BTREE_LEAF_NODE);
        ASSERT(lookup_result.records[0].cell->num_keys == CATALOG_KEY_COUNT);
        ASSERT(lookup_result.records[0].cell->key_size == CATALOG_KEY_SIZE);
        ASSERT(lookup_result.records[0].cell->cell_size == CATALOG_CELL_SIZE);

        ASSERT(lookup_result.records[0].cell->keys != NULL);
        ASSERT(lookup_result.records[0].cell->keys[0]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[0]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[0]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[0]->value.char_val.string, "table20"));

        ASSERT(lookup_result.records[0].cell->keys[1]->type == UNSIGNED_INTEGER);
        ASSERT(lookup_result.records[0].cell->keys[1]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[1]->value.uint32_val == (uint32_t) CATALOG_TABLE);

        ASSERT(lookup_result.records[0].cell->keys[2]->type == CHAR);
        ASSERT(lookup_result.records[0].cell->keys[2]->null_val == false);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.n <= 64);
        ASSERT(lookup_result.records[0].cell->keys[2]->value.char_val.string != NULL);
        ASSERT(!strcmp(lookup_result.records[0].cell->keys[2]->value.char_val.string, ""));

        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->type == CATALOG_TABLE);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_record_lookup() {
    const char *pathname = "build/catalog_db5_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- LOOKUP IN EMPTY PAGE ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table00", 64);
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, "pk_id", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_NOT_FOUND);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* Max out system catalog empty leaf node root. */
    int res = insert_catalog_cells(catalog, 0, 27);
    if (res == -1) { return -1; }

    /* ----- INVALID ARGUMENTS ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table01", 64);
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_INVALID_ARGUMENTS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* ----- LOOKUP NON-EXISTENT CELL ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table28", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_NOT_FOUND);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_catalog_delete_record() {
    const char *pathname = "build/catalog_db6_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* Max out system catalog empty leaf node root and split it. */
    int res = insert_catalog_cells(catalog, 0, 28);
    if (res == -1) { return -1; }

    /* ----- DELETE FIRST CELL ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table00", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        CatalogStatus status = catalog_delete_record(catalog, lookup_result.records[0].cell);
        ASSERT(status == CATALOG_SUCCESS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        BTreePage leaf_page = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_page, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        ASSERT(leaf_page.cell_count == 12);
    }

    /* ----- DELETE MIDDLE CELL ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table07", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        CatalogStatus status = catalog_delete_record(catalog, lookup_result.records[0].cell);
        ASSERT(status == CATALOG_SUCCESS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        BTreePage leaf_page = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_page, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        ASSERT(leaf_page.cell_count == 11);
    }

    /* ----- DELETE LAST CELL ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table12", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        CatalogStatus status = catalog_delete_record(catalog, lookup_result.records[0].cell);
        ASSERT(status == CATALOG_SUCCESS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        BTreePage leaf_page = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_page, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        ASSERT(leaf_page.cell_count == 10);
    }

    /* ----- UNDERFLOW FIRST LEAF NODE ----- */
    {
        Page *second_page = pager_get_page(pager, 58);
        if (!second_page) { return -1; }

        BTreePage second_leaf_page = {0};
        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &second_leaf_page, second_page, &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }

        ASSERT(second_leaf_page.cell_count == 15);

        char *tables_left[12] = {"table01", "table02", "table03", "table04", "table05",
                                 "table06", "table08", "table09", "table10", "table11",
                                 "table13", "table15"};

        BTreePage leaf_page = {0};
        for (uint32_t i = 0; i < 12; i++) {

            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, tables_left[i], 64);
            record_info.type = CATALOG_TABLE;
            strncpy(record_info.object_name, "", 64);
            record_info.root_page_num = UINT32_MAX;
            
            CatalogLookupResult lookup_result = {0};
            CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
            if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

            CatalogStatus status = catalog_delete_record(catalog, lookup_result.records[0].cell);
            ASSERT(status == CATALOG_SUCCESS);

            catalog_lookup_result_free(&lookup_result, &catalog->spec);

            BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_page, pager->pages[1], &catalog->spec);
            if (btree_status != BTREE_SUCCESS) { return -1; }
        }

        // Initial: LEFT = 13 | RIGHT = 15  
        // LEFT = 12 | RIGHT = 15 
        // LEFT = 11 | RIGHT = 15
        // LEFT = 10 | RIGHT = 15  

        // Now try to delete all 10 from LEFT 
        // i = 0: LEFT = 9 | RIGHT = 15
        // i = 1: LEFT = 8 | RIGHT = 15
        // i  = 2: LEFT = 7 | RIGHT = 15 (REDISTRIBUTION)
        // i = 3: LEFT = 7 | RIGHT = 14 
        // ... i = 9: LEFT = 7 | RIGHT = 8
        // i = 10: LEFT = 7 | RIGHT = 7 (MERGE + delete)
        // i = 11: RIGHT = 13
        ASSERT(leaf_page.cell_count == 13);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_record_deletion() {
    const char *pathname = "build/catalog_db6_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- DELETE FROM EMPTY CATALOG ----- */
    {
        BTreeCellContents mock_cell = {0};
        uint32_t mock_root = 0;
        if (!allocate_mock_root_page(pager, &mock_root)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table00", mock_root, &mock_cell)) { return -1; }

        CatalogStatus status = catalog_delete_record(catalog, &mock_cell);
        ASSERT(status == CATALOG_NOT_FOUND);

        btree_cell_contents_free(&mock_cell, &catalog->spec);
    }

    /* Max out system catalog empty leaf node root. */
    int res = insert_catalog_cells(catalog, 0, 27);
    if (res == -1) { return -1; }

    /* ----- INVALID ARGUMENTS ----- */
    {
        BTreeCellContents mock_cell = {0};
        uint32_t mock_root = 0;
        if (!allocate_mock_root_page(pager, &mock_root)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table15", mock_root, &mock_cell)) { return -1; }

        mock_cell.keys[1]->value.uint32_val = 2;
        if (mock_cell.keys[2]->value.char_val.string) { free(mock_cell.keys[2]->value.char_val.string); }
        mock_cell.keys[2]->value.char_val.string = strdup("pk_table_id");

        CatalogStatus status = catalog_delete_record(catalog, &mock_cell);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        btree_cell_contents_free(&mock_cell, &catalog->spec);
    }

    /* ----- DELETE NON-EXISTENT CATALOG CELL ----- */
    {
        BTreeCellContents mock_cell = {0};
        uint32_t mock_root = 0;
        if (!allocate_mock_root_page(pager, &mock_root)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table28", mock_root, &mock_cell)) { return -1; }

        CatalogStatus status = catalog_delete_record(catalog, &mock_cell);
        ASSERT(status == CATALOG_NOT_FOUND);

        btree_cell_contents_free(&mock_cell, &catalog->spec);
    }

    return 0;
}

static int test_catalog_update_record() {
    const char *pathname = "build/catalog_db7_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* Max out system catalog empty leaf node root. */
    int res = insert_catalog_cells(catalog, 0, 27);
    if (res == -1) { return -1; }

    /* ----- NORMAL RECORD UPDATE ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table07", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        uint32_t new_root_page_num = 0;
        if (!allocate_mock_root_page(pager, &new_root_page_num)) { return -1; }

        uint32_t old_root_page_num = lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;
        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        CatalogStatus status = catalog_update_record(catalog, &record_info, new_root_page_num);
        ASSERT(status == CATALOG_SUCCESS);

        lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        ASSERT(old_root_page_num != lookup_result.records[0].cell->BTreePayload.catalog->root_page_num);
        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* ----- UPDATE WITH MERGE & ROOT COLLAPSE ----- */
    {
        /* Split it. */
        res = insert_catalog_cells(catalog, 27, 28);
        if (res == -1) { return -1; }

        /* Underflow leaf until merge. */
        char *tables_left[12] = {"table01", "table02", "table03", "table04", "table05",
                                 "table06", "table08", "table09", "table10", "table11",
                                 "table13", "table15"};

        BTreePage leaf_page = {0};
        for (uint32_t i = 0; i < 10; i++) {

            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, tables_left[i], 64);
            record_info.type = CATALOG_TABLE;
            strncpy(record_info.object_name, "", 64);
            record_info.root_page_num = UINT32_MAX;
            
            CatalogLookupResult lookup_result = {0};
            CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
            if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

            CatalogStatus status = catalog_delete_record(catalog, lookup_result.records[0].cell);
            ASSERT(status == CATALOG_SUCCESS);

            catalog_lookup_result_free(&lookup_result, &catalog->spec);

            BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_page, pager->pages[1], &catalog->spec);
            if (btree_status != BTREE_SUCCESS) { return -1; }
        }

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table17", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        uint32_t new_root_page_num = 0;
        if (!allocate_mock_root_page(pager, &new_root_page_num)) { return -1; }

        uint32_t old_root_page_num = lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;
        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        CatalogStatus status = catalog_update_record(catalog, &record_info, new_root_page_num);
        ASSERT(status == CATALOG_SUCCESS);

        lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        ASSERT(old_root_page_num != lookup_result.records[0].cell->BTreePayload.catalog->root_page_num);
        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_record_update() {
    const char *pathname = "build/catalog_db7_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- UPDATE FROM EMPTY CATALOG ----- */
    {
        BTreeCellContents mock_cell = {0};
        uint32_t mock_root = 0;
        if (!allocate_mock_root_page(pager, &mock_root)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table00", mock_root, &mock_cell)) { return -1; }

        CatalogRecordInfo record_info = {0};
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &mock_cell, &record_info)) { return -1; }

        uint32_t new_mock_root = 0;
        if (!allocate_mock_root_page(pager, &new_mock_root)) { return -1; }

        CatalogStatus status = catalog_update_record(catalog, &record_info, new_mock_root);
        ASSERT(status == CATALOG_ERROR);

        btree_cell_contents_free(&mock_cell, &catalog->spec);
    }

    /* Max out system catalog empty leaf node root. */
    int res = insert_catalog_cells(catalog, 0, 27);
    if (res == -1) { return -1; }

    /* ----- INVALID ARGUMENTS ----- */
    {
        BTreeCellContents mock_cell = {0};
        uint32_t mock_root = 0;
        if (!allocate_mock_root_page(pager, &mock_root)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table30", mock_root, &mock_cell)) { return -1; }

        CatalogRecordInfo record_info = {0};
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &mock_cell, &record_info)) { return -1; }

        uint32_t new_mock_root = 0;
        if (!allocate_mock_root_page(pager, &new_mock_root)) { return -1; }

        record_info.type = CATALOG_INDEX;
        memset(record_info.object_name, 0, 64);

        CatalogStatus status = catalog_update_record(catalog, &record_info, new_mock_root);
        ASSERT(status == CATALOG_INVALID_ARGUMENTS);

        btree_cell_contents_free(&mock_cell, &catalog->spec);
    }

    /* ----- UPDATE NON-EXISTENT CATALOG CELL ----- */
    {
        BTreeCellContents mock_cell = {0};
        uint32_t mock_root = 0;
        if (!allocate_mock_root_page(pager, &mock_root)) { return -1; }
        if (!create_mock_catalog_cell(catalog, "table30", mock_root, &mock_cell)) { return -1; }

        CatalogRecordInfo record_info = {0};
        if (!btree_cell_contents_to_catalog_record_info(&catalog->spec, &mock_cell, &record_info)) { return -1; }

        uint32_t new_mock_root = 0;
        if (!allocate_mock_root_page(pager, &new_mock_root)) { return -1; }

        CatalogStatus status = catalog_update_record(catalog, &record_info, new_mock_root);
        ASSERT(status == CATALOG_ERROR);

        btree_cell_contents_free(&mock_cell, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_catalog_scan() {
    const char *pathname = "build/catalog_db8_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* Max out system catalog empty leaf node root. */
    int res = insert_catalog_cells(catalog, 0, 1);
    if (res == -1) { return -1; }

    /* ----- CATALOG SCAN IN PAGE WITH ONLY 1 CELL ----- */
    {
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_scan(catalog, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);

        ASSERT(lookup_result.num_records == 1);
        for (uint32_t i = 0; i < lookup_result.num_records; i++) {
            ASSERT(lookup_result.records[i].cell != NULL);
            ASSERT(lookup_result.records[i].cell->type == BTREE_LEAF_NODE);
            ASSERT(lookup_result.records[i].cell->BTreePayload.catalog != NULL);
        }

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    res = insert_catalog_cells(catalog, 1, 27);
    if (res == -1) { return -1; }

    /* ----- CATALOG SCAN IN SINGLE LEAF ROOT PAGE ----- */
    {
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_scan(catalog, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);

        ASSERT(lookup_result.num_records == 27);
        for (uint32_t i = 0; i < lookup_result.num_records; i++) {
            ASSERT(lookup_result.records[i].cell != NULL);
            ASSERT(lookup_result.records[i].cell->type == BTREE_LEAF_NODE);
            ASSERT(lookup_result.records[i].cell->BTreePayload.catalog != NULL);
        }

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* Split it. */
    res = insert_catalog_cells(catalog, 27, 28);
    if (res == -1) { return -1; }
    
    /* ----- CATALOG SCAN IN 2 CATALOG LEAF NODES (after split) ----- */
    {
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_scan(catalog, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);

        ASSERT(lookup_result.num_records == 28);
        for (uint32_t i = 0; i < lookup_result.num_records; i++) {
            ASSERT(lookup_result.records[i].cell != NULL);
            ASSERT(lookup_result.records[i].cell->type == BTREE_LEAF_NODE);
            ASSERT(lookup_result.records[i].cell->BTreePayload.catalog != NULL);
        }

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* Underflow leaf until merge. */
    char *tables_left[15] = {"table01", "table02", "table03", "table04", "table05",
                            "table06", "table08", "table09", "table10", "table11",
                            "table13", "table15", "table16", "table17", "table18"};

    BTreePage leaf_page = {0};
    for (uint32_t i = 0; i < 15; i++) {

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, tables_left[i], 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;
        
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(catalog, &record_info, &lookup_result);
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        CatalogStatus status = catalog_delete_record(catalog, lookup_result.records[0].cell);
        ASSERT(status == CATALOG_SUCCESS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        BTreeStatus btree_status = btree_page_attach_load_validate(pager, &leaf_page, pager->pages[1], &catalog->spec);
        if (btree_status != BTREE_SUCCESS) { return -1; }
    }
    
    /* ----- CATALOG SCAN AFTER LEAF NODE MERGE ----- */
    {
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_scan(catalog, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);

        ASSERT(lookup_result.num_records == 13);
        for (uint32_t i = 0; i < lookup_result.num_records; i++) {
            ASSERT(lookup_result.records[i].cell != NULL);
            ASSERT(lookup_result.records[i].cell->type == BTREE_LEAF_NODE);
            ASSERT(lookup_result.records[i].cell->BTreePayload.catalog != NULL);
        }

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_invalid_catalog_scan() {
    const char *pathname = "build/catalog_db8_2.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- SCAN IN EMPTY CATALOG LEAF NODE ROOT ----- */
    {
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_scan(catalog, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_ERROR);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    int res = insert_catalog_cells(catalog, 1, 27);
    if (res == -1) { return -1; }

    /* ----- INVALID ARGUMENTS ----- */
    {
        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_scan(NULL, &lookup_result);
        ASSERT(lookup_status == CATALOG_LOOKUP_INVALID_ARGUMENTS);
    }

    catalog_free(catalog);
    pager_close(pager);
    return 0;
}

static int test_catalog_mixed_record_operations() {
    const char *pathname = "build/catalog_db9_1.db";
    Pager *pager = pager_open(pathname);
    if (!pager) { return -1; }

    uint32_t system_catalog_root_page_num = UINT32_MAX;
    Catalog *catalog = catalog_create(pager, &system_catalog_root_page_num);
    if (!catalog) { return -1; }

    /* ----- INSERT CATALOG TABLE AND INDEX CELLS ----- */
    for (uint32_t i = 0; i < 20; i++) {
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

            if (!allocate_mock_root_page(pager, &record_info.root_page_num)) {
                return -1;
            }

            record_info.object.table = create_mock_table(pager, table_name);
            if (!record_info.object.table) { return -1; }

            BTreeCellContents cell = {0};

            CatalogStatus status = catalog_create_record(
                catalog,
                &record_info,
                &cell
            );
            ASSERT(status == CATALOG_SUCCESS);

            status = catalog_insert_record(catalog, &cell);
            ASSERT(status == CATALOG_SUCCESS);

            btree_cell_contents_free(&cell, &catalog->spec);
            table_free((Table *) record_info.object.table);
        }

        /* ----- CATALOG INDEX ----- */
        {
            CatalogRecordInfo record_info = {0};
            strncpy(record_info.table_name, table_name, 64);
            record_info.type = CATALOG_INDEX;
            strncpy(record_info.object_name, index_name, 64);

            if (!allocate_mock_root_page(pager, &record_info.root_page_num)) {
                return -1;
            }

            record_info.object.table = create_mock_table(pager, table_name);
            if (!record_info.object.table) { return -1; }

            BTreeCellContents cell = {0};

            CatalogStatus status = catalog_create_record(
                catalog,
                &record_info,
                &cell
            );
            ASSERT(status == CATALOG_SUCCESS);

            status = catalog_insert_record(catalog, &cell);
            ASSERT(status == CATALOG_SUCCESS);

            btree_cell_contents_free(&cell, &catalog->spec);
            table_free((Table *) record_info.object.table);
        }
    }

    /* ----- LOOKUP VARIOUS TABLE CELLS ----- */
    uint32_t table_lookup_indexes[4] = {0, 5, 12, 19};

    for (uint32_t i = 0; i < 4; i++) {
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%02d", table_lookup_indexes[i]);

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, table_name, 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );

        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);
        ASSERT(lookup_result.records[0].cell != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->type == CATALOG_TABLE);

        ASSERT(!strcmp(
            lookup_result.records[0].cell->keys[0]->value.char_val.string,
            table_name
        ));

        ASSERT(lookup_result.records[0].cell->keys[1]->value.uint32_val
               == (uint32_t) CATALOG_TABLE);

        ASSERT(!strcmp(
            lookup_result.records[0].cell->keys[2]->value.char_val.string,
            ""
        ));

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* ----- LOOKUP VARIOUS INDEX CELLS ----- */
    uint32_t index_lookup_indexes[4] = {1, 7, 13, 18};

    for (uint32_t i = 0; i < 4; i++) {
        char table_name[64] = {0};
        char index_name[64] = {0};

        snprintf(table_name, 64, "table%02d", index_lookup_indexes[i]);
        snprintf(index_name, 64, "index%02d", index_lookup_indexes[i]);

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, table_name, 64);
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, index_name, 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );

        ASSERT(lookup_status == CATALOG_LOOKUP_SUCCESS);
        ASSERT(lookup_result.records[0].cell != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog != NULL);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->type == CATALOG_INDEX);

        ASSERT(!strcmp(
            lookup_result.records[0].cell->keys[0]->value.char_val.string,
            table_name
        ));

        ASSERT(lookup_result.records[0].cell->keys[1]->value.uint32_val
               == (uint32_t) CATALOG_INDEX);

        ASSERT(!strcmp(
            lookup_result.records[0].cell->keys[2]->value.char_val.string,
            index_name
        ));

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* ----- DELETE VARIOUS TABLE CELLS ----- */
    uint32_t table_delete_indexes[4] = {2, 6, 11, 17};

    for (uint32_t i = 0; i < 4; i++) {
        char table_name[64] = {0};
        snprintf(table_name, 64, "table%02d", table_delete_indexes[i]);

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, table_name, 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        CatalogStatus status = catalog_delete_record(
            catalog,
            lookup_result.records[0].cell
        );
        ASSERT(status == CATALOG_SUCCESS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        lookup_result = (CatalogLookupResult) {0};

        lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );

        ASSERT(lookup_status == CATALOG_LOOKUP_NOT_FOUND);
    }

    /* ----- DELETE VARIOUS INDEX CELLS ----- */
    uint32_t index_delete_indexes[4] = {3, 8, 14, 16};

    for (uint32_t i = 0; i < 4; i++) {
        char table_name[64] = {0};
        char index_name[64] = {0};

        snprintf(table_name, 64, "table%02d", index_delete_indexes[i]);
        snprintf(index_name, 64, "index%02d", index_delete_indexes[i]);

        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, table_name, 64);
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, index_name, 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        CatalogStatus status = catalog_delete_record(
            catalog,
            lookup_result.records[0].cell
        );
        ASSERT(status == CATALOG_SUCCESS);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        lookup_result = (CatalogLookupResult) {0};

        lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );

        ASSERT(lookup_status == CATALOG_LOOKUP_NOT_FOUND);
    }

    /* ----- UPDATE TABLE CELL ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table05", 64);
        record_info.type = CATALOG_TABLE;
        strncpy(record_info.object_name, "", 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        uint32_t old_root_page_num =
            lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        uint32_t new_root_page_num = 0;
        if (!allocate_mock_root_page(pager, &new_root_page_num)) {
            return -1;
        }

        CatalogStatus status = catalog_update_record(
            catalog,
            &record_info,
            new_root_page_num
        );
        ASSERT(status == CATALOG_SUCCESS);

        lookup_result = (CatalogLookupResult) {0};

        lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->root_page_num
               == new_root_page_num);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->root_page_num
               != old_root_page_num);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    /* ----- UPDATE INDEX CELL ----- */
    {
        CatalogRecordInfo record_info = {0};
        strncpy(record_info.table_name, "table12", 64);
        record_info.type = CATALOG_INDEX;
        strncpy(record_info.object_name, "index12", 64);
        record_info.root_page_num = UINT32_MAX;

        CatalogLookupResult lookup_result = {0};
        CatalogLookupStatus lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        uint32_t old_root_page_num =
            lookup_result.records[0].cell->BTreePayload.catalog->root_page_num;

        catalog_lookup_result_free(&lookup_result, &catalog->spec);

        uint32_t new_root_page_num = 0;
        if (!allocate_mock_root_page(pager, &new_root_page_num)) {
            return -1;
        }

        CatalogStatus status = catalog_update_record(
            catalog,
            &record_info,
            new_root_page_num
        );
        ASSERT(status == CATALOG_SUCCESS);

        lookup_result = (CatalogLookupResult) {0};

        lookup_status = catalog_lookup_record(
            catalog,
            &record_info,
            &lookup_result
        );
        if (lookup_status != CATALOG_LOOKUP_SUCCESS) { return -1; }

        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->root_page_num
               == new_root_page_num);
        ASSERT(lookup_result.records[0].cell->BTreePayload.catalog->root_page_num
               != old_root_page_num);

        catalog_lookup_result_free(&lookup_result, &catalog->spec);
    }

    catalog_free(catalog);
    pager_close(pager);
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

    result = unlink("build/catalog_db1_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db1_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db1_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db2_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db2_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db3_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db3_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db4_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db4_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db5_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db5_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db6_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db6_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db7_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db7_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db8_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db8_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db9_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    result = test_catalog_create();
    generate_output(result, 0, "test_catalog_create");
    result = test_invalid_catalog_creation();
    generate_output(result, 1, "test_invalid_catalog_creation");
    result = test_catalog_create_record();
    generate_output(result, 2, "test_catalog_create_record");
    result = test_invalid_catalog_record_creation();
    generate_output(result, 3, "test_invalid_catalog_record_creation");
    result = test_catalog_metadata_pages();
    generate_output(result, 4, "test_catalog_metadata_pages");
    result = test_invalid_catalog_metadata_pages();
    generate_output(result, 5, "test_invalid_catalog_metadata_pages");
    result = test_catalog_insert_record();
    generate_output(result, 6, "test_catalog_insert_record");
    result = test_invalid_catalog_record_insertion();
    generate_output(result, 7, "test_invalid_catalog_record_insertion");
    result = test_catalog_lookup_record();
    generate_output(result, 8, "test_catalog_lookup_record");
    result = test_invalid_catalog_record_lookup();
    generate_output(result, 9, "test_invalid_catalog_record_lookup");
    result = test_catalog_delete_record();
    generate_output(result, 10, "test_catalog_delete_record");
    result = test_invalid_catalog_record_deletion();
    generate_output(result, 11, "test_invalid_catalog_record_deletion");
    result = test_catalog_update_record();
    generate_output(result, 12, "test_catalog_update_record");
    result = test_invalid_catalog_record_update();
    generate_output(result, 13, "test_invalid_catalog_record_update");
    result = test_catalog_scan();
    generate_output(result, 14, "test_catalog_scan");
    result = test_invalid_catalog_scan();
    generate_output(result, 15, "test_invalid_catalog_scan");
    result = test_catalog_mixed_record_operations();
    generate_output(result, 16, "test_catalog_mixed_record_operations");
    
    result = unlink("build/catalog_db1_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db1_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db1_3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db2_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db2_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db3_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db3_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db4_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db4_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db5_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db5_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db6_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db6_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db7_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db7_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db8_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db8_2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/catalog_db9_1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    return 0;
}
