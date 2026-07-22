#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../../include/pager.h"
#include "../../include/page.h"
#include "pager_utils.h"

/* Create and initialize pager. */
Pager *pager_create(int fd, size_t file_length, uint32_t num_pages) {
    Pager *pager = (Pager *) calloc(1, sizeof(Pager));
    if (!pager) {
        perror("pager_alloc");
        return NULL;
    }

    pager->fd = fd;
    pager->file_length = file_length;
    pager->num_pages = num_pages;
    return pager;
}

/* Logical allocation of a new page.
 * If there are free pages in cache/disk, returns their page numbers. */
bool pager_allocate_page(Pager *pager, uint32_t *out_page_num) {

    if (!pager || !out_page_num) {
        return false;
    }

    if (pager->num_pages >= MAX_PAGES) {
        printf("Database is full.\n");
        return false;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return false;
    }

    /* No free pages. */
    if (!get_free_page(pager, out_page_num)) {
        *out_page_num = pager->num_pages;
        pager->num_pages++;
    } 
    /* Else, free page num is stored in out_page_num automatically. */

    return true;
}

/* Open database file descriptor, initialize basic metrics and
create pager. */
Pager *pager_open(const char *filename) {

    if (!filename || !filename[0]) {
        printf("pager_open: Empty filename\n");
        return NULL;
    }

    int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("pager_open");
        return NULL;
    }

    off_t len = lseek(fd, (off_t) 0, SEEK_END);
    if (len < 0) {
        perror("pager_open");
        close(fd);
        return NULL;
    }

    uint32_t num_pages = len / PAGE_SIZE;
    if (num_pages > MAX_PAGES) {
        fprintf(stderr, "pager_open: database exceeds MAX_PAGES.\n");
        close(fd);
        return NULL;
    } else if (len > 0 && len % PAGE_SIZE != 0) {
        printf("pager_open: Database file length is not a multiple of PAGE_SIZE\n");
        close(fd);
        return NULL;
    }

    Pager *pager = pager_create(fd, len, num_pages);
    /*
    if (len == 0) {
        if (!pager_initialize_new_database(pager)) {
            fprintf(stderr, "Failed to initialize new databse with Pages 0 and 1.\n");
            pager_free(pager);
            close(fd);
            return NULL;
        }
    }*/
    return pager;
}

/* Create Page 0 and System Catalog Page and store it in cache ready
to be flushed. */
bool pager_initialize_new_database(Pager *pager) {
    if (pager == NULL) {
        return false;
    }

    PageZeroMetadata *page_zero_metadata = page_zero_create();
    Page *page = pager_get_page(pager, 0);
    if (!page) {
        return false;
    }

    /* Struct has attribute "packed" so no padding is being stored. */
    memcpy(page->page_data, page_zero_metadata, sizeof(PageZeroMetadata));
    free(page_zero_metadata);
    if (!page_mark_dirty(page)) {
        return false;
    }

    page = pager_get_page(pager, 1);
    if (!page) {
        return false;
    }
    /* B+Tree functions responsible for storing information in system catalog
     * since it's normal B+Tree. */
    if (!page_clear(page)) {
        return false;
    }

    pager->num_pages += 2;
    pager->file_length = pager->num_pages * PAGE_SIZE;
    return true;
}

/* Close pager and flush all dirty pages back to the disk. */
bool pager_close(Pager *pager) {

    if (!pager) {
        return false;
    }

    bool flush_check;
    if (!(flush_check = pager_flush_all(pager))) {
        fprintf(stderr, "Flushing database pages failed.\n");
    }

    bool close_check = true;
    if (close(pager->fd) == -1) {
        perror("pager_close");
        close_check = false;
    }

    pager_free(pager);
    return flush_check && close_check;
}

/* Pager return specific page or create a new one in RAM ready
to be stored in the disk. */
Page *pager_get_page(Pager *pager, uint32_t page_num) {

    if (!pager || page_num >= MAX_PAGES) {
        return NULL;
    }

    /* Index of pager->pages corresponds to page_num. */
    if (pager->pages[page_num] != NULL) {
        if (!page_touch(pager->pages[page_num])) {
            return NULL;
        }

        return pager->pages[page_num];
    }

    Page *page = page_create(page_num);
    if ((off_t) page_num * (off_t) PAGE_SIZE >= pager->file_length) {
        pager->pages[page_num] = page;
        return page;
    }

    off_t pos = lseek(pager->fd, (off_t) page_num * (off_t) PAGE_SIZE, SEEK_SET);
    if (pos < 0) {
        perror("pager_get_page");
        page_free(page);
        return NULL;
    }

    if (!read_page_size(pager,page)) {
        page_free(page);
        return NULL;
    }

    page_touch(page);
    pager->pages[page_num] = page;
    return page;
}

/* Free page and connect it to the beginning of the list of free pages. */
bool pager_release_page(Pager *pager, uint32_t page_num) {

    if (!pager || page_num >= pager->num_pages) {
        return false;
    }

    Page *zero = pager_get_page(pager, 0);
    if (!zero) {
        return false;
    }

    /* Can't release Page 0 or System Catalog.*/
    uint32_t system_catalog;
    memcpy(
        &system_catalog,
        zero->page_data + SYSTEM_CATALOG_ROOT_PAGE_OFFSET,
        sizeof(uint32_t)
    );

    if (page_num == 0 || system_catalog == page_num) {
        return false;
    }

    if (!connect_free_page(pager, page_num)) {
        return false;
    }

    /* Will be extremely resource consuming writing 0s on every page being
    freed. Just keep the garbage.
    memset(page->page_data, 0, PAGE_SIZE); */

    return true;
}

/* Remove page from cache and flushing it to the disk. */
bool pager_evict_page(Pager *pager, uint32_t page_num) {
    if (pager == NULL || page_num >= MAX_PAGES) {
        return false;
    }

    /* If it is not in the cache. */
    if (pager->pages[page_num] == NULL) {
        return true;
    }

    if (pager->pages[page_num]->is_dirty == true) {
        bool res = pager_flush_page(pager, page_num);
        if (res == false) {
            return false;
        }
    }

    page_free(pager->pages[page_num]);
    pager->pages[page_num] = NULL;

    return true;
}

/* Removing least recently used page from cache and flushing it. */
bool pager_evict_lru(Pager *pager) {
    if (pager == NULL) {
        return false;
    }

    time_t lru = time(NULL);
    uint32_t lru_page_num;
    bool found = false;
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (pager->pages[i] == NULL) {
            continue;
        }

        if (pager->pages[i]->last_interacted <= lru) {
            found = true;
            lru = pager->pages[i]->last_interacted;
            lru_page_num = i;
        }
    }

    if (!found) {
        return false;
    }

    if (!pager_evict_page(pager, lru_page_num)) {
        return false;
    }

    return true;
}

/* Removing all pages from cache and flushing them. */
bool pager_evict_all(Pager *pager) {

    if (pager == NULL) {
        return false;
    }

    /* MAX_PAGES because there might be more pages in RAM than the disk. */
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (pager->pages[i] == NULL) {
            continue;
        }

        if (!pager_evict_page(pager, i)) {
            fprintf(stderr, "Page eviction failed.\n");
        }
    }

    return true;
}

/* Flush all pages to the disk. */
bool pager_flush_all(Pager *pager) {
    if (!pager) {
        return false;
    }

    bool all_flushed = true;
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (!pager->pages[i] || pager->pages[i]->is_dirty == false) {
            continue;
        }

        if (!pager_flush_page(pager, i)) {
            fprintf(stderr, "Page flushing failed.\n");
            all_flushed = false;
        }
    }

    return all_flushed;
}

/* Flush specific page to the disk. */
bool pager_flush_page(Pager *pager, uint32_t page_num) {

    if (!pager || page_num >= MAX_PAGES) {
        return false;
    }

    if (!pager->pages[page_num]) {
        return false;
    }

    if (pager->pages[page_num]->is_dirty == false) {
        return true;
    }

    off_t pos = lseek(pager->fd, (off_t) page_num * (off_t) PAGE_SIZE, SEEK_SET);
    if (pos < 0) {
        perror("pager_flush_page");
        return false;
    }

    if (!write_page_size(pager, pager->pages[page_num])) {
        return false;
    }
    
    off_t current_file_length = lseek(pager->fd, (off_t) 0, SEEK_END);
    if (current_file_length < 0) {
        perror("pager_flush_page");
        return false;
    }

    if (!page_mark_clean(pager->pages[page_num])) {
        return false;
    }

    if (current_file_length > pager->file_length) {
        pager->file_length = (size_t) current_file_length;
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