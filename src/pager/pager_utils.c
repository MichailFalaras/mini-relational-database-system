#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../include/pager.h"
#include "../../include/page.h"
#include "pager_utils.h"

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
