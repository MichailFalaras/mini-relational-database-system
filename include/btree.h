#ifndef BTREE_H_
#define BTREE_H_

#include <stdint.h>
#include <stdbool.h>

#define BTREE_INTERNAL_NODE_SIZE 14
#define BTREE_LEAF_NODE_SIZE 18

typedef struct value Value;

/* BTree Common Node Header.
 * node_type:
 *  0: leaf node
 *  1: internal node
 * is_root:
 *  0: false
 *  1: true
 * parent_pointer: parent's page number
 * cell_count: amount of internal node IDs
 * or page data.
 * free_space_offset: last write position. */
typedef struct  __attribute__((packed)) BTree_common_node_header {
    uint8_t node_type;
    uint8_t is_root;
    uint32_t parent_pointer;
    uint16_t cell_count;
    uint16_t free_space_offset;
} BTreeCommonHeader;

/* BTree Internal Node Layout.
 * rightmost_child_pointer: N+1 pointer for values
 * bigger than rightmost cell ID. */
typedef struct  __attribute__((packed)) BTree_internal_node_layout {
    uint32_t rightmost_child_pointer;
    /* Arrays/Dynamic data stored onto page's page_data
     * via helper functions. */
} BTreeInternalNode;

/* BTree Leaf Node Layout.
 * previous_leaf_pointer: page number of previous page leaf node
 * next_leaf_pointer: page number of next page leaf node. */
typedef struct  __attribute__((packed)) BTree_leaf_node_layout {
    uint32_t previous_leaf_pointer;
    uint32_t next_leaf_pointer;
    /* Arrays/Dynamic data stored onto page's page_data
     * via helper functions. */
} BTreeLeafNode;

/* System Catalog Leaf Node Metadata.
 * type:
 *  0: table
 *  1: index
 * name: table/index name
 * root_page_num: root table/index page number
 * sql_query_size: sql query size*/
typedef struct __attribute__((packed)) system_catalog_leaf_node_metadata {
    uint8_t type;
    char name[64];
    uint32_t root_page_num;
    uint32_t sql_query_size;
    /* SQL Query directly memcpy'd onto page's page_data. */
} SystemCatalogLeafNodeMetadata;

extern bool btree_init_empty_leaf(void *page_data);

extern bool btree_init_internal(void *page_data, uint32_t rightmost_child_pointer);

extern uint16_t btree_get_available_capacity(void *page_data);

extern bool btree_has_enough_space(void *page_data, uint16_t payload_size);

extern bool btree_compare(Value **values, const void *key, void *context, int *result);

Value **btree_extract_data(void *page_data, uint16_t cell_pointer, void *context);

extern uint16_t btree_lower_bound(void *page_data, const void *key, void *context);

#endif