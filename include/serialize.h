#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct value Value;
typedef enum data_types DataType;
typedef struct schema Schema;
typedef struct btree_page BTreePage;
typedef struct btree_cell_contents BTreeCellContents;
typedef struct btree_cell BTreeCell;
typedef struct btree_index_spec BTreeIndexSpec;

/* Serialize/Deserialize cell contents type agnostic functions. */
extern bool serialize_cell_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell);

extern bool deserialize_cell_contents(const Schema *schema, uint8_t *read_offset, BTreePage *btree_page,
    BTreeCell *cell_view, BTreeCellContents *cell, BTreeIndexSpec *index);

/* Serialize/Deserialize leaf node cell metadata. */
extern bool serialize_leaf_node(uint8_t *write_offset, BTreeCellContents *cell);

extern bool deserialize_leaf_node(const Schema *schema, uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *index);

/* Serialize/Deserialize internal node cell metadata. */
extern bool serialize_internal_node(uint8_t *write_offset, BTreeCellContents *cell);

extern bool deserialize_internal_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *index);

/* Serialize/Deserialize keys. */
extern bool serialize_keys(uint8_t **write_offset, BTreeCellContents *cell);

extern bool deserialize_keys(uint8_t **read_offset, BTreeCellContents *cell, BTreeIndexSpec *index);

/* Serialize/Deserialize Row metadata. */
extern bool serialize_row(uint8_t **write_offset, const Row *row);

extern bool deserialize_row(const Schema *schema, uint8_t **read_offset, BTreeCellContents *cell);

/* Serialize/Deserialize value data. */
extern bool serialize_value_data(Value *value, void *serialized_output);

extern Value *deserialize_value_data(DataType type, void *offset);

#endif