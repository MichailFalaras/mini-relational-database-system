#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/schema.h"
#include "../../include/database.h"
#include "../../include/table.h"
#include "../../include/constraints.h"
#include "../src/constraints/constraints_utils.h"

void shift_indexes(Schema *schema, Database *db, uint32_t index_threshold) {

    for (uint32_t i = 0; i < schema->num_constraints; i++) {
        constraint_shift_indexes(schema->constraints[i], index_threshold);
    }

    uint32_t table_index;
    for (uint32_t i = 0; i < db->table_count; i++) {
        if (db->tables[i]->table_schema == schema) {
            table_index = i;
        }
    }

    for (uint32_t i = 0; i < db->table_count; i++) {
        if (i == table_index) {
            continue;
        }

        Schema *current_schema = db->tables[i]->table_schema;
        for (uint32_t j = 0; j < current_schema->num_constraints; j++) {
            if (current_schema->constraints[j]->type != FOREIGN_KEY) {
                continue;
            }

            if (constraint_references_table(current_schema->constraints[j], table_index)) {
                constraint_shift_indexes(current_schema->constraints[j], index_threshold);
            }
        }
    }
}


/* Both functions probably should be moved to a global utils file. */

/* Close array gap when we free pointer in the array. */
void close_array_gap(void **array, uint32_t count, uint32_t index_of_deletion) {

    if (array == NULL || count == 0 || index_of_deletion >= count) {
        return;
    }

    for (uint32_t i = index_of_deletion; i < count-1; i++) {
        array[i] = array[i+1];
    }

    array[count-1] = NULL;
}

/* Resize array to a bigger/smaller size. */
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