#ifndef PAGER_H_
#define PAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#define MAX_PAGES 100

typedef struct page Page;

/* Pager component that reads/writes pages from the disk
 * fd: file descriptor of database file (e.g. users.db)
 * file_length: file length of database file
 * num_pages: amount of pages in disk
 * (Use MAX_PAGES to talk about pages in RAM)
 * pages: pointer to pages opened */
typedef struct pager {
    int fd;
    size_t file_length;
    uint32_t num_pages;
    Page *pages[MAX_PAGES];
} Pager;

extern Pager *pager_create(int fd, size_t file_length, uint32_t num_pages);

extern Pager *pager_open(const char *filename);

extern bool pager_close(Pager *pager);

extern Page *pager_get_page(Pager *pager, uint32_t page_num);

extern bool pager_flush_page(Pager *pager, uint32_t page_num);

extern bool pager_flush_all(Pager *pager);

extern void pager_free(Pager *pager);

#endif