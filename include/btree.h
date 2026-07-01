#ifndef BTREE_H_
#define BTREE_H_

#include "pager.h"

/* Btree Node that wraps over a page. 
 * Contains pointers to all children
 * Btree nodes. */
typedef struct bree {
    Page *page;
    struct btree **children;
} BtreeNode;

#endif