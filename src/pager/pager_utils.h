#ifndef PAGER_UTILS_H_
#define PAGER_UTILS_H_

#include <stdbool.h>
#include "../../include/encoding.h"
#include "../../include/page.h"

typedef struct pager Pager;

extern bool write_page_size(Pager *pager, Page *page);

extern bool read_page_size(Pager *pager, Page *page);

extern FreePageResult get_free_page(Pager *pager, uint32_t *page_num);

extern PageFreeStatus is_page_free(Pager *pager, uint32_t page_num);

extern bool connect_free_page(Pager *pager, uint32_t page_num);

/* ---------- FREE LIST INLINE HELPERS ---------- */

/* Only used to get free list head from Superblock Page. (Wrapper)*/
static inline uint32_t get_free_list_head(Page *superblock_page) {
    if (!superblock_page || superblock_page->page_num != 0) {
        return 0;
    }

    return load_u32_le(superblock_page->page_data + FREE_LIST_HEAD_OFFSET);
}

/* Only used to set free list head in Superblock Page. (Wrapper)*/
static inline bool set_free_list_head(Page *superblock_page, uint32_t free_list_head) {
    if (!superblock_page || superblock_page->page_num != 0) {
        return false;
    }

    store_u32_le(superblock_page->page_data + FREE_LIST_HEAD_OFFSET, free_list_head);
    return true;
}

/* Get next free list page num. */
static inline uint32_t get_next_free_list_page(Page *page) {
    if (!page || !page->page_num) {
        return 0;
    }

    return load_u32_le(page->page_data);
}

/* Set next free list page num. */
static inline bool set_next_free_list_page(Page *page, uint32_t next_free_list_page) {
    if (!page || !page->page_num) {
        return false;
    }

    store_u32_le(page->page_data, next_free_list_page);
    return true;
}

#endif