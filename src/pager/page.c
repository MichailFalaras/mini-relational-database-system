#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "../../include/page.h"

Page *page_create(uint32_t page_num) {
    Page *page = (Page *) calloc(1, sizeof(Page));
    if (page == NULL) {
        perror("page_create");
        exit(1);
    }

    page->is_dirty = false;
    page->page_num = page_num;
    page->last_interacted = time(NULL);

    return page;
}

bool page_mark_dirty(Page *page) {
    if (page == NULL) {
        return false;
    }

    page->is_dirty = true;
    return true;
}

bool page_mark_clean(Page *page) {
    if (page == NULL) {
        return false;
    }

    page->is_dirty = false;
    return true;
}

/* Might also need to create a page if it doesn't
already exist. */
bool page_touch(Page *page) {
    if (page == NULL) {
        return false;
    }

    page->last_interacted = time(NULL);
    return true;
}

bool page_clear(Page *page) {
    if (page == NULL) {
        return false;
    }

    /* In order for page change's to be written back in disk. */
    page->is_dirty = true;
    memset(page->page_data, 0, PAGE_SIZE); 
    return true;
}

void page_free(Page *page) {
    if (page != NULL) {
        free(page);
    }
}
