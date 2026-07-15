#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/schema.h"
#include "../../include/constraints.h"
#include "../../include/database.h"
#include "schema_utils.h"

/* Not Found Index Sentinel. */
#define ERR_CODE_COL_NOT_FOUND -1
/* Multiple Columns Found Index Sentinel. */
#define ERR_CODE_COL_MULTIPLE -2

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

Schema *schema_create(const Column **columns, const Constraint **constraints, uint32_t num_columns,
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

bool schema_drop(Schema *schema, Database *db) {

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

    // table's schema is_deleted = true
    schema_free(schema);
    return true;
} 

Column *schema_find_column(const Schema *schema, const char *col_name) {

    int32_t res = schema_find_column_index(schema, col_name);
    if (res == ERR_CODE_COL_MULTIPLE || res == ERR_CODE_COL_NOT_FOUND) {
        return NULL;
    }

    return schema->columns[res];
}

int32_t schema_find_column_index(const Schema *schema, const char *col_name) {
    int found = 0;
    uint32_t i = 0;
    int32_t index = 0;

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
