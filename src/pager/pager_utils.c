#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../include/pager.h"
#include "../../include/page.h"
#include "pager_utils.h"

/* Write exactly PAGE_SIZE data using pwrite (without moving file offset). */
bool write_page_size(Pager *pager, Page *page) {
    size_t index = 0;
    size_t bytes_left_to_write = PAGE_SIZE;
    ssize_t bytes_written = 0;

    off_t base_offset = (off_t) page->page_num * (off_t) PAGE_SIZE;

    while (bytes_left_to_write > 0){
        bytes_written = pwrite(pager->fd, page->page_data + index,
                        bytes_left_to_write, base_offset);

        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            } 
            return false;
        }

        if (bytes_written == 0) {
            fprintf(stderr, "Possible zero byte loop terminated.\n");
            return false;
        }

        bytes_left_to_write -= (size_t) bytes_written;
        index +=  (size_t) bytes_written;
    }

    return true;
}

/* Read exacty PAGE_SIZE data using pread (without moving file offset). */
bool read_page_size(Pager *pager, Page *page) {
    size_t index = 0;
    size_t bytes_left_to_read = PAGE_SIZE;
    ssize_t bytes_read = 0;

    off_t base_offset = (off_t) page->page_num * (off_t) PAGE_SIZE;

    while (bytes_left_to_read > 0){
        bytes_read = pread(pager->fd, page->page_data + index, bytes_left_to_read, base_offset);

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (bytes_read == 0) {
            fprintf(stderr, "read_page_size has reached EOF.\n");
            break;
        }

        bytes_left_to_read -= (size_t) bytes_read;
        index += (size_t) bytes_read;
    }

   return true;
}

/* Get first free page from Free List Head. */
FreePageResult get_free_page(Pager *pager, uint32_t *page_num) {
    if (!pager || !page_num) {
        return FREE_PAGE_ERROR;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return FREE_PAGE_ERROR;
    }

    uint32_t free_list_head;
    memcpy(&free_list_head,
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t));
    
    /* No free pages. */
    if (free_list_head == 0) {
        return FREE_PAGE_EMPTY;
    } else if (free_list_head >= pager->num_pages || free_list_head >= MAX_PAGES) {
        fprintf(stderr, "Corrupted Free List page number.\n");
        return FREE_PAGE_ERROR;
    }

    /* Get page from cache or disk. */
    Page *free_page = pager_get_page(pager, free_list_head);
    if (!free_page) {
        return FREE_PAGE_ERROR;
    }
    
    *page_num = free_list_head;
    if (!page_clear(pager, pager->pages[*page_num])) {
        page_free(free_page);
        pager->pages[free_list_head] = NULL;
        return FREE_PAGE_ERROR;
    }
    
    /* Since we are returning the first free page no matter what,
     * update free_list_head of page 0 to show to the free page after
     * the page we are returning. */
    memcpy(zero->page_data + FREE_LIST_HEAD_OFFSET,
        free_page->page_data,
        sizeof(uint32_t));


    if (!page_mark_dirty(zero)) {
        page_free(free_page);
        pager->pages[free_list_head] = NULL;
        return FREE_PAGE_ERROR;
    }

    return FREE_PAGE_FOUND;
}

/* Check if page is already free. */
PageFreeStatus is_page_free(Pager *pager, uint32_t page_num) {
    if (!pager || page_num >= pager->num_pages 
        || page_num >= MAX_PAGES) {
        return PAGE_FREE_ERROR;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return PAGE_FREE_ERROR;
    }

    uint32_t free_list_head;
    memcpy(
        &free_list_head,
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );

    if (free_list_head >= pager->num_pages || free_list_head >= MAX_PAGES) {
        fprintf(stderr, "Corrupted Free List page number.\n");
        return PAGE_FREE_ERROR;
    }

    uint32_t visited = 0;
    while (free_list_head != 0) {
        visited++;

        /* Check if Free List cycle is endless. */
        if (visited > pager->num_pages) {
            fprintf(stderr, "Terminating is_page_free loop.\n");
            return PAGE_FREE_ERROR;
        }

        if (free_list_head == page_num) {
            return PAGE_IS_FREE;
        }

        if (!pager->pages[free_list_head]) {
            /* Something wrong with free list pages then. */
            if (!pager_get_page(pager, free_list_head)) {
                return PAGE_FREE_ERROR;
            }
            /* Load the page into memory and continue the loop. */
        }

        memcpy(
            &free_list_head,
            pager->pages[free_list_head]->page_data,
            sizeof(uint32_t)
        );
    }

    return PAGE_NOT_FREE;
}

/* Connect free page to the beginning of the Free Page List. */
bool connect_free_page(Pager *pager, uint32_t page_num) {

    if (!pager || page_num >= pager->num_pages
        || page_num >= MAX_PAGES) {
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

    PageFreeStatus res = is_page_free(pager, page_num); 
    if (res == PAGE_FREE_ERROR) {
        return false;
    } else if (res == PAGE_IS_FREE) {
        fprintf(stderr, "Page already free.\n");
        return false;
    }

    uint32_t free_list_head;
    memcpy(
        &free_list_head,
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );

    if (free_list_head >= pager->num_pages || free_list_head >= MAX_PAGES) {
        fprintf(stderr, "Corrupted Free List page number.\n");
        return false;
    }

    memcpy(
        zero->page_data + FREE_LIST_HEAD_OFFSET,
        &page_num,
        sizeof(uint32_t)
    );

    memcpy(
        page->page_data,
        &free_list_head,
        sizeof(uint32_t)
    );

    if (!page_mark_dirty(zero) || !page_mark_dirty(page)) {
        return false;
    }

    return true;
}