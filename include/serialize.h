#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct value Value;
typedef enum data_types DataType;
typedef struct column Column;
typedef struct schema Schema;
typedef struct btree_page BTreePage;
typedef struct btree_cell_contents BTreeCellContents;
typedef struct btree_cell_view BTreeCellView;
typedef struct btree_index_spec BTreeIndexSpec;
typedef struct row Row;

/* Serialize/Deserialize cell contents type agnostic functions. */
extern bool serialize_cell_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_cell_contents(uint8_t *read_offset, BTreePage *btree_page,
    BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *spec);

/* Serialize/Deserialize leaf node cell metadata. */
extern bool serialize_leaf_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *spec);

/* Serialize/Deserialize internal node cell metadata. */
extern bool serialize_internal_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_internal_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell, BTreeIndexSpec *spec);

/* Serialize NULL bitmap right before serializing keys/row columns.
 *
 * Separating number of values in key and bitmap columns allows creating
 * prefix keys with NULL bitmap. */
extern bool serialize_null_bitmap(uint8_t **write_offset, Value **key, uint32_t num_vals, uint32_t bitmap_columns);

/* Deserialize bitmap before deserializing keys/row columns. */
extern bool deserialize_null_bitmap(uint8_t **read_offset, uint8_t **bitmap, uint32_t num_columns);

/* Serialize/Deserialize keys. */
extern bool serialize_keys(uint8_t **write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_keys(uint8_t **read_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

/* Serialize/Deserialize Row metadata. */
extern bool serialize_row(uint8_t **write_offset, const Row *row, BTreeIndexSpec *spec);

extern bool deserialize_row(uint8_t **read_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

/* Serialize/Deserialize value data. */
extern bool serialize_value_data(Value *value, Column *column, void *serialized_output);

extern Value *deserialize_value_data(Column *column, bool is_null, void *offset);

/* Serialize/Deserialize catalog contents */
extern bool serialize_catalog_contents(uint8_t *write_offset, BTreePage *btree_page, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_catalog_contents(uint8_t *read_offset, BTreePage *btree_page, BTreeCellView *cell_view, 
BTreeCellContents *cell, BTreeIndexSpec *spec);

/* Serialize/Deserialize catalog leaf key + payload */
extern bool serialize_catalog_leaf_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_catalog_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell,
BTreeIndexSpec *spec);


#endif