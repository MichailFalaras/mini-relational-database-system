#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../../include/pager.h"
#include "../../include/page.h"
#include "pager_utils.h"

bool write_page_size(Pager *pager, Page *page) {
    int index = 0;
    int bytes_left_to_write = PAGE_SIZE;
    int bytes_written = 0;

    while ((bytes_written = write(pager->fd, page->page_data + index, bytes_left_to_write)) >= 0) {
        if (bytes_written == bytes_left_to_write) {
            break;
        }

        bytes_left_to_write -= bytes_written;
        index += bytes_written;
    }

    if (bytes_written < 0) {
        perror("write_page_size");
        return false;
    }

    return true;
} 