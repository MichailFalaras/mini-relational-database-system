#ifndef PAGER_H_
#define PAGER_H_

#include<fcntl.h>
#include"page.h"
#define MAX_PAGES 100

/* Pager component that reads/writes pages from the disk
 * fd: file descriptor of database file (e.g. users.db)
 * file_length: file length of database file
 * num_pages: amount of pages
 * pages: pointer to pages opened */
typedef struct pager {
    size_t fd;
    size_t file_length;
    uint32_t num_pages;
    Page *pages[MAX_PAGES];
} Pager;

#endif