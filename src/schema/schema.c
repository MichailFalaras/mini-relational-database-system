#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../../include/schema.h"
#include "schema_utils.h"
#include "../../include/database.h"
#include "../../include/constraints.h"
#include "../src/constraints/constraints_utils.h"
#include "../../include/expressions.h"
#include "../../include/row.h"
#include "../../include/table.h"

/* Not Found Index Sentinel. */
#define ERR_CODE_COL_NOT_FOUND -1
/* Multiple Columns Found Index Sentinel. */
#define ERR_CODE_COL_MULTIPLE -2

/* Allocate column memory. */
Column *column_alloc(char *column_name, DataType type, uint32_t not_null_rows,
    uint32_t null_rows) {
    
    Column *column = (Column *) malloc(sizeof(Column));
    if (column == NULL) {
        perror("column_alloc");
        exit(1);
    }

    strncpy(column->name, column_name, 63);
    column->name[63] = '\0'; 
    column->type = type;
    column->non_null_rows = not_null_rows;
    column->null_rows = null_rows;

    return column;
}

/* Create Schema. */
Schema *schema_create(Column **columns, Constraint **constraints, uint32_t num_columns,
    uint32_t num_constraints) {

    Schema *schema = (Schema *) calloc(1, sizeof(Schema));
    if (schema == NULL) {
        perror("schema_create");
        exit(1);
    }
    schema->num_columns = num_columns;

    if (schema->num_columns > 0) {
        schema->columns = (Column **) malloc(schema->num_columns*sizeof(Column *));
        if (schema->columns == NULL) {
            perror("schema_create");
            exit(1);
        }
        for (uint32_t i = 0; i < schema->num_columns; i++) {
            schema->columns[i] = column_alloc(columns[i]->name, columns[i]->type, columns[i]->non_null_rows,
                                            columns[i]->null_rows);
        }
    }
    
    schema->num_constraints = num_constraints;
    if (schema->num_constraints > 0) {
        schema->constraints = (Constraint **) malloc(schema->num_constraints*sizeof(Constraint *));
        if (schema->constraints == NULL) {
            perror("schema_create");
            exit(1);
        }

        for (uint32_t i = 0; i < schema->num_constraints; i++) {
            schema->constraints[i] = constraint_copy(constraints[i]);
        }
    }

    return schema;
}

/* Copies a Schema to a newly-created Table's schema field */
extern Schema *schema_copy(const Schema *schema) {
    if (!schema) {
        printf("schema_copy: Input schema is NULL.\n");
        return NULL;
    }

    if (schema->num_columns > 0 && !schema->columns) {
        printf("schema_copy: Source columns array is NULL.\n");
        return NULL;
    }

    if (schema->num_constraints > 0 && !schema->constraints) {
        printf("schema_copy: Source constraints array is NULL.\n");
        return NULL;
    }

    Schema *copy = (Schema *) calloc(1, sizeof(Schema));
    if (!copy) {
        printf("schema_copy: Copy could not be allocated.\n");
        return NULL;
    }

    copy->num_columns = schema->num_columns;
    copy->num_constraints = schema->num_constraints;

    if (copy->num_columns > 0) {
        copy->columns = (Column **) calloc(schema->num_columns, sizeof(Column *));
        if (!copy->columns) {
            printf("schema_copy: Columns pointer array could not be allocated.\n");
            schema_free(copy);
            return NULL;
        }

        for (uint32_t i = 0; i < copy->num_columns; i++) {
            if (!schema->columns[i]) {
                printf("schema_copy: Source column %u is NULL.\n", i);
                schema_free(copy);
                return NULL;
            }

            copy->columns[i] = (Column *) malloc(sizeof(Column));
            if (!copy->columns[i]) {
                printf("schema_copy: Column %u could not be allocated.\n", i);
                schema_free(copy);
                return NULL;
            }

            *copy->columns[i] = *schema->columns[i];
        }
    }

    if (copy->num_constraints > 0) {
        copy->constraints = (Constraint **) calloc(schema->num_constraints, sizeof(Constraint *));
        if (!copy->constraints) {
            printf("schema_copy: Constraints pointer array could not be allocated.\n");
            schema_free(copy);
            return NULL;
        }

        for (uint32_t i = 0; i < schema->num_constraints; i++) {
            if (!schema->constraints[i]) {
                printf("schema_copy: Source constraint %u is NULL.\n", i);
                schema_free(copy);
                return NULL;
            }

            copy->constraints[i] = constraint_copy(schema->constraints[i]);
            if (!copy->constraints[i]) {
                printf("schema_copy: Constraint %u could not be allocated.\n", i);
                schema_free(copy);
                return NULL;
            }
        }
        
    }

    return copy;
}

/* Drop Schema if only if its columns aren't referenced in any
other tables. */
bool schema_drop(Schema *schema, const Database *db) {

    if (schema == NULL || db == NULL) {
        return false;
    }

    for (uint32_t i = 0; i < db->table_count; i++) {
        if (db->tables[i]->table_schema == schema) {
            continue;
        }
        
        Schema *current_schema = db->tables[i]->table_schema;
        for (uint32_t j = 0; j < current_schema->num_constraints; j++) {
            if (current_schema->constraints[j]->type != FOREIGN_KEY) {
                continue;
            }

            for (uint32_t k = 0; k < schema->num_columns; k++) {
                /* If another table's schema's constraints reference this schema's
                columns, then you cannot DROP SCHEMA.*/
                if (constraint_references_column(current_schema->constraints[j], k)) {
                    return false;
                }
            }
        }
    }

    return true;
} 

/* Schema find column from column name and return pointer. */
Column *schema_find_column(const Schema *schema, const char *col_name) {

    if (schema == NULL) {
        return NULL;
    }

    int32_t res = schema_find_column_index(schema, col_name);
    if (res == ERR_CODE_COL_MULTIPLE || res == ERR_CODE_COL_NOT_FOUND) {
        return NULL;
    }

    return schema->columns[res];
}

/* Schema find column from column name and return its index. */
int32_t schema_find_column_index(const Schema *schema, const char *col_name) {
    int found = 0;
    uint32_t i = 0;
    int32_t index = 0;

    if (schema == NULL) {
        return ERR_CODE_COL_NOT_FOUND;
    }

    for (; i < schema->num_columns; i++) {
        if (!strcasecmp(schema->columns[i]->name, col_name)) {
            found++;
            index = i;
        }
    }

    if (found > 1) {
        return ERR_CODE_COL_MULTIPLE; 
    }

    return found ? index : ERR_CODE_COL_NOT_FOUND;
}

/* Schema add column (deep-copy).*/
bool schema_add_column(Schema *schema, Column *new_column) {

    if (schema == NULL || new_column == NULL) {
        return false;
    }

    int32_t res = schema_find_column_index(schema, new_column->name);
    /* Multiple or already existing column returns error. */
    if (res != ERR_CODE_COL_NOT_FOUND) {
        return false;
    }

    schema->num_columns++;
    schema->columns = (Column **) resize_array((void **) schema->columns, schema->num_columns);
    schema->columns[schema->num_columns-1] = column_alloc(new_column->name, new_column->type,
                                            new_column->non_null_rows, new_column->null_rows);

    return true;
}

/* Schema drop column if it isn't referenced by any other tables.
 * Also deletes constraints related to the column. */
bool schema_drop_column(Schema *schema, Database *db, const char *col_name) {

    if (schema == NULL) {
        return false;
    }
    
    int32_t res = schema_find_column_index(schema, col_name);
    if (res == ERR_CODE_COL_NOT_FOUND || res == ERR_CODE_COL_MULTIPLE) {
        return false;
    }

    for (uint32_t i = 0; i < db->table_count; i++) {
        if (db->tables[i]->table_schema == schema) {
            continue;
        }
        
        Schema *current_schema = db->tables[i]->table_schema;
        for (uint32_t j = 0; j < current_schema->num_constraints; j++) {
            if (current_schema->constraints[j]->type != FOREIGN_KEY) {
                continue;
            }
            
            if (constraint_references_column(current_schema->constraints[j], res)) {
                return false;
            }
        }
    }

    for (uint32_t i = 0; i < schema->num_constraints;) {

        if (schema->constraints[i]->type == FOREIGN_KEY) {
            i++;
            continue;
        }

        if (constraint_references_column(schema->constraints[i], res)){

            if (schema->constraints[i]->type == PRIMARY_KEY){
                return false;
            }

            constraint_free(schema->constraints[i]);
            close_array_gap((void **)schema->constraints, schema->num_constraints, i);
            schema->num_constraints--;
            schema->constraints = (Constraint **) resize_array((void **) schema->constraints, schema->num_constraints);
        } else {
            i++;
        }
    }


    free(schema->columns[res]);
    close_array_gap((void **)schema->columns, schema->num_columns, res);
    schema->num_columns--;
    schema->columns = (Column **) resize_array((void **) schema->columns, schema->num_columns);

    shift_indexes(schema, db, res);


    return true;
}

/* Schema rename column. */
bool schema_rename_column(Schema *schema, const char *old_col_name, const char *new_col_name) {

    if (schema == NULL) {
        return false;
    }

    int32_t res = schema_find_column_index(schema, new_col_name);
    if (res != ERR_CODE_COL_NOT_FOUND) {
        return false;
    }

    res = schema_find_column_index(schema, old_col_name);
    if (res == ERR_CODE_COL_NOT_FOUND || res == ERR_CODE_COL_MULTIPLE) {
        return false;
    }

    Column *column = schema_find_column(schema, old_col_name);
    strncpy(column->name, new_col_name, 63);
    column->name[63] = '\0';

    return true;
}

/* Schema modify column if it isn't referenced by any other tables. */
bool schema_modify_column(Schema *schema, const Database *db, const char *old_col_name, Column *new_column) {

    if (schema == NULL || new_column == NULL) {
        return false;
    }

    int32_t res = schema_find_column_index(schema, old_col_name);
    if (res == ERR_CODE_COL_NOT_FOUND || res == ERR_CODE_COL_MULTIPLE) {
        return false;
    }

    if (schema->columns[res]->type != new_column->type) {
        for (uint32_t i = 0; i < db->table_count; i++) {
            if (db->tables[i]->table_schema == schema) {
                continue;
            }
        
            Schema *current_schema = db->tables[i]->table_schema;
            for (uint32_t j = 0; j < current_schema->num_constraints; j++) {
                if (current_schema->constraints[j]->type != FOREIGN_KEY) {
                    continue;
                }

                if (constraint_references_column(current_schema->constraints[j], res)) {
                    return false;
                }
            }
        }
    }

    /* NOT NULL check in schema_..._constraint funcs*/

    Column *temp = schema->columns[res];
    schema->columns[res] = column_alloc(new_column->name, new_column->type, 
        new_column->non_null_rows, new_column->null_rows);

    free(temp);
    return true;
}

/* Schema find constraint from constraint name and returns index/error code. */
int32_t schema_find_constraint_index(const Schema *schema, const char *constraint_name) {
    int found = 0;
    uint32_t i = 0;
    int32_t index = 0;

    if (schema == NULL) {
        return ERR_CODE_COL_NOT_FOUND;
    }

    for (; i < schema->num_constraints; i++) {
        if (!strcasecmp(schema->constraints[i]->constraint_name, constraint_name)) {
            found++;
            index = i;
        }
    }

    if (found > 1) {
        return ERR_CODE_COL_MULTIPLE; 
    }

    return found ? index : ERR_CODE_COL_NOT_FOUND;
}

/* Schema add constraint. */
bool schema_add_constraint(Schema *schema, const Database *db, Constraint *new_constraint) {

    if (schema == NULL || new_constraint == NULL) {
        return false;
    }

    int32_t res = schema_find_constraint_index(schema, new_constraint->constraint_name);
    if (res != ERR_CODE_COL_NOT_FOUND) {
        return false;
    }

    bool valid = constraint_validate_column_refs(db, new_constraint, schema->num_columns);
    if (valid == false) {
        return false;
    }

    schema->num_constraints++;
    schema->constraints = (Constraint **) resize_array((void **) schema->constraints, schema->num_constraints);
    schema->constraints[schema->num_constraints-1] = constraint_copy(new_constraint);

    return true;
}

/* Schema drop constraint. */
bool schema_drop_constraint(Schema *schema, const char *constraint_name) {

    if (schema == NULL) {
        return false;
    }

    int32_t res = schema_find_constraint_index(schema, constraint_name);
    if (res == ERR_CODE_COL_NOT_FOUND || res == ERR_CODE_COL_MULTIPLE) {
        return false;
    }

    constraint_free(schema->constraints[res]);
    close_array_gap((void **)schema->constraints, schema->num_constraints, res);
    schema->num_constraints--;
    schema->constraints = (Constraint **) resize_array((void **) schema->constraints, schema->num_constraints);

    return true;
}

/* Schema validate row data/values. */
bool schema_validate_row(const Schema *schema, const Row *row, const EvaluationContext *context) {

    if (schema == NULL || row == NULL) {
        return false;
    }

    if (row->is_deleted == true) {
        return false;
    }

    if (row->n_columns != schema->num_columns) {
        return false;
    }

    for (uint32_t i = 0; i < schema->num_columns; i++) {
        if (row->values[i] == NULL) {
            return false;
        }

        if (row->values[i]->type != schema->columns[i]->type) {
            return false;
        }
    }

    for (uint32_t j = 0; j < schema->num_constraints; j++) {
        switch(schema->constraints[j]->type) {
            case PRIMARY_KEY: {
                for (uint32_t k = 0; k < schema->constraints[j]->constraint_data.primary_key.amount_columns; k++) {
                    uint32_t index = schema->constraints[j]->constraint_data.primary_key.primary_key_columns[k];

                    if (schema->columns[index]->type == NULL_TYPE || row->values[index]->value.null_val == true) {
                        return false;
                    }
                }
                break;
            }
            case NOT_NULL: {
                uint32_t index = schema->constraints[j]->constraint_data.not_null.column_ref;
                if (row->values[index]->value.null_val == true) {
                    return false;
                }
                break;
            }
            case CHECK: {
                Value *val = evaluate_expression(
                    schema->constraints[j]->constraint_data.check.constraint_expr, context);

                if (val == NULL) {
                    return false;
                }

                if (val->type != BOOL) {
                    value_free(val);
                    return false;
                }

                if (val->value.bool_val == false) {
                    value_free(val);
                    return false;
                }

                value_free(val);
                break;
            }
                
            /* Catalog dependent validation. */
            case UNIQUE:
            case FOREIGN_KEY:
            case DEFAULT: break;
            default:
                printf("constraints[j]->type doesn't match with Constraint types\n");
                break;
        }
    }

    return true;
}

/* Free Schema and its insides. */
void schema_free(Schema *schema) {
    if (schema != NULL) {
        if (schema->columns != NULL) {
            for (uint32_t i = 0; i < schema->num_columns; i++) {
                free(schema->columns[i]);
            }
            free(schema->columns);
        }

        if (schema->constraints != NULL) {
            for (uint32_t i = 0; i < schema->num_constraints; i++) {
                constraint_free(schema->constraints[i]);
            }
            free(schema->constraints);
        }
        
        free(schema);
    }
}