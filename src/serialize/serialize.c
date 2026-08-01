#include <stdio.h>
#include <stdlib.h>
#include "../../include/index.h"
#include "../../include/row.h"
#include "../../include/data_types.h"
#include "../src/data_types/data_types_utils.h"
#include "../src/btree/btree_utils.h"

/* BTreeLeafNode specific cell data serialization. */
bool serialize_cell_data(void *page_data, uint16_t offset, void *payload, Value **keys, void *context) {
    if (!page_data || !payload || !keys || !context) {
        return false;
    }

    KeyExtractionContext *ctx = (KeyExtractionContext *) context;
    /* Get node type. */
    uint8_t node_type = 0;
    if (!get_node_type(page_data, &node_type)) {
        return false;
    }
    
    /* Get key size to skip ahead for leaf nodes. */
    uint16_t key_size = btree_get_key_size(keys, context);
    if (!key_size) {
        return false;
    }

    /* Write either Child Pointer or Row. */
    uint8_t *current_offset = (uint8_t*) page_data + offset;
    if (node_type) {
        memcpy(current_offset, (uint32_t *) payload, sizeof(uint32_t));
        current_offset += 4;
    } else {
        memcpy(current_offset + key_size, (Row *) payload, sizeof(Row));       
    }

    /* Write keys. */
    for (uint32_t i = 0; i < ctx->index_key->num_columns; i++) {
        if (!serialize_value_data(keys[i], current_offset)) {
            return false;
        }

        current_offset += get_data_type_size(ctx->data_types[i]);
    }

    return true;
}

/* Serialize DataType specific Value data. */
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

/* Deserialize value data. */
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