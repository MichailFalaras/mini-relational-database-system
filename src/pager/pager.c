#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../../include/pager.h"
#include "../../include/page.h"

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

bool pager_close(Pager *pager) {

    if (pager == NULL) {
        return false;
    }

    for (int i = 0; i < MAX_PAGES; i++) {
        if (pager->pages[i] == NULL) {
            continue;
        }

        if (pager->pages[i]->is_dirty == false) {
            continue;
        }

        off_t pos = lseek(pager->fd, (off_t) i*PAGE_SIZE, SEEK_SET);
        if (pos < 0) {
            perror("pager_close");
            exit(1);
        }

        int res = write(pager->fd, pager->pages[i]->page_data, PAGE_SIZE);
        if (res < 0) {
            perror("pager_close");
            exit(1);
        }
    }

    close(pager->fd);
    pager_free(pager);
    return true;
}

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

