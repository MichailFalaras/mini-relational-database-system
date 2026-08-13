#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/serialize.h"
#include "../../include/index.h"
#include "../../include/row.h"
#include "../../include/data_types.h"
#include "../src/data_types/data_types_utils.h"
#include "../src/btree/btree_utils.h"
#include "../../include/schema.h"
#include "../../include/catalog.h"

/* Serialize/Deserialize cell contents type agnostic functions. */
bool serialize_cell_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell) {
    if (!write_offset || !btree_page || !btree_page->page
        || !btree_page->data || !cell) {
        return false;
    }

    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return serialize_internal_node(write_offset, cell);

        case BTREE_LEAF_NODE:
            return serialize_leaf_node(write_offset, cell);

        default:
            fprintf(stderr, "serialize_cell_contents: BTreePage type is not valid.\n");
            return false;
    }
}

bool deserialize_cell_contents(const Schema *schema, uint8_t *read_offset, BTreePage *btree_page,
    BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *index) {
    if (!schema || !read_offset || !btree_page || !btree_page->page
        || !btree_page->data || !cell_view || !cell || !index) {         
        return false;
    }

    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            if (!deserialize_internal_node(read_offset, cell_view, cell, index)) {
                return false;
            }

            break;
        case BTREE_LEAF_NODE:
            if (!deserialize_leaf_node(schema, read_offset, cell_view, cell, index)) {
                return false;
            }

            break;
        default:
            fprintf(stderr, "serialize_cell_contents: BTreePage type is not valid.\n");
            return false;
    }

    return true;
}

/* Serialize/Deserialize leaf node cell metadata. */
bool serialize_leaf_node(uint8_t *write_offset, BTreeCellContents *cell) {
    if (!write_offset || !cell) {
        return false;
    }

    if (!serialize_keys(&write_offset, cell)) {
        return false;
    }

    if (!serialize_row(&write_offset, cell->BTreePayload.row)) {
        return false;
    }

    return true;
}

bool deserialize_leaf_node(const Schema *schema, uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *index) {
    if (!read_offset || !cell || !cell_view || !index) {
        return false;
    }
    cell->key_size = cell_view->key.key_size;
    cell->num_keys = index->index_key->num_columns;
    cell->cell_size = cell_view->payload_size + cell->key_size;

    cell->keys = (Value **) calloc(index->index_key->num_columns, sizeof(Value *));
    if (!cell->keys) {
        return false;
    }
    
    read_offset += cell_view->offset;
    if (!deserialize_keys(&read_offset, cell, index)) {
        return false;
    }

    if (!deserialize_row(schema, &read_offset, cell)) {
        value_free_array(cell->keys, cell->num_keys);
        return false;
    }

    return true;
}

/* Serialize/Deserialize internal node cell metadata. */
bool serialize_internal_node(uint8_t *write_offset, BTreeCellContents *cell) {
    if (!write_offset || !cell) {
        return false;
    }

    memcpy(write_offset, &cell->BTreePayload.child_pointer, sizeof(uint32_t));
    write_offset += sizeof(uint32_t);
    
    if (!serialize_keys(&write_offset, cell)) {
        return false;
    }

    return true;
}

bool deserialize_internal_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *index) {
    if (!read_offset || !cell_view || !cell || !index) {
        return false;
    }

    cell->num_keys = index->index_key->num_columns;
    cell->key_size = cell_view->key.key_size;
    cell->cell_size = cell->key_size + sizeof(uint32_t);

    memcpy(&cell->BTreePayload.child_pointer, read_offset, sizeof(uint32_t));
    read_offset += sizeof(uint32_t);

    cell->keys = (Value **) calloc(index->index_key->num_columns, sizeof(Value *));
    if (!cell->keys) {
        return false;
    }

    if (!deserialize_keys(&read_offset, cell, index)) {
        return false;
    }

    return true;
}

/* Serialize/Deserialize keys. */
bool serialize_keys(uint8_t **write_offset, BTreeCellContents *cell) {
    if (!write_offset || !cell) {
        return false;
    }

    for (uint32_t i = 0; i < cell->num_keys; i++) {
        if (!serialize_value_data(cell->keys[i], *write_offset)) {
            return false;
        }

        *write_offset += get_data_type_size(cell->keys[i]->type);
    }

    return true;
}

bool deserialize_keys(uint8_t **read_offset, BTreeCellContents *cell, BTreeIndexSpec *index) {
    if (!read_offset || !cell || !index) {
        return false;
    }

    for (uint32_t i = 0; i < index->index_key->num_columns; i++) {
        cell->keys[i] = deserialize_value_data(index->column_types[i], *read_offset);
        if (!cell->keys[i]) {
            value_free_array(cell->keys, cell->num_keys);
            return false;
        }

        *read_offset += get_data_type_size(index->column_types[i]);
    }

    return true;
}

/* Serialize/Deserialize Row metadata. */
bool serialize_row(uint8_t **write_offset, const Row *row) {
    if (!write_offset || !row) {
        return false;
    }

    memcpy(*write_offset, &row->is_deleted, sizeof(uint8_t));
    *write_offset += sizeof(uint8_t);

    memcpy(*write_offset, &row->n_columns, sizeof(uint32_t));
    *write_offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < row->n_columns; i++) {
        if (!serialize_value_data(row->values[i], *write_offset)) {
            return false;
        }
        
        *write_offset += get_data_type_size(row->values[i]->type);
    }

    return true;
}

bool deserialize_row(const Schema *schema, uint8_t **read_offset, BTreeCellContents *cell) {
    if (!schema || !read_offset || !cell) {
        return false;
    }

    cell->BTreePayload.row = (Row *) calloc(1, sizeof(Row));
    if (!cell->BTreePayload.row) {
        return false;
    }

    memcpy(&cell->BTreePayload.row->is_deleted, *read_offset, sizeof(uint8_t));
    *read_offset += sizeof(uint8_t);

    memcpy(&cell->BTreePayload.row->n_columns, *read_offset, sizeof(uint32_t));
    *read_offset += sizeof(uint32_t);

    if (cell->BTreePayload.row->n_columns != schema->num_columns) {
        row_free(cell->BTreePayload.row);
        return false;
    }

    cell->BTreePayload.row->values = (Value **) calloc(cell->BTreePayload.row->n_columns, sizeof(Value *));
    if (!cell->BTreePayload.row->values) {
        row_free(cell->BTreePayload.row);
        return false;
    }

    for (uint32_t i = 0; i < cell->BTreePayload.row->n_columns; i++) {
        cell->BTreePayload.row->values[i] = deserialize_value_data(schema->columns[i]->type, *read_offset);
        if (!cell->BTreePayload.row->values[i]) {
            value_free_array(cell->BTreePayload.row->values, cell->BTreePayload.row->n_columns);
            row_free(cell->BTreePayload.row);
            return false;
        }

        *read_offset += get_data_type_size(schema->columns[i]->type);
    }

    return true;
}

/* Serialize/Deserialize value data. */
bool serialize_value_data(Value *value, void *serialized_output) {
    if (!value || !serialized_output) {
        return false;
    }

    switch (value->type) {
        case INTEGER:
            memcpy(serialized_output, &value->value.int32_val, get_data_type_size(INTEGER));
            break;
        case UNSIGNED_INTEGER:
            memcpy(serialized_output, &value->value.uint32_val, get_data_type_size(UNSIGNED_INTEGER));
            break;
        case NUMERIC:
            memcpy(serialized_output, &value->value.numeric_val, get_data_type_size(NUMERIC));
            break;
        case FLOAT:
            memcpy(serialized_output, &value->value.float_val, get_data_type_size(FLOAT));
            break;
        case DOUBLE:
            memcpy(serialized_output, &value->value.double_val, get_data_type_size(DOUBLE));
            break;
        case CHAR:
            memcpy(serialized_output, &value->value.char_val, get_data_type_size(CHAR));
            break;
        case VARCHAR:
            memcpy(serialized_output, &value->value.varchar_val, get_data_type_size(VARCHAR));
            break;
        case TEXT:
            memcpy(serialized_output, &value->value.text_val, get_data_type_size(TEXT));
            break;
        case DATE:
            memcpy(serialized_output, &value->value.date_val, get_data_type_size(DATE));
            break;
        case TIMESTAMP:
            memcpy(serialized_output, &value->value.timestamp_val, get_data_type_size(TIMESTAMP));
            break;
        case BLOB:
            memcpy(serialized_output, &value->value.blob_val, get_data_type_size(BLOB));
            break;
        case BOOL:
            memcpy(serialized_output, &value->value.bool_val, get_data_type_size(BOOL));
            break;
        case JSONB:
            memcpy(serialized_output, &value->value.jsonb_val, get_data_type_size(JSONB));
            break;
        case NULL_TYPE:
            memcpy(serialized_output, &value->value.null_val, get_data_type_size(NULL_TYPE));
            break;
        default:
            printf("serialize_value_data: Unsupported data type.\n");
            return false;
    }

    return true;
}

Value *deserialize_value_data(DataType type, void *offset) {
    if (!offset) {
        return NULL;
    }

    Value *value = (Value *) malloc(sizeof(Value));
    if (!value) {
        return NULL;
    }
    value->type = type;

    switch (type) {
        case INTEGER:
            memcpy(&value->value.int32_val, offset, get_data_type_size(INTEGER));
            break;
        case UNSIGNED_INTEGER:
            memcpy(&value->value.uint32_val, offset, get_data_type_size(UNSIGNED_INTEGER));
            break;
        case NUMERIC:
            memcpy(&value->value.numeric_val, offset, get_data_type_size(NUMERIC));
            break;
        case FLOAT:
            memcpy(&value->value.float_val, offset, get_data_type_size(FLOAT));
            break;
        case DOUBLE:
            memcpy(&value->value.double_val, offset, get_data_type_size(DOUBLE));
            break;
        case CHAR:
            memcpy(&value->value.char_val, offset, get_data_type_size(CHAR));
            break;
        case VARCHAR:
            memcpy(&value->value.varchar_val, offset, get_data_type_size(VARCHAR));
            break;
        case TEXT:
            memcpy(&value->value.text_val, offset, get_data_type_size(TEXT));
            break;
        case DATE:
            memcpy(&value->value.date_val, offset, get_data_type_size(DATE));
            break;
        case TIMESTAMP:
            memcpy(&value->value.timestamp_val, offset, get_data_type_size(TIMESTAMP));
            break;
        case BLOB:
            memcpy(&value->value.blob_val, offset, get_data_type_size(BLOB));
            break;
        case BOOL:
            memcpy(&value->value.bool_val, offset, get_data_type_size(BOOL));
            break;
        case JSONB:
            memcpy(&value->value.jsonb_val, offset, get_data_type_size(JSONB));
            break;
        case NULL_TYPE:
            memcpy(&value->value.null_val, offset, get_data_type_size(NULL_TYPE));
            break;
        default:
            printf("serialize_value_data: Unsupported data type.\n");
            return NULL;
    }

    return value;
} 

/* Serialize catalog contents */
bool serialize_catalog_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell) {
    if (!write_offset || !btree_page || !cell) {
        printf("serialize_catalog_contents: Invalid input data.\n");
        return false;
    }

    switch(btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return serialize_internal_node(write_offset, cell);

        case BTREE_LEAF_NODE:
            return serialize_catalog_leaf_node(write_offset, cell);

        default:
            printf("serialize_catalog_contents: BTreePage type is not valid.\n");
            return false;
    }
}

/* Serialize catalog leaf key + payload */
bool serialize_catalog_leaf_node(uint8_t *write_offset, BTreeCellContents *cell) {
    if (!write_offset || !cell) {
        printf("serialize_catalog_leaf_node: Invalid input data.\n");
        return false;
    }

    // Validating catalog payload
    CatalogPayload *catalog = cell->BTreePayload.catalog;
    if (!catalog) {
        printf("serialize_catalog_leaf_node: Catalog payload is NULL.\n");
        return false;
    }

    if (catalog->ddl_size > 0 && !catalog->ddl) {
        printf("serialize_catalog_leaf_node: Invalid DDL payload.\n");
        return false;
    }
    
    // Serialize the Catalog key
    if (!serialize_keys(&write_offset, cell)) {
        return false;
    }

    // Serializing the Catalog payload
    memcpy(write_offset, &catalog->type, sizeof(CatalogEntryType));
    write_offset += sizeof(CatalogEntryType);

    memcpy(write_offset, &catalog->root_page_num, sizeof(uint32_t));
    write_offset += sizeof(uint32_t);

    memcpy(write_offset, &catalog->ddl_size, sizeof(uint32_t));
    write_offset += sizeof(uint32_t);

    if (catalog->ddl_size > 0) {
        memcpy(write_offset, catalog->ddl, catalog->ddl_size);
    }

    return true;
}

/* Deserialize catalog leaf payload */
bool deserialize_catalog_contents(uint8_t *read_offset, BTreePage *btree_page, BTreeCellView *cell_view, 
BTreeCellContents *cell, BTreeIndexSpec *index) {

    if (!read_offset || !btree_page || !btree_page->page || 
        !btree_page->data  || !cell || !cell_view || !index) {
        printf("deserialize_catalog_contents: Invalid input data.\n");
        return false;
    }

    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return deserialize_internal_node(read_offset, cell_view, cell, index);

        case BTREE_LEAF_NODE:
            return deserialize_catalog_leaf_node(read_offset, cell_view, cell, index);

        default:
            printf("deserialize_catalog_contents: BTreePage type is not valid.\n");
            return false;
    }

}

bool deserialize_catalog_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell,
BTreeIndexSpec *index) {
    if (!read_offset || !cell_view || !cell || !index || !index->index_key) {
        printf("deserialize_catalog_leaf_node: Invalid input data.\n");
        return false;
    }

    // Reject a payload that doesn't even have the fixed metadata
    const uint32_t fixed_payload_size = sizeof(CatalogEntryType) + sizeof(uint32_t) + sizeof(uint32_t);

    if (cell_view->payload_size < fixed_payload_size) {
        printf("deserialize_catalog_leaf_node: Catalog payload is too small.\n");
        return false;
    }

    // Setting key metadata
    cell->key_size = cell_view->key.key_size;
    cell->num_keys = index->index_key->num_columns;
    cell->cell_size = cell->key_size + cell_view->payload_size;

    cell->keys = (Value **) calloc(cell->num_keys, sizeof(Value *));
    if (!cell->keys) {
        printf("deserialize_catalog_leaf_node: Catalog key array could not be allocated.\n");
        return false;
    }

    // Deserializing the node's key
    if (!deserialize_keys(&read_offset, cell, index)) {
        cell->keys = NULL;
        return false;
    }

    // Allocating the catalog payload structure
    CatalogPayload *catalog = (CatalogPayload *) calloc(1, sizeof(CatalogPayload));
    if (!catalog) {
        printf("deserialize_catalog_leaf_node: Catalog payload could not be allocated.\n");

        value_free_array(cell->keys, cell->num_keys);
        cell->keys = NULL;
        return false;
    }

    // Deserializing the Catalog payload
    memcpy(&catalog->type, read_offset, sizeof(CatalogEntryType));
    read_offset += sizeof(CatalogEntryType);

    memcpy(&catalog->root_page_num, read_offset, sizeof(uint32_t));
    read_offset += sizeof(uint32_t);

    memcpy(&catalog->ddl_size, read_offset, sizeof(uint32_t));
    read_offset += sizeof(uint32_t);

    // Verifying the DDL statement's size before allocating it and deserializing it
    uint32_t remaining_payload_size = cell_view->payload_size - fixed_payload_size;

    if (catalog->ddl_size != remaining_payload_size) {
        printf("deserialize_catalog_leaf_node: Invalid catalog DDL size.\n");
        free(catalog);
        
        value_free_array(cell->keys, cell->num_keys);
        cell->keys = NULL;
        return false;
    }

    // If there's actually a DDL statement, allocate its memory and deserialize it
    if (catalog->ddl_size > 0) {
        catalog->ddl = (char *) malloc((size_t) catalog->ddl_size + 1);

        if (!catalog->ddl) {
            printf("deserialize_catalog_leaf_node: DDL string could not be allocated.\n");
            free(catalog);

            value_free_array(cell->keys, cell->num_keys);
            cell->keys = NULL;
            return false;
        }

        memcpy(catalog->ddl, read_offset, catalog->ddl_size);

        catalog->ddl[catalog->ddl_size] = '\0';
    }

    if (catalog->type != CATALOG_TABLE && catalog->type != CATALOG_INDEX) {
        printf("deserialize_catalog_leaf_node: Invalid catalog entry type.\n");
        free(catalog->ddl);
        free(catalog);

        value_free_array(cell->keys, cell->num_keys);
        cell->keys = NULL;
        return false;
    }

    cell->BTreePayload.catalog = catalog;

    return true;
}
