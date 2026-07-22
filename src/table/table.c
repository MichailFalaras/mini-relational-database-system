#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/table.h"
#include "../../include/schema.h"
#include "../schema/schema_utils.h"
#include "../../include/constraints.h"
#include "../../include/index.h"
#include"../index/index_utils.h"

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

    new_table->secondary_indexes = (Index **) calloc(MAX_INDEXES, sizeof(Index *));
    if (!new_table->secondary_indexes) {
        printf("table_metadata_create: Secondary indexes pointer array could not be allocated\n");
        table_free(new_table);
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
    if (table->secondary_indexes) {

        for (uint32_t i = 0; i < MAX_INDEXES; i++) {
            if (table->secondary_indexes[i]) {
                index_free(table->secondary_indexes[i]);
                table->secondary_indexes[i] = NULL;
            }
        }
        
        free(table->secondary_indexes);
        table->secondary_indexes = NULL;
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


/* Rename Table */
bool table_alter_rename(Table *table, const char *new_name) {
    if (!table) {
        printf("table_alter_rename: Input table is NULL.\n");
        return false;
    }

    if (!new_name || new_name[0] == '\0') {
        printf("table_alter_rename: Invalid input name.\n");
        return false;
    }

    if (strlen(new_name) >= sizeof(table->name)) {
        printf("table_alter_rename: New name exceeds table name length limit.\n");
        return false;
    }

    strcpy(table->name, new_name);
    return true;
}


/* Add Column to the Table */
bool table_alter_add_col(Table *table, const Column *new_col) {
    if (!table || !table->table_schema) {
        printf("table_alter_add_col: Invalid input Table.\n");
        return false;
    }

    if (!new_col) {
        printf("table_alter_add_col: Input Column is NULL.\n");
        return false;
    }

    if (!schema_add_column(table->table_schema, new_col)) {
        printf("table_alter_add_col: Input Column could not be added to the table.\n");
        return false;
    }

    return true;
}


/* Drop Column from the Table */
bool table_alter_drop_col(Table *table, Database *db, const char *col_name) {
    // Validating input data
    if (!table || !table->table_schema) {
        printf("table_alter_drop_col: Invalid input Table.\n");
        return false;
    }

    if (!db) {
        printf("table_alter_drop_col: Input database is NULL.\n");
        return false;
    }

    if (!col_name || col_name[0] == '\0') {
        printf("table_alter_drop_col: Invalid input Column name.\n");
        return false;
    }

    // Checking if the column exists in the table
    int32_t col_pos = schema_find_column_index(table->table_schema, col_name);
    if (col_pos < 0) {
        printf("table_alter_drop_col: Input Column doesn't exist on the table.\n");
        return false;
    }

    // Checking if the column belongs in the primary index, in order to reject the drop operation
    if (table->primary_index && index_key_has_column(table->primary_index, (uint32_t) col_pos)) {
        printf("table_alter_drop_col: Column belongs to the primary index.\n");
        return false;
    }

    if (table->total_secondary_indexes > 0 && !table->secondary_indexes) {
        printf("table_alter_drop_col: Secondary-index array is NULL.\n");
        return false;
    }

    // Also checking if the column belongs in a secondary index, in order to reject the drop operation
    for (uint32_t i = 0; i < table->total_secondary_indexes; i++) {
        if (!table->secondary_indexes[i]) {
            printf("table_alter_drop_col: Table has invalid secondary index.\n");
            return false;
        }

        if (index_key_has_column(table->secondary_indexes[i], (uint32_t) col_pos)) {
            printf("table_alter_drop_col: Column belongs to secondary index.\n");
            return false;
        }
    }

    // Only if the secondary indexes are dropped, does the schema get updated
    if (!schema_drop_column(table->table_schema, db, col_name)) {
        printf("table_alter_add_col: Input Column could not be removed from the table.\n");
        return false;
    }

    // Decrement the indexes of subsequent columns that are present in any remaining index
    if (table->primary_index &&
        !index_key_shift_after_column_drop(table->primary_index, (uint32_t) col_pos)) {
        
        printf("table_alter_drop_col: Primary index metadata could not be shifted.\n");
        return false;
    }

    for (uint32_t i = 0; i < table->total_secondary_indexes; i++) {
        if (table->secondary_indexes[i] &&
            !index_key_shift_after_column_drop(table->secondary_indexes[i], (uint32_t) col_pos)) { 

            printf("table_alter_drop_col: Secondary index metadata could not be shifted.\n");
            return false;
        }

    }

    return true;
}


/* Rename column in a Table */
bool table_alter_rename_col(Table *table, const char *old_col_name, const char *new_col_name) {
    // Vaidating inputs
    if (!table || !table->table_schema) {
        printf("table_alter_rename_col: Invalid input table.\n");
        return false;
    }

    if (!old_col_name || old_col_name[0] == '\0') {
        printf("table_alter_rename_col: Invalid current column name.\n");
        return false;
    }

    if (!new_col_name || new_col_name[0] == '\0') {
        printf("table_alter_rename_col: Invalid new column name.\n");
        return false;
    }

    if (strlen(new_col_name) >= sizeof(((Column *) 0)->name)) {
        printf("table_alter_rename_col: New column name exceeds length limit.\n");
        return false;
    }

    if (!schema_rename_column(table->table_schema, old_col_name, new_col_name)) {
        printf("table_alter_rename_col: Column could not be renamed.\n");
        return false;
    }

    return true;
}


/* Modify column in a Table */
bool table_alter_modify_col(Table *table, const Database *db, const char *old_col_name, const Column *new_column_def) {
    // Vaidating inputs
    if (!table || !table->table_schema) {
        printf("table_alter_modify_col: Invalid input table.\n");
        return false;
    }

    if (!db) {
        printf("table_alter_modify_col: Input database is NULL.\n");
        return false;
    }

    if (!old_col_name || old_col_name[0] == '\0') {
        printf("table_alter_modify_col: Invalid current column name.\n");
        return false;
    }

    if (!new_column_def) {
        printf("table_alter_modify_col: New Column is NULL.\n");
        return false;
    }

    if (!schema_modify_column(table->table_schema, db, old_col_name, new_column_def)) {
        printf("table_alter_modify_col: Target column could not be modified.\n");
        return false;
    }

    return true;
}