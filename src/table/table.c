#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/table.h"
#include "../../include/schema.h"
#include "../../include/constraints.h"
#include "../../include/index.h"

#define INVALID_ROOT_PAGE UINT32_MAX

/* Creation of logical Table struct */
Table *table_metadata_create(const char *table_name, const Schema *schema) {
    // Input validation
    if (!table_name || table_name[0] == '\0') {
        printf("table_metadata_create: Invalid input table name.\n");
        return NULL;
    }

    if (!schema) {
        printf("table_metadata_create: Input schema is NULL.\n");
        return NULL;
    }

    if (schema->num_columns > 0 && !schema->columns) {
        printf("table_metadata_create: Schema column array is NULL.\n");
        return NULL;
    }

    if (schema->num_constraints > 0 && !schema->constraints) {
        printf("table_metadata_create: Schema constraints array is NULL.\n");
        return NULL;
    }

    // Table creation
    Table *new_table = (Table *) calloc(1, sizeof(Table));
    if (!new_table) {
        printf("table_metadata_create: New table could not be allocated.\n");
        return NULL;
    }

    // Table fields assignment
    if (strlen(table_name) >= sizeof(new_table->name)) {
        printf("table_metadata_create: Input table name exceeds length limit.\n");
        table_free(new_table);
        return NULL;
    }

    strcpy(new_table->name, table_name);

    new_table->table_schema = schema_copy(schema);
    if (!new_table->table_schema) {
        printf("table_metadata_create: Schema could not be copied.\n");
        table_free(new_table);
        return NULL;
    }

    // Already assigned by calloc(), but displayed for display of logical initialization
    new_table->primary_index = NULL;

    for (uint32_t i = 0; i < MAX_INDEXES; i++) {
        new_table->secondary_indexes[i] = NULL;
    }

    new_table->is_deleted = false;
    new_table->row_count = 0;
    new_table->total_secondary_indexes = 0;


    // Create indexes that correspond to the PRIMARY KEY and any UNIQUE constraints
    for (uint32_t i = 0; i < new_table->table_schema->num_constraints; i++) {
        
        const Constraint *constraint = new_table->table_schema->constraints[i];
        if (!constraint) {
            printf("table_metadata_create: Schema contains NULL constraint.\n");
            table_free(new_table);
            return NULL;
        }

        switch (constraint->type) {
            case PRIMARY_KEY: {
                if (new_table->primary_index) {
                    printf("table_metadata_create: Schema contains multiple primary key constraints.\n");
                    table_free(new_table);
                    return NULL;
                }

                const PrimaryKeyConstraint *primary_key = &constraint->constraint_data.primary_key;

                // Primary Index key 
                IndexKey *key = index_key_create(primary_key->primary_key_columns, primary_key->amount_columns);
                if (!key) {
                    printf("table_metadata_create: Primary Index key could not be created.\n");
                    table_free(new_table);
                    return NULL;
                }

                // Primary Index at a placeholder page for now
                new_table->primary_index = index_metadata_create(
                    constraint->constraint_name,
                    PRIMARY_INDEX,
                    key,
                    INVALID_ROOT_PAGE
                );

                // Index key is deep-copied. Free the old copy 
                index_key_free(key);

                if (!new_table->primary_index) {
                    printf("table_metadata_create: Primary index metadata could not be created.\n");
                    table_free(new_table);
                    return NULL;
                }

                break;
            }

            case UNIQUE: {
                // Checking if we're exceeding the maximum number of allowed secondary indexes
                if (new_table->total_secondary_indexes >= MAX_INDEXES) {
                    printf("table_metadata_create: Maximum number of secondary indexes exceeded.\n");
                    table_free(new_table);
                    return NULL;
                }

                const UniqueConstraint *unique = &constraint->constraint_data.unique_cols;

                // Unique Secondary Index key
                IndexKey *key = index_key_create(unique->column_refs, unique->amount_columns);
                if (!key) {
                    printf("table_metadata_create: Unique Index key could not be created.\n");
                    table_free(new_table);
                    return NULL;
                }

                // Unique Secondary Index at a placeholder page for now
                Index *unique_index = index_metadata_create(
                    constraint->constraint_name,
                    SECONDARY_INDEX,
                    key,
                    INVALID_ROOT_PAGE
                );

                // Index key is deep-copied. Free the old copy 
                index_key_free(key);

                if (!unique_index) {
                    printf("table_metadata_create: Unique index metadata could not be created.\n");
                    table_free(new_table);
                    return NULL;
                }

                new_table->secondary_indexes[new_table->total_secondary_indexes] = unique_index;
                new_table->total_secondary_indexes++;
                
                break;
            }

            // These constraints don't correspond to a secondary index
            case FOREIGN_KEY:
            case CHECK:
            case NOT_NULL:
            case DEFAULT:
                break;

            default:
                printf("table_metadata_create: Invalid constraint type.\n");
                table_free(new_table);
                return NULL;
        }
    }


    return new_table;
}

/* Deallocation of logical Table struct */
void table_free(Table *table) {
    if (!table) {
        printf("table_free: Table struct is NULL.\n");
        return;
    }

    // Free primary key index
    if (table->primary_index) {
        index_free(table->primary_index);
        table->primary_index = NULL;
    }

    // Free any secondary indexes
    for (uint32_t i = 0; i < MAX_INDEXES; i++) {
        if (table->secondary_indexes[i]) {
            index_free(table->secondary_indexes[i]);
            table->secondary_indexes[i] = NULL;
        }
    }

    // Free schema
    if (table->table_schema) {
        schema_free(table->table_schema);
        table->table_schema = NULL;
    }

    free(table);
}

/* Check if a table has a particular column */
bool table_has_column(const Table *table, const char *col_name) {
    if (!table || !table->table_schema) {
        printf("table_has_column: Invalid input table.\n");
        return false;
    }

    if (!col_name || col_name[0] == '\0') {
        printf("table_has_column: Invalid input column name.\n");
        return false;
    }

    // As long as there's a valid index for the searched column, the column exists
    if (schema_find_column_index(table->table_schema, col_name) >= 0) {
        return true;
    }

    return false;
}

/* Returns a pointer to a table's column */
Column *table_find_column(const Table *table, const char *col_name) {
    if (!table || !table->table_schema) {
        printf("table_find_column: Invalid input table.\n");
        return NULL;
    }

    if (!col_name || col_name[0] == '\0') {
        printf("table_find_column: Invalid input column name.\n");
        return NULL;
    }

    return schema_find_column(table->table_schema, col_name);
}

/* Returns a pointer to a table's index */
Index *table_find_index(const Table *table, const char *index_name) {
    if (!table) {
        printf("table_find_index: Input table is NULL.\n");
        return NULL;
    }

    if (!index_name || index_name[0] == '\0') {
        printf("table_find_index: Invalid input index name.\n");
        return NULL;
    }

    // Checking for a primary index match
    if(table->primary_index && strcasecmp(table->primary_index->name, index_name) == 0) {
        return table->primary_index;
    }

    // And then for any secondary index match
    for (uint32_t i = 0; i < table->total_secondary_indexes; i++) {
        if (!table->secondary_indexes[i]) {
            printf("table_find_index: Table has NULL secondary index.\n");
            return NULL;
        }

        if (strcasecmp(table->secondary_indexes[i]->name, index_name) == 0) {
            return table->secondary_indexes[i];
        }
    }

    return NULL;
}