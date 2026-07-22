#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../include/pager.h"
#include "../../include/page.h"
#include "pager_utils.h"

/* Write exactly PAGE_SIZE data. */
bool write_page_size(Pager *pager, Page *page) {
    int index = 0;
    int bytes_left_to_write = PAGE_SIZE;
    int bytes_written = 0;

    while (bytes_left_to_write > 0){
        bytes_written = write(pager->fd, page->page_data + index, bytes_left_to_write);

        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            } 
            return false;
        }

        bytes_left_to_write -= bytes_written;
        index += bytes_written;
    }

    return true;
}

/* Read exacty PAGE_SIZE data. */
bool read_page_size(Pager *pager, Page *page) {
    int index = 0;
    int bytes_left_to_read = PAGE_SIZE;
    int bytes_read = 0;

    while (bytes_left_to_read > 0){
        bytes_read = read(pager->fd, page->page_data + index, bytes_left_to_read);

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        bytes_left_to_read -= bytes_read;
        index += bytes_read;
    }

   return true;
}

/* Get first free page from Free List Head. */
bool get_free_page(Pager *pager, uint32_t *page_num) {
    if (!pager || !page_num) {
        return false;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return false;
    }

    uint32_t free_list_head;
    memcpy(&free_list_head,
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t));
    
    /* No free pages. */
    if (free_list_head == 0) {
        return false;
    }

    /* Get page from cache or disk. */
    Page *free_page = pager_get_page(pager, free_list_head);
    if (!free_page) {
        return false;
    }
    
    *page_num = free_list_head;
    
    if (!page_mark_dirty(zero)) {
        return false;
    }

    /* Since we are returning the first free page no matter what,
     * update free_list_head of page 0 to show to the free page after
     * the page we are returning. */
    memcpy(zero->page_data + FREE_LIST_HEAD_OFFSET,
        free_page->page_data,
        sizeof(uint32_t));

    return true;
}

/* Check if page is already free. */
bool is_page_free(Pager *pager, uint32_t page_num) {
    if (!pager || page_num >= MAX_PAGES) {
        return false;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return false;
    }

    uint32_t free_list_head;
    memcpy(
        &free_list_head,
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );

    while (free_list_head != 0) {
        if (free_list_head == page_num) {
            return true;
        }

        if (!pager->pages[free_list_head]) {
            /* Something wrong with free list pages then. */
            if (!pager_get_page(pager, free_list_head)) {
                return false;
            }
            /* Load the page into memory and continue the loop. */
        }

        memcpy(
            &free_list_head,
            pager->pages[free_list_head]->page_data,
            sizeof(uint32_t)
        );
    }

    return false;
}

/* Connect free page to the beginning of the Free Page List. */
bool connect_free_page(Pager *pager, uint32_t page_num) {

    if (!pager || page_num >= pager->num_pages) {
        return false;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return false;
    }

    Page *page = pager_get_page(pager, page_num);
    if (!page) {
        return false;
    }

    if (is_page_free(pager, page_num)) {
        fprintf(stderr, "Page already free.\n");
        return false;
    }

    uint32_t free_list_head;
    memcpy(
        &free_list_head,
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );

    memcpy(
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        &page_num,
        sizeof(uint32_t)
    );

    memcpy(
        pager->pages[page_num]->page_data,
        &free_list_head,
        sizeof(uint32_t)
    );

    if (!page_mark_dirty(zero) || !page_mark_dirty(pager->pages[page_num])) {
        return false;
    }

    return true;
}