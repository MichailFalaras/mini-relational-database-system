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
    if (!source || !amount) {
        return NULL;
    }

    uint32_t *copy = (uint32_t *) calloc(amount, sizeof(uint32_t));
    if (!copy) {
        perror("copy_uint32_array");
        exit(1);
    }

    memcpy(copy, source, amount*sizeof(uint32_t));
    return copy;
}

/* When Column gets removed, you have to update the Constraints column refs to
match the new indexes, which means decrementing the position indexes after the removed Column */
void constraint_shift_local_column_refs(Constraint *constraint, uint32_t index_threshold) {
    if (!constraint) {
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
            for (uint32_t i = 0; i < constraint->constraint_data.foreign_key.amount_columns; i++) {
                if (constraint->constraint_data.foreign_key.foreign_key_columns[i] > index_threshold) {
                    constraint->constraint_data.foreign_key.foreign_key_columns[i]--;
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
            if (constraint->constraint_data.not_null.column_ref > index_threshold) {
                constraint->constraint_data.not_null.column_ref--;
            }
            break;

        case DEFAULT: 
            if (constraint->constraint_data.default_value.column_ref > index_threshold) {
                constraint->constraint_data.default_value.column_ref--;
            }
            break;

        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            break;
    }
}

/* Decrements the referenced column indexes of another table's constraint */
void constraint_shift_referenced_column_refs(Constraint *constraint, uint32_t index_threshold) {
    if (!constraint || constraint->type != FOREIGN_KEY) {
        return;
    }

    ForeignKeyConstraint *foreign_key = &constraint->constraint_data.foreign_key;

    if (foreign_key->amount_referenced_columns > 0 && !foreign_key->referenced_columns) {
        return;
    }

    for (uint32_t i = 0; i < foreign_key->amount_referenced_columns; i++) {
        if (foreign_key->referenced_columns[i] > index_threshold) {
            foreign_key->referenced_columns[i]--;
        }
    }
}

/* Check if column_refs are valid. */
bool constraint_validate_column_refs(const Database *db, const Constraint *constraint, uint32_t num_columns) {
    if (!db|| !constraint || !num_columns) {
        return false;
    }

    if (!constraint_validate_definition(constraint)) {
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

        case FOREIGN_KEY: {
            /* Checking referencing FOREIGN KEY columns */
            const ForeignKeyConstraint *foreign_key = &constraint->constraint_data.foreign_key;

            for (uint32_t i = 0; i < foreign_key->amount_columns; i++) {
                if (foreign_key->foreign_key_columns[i] >= num_columns) {
                    return false;
                }
            }

            // Referenced table index might not exist too.
            Table *referenced_table = database_find_table(db, foreign_key->referenced_table_name);
            if (!referenced_table || !referenced_table->table_schema) {
                return false;
            }

            /* Checking referenced FOREIGN KEY columns of target table */
            uint32_t ref_num_columns = referenced_table->table_schema->num_columns;

            for (uint32_t i = 0; i < foreign_key->amount_referenced_columns; i++) {
                if (foreign_key->referenced_columns[i] >= ref_num_columns) {
                    return false;
                }
            }
            break;

        }
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
            break;

        case DEFAULT:
            if (constraint->constraint_data.default_value.column_ref >= num_columns) {
                return false;
            }
            break;

        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            return false;
    }

    return true;
}