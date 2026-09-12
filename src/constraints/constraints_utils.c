#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/constraints.h"
#include "../../include/schema.h"
#include "constraints_utils.h"
#include "../../include/database.h"
#include "../../include/table.h"
#include "../../include/row.h"
#include "../../include/table.h"
#include "../table/table_utils.h"
#include "../../include/pager.h"
#include "../../include/expressions.h"

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

// Validates a constraint against a single row
bool constraint_validate_row(Pager *pager, const Constraint *constraint, const Schema *schema, 
    const Row *row, const EvaluationContext *context) {
    
    // Validate inputs
    if (!constraint || 
        !schema || 
        !row || 
        !row->values ||
        row->n_columns != schema->num_columns ||
        !context) {
        return false;
    }
    
    // Validate per row constraint
    switch (constraint->type) {
        case NOT_NULL: {
            // If the referenced column is NULL, the current row fails to satisfy the constraint
            uint32_t column_ref = constraint->constraint_data.not_null.column_ref;

            if (column_ref >= schema->num_columns || !row->values[column_ref]) {
                return false;
            }
            
            if (row->values[column_ref]->null_val) {
                return false;
            }

            break;
        }

        case CHECK: {
            const CheckConstraint *check = &constraint->constraint_data.check;
            if (!check->constraint_expr) {
                return false;
            }

            Value *result = evaluate_expression(check->constraint_expr, context);
            if (!result) {
                return false;
            }

            bool valid = (result->type == BOOL && !result->null_val && result->value.bool_val);

            value_free(result);
            return valid;
        }

        case FOREIGN_KEY: {
            if (!pager || !pager->num_pages || !context->db) {
                return false;
            }

            const ForeignKeyConstraint *foreign_key = &constraint->constraint_data.foreign_key;

            // Firstly, inspect local Foreign Key values
            // A NULL-containing Foreign Key doesn't require a referenced row
            for (uint32_t i = 0; i < foreign_key->amount_columns; i++) {
                uint32_t local_col = foreign_key->foreign_key_columns[i];

                if (local_col >= row->n_columns || !row->values[local_col]) {
                    return false;
                }

                if (row->values[local_col]->null_val) {
                    return true;
                }
            }

            // At this point, no Foreign Key column is NULL, so we locate the referenced table
            Table *referenced_table = database_find_table(
                context->db, 
                foreign_key->referenced_table_name
            );

            if (!referenced_table) {
                return false;
            }

            // Build the referenced set of columns that will be used as a search key
            // to validate the existence of the referenced row in the referenced table
            Value **key_values = (Value **) calloc(foreign_key->amount_columns, sizeof(Value *));
            if (!key_values) {
                return false;
            }

            for (uint32_t i = 0; i < foreign_key->amount_columns; i++) {
                key_values[i] = row->values[foreign_key->foreign_key_columns[i]];
            }

            // Search for referenced row
            TableRowResult result = {0};

            TableLookupStatus status = table_find_exact(
                referenced_table,
                pager,
                key_values,
                foreign_key->referenced_columns,
                foreign_key->amount_referenced_columns,
                &result
            );

            free(key_values);

            if (status != TABLE_LOOKUP_SUCCESS) {
                table_row_result_free(&result);
                return false;
            }
            
            // If it exists, the constraint is validated
            bool exists = result.count > 0;

            table_row_result_free(&result);
            return exists;
        }

    
        // Those constraints are not validated per row, so return true
        case PRIMARY_KEY:
        case UNIQUE:
        case DEFAULT:
            return true;

        default:
            return false;
    }

    return true;
}

// Check whether a Primary Key or Unique constraint is referenced
// by any Foreign Key in the database
bool constraint_is_referenced_by_foreign_key(const Database *db, const Table *table, 
    const Constraint *target_constraint) {
    
    if (!db || !table || !table->table_schema || !target_constraint) {
        return false;
    }

    if (db->table_count > 0 && !db->tables) {
        return false;
    }

    // Determine the type of constraint and target columns to search for
    uint32_t *target_columns = NULL;
    uint32_t target_column_count = 0;

    switch (target_constraint->type) {
        case PRIMARY_KEY:
            target_columns = target_constraint->constraint_data.primary_key.primary_key_columns;
            target_column_count = target_constraint->constraint_data.primary_key.amount_columns;
            break;

        case UNIQUE:
            target_columns = target_constraint->constraint_data.unique_cols.column_refs;
            target_column_count = target_constraint->constraint_data.unique_cols.amount_columns;
            break;

        default:
            return false;
    }

    if (!target_columns || target_column_count == 0) {
        return false;
    }

    // Search all tables including the current one because self-referencing Foreign Keys are valid
    for (uint32_t i = 0; i < db->table_count; i++) {
        Table *current_table = db->tables[i];

        if (!current_table || !current_table->table_schema) {
            return false;
        }

        Schema *schema = current_table->table_schema;

        if (!schema->constraints) {
            return false;
        }

        // No constraints to inspect for the current table
        if (schema->num_constraints == 0) {
            continue;
        }

        // Inspect every constraint for a Foreign Key
        for (uint32_t j = 0; j < schema->num_constraints; j++) {
            Constraint *constraint = schema->constraints[j];
            if (!constraint) {
                return false;
            }

            if (constraint->type != FOREIGN_KEY) {
                continue;
            }

            const ForeignKeyConstraint *foreign_key = &constraint->constraint_data.foreign_key;

            // Foreign Key doesn't reference current table
            if (strcmp(
                    foreign_key->referenced_table_name,
                    table->name) != 0) {
                continue;
            }

            // Numbers of referenced columns don't match
            if (foreign_key->amount_referenced_columns != target_column_count) {
                continue;
            }

            if (!foreign_key->referenced_columns) {
                return false;
            }

            bool matches = true;

            for (uint32_t k = 0; k < target_column_count; k++) {
                if (foreign_key->referenced_columns[k] != target_columns[k]) {
                    matches = false;
                    break;
                }
            }

            if (matches) {
                return true;
            }
        }
    }

    return false;
}