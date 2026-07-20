#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../../include/pager.h"
#include "../../include/page.h"
#include "pager_utils.h"

/* Create and initialize pager. */
Pager *pager_create(int fd, size_t file_length, uint32_t num_pages) {
    Pager *pager = (Pager *) calloc(1, sizeof(Pager));
    if (pager == NULL) {
        perror("pager_alloc");
        exit(1);
    }

    pager->fd = fd;
    pager->file_length = file_length;
    pager->num_pages = num_pages;
    return pager;
}

/* Logical allocation of a new page. */
bool pager_allocate_page(Pager *pager, uint32_t *out_page_num) {

    if (pager == NULL) {
        return false;
    }

    if (pager->num_pages >= MAX_PAGES) {
        printf("Database is full.\n");
        return false;
    }

    /* Sequential file growth. 
     * We won't have page 9 if we didnt have pages from 0-8.*/
    *out_page_num = pager->num_pages;
    pager->num_pages++;
    return true;
}

/* Open database file descriptor, initialize basic metrics and
create pager. */
Pager *pager_open(const char *filename) {

    if (filename == NULL || filename[0] == '\0') {
        printf("pager_open: Empty filename\n");
        return NULL;
    }

    int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("pager_open");
        exit(1);
    }

    off_t len = lseek(fd, (off_t) 0, SEEK_END);
    if (len < 0) {
        perror("pager_open");
        exit(1);
    }

    uint32_t num_pages = len / PAGE_SIZE;
    if (len % PAGE_SIZE != 0) {
        printf("pager_open: Database file length is not a multiple of PAGE_SIZE\n");
        exit(1);
    }

    Pager *pager = pager_create(fd, len, num_pages);
    return pager;
}

/* Close pager and flush all dirty pages back to the disk. */
bool pager_close(Pager *pager) {

    if (pager == NULL) {
        return false;
    }

    bool res = pager_flush_all(pager);
    /* Supposedly this isn't correct.
    if (res == false) {
        return false;
    }*/

    close(pager->fd);
    pager_free(pager);
    return true;
}

/* Pager return specific page or create a new one in RAM ready
to be stored in the disk. */
Page *pager_get_page(Pager *pager, uint32_t page_num) {

    if (pager == NULL || page_num >= MAX_PAGES) {
        return NULL;
    }

    /* Index of pager->pages corresponds to page_num. */
    if (pager->pages[page_num] != NULL) {
        bool res = page_touch(pager->pages[page_num]);
        if (res == false) {
            return NULL;
        }
        return pager->pages[page_num];
    }

    Page *page = page_create(page_num);
    if (page_num * PAGE_SIZE >= pager->file_length) {
        pager->pages[page_num] = page;
        return page;
    }

    off_t pos = lseek(pager->fd, (off_t) page_num*PAGE_SIZE, SEEK_SET);
    if (pos < 0) {
        perror("pager_get_page");
        exit(1);
    }

    int res = read(pager->fd, page->page_data, PAGE_SIZE);
    if (res < 0) {
        perror("pager_get_page");
        exit(1);
    }

    pager->pages[page_num] = page;
    return page;
}

/* Flush all pages to the disk. */
bool pager_flush_all(Pager *pager) {
    bool res;

    for (uint32_t i = 0; i < pager->num_pages; i++) {
        if (pager->pages[i] == NULL || pager->pages[i]->is_dirty == false) {
            continue;
        }

        res = pager_flush_page(pager, i);
        if (res == false) {
            return false;
        }
    }

    return true;
}

/* Flush specific page to the disk. */
bool pager_flush_page(Pager *pager, uint32_t page_num) {

    if (pager == NULL || page_num >= MAX_PAGES) {
        return false;
    }

    if (pager->pages[page_num] == NULL) {
        return false;
    }

    if (pager->pages[page_num]->is_dirty == false) {
        return false;
    }

    off_t pos = lseek(pager->fd, (off_t) page_num*PAGE_SIZE, SEEK_SET);
    if (pos < 0) {
        perror("pager_flush_page");
        exit(1);
    }

    bool res = write_page_size(pager, pager->pages[page_num]);
    if (res == false) {
        return false;
    }
    
    off_t current_file_length = lseek(pager->fd, (off_t) 0, SEEK_END);
    if (current_file_length < 0) {
        perror("pager_flush_page");
        exit(1);
    }

    res = page_mark_clean(pager->pages[page_num]);
    if (res == false) {
        return false;
    }

    if (current_file_length > pager->file_length) {
        pager->file_length += PAGE_SIZE;
    }
    
    return true;       
}

/* Free pager component and caches pages. */
void pager_free(Pager *pager) {
    if (pager != NULL) {
        for (uint32_t i = 0; i < MAX_PAGES; i++) {
            if (pager->pages[i] != NULL) {
                page_free(pager->pages[i]);
                pager->pages[i] = NULL;
            }
        }
        free(pager);
    }
}