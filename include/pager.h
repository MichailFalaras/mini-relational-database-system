#ifndef PAGER_H_
#define PAGER_H_

#include<fcntl.h>
#include<time.h>
#include "row.h"
#define PAGE_SIZE 4096
#define MAX_PAGES 100

/* Page structure that contains:
 * is_dirty: flag that denotes modified page
 * n_rows: amount of rows
 * size: size of page in bytes
 * rows: rows contained in a page 
 * last_interacted: time of last interaction. */
typedef struct page {
    int is_dirty;
    int n_rows;
    size_t size;
    Row **rows;
    time_t last_interacted;
} Page;

/* Pager component that reads/writes pages from the disk
 * fd: file descriptor of database file (e.g. users.db)
 * file_length: file length of database file
 * raw_pages: buffer of raw binary pages. */
typedef struct pager {
    size_t fd;
    size_t file_length;
    void *raw_pages[MAX_PAGES];
} Pager;

#endif