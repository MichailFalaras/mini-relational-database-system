#ifndef PAGE_H_
#define PAGE_H_

#include<fcntl.h>
#include<time.h>
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

/* Both disk page (minimum size unit of I/O)
and logical BTree node that contains:
 * is_dirty: true if page is modified
 * page_num: page id
 * page_data: raw binary page data
 * last_interacted: timestamp of last interaction. */
typedef struct page {
    bool is_dirty;
    uint32_t page_num;

    /* Also contains metadata/indexes to other
    pages connected before the page's payload (Actual data). */
    uint8_t page_data[PAGE_SIZE];

    time_t last_interacted;
} Page;

#endif