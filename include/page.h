#ifndef PAGE_H_
#define PAGE_H_

#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define FREE_LIST_HEAD_OFFSET offsetof(PageZeroMetadata, free_list_head)
#define SYSTEM_CATALOG_ROOT_PAGE_OFFSET offsetof(PageZeroMetadata, catalog_root)

/* Page Zero Metada struct to be stored in Page 0.
 * magic: magic string in order for page to be recognized
 * version: version of rdbms
 * page_size: page size constant value
 * catalog_root: page num of System Catalog's page. */
typedef struct __attribute__((packed)) {
    char magic[10];
    uint32_t version;
    uint16_t page_size;
    uint32_t catalog_root;
    uint32_t free_list_head;
} PageZeroMetadata;

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

extern Page *page_create(uint32_t page_num);

extern PageZeroMetadata *page_zero_create(void);

extern bool page_mark_dirty(Page *page);

extern bool page_mark_clean(Page *page);

extern bool page_touch(Page *page);

extern bool page_clear(Page *page);

extern void page_free(Page *page);

#endif