#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/serialize.h"
#include "../../include/index.h"
#include "../../include/row.h"
#include "../../include/data_types.h"
#include "../src/data_types/data_types_utils.h"
#include "../../include/btree.h"
#include "../src/btree/btree_utils.h"
#include "../../include/schema.h"
#include "../../include/catalog.h"

/* Serialize/Deserialize cell contents type agnostic functions. */
bool serialize_cell_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !btree_page || !btree_page->page
        || !btree_page->data || !cell || !spec) {
        return false;
    }

    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return serialize_internal_node(write_offset, cell, spec);

        case BTREE_LEAF_NODE:
            return serialize_leaf_node(write_offset, cell, spec);

        default:
            fprintf(stderr, "serialize_cell_contents: BTreePage type is not valid.\n");
            return false;
    }
}

bool deserialize_cell_contents(uint8_t *read_offset, BTreePage *btree_page,
    BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!read_offset || !btree_page || !btree_page->page
        || !btree_page->data || !cell_view || !cell || !spec) {         
        return false;
    }

    cell->type = btree_page->type;
    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            if (!deserialize_internal_node(read_offset, cell_view, cell, spec)) {
                return false;
            }

            break;
        case BTREE_LEAF_NODE:
            if (!deserialize_leaf_node(read_offset, cell_view, cell, spec)) {
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
bool serialize_leaf_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !cell || !spec) {
        return false;
    }

    if (!serialize_keys(&write_offset, cell, spec)) {
        return false;
    }

    if (!serialize_row(&write_offset, cell->BTreePayload.row, spec)) {
        return false;
    }

    return true;
}

bool deserialize_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!read_offset || !cell || !cell_view || !spec) {
        return false;
    }
    cell->key_size = cell_view->key.key_size;
    cell->num_keys = spec->index_key->num_columns;
    cell->cell_size = cell_view->payload_size + cell->key_size;

    cell->keys = (Value **) calloc(spec->index_key->num_columns, sizeof(Value *));
    if (!cell->keys) {
        return false;
    }
    
    if (!deserialize_keys(&read_offset, cell, spec)) {
        return false;
    }

    if (!deserialize_row(&read_offset, cell, spec)) {
        value_free_array(cell->keys, cell->num_keys);
        return false;
    }

    return true;
}

/* Serialize/Deserialize internal node cell metadata. */
bool serialize_internal_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !cell || !spec) {
        return false;
    }

    memcpy(write_offset, &cell->BTreePayload.child_pointer, sizeof(uint32_t));
    write_offset += sizeof(uint32_t);
    
    if (!serialize_keys(&write_offset, cell, spec)) {
        return false;
    }

    return true;
}

bool deserialize_internal_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!read_offset || !cell_view || !cell || !spec) {
        return false;
    }

    cell->num_keys = spec->index_key->num_columns;
    cell->key_size = cell_view->key.key_size;
    cell->cell_size = cell->key_size + sizeof(uint32_t);

    memcpy(&cell->BTreePayload.child_pointer, read_offset, sizeof(uint32_t));
    read_offset += sizeof(uint32_t);

    cell->keys = (Value **) calloc(spec->index_key->num_columns, sizeof(Value *));
    if (!cell->keys) {
        return false;
    }

    if (!deserialize_keys(&read_offset, cell, spec)) {
        return false;
    }

    return true;
}

/* Serialize NULL bitmap right before serializing keys/row columns.
 *
 * Separating number of values in key and bitmap columns allows creating
 * prefix keys with NULL bitmap. */
bool serialize_null_bitmap(uint8_t **write_offset, Value **key, uint32_t num_vals, uint32_t bitmap_columns) {
    if (!write_offset || !key
        || !num_vals || !bitmap_columns
        || num_vals > bitmap_columns) {
        return false;
    }

    uint32_t bitmap_size = (bitmap_columns + 7) / 8;
    uint8_t *bitmap = (uint8_t *) malloc(bitmap_size);
    if (!bitmap) {
        return false;
    }
    memset(bitmap, 0, bitmap_size);

    for (uint32_t i = 0; i < num_vals; i++) {
        // To handle multi byte bitmaps
        uint32_t bitmap_spec = i / 8; 
        uint32_t bitmap_shift = i % 8;

        if (!key[i]) {
            free(bitmap);
            return false;
        }

        if (key[i]->null_val) {
            bitmap[bitmap_spec] |= 1 << bitmap_shift; 
        }
    }

    memcpy(*write_offset, bitmap, bitmap_size);
    *write_offset += bitmap_size;

    free(bitmap);
    return true;
}

/* Deserialize bitmap before deserializing keys/row columns. */
bool deserialize_null_bitmap(uint8_t **read_offset, uint8_t **bitmap, uint32_t num_columns) {
    if (!read_offset || !bitmap || num_columns == 0) {
        return false;
    }

    uint32_t bitmap_size = (num_columns + 7) / 8;
    *bitmap = (uint8_t *) malloc(bitmap_size);
    if (!(*bitmap)) {
        return false;
    }

    memcpy(*bitmap, *read_offset, bitmap_size);
    *read_offset += bitmap_size;

    return true;
}

/* Serialize/Deserialize keys. */
bool serialize_keys(uint8_t **write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !cell || !spec
        || !spec->index_key || cell->num_keys != spec->index_key->num_columns) {
        return false;
    }

    if (!serialize_null_bitmap(write_offset, cell->keys, cell->num_keys, cell->num_keys)) {
        return false;
    }

    for (uint32_t i = 0; i < cell->num_keys; i++) {
        if (!serialize_value_data(cell->keys[i], get_key_column(spec, i), *write_offset)) {
            return false;
        }

        *write_offset += get_serialized_key_size(spec, i);
    }

    return true;
}

bool deserialize_keys(uint8_t **read_offset, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!read_offset || !cell || !spec) {
        return false;
    }

    uint8_t *bitmap;
    if (!deserialize_null_bitmap(read_offset, &bitmap, spec->index_key->num_columns)) {
        value_free_array(cell->keys, cell->num_keys);
        cell->keys = NULL;
        return false;
    }

    uint32_t bitmap_size = (spec->index_key->num_columns + 7) / 8;
    uint32_t key_size = bitmap_size;
    for (uint32_t i = 0; i < spec->index_key->num_columns; i++) {
        uint32_t bitmap_spec = i / 8; 
        uint32_t bitmap_shift = i % 8;

        bool is_null = ((bitmap[bitmap_spec] & (1 << bitmap_shift)) != 0);

        cell->keys[i] = deserialize_value_data(get_key_column(spec, i), is_null, *read_offset);
        if (!cell->keys[i]) {
            value_free_array(cell->keys, cell->num_keys);
            free(bitmap);
            return false;
        }

        key_size += get_serialized_key_size(spec, i);
        *read_offset += get_serialized_key_size(spec, i);
    }
    cell->key_size = key_size;
    cell->num_keys = spec->index_key->num_columns;

    free(bitmap);
    return true;
}

/* Serialize/Deserialize Row metadata. */
bool serialize_row(uint8_t **write_offset, const Row *row, BTreeIndexSpec *spec) {
    if (!write_offset || !row || !row->values || !spec
        || !spec->index_key || row->n_columns != spec->schema->num_columns) {
        return false;
    }

    memcpy(*write_offset, &row->is_deleted, sizeof(uint8_t));
    *write_offset += sizeof(uint8_t);

    memcpy(*write_offset, &row->n_columns, sizeof(uint32_t));
    *write_offset += sizeof(uint32_t);

    if (!serialize_null_bitmap(write_offset, row->values, row->n_columns, row->n_columns)) {
        return false;
    }

    for (uint32_t i = 0; i < row->n_columns; i++) {
        if (!serialize_value_data(row->values[i], get_column(spec, i), *write_offset)) {
            return false;
        }
        
        *write_offset += get_serialized_column_size(spec, i);
    }

    return true;
}

bool deserialize_row(uint8_t **read_offset, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!read_offset || !cell || !spec) {
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

    if (cell->BTreePayload.row->n_columns != spec->schema->num_columns) {
        row_free(cell->BTreePayload.row);
        return false;
    }

    uint8_t *bitmap;
    if (!deserialize_null_bitmap(read_offset, &bitmap, cell->BTreePayload.row->n_columns)) {
        row_free(cell->BTreePayload.row);
        return false;
    }

    uint32_t bitmap_size = (cell->BTreePayload.row->n_columns + 7) / 8;
    uint32_t cell_size = cell->key_size
                        + sizeof(uint8_t)
                        + sizeof(uint32_t)
                        + bitmap_size;
    cell->BTreePayload.row->values = (Value **) calloc(cell->BTreePayload.row->n_columns, sizeof(Value *));
    if (!cell->BTreePayload.row->values) {
        row_free(cell->BTreePayload.row);
        free(bitmap);
        return false;
    }

    for (uint32_t i = 0; i < cell->BTreePayload.row->n_columns; i++) {
        uint32_t bitmap_spec = i / 8; 
        uint32_t bitmap_shift = i % 8;

        bool is_null = ((bitmap[bitmap_spec] & (1 << bitmap_shift)) != 0);

        cell->BTreePayload.row->values[i] = deserialize_value_data(get_column(spec, i), is_null, *read_offset);
        if (!cell->BTreePayload.row->values[i]) {
            row_free(cell->BTreePayload.row);
            free(bitmap);
            return false;
        }

        cell_size += get_serialized_column_size(spec, i);
        *read_offset += get_serialized_column_size(spec, i);
    }

    cell->cell_size = cell_size;
    free(bitmap);
    return true;
}

/* Serialize/Deserialize value data. */
bool serialize_value_data(Value *value, Column *column, void *serialized_output) {
    if (!value || !serialized_output || !column
        || value->type != column->type
        || !column->serialized_size) {
        return false;
    }

    if (value->null_val) {
        memset(serialized_output, 0, column->serialized_size);
        return true;
    }

    uint8_t *output = (uint8_t *) serialized_output;
    switch (value->type) {
        case INTEGER:
            memcpy(output, &value->value.int32_val, column->serialized_size);
            break;
        case UNSIGNED_INTEGER:
            memcpy(output, &value->value.uint32_val, column->serialized_size);
            break;
        case NUMERIC:
            memcpy(output, &value->value.numeric_val.val, sizeof(int64_t));
            output += sizeof(int64_t);

            memcpy(output, &value->value.numeric_val.scale, sizeof(uint32_t));
            break;
        case FLOAT:
            memcpy(output, &value->value.float_val, column->serialized_size);
            break;
        case DOUBLE:
            memcpy(output, &value->value.double_val, column->serialized_size);
            break;
        case CHAR: {
            if (!value->value.char_val.string) {
                return false;
            }

            size_t len = strlen(value->value.char_val.string);

            if (len > column->type_parameter) {
                return false;
            }

            memset(output, 0, column->serialized_size);
            memcpy(output, value->value.char_val.string, len);
            break;
        }
        case VARCHAR: {
            if (!value->value.varchar_val.string) {
                return false;
            }

            size_t len = strlen(value->value.varchar_val.string);

            if (len > column->type_parameter) {
                return false;
            }

            memset(output, 0, column->serialized_size);
            memcpy(output, value->value.varchar_val.string, len);
            break;
        }
        case TEXT: {
            if (!strlen(value->value.text_val)) {
                return false;
            }

            if (strlen(value->value.text_val) > column->serialized_size) {
                return false;
            }

            memset(output, 0, column->serialized_size);
            memcpy(output, value->value.text_val, strlen(value->value.text_val));
            break;
        }
        case DATE:
            memcpy(output, &value->value.date_val, column->serialized_size);
            break;
        case TIMESTAMP:
            memcpy(output, &value->value.timestamp_val, column->serialized_size);
            break;
        // case BLOB:
        //     memcpy(serialized_output, &value->value.blob_val.size, sizeof(uint32_t));
        //     serialized_output += sizeof(uint32_t);

        //     memcpy(serialized_output, value->value.blob_val.buffer, value->value.blob_val.size);
        //     break;
        case BOOL:
            memcpy(output, &value->value.bool_val, column->serialized_size);
            break;
        // case JSONB:
        //     memcpy(serialized_output, &value->value.jsonb_val.size, sizeof(uint32_t));
        //     serialized_output += sizeof(uint32_t);

        //     memcpy(serialized_output, value->value.jsonb_val.buffer, value->value.jsonb_val.size);
        //     break;
        default:
            printf("serialize_value_data: Unsupported data type.\n");
            return false;
    }

    return true;
}

Value *deserialize_value_data(Column *column, bool is_null, void *offset) {
    if (!column || !offset) {
        return NULL;
    }

    Value *value = (Value *) calloc(1, sizeof(Value));
    if (!value) {
        return NULL;
    }
    value->type = column->type;
    value->null_val = is_null;

    if (value->null_val) {
        memset(&(value->value), 0, sizeof(value->value));
        return value;
    }

    uint8_t *read_offset = (uint8_t *) offset;
    switch (column->type) {
        case INTEGER:
            memcpy(&value->value.int32_val, read_offset, column->serialized_size);
            break;
        case UNSIGNED_INTEGER:
            memcpy(&value->value.uint32_val, read_offset, column->serialized_size);
            break;
        case NUMERIC:
            memcpy(&value->value.numeric_val.val, read_offset, sizeof(int64_t));
            read_offset += sizeof(int64_t);

            memcpy(&value->value.numeric_val.scale, read_offset, sizeof(uint32_t));
            break;
        case FLOAT:
            memcpy(&value->value.float_val, read_offset, column->serialized_size);
            break;
        case DOUBLE:
            memcpy(&value->value.double_val, read_offset, column->serialized_size);
            break;
        case CHAR: {
            char *string = (char *) calloc(column->serialized_size+1, sizeof(char));
            if (!string) {
                value_free(value);
                return NULL;
            }

            memcpy(string, read_offset, column->serialized_size);
            value->value.char_val.n = column->type_parameter;
            value->value.char_val.string = string;
            break;
        }
        case VARCHAR: {
            char *string = (char *) calloc(column->serialized_size+1, sizeof(char));
            if (!string) {
                value_free(value);
                return NULL;
            }

            memcpy(string, read_offset, column->serialized_size);
            value->value.varchar_val.max_n = column->type_parameter;
            value->value.varchar_val.string = string;
            break;
        }
        case TEXT: {
            char *string = (char *) calloc(column->serialized_size+1, sizeof(char));
            if (!string) {
                value_free(value);
                return NULL;
            }

            memcpy(string, read_offset, column->serialized_size);
            value->value.text_val = string;
            break;
        }
        case DATE:
            memcpy(&value->value.date_val, read_offset, column->serialized_size);
            break;
        case TIMESTAMP:
            memcpy(&value->value.timestamp_val, read_offset, column->serialized_size);
            break;
        // case BLOB:
        //     memcpy(&value->value.blob_val.size, offset, sizeof(uint32_t));
        //     offset += sizeof(uint32_t);

        //     uint8_t *buffer = (uint8_t *) malloc(DATA_TYPE_BLOB_SIZE);
        //     if (!buffer) {
        //         value_free(value);
        //         return NULL;
        //     }

        //     memcpy(buffer, offset, DATA_TYPE_BLOB_SIZE);
        //     value->value.blob_val.buffer = buffer;
        //     break;
        case BOOL:
            memcpy(&value->value.bool_val, read_offset, column->serialized_size);
            break;
        // case JSONB:
        //     memcpy(&value->value.jsonb_val.size, offset, sizeof(uint32_t));
        //     offset += sizeof(uint32_t);

        //     uint8_t *buffer = (uint8_t *) malloc(DATA_TYPE_JSONB_SIZE);
        //     if (!buffer) {
        //         value_free(value);
        //         return NULL;
        //     }

        //     memcpy(buffer, offset, DATA_TYPE_JSONB_SIZE);
        //     value->value.jsonb_val.buffer = buffer;
        //     break;
        default:
            printf("deserialize_value_data: Unsupported data type.\n");
            free(value);
            return NULL;
    }

    return value;
} 

/* Serialize/Deserialize catalog contents */
bool serialize_catalog_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !btree_page || !cell || !spec) {
        printf("serialize_catalog_contents: Invalid input data.\n");
        return false;
    }

    switch(btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return serialize_internal_node(write_offset, cell, spec);

        case BTREE_LEAF_NODE:
            return serialize_catalog_leaf_node(write_offset, cell, spec);

        default:
            printf("serialize_catalog_contents: BTreePage type is not valid.\n");
            return false;
    }
}

bool deserialize_catalog_contents(uint8_t *read_offset, BTreePage *btree_page, BTreeCellView *cell_view, 
BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!read_offset || !btree_page || !btree_page->page || 
        !btree_page->data  || !cell || !cell_view || !spec) {
        printf("deserialize_catalog_contents: Invalid input data.\n");
        return false;
    }

    cell->type = btree_page->type;
    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return deserialize_internal_node(read_offset, cell_view, cell, spec);

        case BTREE_LEAF_NODE:
            return deserialize_catalog_leaf_node(read_offset, cell_view, cell, spec);

        default:
            printf("deserialize_catalog_contents: BTreePage type is not valid.\n");
            return false;
    }

}

/* Serialize/Deserialize catalog leaf key + payload */
bool serialize_catalog_leaf_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !cell || !spec) {
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
    if (!serialize_keys(&write_offset, cell, spec)) {
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

bool deserialize_catalog_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell,
BTreeIndexSpec *spec) {
    if (!read_offset || !cell_view || !cell || !spec || !spec->index_key) {
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
    cell->num_keys = spec->index_key->num_columns;
    cell->cell_size = cell->key_size + cell_view->payload_size;

    cell->keys = (Value **) calloc(cell->num_keys, sizeof(Value *));
    if (!cell->keys) {
        printf("deserialize_catalog_leaf_node: Catalog key array could not be allocated.\n");
        return false;
    }

    // Deserializing the node's key
    if (!deserialize_keys(&read_offset, cell, spec)) {
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
