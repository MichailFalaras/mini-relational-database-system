#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../include/utils.h"

/* Read query. Supports one-line & multi-line queries. */
char *read_query() {
    char temp[64];
    char *buffer = NULL, *new_buffer;
    int n;
    int buffer_size = 0, old_buffer_size = 0;

    /* Reads from stdin and stores in dynamically allocated buffer. */
    while ((n = read(STDIN_FILENO, temp, 64)) > 0) {
        old_buffer_size = buffer_size;
        buffer_size += n;
        new_buffer = (char *) realloc(buffer, buffer_size);
        if (new_buffer == NULL) {
            printf("Memory error.\n");
            exit(1);
        }
        buffer = new_buffer;

        memcpy(buffer + old_buffer_size, temp, n);

        /* Remove '\n' from multi-line queries.
        Replace with ' '. */
        char *temp = strchr(buffer+old_buffer_size, '\n');
        if (temp != NULL) {
            *temp = ' ';
        }
    }
    if (n < 0) {
        perror("read");
        exit(1);
    }

    /* If EOF and buffer isn't empty then add terminating null byte. */
    if (buffer != NULL) {
        *(buffer + buffer_size-1) = '\0';
    }
    
    return buffer;
}