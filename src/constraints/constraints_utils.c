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
bool constraint_validate_column_refs(const Database *db, const Schema *schema, const Constraint *constraint) {
    if (!db|| !constraint || schema->num_columns == 0) {
        return false;
    }

    if (!constraint_validate_definition(constraint)) {
        return false;
    }

    uint32_t num_columns = schema->num_columns;

    switch (constraint->type) {
        case PRIMARY_KEY: {
            const PrimaryKeyConstraint *primary_key = &constraint->constraint_data.primary_key; 

            if (!constraint_column_refs_are_unique(
                    primary_key->primary_key_columns, 
                    primary_key->amount_columns
                )) {
                return false;
            }

            for (uint32_t i = 0; i < primary_key->amount_columns; i++) {
                if (primary_key->primary_key_columns[i] >= num_columns) {
                    return false;
                }
            }
            break;
        }

        case FOREIGN_KEY: 
            return constraint_validate_foreign_key(db, schema, constraint);

        case UNIQUE: {
            const UniqueConstraint *unique = &constraint->constraint_data.unique_cols;

            if (!constraint_column_refs_are_unique(
                    unique->column_refs,
                    unique->amount_columns
                )) {
                return false;
            }

            for (uint32_t i = 0; i < unique->amount_columns; i++) {
                if (unique->column_refs[i] >= num_columns) {
                    return false;
                }
            }
            break;
        }

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

// Validate that column references which semantically form a column set 
// contain no duplicates, e.g. reject UNIQUE(email, email)
bool constraint_column_refs_are_unique(const uint32_t *column_refs, uint32_t amount_columns) {
    if (!column_refs || amount_columns == 0) {
        return false;
    }

    for (uint32_t i = 0; i < amount_columns; i++) {
        for (uint32_t j = i+1; j < amount_columns; j++) {
            if (column_refs[i] == column_refs[j]) {
                return false;
            }
        }
    }

    return true;
}

/* Complete validation of FOREIGN KEY constraint semantics. */
bool constraint_validate_foreign_key(const Database *db, const Schema *local_schema, 
    const Constraint *constraint) {

    if (!db || !local_schema || !constraint || constraint->type != FOREIGN_KEY) {
        return false;
    }

    const ForeignKeyConstraint *foreign_key = &constraint->constraint_data.foreign_key;

    // A FOREIGN KEY maps local and referenced columns positionally
    if (foreign_key->amount_columns != foreign_key->amount_referenced_columns) {
        return false;
    }

    // Duplicate columns are invalid on either side of an FK definition
    if (!constraint_column_refs_are_unique(
            foreign_key->foreign_key_columns,
            foreign_key->amount_columns)) {
        return false;
    }

    if (!constraint_column_refs_are_unique(
            foreign_key->referenced_columns,
            foreign_key->amount_referenced_columns)) {
        return false;
    }

    // Validate local column references
    for (uint32_t i = 0; i < foreign_key->amount_columns; i++) {
        if (foreign_key->foreign_key_columns[i] >= local_schema->num_columns) {
            return false;
        }
    }

    // Locate referenced table
    Table *referenced_table = database_find_table(db, foreign_key->referenced_table_name);

    if (!referenced_table || !referenced_table->table_schema) {
        return false;
    }

    const Schema *referenced_schema = referenced_table->table_schema;

    // Validate referenced column references
    for (uint32_t i = 0; i < foreign_key->amount_referenced_columns; i++) {
        if (foreign_key->referenced_columns[i] >= referenced_schema->num_columns) {
            return false;
        }
    }

    // Corresponding FOREIGN KEY columns must have compatible types.
    // Current policy: exact DataType equality.
    for (uint32_t i = 0; i < foreign_key->amount_columns; i++) {
        
        uint32_t local_column_index = foreign_key->foreign_key_columns[i];
        uint32_t referenced_column_index = foreign_key->referenced_columns[i];

        const Column *local_column = local_schema->columns[local_column_index];
        const Column *referenced_column = referenced_schema->columns[referenced_column_index];

        if (!local_column || !referenced_column) {
            return false;
        }

        if (local_column->type != referenced_column->type) {
            return false;
        }
    }

    // Referenced columns must exactly match the ordered column tuple 
    // of a PRIMARY KEY or UNIQUE constraint
    bool references_candidate_key = false;

    for (uint32_t i = 0; i < referenced_schema->num_constraints; i++) {
        const Constraint *candidate = referenced_schema->constraints[i];

        if (!candidate) {
            continue;
        }

        if (candidate->type == PRIMARY_KEY) {
            const PrimaryKeyConstraint *primary_key = &candidate->constraint_data.primary_key;

            if (primary_key->amount_columns != foreign_key->amount_referenced_columns) {
                continue;
            }

            bool matches = true;

            for (uint32_t j = 0; j < foreign_key->amount_referenced_columns; j++) {
                if (foreign_key->referenced_columns[j] != primary_key->primary_key_columns[j]) {
                    matches = false;
                    break;
                }
            }

            if (matches) {
                references_candidate_key = true;
                break;
            }
        }
        else if (candidate->type == UNIQUE) {
            const UniqueConstraint *unique = &candidate->constraint_data.unique_cols;

            if (unique->amount_columns != foreign_key->amount_referenced_columns) {
                continue;
            }

            bool matches = true;

            for (uint32_t j = 0; j < foreign_key->amount_referenced_columns; j++) {
                if (foreign_key->referenced_columns[j] != unique->column_refs[j]) {
                    matches = false;
                    break;
                }
            }

            if (matches) {
                references_candidate_key = true;
                break;
            }
        }
    }

    if (!references_candidate_key) {
        return false;
    }

    return true;
}