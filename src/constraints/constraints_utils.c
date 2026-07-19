#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/constraints.h"
#include "../../include/schema.h"
#include "constraints_utils.h"
#include "../../include/database.h"
#include "../../include/table.h"

/* Helper function to deep-copy uint32_t array (of column_refs). */
uint32_t *copy_uint32_array(const uint32_t *source, uint32_t amount) {

    if (source == NULL) {
        return NULL;
    }

    if (amount == 0) {
        return NULL;
    }

    uint32_t *copy = (uint32_t *) malloc(amount*sizeof(uint32_t));
    if (copy == NULL) {
        perror("copy_uint32_array");
        exit(1);
    }

    memcpy(copy, source, amount*sizeof(uint32_t));
    return copy;
}

/* When Column gets removed, you have to update the Constraints column refs to
match the new indexes.*/
void constraint_shift_indexes(Constraint *constraint, uint32_t index_threshold) {

    if (constraint == NULL) {
        return;
    }

    switch (constraint->type) {
        case PRIMARY_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.primary_key.amount_columns; i++) {
                if (constraint->constraint_data.primary_key.primary_key_columns[i] > index_threshold) {
                    constraint->constraint_data.primary_key.primary_key_columns[i]--;
                }
            }
            break;
        case FOREIGN_KEY:
            /*for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_foreign_keys; i++) {
                
                if (constraint->constraint_data.foreign_keys.foreign_key_columns[i] > index_threshold) {
                    constraint->constraint_data.foreign_keys.foreign_key_columns[i]--;
                }
            }*/
            for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_referenced_columns; i++) {
                if (constraint->constraint_data.foreign_keys.referenced_columns[i] > index_threshold) {
                    constraint->constraint_data.foreign_keys.referenced_columns[i]--;
                }
            }
            break;
        case UNIQUE:
            for (uint32_t i = 0; i < constraint->constraint_data.unique_cols.amount_columns; i++) {
                if (constraint->constraint_data.unique_cols.column_refs[i] > index_threshold) {
                    constraint->constraint_data.unique_cols.column_refs[i]--;
                }
            }
            break;
        case CHECK:
            for (uint32_t i = 0; i < constraint->constraint_data.check.amount_columns; i++) {
                if (constraint->constraint_data.check.column_refs[i] > index_threshold) {
                    constraint->constraint_data.check.column_refs[i]--;
                }
            }
            break;
        case NOT_NULL:
        case DEFAULT: break;
        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            return;
    }
}

/* Check if column_refs are valid. */
bool constraint_validate_column_refs(const Database *db, const Constraint *constraint, uint32_t num_columns) {

    if (db == NULL || constraint == NULL || num_columns == 0) {
        return false;
    }

    switch (constraint->type) {
        case PRIMARY_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.primary_key.amount_columns; i++) {
                if (constraint->constraint_data.primary_key.primary_key_columns[i] >= num_columns) {
                    return false;
                }
            }
            break;
        case FOREIGN_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_foreign_keys; i++) {
                if (constraint->constraint_data.foreign_keys.foreign_key_columns[i] >= num_columns) {
                    return false;
                }
            }

            /* Referenced table index might not exist too. */
            if (constraint->constraint_data.foreign_keys.referenced_table >= db->table_count) {
                return false;
            }

            uint32_t ref_num_columns = db->tables[constraint->constraint_data.foreign_keys.referenced_table]->table_schema->num_columns;
            for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_referenced_columns; i++) {
                if (constraint->constraint_data.foreign_keys.referenced_columns[i] >= ref_num_columns) {
                    return false;
                }
            }
            break;
        case UNIQUE:
            for (uint32_t i = 0; i < constraint->constraint_data.unique_cols.amount_columns; i++) {
                if (constraint->constraint_data.unique_cols.column_refs[i] >= num_columns) {
                    return false;
                }
            }
            break;
        case CHECK:
            for (uint32_t i = 0; i < constraint->constraint_data.check.amount_columns; i++) {
                if (constraint->constraint_data.check.column_refs[i] >= num_columns) {
                    return false;
                }
            }
            break;
        case NOT_NULL:
            if (constraint->constraint_data.not_null.column_ref >= num_columns) {
                return false;
            }
        case DEFAULT: break;
        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            return false;
    }

    return true;
}