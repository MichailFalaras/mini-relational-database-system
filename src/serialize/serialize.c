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
#include "../../include/table.h"
#include "../../include/constraints.h"
#include "../../include/expressions.h"
#include "../../include/encoding.h"

/* Serialize/Deserialize cell contents type agnostic functions. */
bool serialize_cell_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell, BTreeIndexSpec *spec) {
    if (!write_offset || !btree_page || !btree_page->page
        || !btree_page->data || !cell || !spec) {
        return false;
    }

    if (spec->payload_type != BTREE_ROW_PAYLOAD
        && spec->payload_type != BTREE_CATALOG_PAYLOAD) {
        return false;
    }

    switch (btree_page->type) {
        case BTREE_INTERNAL_NODE:
            return serialize_internal_node(write_offset, cell, spec);

        case BTREE_LEAF_NODE:
            if (spec->payload_type == BTREE_CATALOG_PAYLOAD) {
                return serialize_catalog_leaf_node(write_offset, cell, spec);
            }

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

    if (spec->payload_type != BTREE_ROW_PAYLOAD
        && spec->payload_type != BTREE_CATALOG_PAYLOAD) {
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
            if (spec->payload_type == BTREE_CATALOG_PAYLOAD) {
                if (!deserialize_catalog_leaf_node(read_offset, cell_view, cell, spec)) {
                    return false;
                }
                
                break;
            }

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

    store_u32_le(write_offset, cell->BTreePayload.child_pointer);
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

    cell->BTreePayload.child_pointer = load_u32_le(read_offset);
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

    store_u8_le(*write_offset, row->is_deleted);
    *write_offset += sizeof(uint8_t);

    store_u32_le(*write_offset, row->n_columns);
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

    cell->BTreePayload.row->is_deleted = load_u8_le(*read_offset);
    *read_offset += sizeof(uint8_t);

    cell->BTreePayload.row->n_columns = load_u32_le(*read_offset);
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
            store_i32_le(output, value->value.int32_val);
            break;
        case UNSIGNED_INTEGER:
            store_u32_le(output, value->value.uint32_val);
            break;
        case NUMERIC:
            store_i64_le(output, value->value.numeric_val.val);
            output += sizeof(int64_t);

            store_u32_le(output, value->value.numeric_val.scale);
            break;
        case FLOAT:
            store_float_le(output, value->value.float_val);
            break;
        case DOUBLE:
            store_double_le(output, value->value.double_val);
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
            store_u64_le(output, value->value.date_val);
            break;
        case TIMESTAMP:
            store_u64_le(output, value->value.timestamp_val);
            break;
        // case BLOB:
        //     memcpy(serialized_output, &value->value.blob_val.size, sizeof(uint32_t));
        //     serialized_output += sizeof(uint32_t);

        //     memcpy(serialized_output, value->value.blob_val.buffer, value->value.blob_val.size);
        //     break;
        case BOOL:
            store_u8_le(output, value->value.bool_val);
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
            value->value.int32_val = load_i32_le(read_offset);
            break;
        case UNSIGNED_INTEGER:
            value->value.uint32_val = load_u32_le(read_offset);
            break;
        case NUMERIC:
            value->value.numeric_val.val = load_i64_le(read_offset);
            read_offset += sizeof(int64_t);

            value->value.numeric_val.scale = load_u32_le(read_offset);
            break;
        case FLOAT:
            value->value.float_val = load_float_le(read_offset);
            break;
        case DOUBLE:
            value->value.double_val = load_double_le(read_offset);
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
            value->value.date_val = load_u64_le(read_offset);
            break;
        case TIMESTAMP:
            value->value.timestamp_val = load_u64_le(read_offset);
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
            value->value.bool_val = load_u8_le(read_offset);
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

/* ---------- CATALOG CONTENTS --------- */

/* Serialize/Deserialize Catalog Leaf Cell. */
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
    
    // Serialize the Catalog key
    if (!serialize_keys(&write_offset, cell, spec)) {
        return false;
    }

    if (catalog->type != CATALOG_TABLE && catalog->type != CATALOG_INDEX) {
        return false;
    }

    // Serializing the Catalog payload
    uint8_t disk_type = catalog->type == CATALOG_TABLE ? 0 : 1;
    store_u8_le(write_offset, disk_type);
    write_offset += sizeof(uint8_t);

    store_u32_le(write_offset, catalog->root_page_num);
    write_offset += sizeof(uint32_t);

    store_u32_le(write_offset, catalog->metadata_page_num);
    write_offset += sizeof(uint32_t);

    return true;
}

bool deserialize_catalog_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell,
    BTreeIndexSpec *spec) {
    if (!read_offset || !cell_view || !cell || !spec || !spec->index_key) {
        printf("deserialize_catalog_leaf_node: Invalid input data.\n");
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
    uint8_t disk_type = 0;
    disk_type = load_u8_le(read_offset);
    catalog->type = (CatalogEntryType) disk_type;
    read_offset += sizeof(uint8_t);

    catalog->root_page_num = load_u32_le(read_offset);
    read_offset += sizeof(uint32_t);

    catalog->metadata_page_num = load_u32_le(read_offset);
    read_offset += sizeof(uint32_t);

    if (catalog->type != CATALOG_TABLE && catalog->type != CATALOG_INDEX) {
        printf("deserialize_catalog_leaf_node: Invalid catalog entry type.\n");
        free(catalog);

        value_free_array(cell->keys, cell->num_keys);
        cell->keys = NULL;
        return false;
    }

    cell->BTreePayload.catalog = catalog;

    return true;
}

/* Serialization/Deserialization of Components stored in Metadata Page.*/
bool serialize_index_metadata(uint8_t **write_offset, const Index *index) {
    if (!write_offset || !*write_offset || !index || !index->key) {
        return false;
    }

    if (!index->key->num_columns || !index->key->column_index_array) {
        return false;
    }

    uint8_t index_type = (uint8_t) index->type;
    store_u8_le(*write_offset, index_type);
    *write_offset += sizeof(uint8_t);

    store_u32_le(*write_offset, index->key->num_columns); 
    *write_offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        store_u32_le(*write_offset, index->key->column_index_array[i]);
        *write_offset += sizeof(uint32_t);
    }

    uint8_t is_unique = index->is_unique ? 1 : 0;
    store_u8_le(*write_offset, is_unique);
    *write_offset += sizeof(uint8_t);

    return true;
}

bool serialize_table_metadata(uint8_t **write_offset, const Table *table) {
    if (!write_offset || !*write_offset || !table) {
        return false;
    }

    uint8_t is_materialized = table->is_materialized ? 1 : 0;
    store_u8_le(*write_offset, is_materialized);
    *write_offset += sizeof(uint8_t);

    // Should be increased when reconstructing indexes
    // memcpy(*write_offset, &(table->total_secondary_indexes), sizeof(uint32_t));
    // *write_offset += sizeof(uint32_t);

    store_u32_le(*write_offset, table->row_count);
    *write_offset += sizeof(uint32_t);

    if (!serialize_schema_metadata(write_offset, table->table_schema)) {
        return false;
    }

    return true;
}

bool serialize_schema_metadata(uint8_t **write_offset, const Schema *schema) {
    if (!write_offset || !*write_offset || !schema) {
        return false;
    }

    if (schema->num_columns > 0 && !schema->columns) {
        return false;
    }

    if (schema->num_constraints > 0 && !schema->constraints) {
        return false;
    }

    store_u32_le(*write_offset, schema->num_columns);
    *write_offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < schema->num_columns; i++) {
        if (!serialize_column(write_offset, schema->columns[i])) {
            return false;
        }
    }
    
    store_u32_le(*write_offset, schema->num_constraints);
    *write_offset += sizeof(uint32_t);
    
    for (uint32_t i = 0; i < schema->num_constraints; i++) {
        if (!serialize_constraint(write_offset, schema->constraints[i])) {
            return false;
        }
    }

    return true;
}

bool serialize_column(uint8_t **write_offset, const Column *column) {
    if (!write_offset || !*write_offset || !column) {
        return false;
    }

    memcpy(*write_offset, &(column->name), 64*sizeof(uint8_t));
    *write_offset += 64*sizeof(uint8_t);

    uint8_t data_type = (uint8_t) column->type;
    store_u8_le(*write_offset, data_type);
    *write_offset += sizeof(uint8_t);

    store_u32_le(*write_offset, column->type_parameter);
    *write_offset += sizeof(uint32_t);

    // Reconstructed during Column allocation
    // memcpy(*write_offset, &(column->serialized_size), sizeof(uint32_t));
    // *write_offset += sizeof(uint32_t);

    store_u32_le(*write_offset, column->non_null_rows);
    *write_offset += sizeof(uint32_t);

    store_u32_le(*write_offset, column->null_rows);
    *write_offset += sizeof(uint32_t);

    return true;
}

bool serialize_constraint(uint8_t **write_offset, const Constraint *constraint) {
    if (!write_offset || !*write_offset || !constraint) {
        return false;
    }

    memcpy(*write_offset, &(constraint->constraint_name), 64*sizeof(uint8_t));
    *write_offset += 64*sizeof(uint8_t);

    uint8_t constraint_type = (uint8_t) constraint->type;
    store_u8_le(*write_offset, constraint_type);
    *write_offset += sizeof(uint8_t);

    switch(constraint->type) {
        case PRIMARY_KEY: {
            if (constraint->constraint_data.primary_key.amount_columns > 0
                && !constraint->constraint_data.primary_key.primary_key_columns) {
                return false;
            }

            uint32_t amount_columns = (uint32_t) constraint->constraint_data.primary_key.amount_columns;
            store_u32_le(*write_offset, amount_columns);
            *write_offset += sizeof(uint32_t);

            for (uint32_t j = 0; j < constraint->constraint_data.primary_key.amount_columns; j++) {
                store_u32_le(*write_offset, constraint->constraint_data.primary_key.primary_key_columns[j]);
                *write_offset += sizeof(uint32_t);
            }

            break;
        }
        case FOREIGN_KEY:
            if (constraint->constraint_data.foreign_key.amount_columns > 0
                && !constraint->constraint_data.foreign_key.foreign_key_columns) {
                return false;
            }

            if (constraint->constraint_data.foreign_key.amount_referenced_columns > 0
                && !constraint->constraint_data.foreign_key.referenced_columns) {
                return false;
            }

            store_u32_le(*write_offset, constraint->constraint_data.foreign_key.amount_columns);
            *write_offset += sizeof(uint32_t);

            for (uint32_t j = 0; j < constraint->constraint_data.foreign_key.amount_columns; j++) {
                store_u32_le(*write_offset, constraint->constraint_data.foreign_key.foreign_key_columns[j]);
                *write_offset += sizeof(uint32_t);
            }

            memcpy(*write_offset, constraint->constraint_data.foreign_key.referenced_table_name, 64*sizeof(uint8_t));
            *write_offset += 64*sizeof(uint8_t);

            store_u32_le(*write_offset, constraint->constraint_data.foreign_key.amount_referenced_columns);
            *write_offset += sizeof(uint32_t);

            for (uint32_t j = 0; j < constraint->constraint_data.foreign_key.amount_referenced_columns; j++) {
                store_u32_le(*write_offset, constraint->constraint_data.foreign_key.referenced_columns[j]);
                *write_offset += sizeof(uint32_t);
            }

            break;
        case UNIQUE:
            if (constraint->constraint_data.unique_cols.amount_columns > 0
                && !constraint->constraint_data.unique_cols.column_refs) {
                return false;
            }

            store_u32_le(*write_offset, constraint->constraint_data.unique_cols.amount_columns);
            *write_offset += sizeof(uint32_t);

            for (uint32_t j = 0; j < constraint->constraint_data.unique_cols.amount_columns; j++) {
                store_u32_le(*write_offset, constraint->constraint_data.unique_cols.column_refs[j]);
                *write_offset += sizeof(uint32_t);
            }

            break;
        case CHECK:
            if (constraint->constraint_data.check.amount_columns > 0
                && !constraint->constraint_data.check.column_refs) {
                return false;
            }

            if (!constraint->constraint_data.check.constraint_expr) {
                return false;
            }

            store_u32_le(*write_offset, constraint->constraint_data.check.amount_columns);
            *write_offset += sizeof(uint32_t);

            for (uint32_t j = 0; j < constraint->constraint_data.check.amount_columns; j++) {
                store_u32_le(*write_offset, constraint->constraint_data.check.column_refs[j]);
                *write_offset += sizeof(uint32_t);
            }

            
            if (!serialize_expression_node(write_offset, constraint->constraint_data.check.constraint_expr)) {
                return false;
            }

            break;
        case NOT_NULL:
            store_u32_le(*write_offset, constraint->constraint_data.not_null.column_ref);
            *write_offset += sizeof(uint32_t);

            break;
        case DEFAULT:
            if (!constraint->constraint_data.default_value.default_expr) {
                return false;
            }

            store_u32_le(*write_offset, constraint->constraint_data.default_value.column_ref);
            *write_offset += sizeof(uint32_t);

            if (!serialize_expression_node(write_offset, constraint->constraint_data.default_value.default_expr)) {
                return false;
            }
            
            break;
        default:
            fprintf(stderr, "serialize_schema_metadata: Constraint type does not match.\n");
            return false;
    }

    return true;
}

/*
 * Serialization of literal values.
 * Different than normal Value serialization, where we use Schema/Column
 * to identify the properties of Row/Payload Values.
 * 
 * Supported literal types are:
 * INTEGER
 * NUMERIC
 * CHAR(n), where n = strlen(string_literal)
 * DATE
 * TIMESTAMP
 * BOOL
 */
bool serialize_literal_value(uint8_t **write_offset, const Value *literal) {
    if (!write_offset || !*write_offset || !literal) {
        return false;
    }

    uint8_t literal_type = (uint8_t) literal->type;
    store_u8_le(*write_offset, literal_type);
    *write_offset += sizeof(uint8_t);

    uint8_t null_val = literal->null_val ? 1 : 0;
    store_u8_le(*write_offset, null_val);
    *write_offset += sizeof(uint8_t);

    if (literal->null_val) {
        return true; // Everything needed was already serialized for NULL
    }

    switch (literal->type) {
        case INTEGER:
            store_i32_le(*write_offset, literal->value.int32_val);
            *write_offset += sizeof(int32_t);

            break;
        case NUMERIC:
            store_i64_le(*write_offset, literal->value.numeric_val.val);
            *write_offset += sizeof(int64_t);

            store_u32_le(*write_offset, literal->value.numeric_val.scale);
            *write_offset += sizeof(uint32_t);

            break;
        case CHAR:
            if (!literal->value.char_val.string) {
                return false;
            }

            store_u32_le(*write_offset, literal->value.char_val.n);
            *write_offset += sizeof(uint32_t);

            memcpy(*write_offset,
                literal->value.char_val.string,
                literal->value.char_val.n*sizeof(uint8_t)
            );
            *write_offset += literal->value.char_val.n*sizeof(uint8_t);
            
            break;
        case DATE:
            store_u64_le(*write_offset, literal->value.date_val);
            *write_offset += sizeof(uint64_t);

            break;
        case TIMESTAMP:
            store_u64_le(*write_offset, literal->value.timestamp_val);
            *write_offset += sizeof(uint64_t);

            break;
        // case BLOB:
        case BOOL: {
            uint8_t bool_val = literal->value.bool_val ? 1 : 0;
            store_u8_le(*write_offset, bool_val);
            *write_offset += sizeof(uint8_t);

            break;
        }
        // case JSONB:
        default:
            printf("serialize_value_data: Unsupported data type.\n");
            return false;
    }

    return true;
}

bool serialize_expression_node(uint8_t **write_offset, const ExpressionNode *expr_node) {
    if (!write_offset || !*write_offset || !expr_node) {
        return false;
    }

    uint8_t expression_type = (uint8_t) expr_node->type;
    store_u8_le(*write_offset, expression_type);
    *write_offset += sizeof(uint8_t);

    switch (expr_node->type) {
        case EXPR_LITERAL:
            if (!serialize_literal_value(write_offset, expr_node->expression_data.literal_value.literal)) {
                return false;
            }

            break;
        case EXPR_COLUMN_REF:
            memcpy(*write_offset, &expr_node->expression_data.column_value.column_name, 64*sizeof(uint8_t));
            *write_offset += 64*sizeof(uint8_t);

            store_i32_le(*write_offset, expr_node->expression_data.column_value.column_index);
            *write_offset += sizeof(int32_t);

            store_i32_le(*write_offset, expr_node->expression_data.column_value.relation_index);
            *write_offset += sizeof(int32_t);

            break;
        case EXPR_TABLE_REF:
            memcpy(*write_offset, &expr_node->expression_data.table_value.table_name, 64*sizeof(uint8_t));
            *write_offset += 64*sizeof(uint8_t);

            break;
        case EXPR_UNARY: {
            uint8_t operator_type = (uint8_t) expr_node->expression_data.unary_expr.op;
            store_u8_le(*write_offset, operator_type);
            *write_offset += sizeof(uint8_t);

            if (!serialize_expression_node(write_offset, expr_node->expression_data.unary_expr.operand)) {
                return false;
            }

            break;
        }
        case EXPR_BINARY: {
            if (!serialize_expression_node(write_offset, expr_node->expression_data.binary_expr.left_operand)) {
                return false;
            }
            
            uint8_t operator_type = (uint8_t) expr_node->expression_data.binary_expr.op;
            store_u8_le(*write_offset, operator_type);
            *write_offset += sizeof(uint8_t);

            if (!serialize_expression_node(write_offset, expr_node->expression_data.binary_expr.right_operand)) {
                return false;
            }

            break;
        }
        case EXPR_IS_NULL:
            if (!serialize_expression_node(write_offset, expr_node->expression_data.is_null_expr.operand)) {
                return false;
            }
            
            break;
        case EXPR_IS_NOT_NULL:
            if (!serialize_expression_node(write_offset, expr_node->expression_data.is_not_null_expr.operand)) {
                return false;
            }
            
            break;
        case EXPR_IN:
            if (!serialize_expression_node(write_offset, expr_node->expression_data.in_expr.operand)) {
                return false;
            }

            if (expr_node->expression_data.in_expr.option_count > 0
                && !expr_node->expression_data.in_expr.set_options) {
                return false;
            }

            store_u32_le(*write_offset, expr_node->expression_data.in_expr.option_count);
            *write_offset += sizeof(uint32_t);

            for (uint32_t i = 0; i < expr_node->expression_data.in_expr.option_count; i++) {
                if (!serialize_expression_node(write_offset, expr_node->expression_data.in_expr.set_options[i])) {
                    return false;
                }
            }
            
            break;
        case EXPR_BETWEEN:
            if (!serialize_expression_node(write_offset, expr_node->expression_data.between_expr.operand)
                || !serialize_expression_node(write_offset, expr_node->expression_data.between_expr.lower)
                || !serialize_expression_node(write_offset, expr_node->expression_data.between_expr.upper)) {
                return false;
            }
            
            break;
        case EXPR_FUNCTIONS: {
            uint8_t aggregate_function_type = (uint8_t) expr_node->expression_data.aggregate_func_expr.type;
            store_u8_le(*write_offset, aggregate_function_type);
            *write_offset += sizeof(uint8_t);

            if (!serialize_expression_node(write_offset, expr_node->expression_data.aggregate_func_expr.expression)) {
                return false;
            }

            store_u8_le(*write_offset, expr_node->expression_data.aggregate_func_expr.wildcard);
            *write_offset += sizeof(uint8_t);
            
            break;
        }
        default:
            fprintf(stderr, "serialize_expression_node: Expression type does not match.\n");
            return false;
    }

    return true;
}

bool deserialize_index_metadata(Pager *pager, BTreeCellContents *catalog_cell, Index **index) {
    if (!catalog_cell || !catalog_cell->BTreePayload.catalog
        || catalog_cell->BTreePayload.catalog->type > CATALOG_INDEX
        || !catalog_cell->BTreePayload.catalog->root_page_num
        || !catalog_cell->BTreePayload.catalog->metadata_page_num || !index) {
        return false;
    }

    *index = (Index *) calloc(1, sizeof(Index));
    if (!(*index)) {
        return false;
    }

    // Copy over metadata already stored in CatalogPayload
    strncpy((*index)->name, catalog_cell->keys[2]->value.char_val.string, 64); // Index name
    (*index)->root_page_num = catalog_cell->BTreePayload.catalog->root_page_num; // Index Root Page Num

    Page *metadata_page = pager_get_page(pager, catalog_cell->BTreePayload.catalog->metadata_page_num);
    if (!metadata_page) {
        index_free(*index);
        *index = NULL;
        return false;
    }
    uint8_t *read_offset = metadata_page->page_data + sizeof(uint32_t);

    uint8_t index_type;
    index_type = load_u8_le(read_offset);
    read_offset += sizeof(uint8_t);

    if (index_type > SECONDARY_INDEX) {
        index_free(*index);
        *index = NULL;
        return false;
    }

    (*index)->type = (IndexType) index_type;

    (*index)->key = (IndexKey *) calloc(1, sizeof(IndexKey));
    if (!(*index)->key) {
        index_free(*index);
        *index = NULL;
        return false;
    }

    (*index)->key->num_columns = load_u32_le(read_offset);
    read_offset += sizeof(uint32_t);

    if ((*index)->key->num_columns == 0) {
        index_free(*index);
        *index = NULL;        
        return false;
    }

    (*index)->key->column_index_array = (uint32_t *) calloc((*index)->key->num_columns, sizeof(uint32_t));
    if (!(*index)->key->column_index_array) {
        index_free(*index);
        *index = NULL;     
        return false;
    }

    for (uint32_t i = 0; i < (*index)->key->num_columns; i++) {
        (*index)->key->column_index_array[i] = load_u32_le(read_offset);
        read_offset += sizeof(uint32_t);
    }

    uint8_t is_unique;
    is_unique = load_u8_le(read_offset);
    read_offset += sizeof(uint8_t);

    if (is_unique > 1) {
        index_free(*index);
        *index = NULL;
        return false;
    }
    (*index)->is_unique = is_unique;

    if ((*index)->type == PRIMARY_INDEX && !(*index)->is_unique) {
        index_free(*index);
        *index = NULL;
        return false;
    }

    return true;
}

bool deserialize_table_metadata(Pager *pager, BTreeCellContents *catalog_cell, Table **table) {
    if (!catalog_cell || !catalog_cell->BTreePayload.catalog
        || catalog_cell->BTreePayload.catalog->type > CATALOG_INDEX
        || !catalog_cell->BTreePayload.catalog->root_page_num
        || !catalog_cell->BTreePayload.catalog->metadata_page_num || !table) {
        return false;
    }

    *table = (Table *) calloc(1, sizeof(Table));
    if (!(*table)) {
        return false;
    }

    strncpy((*table)->name, catalog_cell->keys[0]->value.char_val.string, 64); // Table name
    (*table)->name[63] = '\0';
    (*table)->is_deleted = false; // Since we deserialize this from disk means it wasn't deleted

    Page *metadata_page = pager_get_page(pager, catalog_cell->BTreePayload.catalog->metadata_page_num);
    if (!metadata_page) {
        table_free(*table);
        *table = NULL;
        return false;
    }
    uint8_t *read_offset = metadata_page->page_data + sizeof(uint32_t);

    uint8_t is_materialized;
    is_materialized = load_u8_le(read_offset);
    read_offset += sizeof(uint8_t);

    if (is_materialized > 1) {
        table_free(*table);
        *table = NULL;
        return false;
    }
    (*table)->is_materialized = is_materialized;

    // Should be increased when reconstructing indexes
    (*table)->total_secondary_indexes = 0;
    
    (*table)->secondary_indexes = (Index **) calloc(MAX_INDEXES, sizeof(Index *));
    if (!(*table)->secondary_indexes) {
        table_free(*table);
        *table = NULL;
        return false;
    }

    (*table)->row_count = load_u32_le(read_offset);
    read_offset += sizeof(uint32_t);

    (*table)->table_schema = deserialize_schema_metadata(&read_offset);
    if (!(*table)->table_schema) {
        table_free(*table);
        *table = NULL;
        return false;
    }

    return true;
}

Schema *deserialize_schema_metadata(uint8_t **read_offset) {
    if (!read_offset || !*read_offset) {
        return NULL;
    }

    Schema *schema = (Schema *) calloc(1, sizeof(Schema));
    if (!schema) {
        return NULL;
    }

    schema->num_columns = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    if (schema->num_columns > 0) {
        schema->columns = (Column **) calloc(schema->num_columns, sizeof(Column *));
        if (!schema->columns) {
            schema_free(schema);
            return NULL;
        }

        for (uint32_t i = 0; i < schema->num_columns; i++) {
            schema->columns[i] = deserialize_column(read_offset);
            if (!schema->columns[i]) {
                schema_free(schema);
                return NULL;
            }
        }
    }

    schema->num_constraints = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    if (schema->num_constraints > 0) {
        schema->constraints = (Constraint **) calloc(schema->num_constraints, sizeof(Constraint *));
        if (!schema->constraints) {
            schema_free(schema);
            return NULL;
        }

        for (uint32_t i = 0; i < schema->num_constraints; i++) {
            schema->constraints[i] = deserialize_constraint(read_offset);
            if (!schema->constraints[i]) {
                schema_free(schema);
                return NULL;
            }
        }
    }

    return schema;
}

Column *deserialize_column(uint8_t **read_offset) {
    if (!read_offset || !*read_offset) {
        return NULL;
    }

    char column_name[64];
    memcpy(column_name, *read_offset, 64*sizeof(uint8_t));
    *read_offset += 64*sizeof(uint8_t);
    column_name[63] = '\0';

    uint8_t data_type;
    data_type = load_u8_le(*read_offset);
    *read_offset += sizeof(uint8_t);

    if (data_type > JSONB) {
        return NULL;
    }

    uint32_t type_parameter;
    type_parameter = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    // Reconstructed during Column allocation

    uint32_t non_null_rows;
    non_null_rows = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    uint32_t null_rows;
    null_rows = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    Column *column = column_alloc(
        column_name,
        (DataType) data_type,
        type_parameter,
        non_null_rows,
        null_rows
    );
    if (!column) {
        return NULL;
    }

    return column;
}

Constraint *deserialize_constraint(uint8_t **read_offset) {
    if (!read_offset || !*read_offset) {
        return NULL;
    }

    char constraint_name[64];
    memcpy(constraint_name, *read_offset, 64*sizeof(uint8_t));
    *read_offset += 64*sizeof(uint8_t);
    constraint_name[63] = '\0';

    uint8_t constraint_type;
    constraint_type = load_u8_le(*read_offset);
    *read_offset += sizeof(uint8_t);

    if (constraint_type > DEFAULT) {
        return NULL;
    }

    Constraint *constraint = constraint_alloc(
        constraint_name,
        (ConstraintType) constraint_type
    );
    if (!constraint) {
        return NULL;
    }

    switch(constraint->type) {
        case PRIMARY_KEY: {
            uint32_t amount_columns;
            amount_columns = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            constraint->constraint_data.primary_key.amount_columns = amount_columns;

            if (amount_columns > 0) {
                constraint->constraint_data.primary_key.primary_key_columns = (uint32_t *) calloc(amount_columns, sizeof(uint32_t));
                if (!constraint->constraint_data.primary_key.primary_key_columns) {
                    constraint_free(constraint);
                    return NULL;
                }

                for (uint32_t j = 0; j < amount_columns; j++) {
                    constraint->constraint_data.primary_key.primary_key_columns[j] = load_u32_le(*read_offset);
                    *read_offset += sizeof(uint32_t);
                }
            }

            break;
        }
        case FOREIGN_KEY:
            constraint->constraint_data.foreign_key.amount_columns = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            if (constraint->constraint_data.foreign_key.amount_columns > 0) {
                constraint->constraint_data.foreign_key.foreign_key_columns = (uint32_t *) calloc(
                                                                    constraint->constraint_data.foreign_key.amount_columns,
                                                                    sizeof(uint32_t)
                                                                );
                if (!constraint->constraint_data.foreign_key.foreign_key_columns) {
                    constraint_free(constraint);
                    return NULL;
                }

                for (uint32_t j = 0;
                    j < constraint->constraint_data.foreign_key.amount_columns;
                    j++) {

                    constraint->constraint_data.foreign_key.foreign_key_columns[j] = load_u32_le(*read_offset);
                    *read_offset += sizeof(uint32_t);
                }
            }

            memcpy(
                constraint->constraint_data.foreign_key.referenced_table_name,
                *read_offset,
                64*sizeof(uint8_t)
            );
            *read_offset += 64*sizeof(uint8_t);
            constraint->constraint_data.foreign_key.referenced_table_name[63] = '\0';

            constraint->constraint_data.foreign_key.amount_referenced_columns = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            if (constraint->constraint_data.foreign_key.amount_referenced_columns > 0) {
                constraint->constraint_data.foreign_key.referenced_columns = (uint32_t *) malloc(
                    constraint->constraint_data.foreign_key.amount_referenced_columns*sizeof(uint32_t)
                );
                if (!constraint->constraint_data.foreign_key.referenced_columns) {
                    constraint_free(constraint);
                    return NULL;
                }

                for (uint32_t j = 0;
                    j < constraint->constraint_data.foreign_key.amount_referenced_columns;
                    j++) {

                    constraint->constraint_data.foreign_key.referenced_columns[j] = load_u32_le(*read_offset);
                    *read_offset += sizeof(uint32_t);
                }
            }

            break;
        case UNIQUE:
            constraint->constraint_data.unique_cols.amount_columns = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            if (constraint->constraint_data.unique_cols.amount_columns > 0) {
                constraint->constraint_data.unique_cols.column_refs = (uint32_t *) malloc(
                    constraint->constraint_data.unique_cols.amount_columns*sizeof(uint32_t)
                );
                if (!constraint->constraint_data.unique_cols.column_refs) {
                    constraint_free(constraint);
                    return NULL;
                }

                for (uint32_t j = 0;
                    j < constraint->constraint_data.unique_cols.amount_columns;
                    j++) {

                    constraint->constraint_data.unique_cols.column_refs[j] = load_u32_le(*read_offset);
                    *read_offset += sizeof(uint32_t);
                }
            }

            break;
        case CHECK:
            constraint->constraint_data.check.amount_columns = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            if (constraint->constraint_data.check.amount_columns > 0) {
                constraint->constraint_data.check.column_refs = (uint32_t *) malloc(
                    constraint->constraint_data.check.amount_columns*sizeof(uint32_t)
                );
                if (!constraint->constraint_data.check.column_refs) {
                    constraint_free(constraint);
                    return NULL;
                }

                for (uint32_t j = 0;
                    j < constraint->constraint_data.check.amount_columns;
                    j++) {

                    constraint->constraint_data.check.column_refs[j] = load_u32_le(*read_offset);
                    *read_offset += sizeof(uint32_t);
                }
            }

            constraint->constraint_data.check.constraint_expr =
                deserialize_expression_node(read_offset);

            if (!constraint->constraint_data.check.constraint_expr) {
                constraint_free(constraint);
                return NULL;
            }

            break;
        case NOT_NULL:
            constraint->constraint_data.not_null.column_ref = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            break;
        case DEFAULT:
            constraint->constraint_data.default_value.column_ref = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            constraint->constraint_data.default_value.default_expr =
                deserialize_expression_node(read_offset);

            if (!constraint->constraint_data.default_value.default_expr) {
                constraint_free(constraint);
                return NULL;
            }

            break;
        default:
            fprintf(stderr, "deserialize_constraint: Constraint type does not match.\n");
            constraint_free(constraint);
            return NULL;
    }

    return constraint;
}

Value *deserialize_literal_value(uint8_t **read_offset) {
    if (!read_offset || !*read_offset) {
        return NULL;
    }

    uint8_t literal_type;
    literal_type = load_u8_le(*read_offset);
    *read_offset += sizeof(uint8_t);

    if (literal_type > JSONB) {
        return NULL;
    }

    uint8_t null_val;
    null_val = load_u8_le(*read_offset);
    *read_offset += sizeof(uint8_t);

    if (null_val > 1) {
        return NULL;
    }

    Value *literal = (Value *) calloc(1, sizeof(Value));
    if (!literal) {
        return NULL;
    }

    literal->type = (DataType) literal_type;
    literal->null_val = null_val;

    if (literal->null_val) {
        return literal; // Everything needed was already deserialized for NULL
    }

    switch (literal->type) {
        case INTEGER:
            literal->value.int32_val = load_i32_le(*read_offset);
            *read_offset += sizeof(int32_t);

            break;
        case NUMERIC:
            literal->value.numeric_val.val = load_i64_le(*read_offset);
            *read_offset += sizeof(int64_t);

            literal->value.numeric_val.scale = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            break;
        case CHAR:
            literal->value.char_val.n = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            literal->value.char_val.string = (char *) calloc(
                literal->value.char_val.n + 1,
                sizeof(char)
            );
            if (!literal->value.char_val.string) {
                value_free(literal);
                return NULL;
            }

            memcpy(
                literal->value.char_val.string,
                *read_offset,
                literal->value.char_val.n*sizeof(uint8_t)
            );
            *read_offset += literal->value.char_val.n*sizeof(uint8_t);

            literal->value.char_val.string[literal->value.char_val.n] = '\0';

            break;
        case DATE:
            literal->value.date_val = load_u64_le(*read_offset);
            *read_offset += sizeof(uint64_t);

            break;
        case TIMESTAMP:
            literal->value.timestamp_val = load_u64_le(*read_offset);
            *read_offset += sizeof(uint64_t);

            break;
        // case BLOB:
        case BOOL: {
            uint8_t bool_val;
            bool_val = load_u8_le(*read_offset);
            *read_offset += sizeof(uint8_t);

            if (bool_val > 1) {
                value_free(literal);
                return NULL;
            }

            literal->value.bool_val = bool_val;
            break;
        }
        // case JSONB:
        default:
            fprintf(stderr, "deserialize_literal_value: Unsupported data type.\n");
            value_free(literal);
            return NULL;
    }

    return literal;
}

ExpressionNode *deserialize_expression_node(uint8_t **read_offset) {
    if (!read_offset || !*read_offset) {
        return NULL;
    }

    uint8_t expression_type;
    expression_type = load_u8_le(*read_offset);
    *read_offset += sizeof(uint8_t);

    if (expression_type > EXPR_FUNCTIONS) {
        return NULL;
    }

    ExpressionNode *expr_node = (ExpressionNode *) calloc(1, sizeof(ExpressionNode));
    if (!expr_node) {
        return NULL;
    }
    expr_node->type = (ExpressionType) expression_type;

    switch (expr_node->type) {
        case EXPR_LITERAL:
            expr_node->expression_data.literal_value.literal =
                deserialize_literal_value(read_offset);

            if (!expr_node->expression_data.literal_value.literal) {
                expression_node_free(expr_node);
                return NULL;
            }

            break;
        case EXPR_COLUMN_REF:
            memcpy(
                &expr_node->expression_data.column_value.column_name,
                *read_offset,
                64*sizeof(uint8_t)
            );
            *read_offset += 64*sizeof(uint8_t);
            expr_node->expression_data.column_value.column_name[63] = '\0';

            expr_node->expression_data.column_value.column_index = load_i32_le(*read_offset);
            *read_offset += sizeof(int32_t);

            expr_node->expression_data.column_value.relation_index = load_i32_le(*read_offset);
            *read_offset += sizeof(int32_t);

            break;
        case EXPR_TABLE_REF:
            memcpy(
                &expr_node->expression_data.table_value.table_name,
                *read_offset,
                64*sizeof(uint8_t)
            );
            *read_offset += 64*sizeof(uint8_t);
            expr_node->expression_data.table_value.table_name[63] = '\0';

            break;
        case EXPR_UNARY: {
            uint8_t operator_type;
            operator_type = load_u8_le(*read_offset);
            *read_offset += sizeof(uint8_t);

            if (operator_type >= OP_ERROR) {
                expression_node_free(expr_node);
                return NULL;
            }
            expr_node->expression_data.unary_expr.op = (OperatorType) operator_type;

            expr_node->expression_data.unary_expr.operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.unary_expr.operand) {
                expression_node_free(expr_node);
                return NULL;
            }

            break;
        }
        case EXPR_BINARY: {
            expr_node->expression_data.binary_expr.left_operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.binary_expr.left_operand) {
                expression_node_free(expr_node);
                return NULL;
            }

            uint8_t operator_type;
            operator_type = load_u8_le(*read_offset);
            *read_offset += sizeof(uint8_t);

            if (operator_type >= OP_ERROR) {
                expression_node_free(expr_node);
                return NULL;
            }
            expr_node->expression_data.binary_expr.op = (OperatorType) operator_type;

            expr_node->expression_data.binary_expr.right_operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.binary_expr.right_operand) {
                expression_node_free(expr_node);
                return NULL;
            }

            break;
        }
        case EXPR_IS_NULL:
            expr_node->expression_data.is_null_expr.operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.is_null_expr.operand) {
                expression_node_free(expr_node);
                return NULL;
            }
            
            break;
        case EXPR_IS_NOT_NULL:
            expr_node->expression_data.is_not_null_expr.operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.is_not_null_expr.operand) {
                expression_node_free(expr_node);
                return NULL;
            }
            
            break;
        case EXPR_IN:
            expr_node->expression_data.in_expr.operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.in_expr.operand) {
                expression_node_free(expr_node);
                return NULL;
            }

            expr_node->expression_data.in_expr.option_count = load_u32_le(*read_offset);
            *read_offset += sizeof(uint32_t);

            if (expr_node->expression_data.in_expr.option_count > 0) {
                expr_node->expression_data.in_expr.set_options = (ExpressionNode **) calloc(
                    expr_node->expression_data.in_expr.option_count,
                    sizeof(ExpressionNode *)
                );
                if (!expr_node->expression_data.in_expr.set_options) {
                    expression_node_free(expr_node);
                    return NULL;
                }

                for (uint32_t i = 0;
                    i < expr_node->expression_data.in_expr.option_count;
                    i++) {

                    expr_node->expression_data.in_expr.set_options[i] =
                        deserialize_expression_node(read_offset);

                    if (!expr_node->expression_data.in_expr.set_options[i]) {
                        expression_node_free(expr_node);
                        return NULL;
                    }
                }
            }
            
            break;
        case EXPR_BETWEEN:
            expr_node->expression_data.between_expr.operand =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.between_expr.operand) {
                expression_node_free(expr_node);
                return NULL;
            }

            expr_node->expression_data.between_expr.lower =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.between_expr.lower) {
                expression_node_free(expr_node);
                return NULL;
            }

            expr_node->expression_data.between_expr.upper =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.between_expr.upper) {
                expression_node_free(expr_node);
                return NULL;
            }
            
            break;
        case EXPR_FUNCTIONS: {
            uint8_t aggregate_function_type;
            aggregate_function_type = load_u8_le(*read_offset);
            *read_offset += sizeof(uint8_t);

            if (aggregate_function_type > EXPR_AGGREGATE_MAX) {
                expression_node_free(expr_node);
                return NULL;
            }
            expr_node->expression_data.aggregate_func_expr.type =
                (AggregateFunctionType) aggregate_function_type;

            expr_node->expression_data.aggregate_func_expr.expression =
                deserialize_expression_node(read_offset);

            if (!expr_node->expression_data.aggregate_func_expr.expression) {
                expression_node_free(expr_node);
                return NULL;
            }

            expr_node->expression_data.aggregate_func_expr.wildcard = load_u8_le(*read_offset);
            *read_offset += sizeof(uint8_t);
            
            break;
        }
        default:
            fprintf(stderr, "deserialize_expression_node: Expression type does not match.\n");
            expression_node_free(expr_node);
            return NULL;
    }

    return expr_node;
}

/* ---------- RESERVED PAGE CONTENTS ---------- */

bool serialize_page_zero_metadata(uint8_t **write_offset, const PageZeroMetadata *page_zero) {
    if (!write_offset || !(*write_offset) || !page_zero) {
        return false;
    }

    memcpy(*write_offset, &(page_zero->magic), DB_MAGIC_STRING_LEN);
    *write_offset += DB_MAGIC_STRING_LEN;

    store_u32_le(*write_offset, page_zero->version);
    *write_offset += sizeof(uint32_t);

    store_u16_le(*write_offset, page_zero->page_size);
    *write_offset += sizeof(uint16_t);

    store_u32_le(*write_offset, page_zero->catalog_root);
    *write_offset += sizeof(uint32_t);

    store_u32_le(*write_offset, page_zero->free_list_head);
    *write_offset += sizeof(uint32_t);

    return true;
}

bool deserialize_page_zero_metadata(uint8_t **read_offset, PageZeroMetadata *page_zero) {
    if (!read_offset || !(*read_offset) || !page_zero) {
        return false;
    }

    memcpy(&(page_zero->magic), *read_offset, DB_MAGIC_STRING_LEN);
    *read_offset += DB_MAGIC_STRING_LEN;

    page_zero->version = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    page_zero->page_size = load_u16_le(*read_offset);
    *read_offset += sizeof(uint16_t);

    page_zero->catalog_root = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    page_zero->free_list_head = load_u32_le(*read_offset);
    *read_offset += sizeof(uint32_t);

    return true;
}

/* ---------- SERIALIZED PAGE SIZE ---------- */

size_t serialized_index_metadata_size(const Index *index) {
    if (!index) {
        return (size_t) 0;
    }

    if (!index->key->num_columns || !index->key->column_index_array) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += sizeof(uint8_t) + sizeof(uint32_t);
    for (uint32_t i = 0; i < index->key->num_columns; i++) {
        size += sizeof(uint32_t);
    }
    size += sizeof(uint8_t);

    return size;
}

size_t serialized_table_metadata_size(const Table *table) {
    if (!table) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += sizeof(uint8_t) + sizeof(uint32_t);
    size += serialized_schema_size(table->table_schema);

    return size;
}

size_t serialized_schema_size(const Schema *schema) {
    if (!schema) {
        return (size_t) 0;
    }

    if (schema->num_columns > 0 && !schema->columns) {
        return (size_t) 0;
    }

    if (schema->num_constraints > 0 && !schema->constraints) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += sizeof(uint32_t);
    for (uint32_t i = 0; i < schema->num_columns; i++) {
        size += serialized_column_size(schema->columns[i]);
    }

    size += sizeof(uint32_t);
    for (uint32_t i = 0; i < schema->num_constraints; i++) {
        size += serialized_constraint_size(schema->constraints[i]);
    }

    return size;
}

size_t serialized_column_size(Column *column) {
    if (!column) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += 64*sizeof(uint8_t)
            + sizeof(uint8_t)
            + sizeof(uint32_t)
            + sizeof(uint32_t)
            + sizeof(uint32_t);

    return size;
}

size_t serialized_constraint_size(Constraint *constraint) {
    if (!constraint) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += 64*sizeof(uint8_t)
            + sizeof(uint8_t);

    switch(constraint->type) {
        case PRIMARY_KEY: {
            if (constraint->constraint_data.primary_key.amount_columns > 0
                && !constraint->constraint_data.primary_key.primary_key_columns) {
                return (size_t) 0;
            }
for (uint32_t i = 0; i < constraint->constraint_data.primary_key.amount_columns; i++) {
      size += sizeof(uint32_t);
}
            size += sizeof(uint32_t);
            break;
        }
        case FOREIGN_KEY:
            if (constraint->constraint_data.foreign_key.amount_columns > 0
                && !constraint->constraint_data.foreign_key.foreign_key_columns) {
                return (size_t) 0;
            }

            if (constraint->constraint_data.foreign_key.amount_referenced_columns > 0
                && !constraint->constraint_data.foreign_key.referenced_columns) {
                return (size_t) 0;
            }

            for (uint32_t i = 0; i < constraint->constraint_data.foreign_key.amount_columns; i++) {
                size += sizeof(uint32_t);
            }

            for (uint32_t i = 0; i < constraint->constraint_data.foreign_key.amount_referenced_columns; i++) {
                size += sizeof(uint32_t);
            }

            size += sizeof(uint32_t)
                    + sizeof(uint32_t)
                    + 64 * sizeof(uint8_t);
     
                    
            break;
        case UNIQUE:
            if (constraint->constraint_data.unique_cols.amount_columns > 0
                && !constraint->constraint_data.unique_cols.column_refs) {
                return (size_t) 0;
            }

            for (uint32_t i = 0; i < constraint->constraint_data.unique_cols.amount_columns; i++) {
                size += sizeof(uint32_t);
            }

            size += sizeof(uint32_t);
            break;
        case CHECK:
            if (constraint->constraint_data.check.amount_columns > 0
                && !constraint->constraint_data.check.column_refs) {
                return (size_t) 0;
            }

            if (!constraint->constraint_data.check.constraint_expr) {
                return (size_t) 0;
            }

            for (uint32_t i = 0; i < constraint->constraint_data.check.amount_columns; i++) {
                size += sizeof(uint32_t);
            }
            
            size += sizeof(uint32_t)
                    + serialized_expression_node_size(constraint->constraint_data.check.constraint_expr);

            break;
        case NOT_NULL:
            size += sizeof(uint32_t);
            break;
        case DEFAULT:
            if (!constraint->constraint_data.default_value.default_expr) {
                return (size_t) 0;
            }

            size += sizeof(uint32_t)
                    + serialized_expression_node_size(constraint->constraint_data.default_value.default_expr);
            
            break;
        default:
            fprintf(stderr, "serialized_constraint_size: Constraint type does not match.\n");
            return (size_t) 0;
    }

    return size;
}

size_t serialized_literal_value_size(Value *literal) {
    if (!literal) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += sizeof(uint8_t) + sizeof(uint8_t);

    if (literal->null_val) {
        return size;
    }

    switch (literal->type) {
        case INTEGER:
            size += sizeof(int32_t);
            break;
        case NUMERIC:
            size += sizeof(int64_t) + sizeof(uint32_t);
            break;
        case CHAR:
            if (!literal->value.char_val.string) {
                return (size_t) 0;
            }

            size += sizeof(uint32_t) + literal->value.char_val.n*sizeof(uint8_t);             
            break;
        case DATE:
            size += sizeof(uint64_t);
            break;
        case TIMESTAMP:
            size += sizeof(uint64_t);
            break;
        // case BLOB:
        case BOOL: {
            size += sizeof(uint8_t);
            break;
        }
        // case JSONB:
        default:
            printf("serialize_literal_value_size: Unsupported data type.\n");
            return (size_t) 0;
    }

    return size;
}

size_t serialized_expression_node_size(ExpressionNode *expr_node) {
    if (!expr_node) {
        return (size_t) 0;
    }
    size_t size = 0;

    size += sizeof(uint8_t);

    switch (expr_node->type) {
        case EXPR_LITERAL:
            size += serialized_literal_value_size(expr_node->expression_data.literal_value.literal);
            break;
        case EXPR_COLUMN_REF:
            size += 64*sizeof(uint8_t)
                    + sizeof(int32_t)
                    + sizeof(int32_t);

            break;
        case EXPR_TABLE_REF:
            size += 64*sizeof(uint8_t);

            break;
        case EXPR_UNARY: {
            size += sizeof(uint8_t);
            size += serialized_expression_node_size(expr_node->expression_data.unary_expr.operand);

            break;
        }
        case EXPR_BINARY: {
            size += serialized_expression_node_size(expr_node->expression_data.binary_expr.left_operand);
            size += sizeof(uint8_t);
            size += serialized_expression_node_size(expr_node->expression_data.binary_expr.right_operand);

            break;
        }
        case EXPR_IS_NULL:
            size += serialized_expression_node_size(expr_node->expression_data.is_null_expr.operand);
            
            break;
        case EXPR_IS_NOT_NULL:
            size += serialized_expression_node_size(expr_node->expression_data.is_not_null_expr.operand);
            
            break;
        case EXPR_IN:
            size += serialized_expression_node_size(expr_node->expression_data.in_expr.operand);

            if (expr_node->expression_data.in_expr.option_count > 0
                && !expr_node->expression_data.in_expr.set_options) {
                return (size_t) 0;
            }

            size += sizeof(uint32_t);

            for (uint32_t i = 0; i < expr_node->expression_data.in_expr.option_count; i++) {
                size += serialized_expression_node_size(expr_node->expression_data.in_expr.set_options[i]);
            }
            
            break;
        case EXPR_BETWEEN:
            size += serialized_expression_node_size(expr_node->expression_data.between_expr.operand);
            size += serialized_expression_node_size(expr_node->expression_data.between_expr.lower);
            size += serialized_expression_node_size(expr_node->expression_data.between_expr.upper);
            
            break;
        case EXPR_FUNCTIONS: {
            size += sizeof(uint8_t);
            size += serialized_expression_node_size(expr_node->expression_data.aggregate_func_expr.expression);
            size += sizeof(uint8_t);
            
            break;
        }
        default:
            fprintf(stderr, "serialized_expression_node_size: Expression type does not match.\n");
            return (size_t) 0;
    }

    return size;
}