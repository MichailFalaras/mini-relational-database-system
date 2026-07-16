#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/schema.h"

void close_array_gap(void **array, uint32_t count, uint32_t index_of_deletion) {

    if (array == NULL || count == 0 || index_of_deletion >= count) {
        return;
    }

    for (uint32_t i = index_of_deletion; i < count-1; i++) {
        array[i] = array[i+1];
    }

    array[count-1] = NULL;
}

void **resize_array(void **array, uint32_t new_size) {

    if (new_size == 0) {
        free(array);
        return NULL;
    }

    void **new_array = (void **) realloc(array, new_size*sizeof(void *));
    if (new_array == NULL) {
        perror("resize_array");
        exit(1);
    }

    return new_array;
}