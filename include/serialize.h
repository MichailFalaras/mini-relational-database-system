#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct table Table;
typedef struct schema Schema;
typedef struct column Column;
typedef struct constraint Constraint;
typedef struct row Row;
typedef enum data_types DataType;
typedef struct value Value;
typedef struct index Index;
typedef struct expression_node ExpressionNode;
typedef struct page_zero_metadata PageZeroMetadata;
typedef struct btree_page BTreePage;
typedef struct btree_cell_contents BTreeCellContents;
typedef struct btree_cell_view BTreeCellView;
typedef struct btree_index_spec BTreeIndexSpec;

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

/* ---------- CATALOG CONTENTS --------- */

/* Serialize/Deserialize Catalog Leaf Cell. */
extern bool serialize_catalog_leaf_node(uint8_t *write_offset, BTreeCellContents *cell, BTreeIndexSpec *spec);

extern bool deserialize_catalog_leaf_node(uint8_t *read_offset, BTreeCellView *cell_view, BTreeCellContents *cell,
    BTreeIndexSpec *spec);

/* Serialization/Deserialization of Components stored in Metadata Page.*/
extern bool serialize_index_metadata(uint8_t **write_offset, const Index *index);

extern bool serialize_table_metadata(uint8_t **write_offset, const Table *table);

extern bool serialize_schema_metadata(uint8_t **write_offset, const Schema *schema);

extern bool serialize_column(uint8_t **write_offset, const Column *column);

extern bool serialize_constraint(uint8_t **write_offset, const Constraint *constraint);

/* Supported literal types are: INTEGER, NUMERIC, CHAR(n), DATE, TIMESTAMP, BOOL */
extern bool serialize_literal_value(uint8_t **write_offset, const Value *literal);

extern bool serialize_expression_node(uint8_t **write_offset, const ExpressionNode *expr_node);

extern bool deserialize_index_metadata(uint8_t **read_offset, Index **index);

extern bool deserialize_table_metadata(uint8_t **read_offset, Table **table);

extern Schema *deserialize_schema_metadata(uint8_t **read_offset);

extern Column *deserialize_column(uint8_t **read_offset);

extern Constraint *deserialize_constraint(uint8_t **read_offset);

extern Value *deserialize_literal_value(uint8_t **read_offset);

extern ExpressionNode *deserialize_expression_node(uint8_t **read_offset);

/* ---------- RESERVED PAGE CONTENTS --------- */

extern bool serialize_page_zero_metadata(uint8_t **write_offset, const PageZeroMetadata *page_zero);

extern bool deserialize_page_zero_metadata(uint8_t **read_offset, PageZeroMetadata *page_zero);


#endif