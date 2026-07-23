#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/pager.h"
#include "../../include/page.h"

/* Create page. */
Page *page_create(Pager *pager, uint32_t page_num) {
    Page *page = (Page *) calloc(1, sizeof(Page));
    if (page == NULL) {
        perror("page_create");
        return NULL;
    }

    page->is_dirty = false;
    page->page_num = page_num;
    page->last_interacted = ++pager->access_counter;

    return page;
}

/* Create PageZeroMetadata struct & initialize it. */
PageZeroMetadata *page_zero_create(void) {
    PageZeroMetadata *page_zero = (PageZeroMetadata *) calloc(1, sizeof(PageZeroMetadata));
    if (!page_zero) {
        perror("page_zero_create");
        return NULL;
    }

    memcpy(page_zero->magic, DB_MAGIC_STRING, DB_MAGIC_STRING_LEN);
    page_zero->version = 1;
    page_zero->page_size = PAGE_SIZE;
    page_zero->catalog_root = 1;
    page_zero->free_list_head = 0;
    
    return page_zero;
}

/* Mark a page dirty. */
bool page_mark_dirty(Page *page) {
    if (!page) {
        return false;
    }

    page->is_dirty = true;
    return true;
}

/* Mark a page clean. */
bool page_mark_clean(Page *page) {
    if (!page) {
        return false;
    }

    page->is_dirty = false;
    return true;
}

/* Update time of last interaction with page. */
bool page_touch(Pager *pager, Page *page) {
    if (!page) {
        return false;
    }

    page->last_interacted = ++pager->access_counter;
    return true;
}

/* Clear page, mark it dirty and update time of
last interaction. */
bool page_clear(Pager *pager, Page *page) {
    if (!page) {
        return false;
    }

    /* In order for page change's to be written back in disk. */
    page->is_dirty = true;
    memset(page->page_data, 0, PAGE_SIZE); 
    if (!page_touch(pager, page)) {
        return false;
    }

    return true;
}

/* Free page. */
void page_free(Page *page) {
    if (page != NULL) {
        free(page);
    }
}
