#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/constraints.h"
#include "../../include/expressions.h"

/* Allocate memory for Constraint struct. */
Constraint *constraint_alloc(char *constraint_name, ConstraintType type) {
    Constraint *constraint = (Constraint *) malloc(sizeof(Constraint));
    if (constraint == NULL) {
        perror("constraint_create");
        exit(1);
    }
    strncpy(constraint->constraint_name, constraint_name, 63);
    constraint->constraint_name[63] = '\0';

    constraint->type = type;
    memset(&constraint->constraint_data, 0, sizeof(constraint->constraint_data));

    return constraint;
}

/* Deep-copy for Constraint structs. */
Constraint *constraint_copy(const Constraint *source) {

    if (source == NULL) {
        return NULL;
    }

    Constraint *copy = NULL;
    switch (source->type) {
        case PRIMARY_KEY:
            copy = constraint_create_primary_key(source->constraint_name, source->constraint_data.primary_key.primary_key_columns,
                    source->constraint_data.primary_key.amount_columns);
            break;
        case FOREIGN_KEY:
            copy = constraint_create_foreign_keys(source->constraint_name, source->constraint_data.foreign_keys.foreign_key_columns,
                    source->constraint_data.foreign_keys.amount_foreign_keys, source->constraint_data.foreign_keys.referenced_table,
                    source->constraint_data.foreign_keys.referenced_columns, source->constraint_data.foreign_keys.amount_referenced_columns);
            break;
        case UNIQUE:
            copy = constraint_create_unique(source->constraint_name, source->constraint_data.unique_cols.column_refs,
                    source->constraint_data.unique_cols.amount_columns);
            break;
        case CHECK:
            copy = constraint_create_check(source->constraint_name, source->constraint_data.check.constraint_expr,
                    source->constraint_data.check.column_refs, source->constraint_data.check.amount_columns);
            break;
        case NOT_NULL:
            copy = constraint_create_not_null(source->constraint_name, source->constraint_data.not_null.column_ref);
            break;
        case DEFAULT:
            copy = constraint_create_default(source->constraint_name, source->constraint_data.default_value.column_ref,
                    source->constraint_data.default_value.default_expr);
            break;
        default:
            printf("Source type doesn't match existing Constraint types.\n");
            return NULL;
    }

    return copy;
}

/* ASTConstraintNode are only created when we CREATE TABLE or ALTER TABLE constraints.
 * Therefore, before this function gets called, Query Planner figures out in which
 * index the columns have been saved.
 * Query Planner responsible for figuring out the type of constraint and creating
 * the corresponding dynamically allocated array of column indexes. */
Constraint *constraint_create_primary_key(char *constraint_name, uint32_t *column_refs,
    uint32_t amount_columns) {
    
    if (column_refs == NULL) {
        return NULL;
    }

    Constraint *constraint = constraint_alloc(constraint_name, PRIMARY_KEY);

    constraint->constraint_data.primary_key.primary_key_columns = copy_uint32_array(column_refs,
                                                                                     amount_columns);
    constraint->constraint_data.primary_key.amount_columns = amount_columns;

    return constraint;
}

Constraint *constraint_create_foreign_keys(char *constraint_name, uint32_t *foreign_key_columns,
    uint32_t amount_foreign_keys, uint32_t referenced_table, uint32_t *referenced_columns,
    uint32_t amount_referenced_columns) {
    
    if (foreign_key_columns == NULL || referenced_columns == NULL) {
        return NULL;
    }

    Constraint *constraint = constraint_alloc(constraint_name, FOREIGN_KEY);

    constraint->constraint_data.foreign_keys.foreign_key_columns = copy_uint32_array(foreign_key_columns,
                                                                                    amount_foreign_keys);
    constraint->constraint_data.foreign_keys.amount_foreign_keys = amount_foreign_keys;
    constraint->constraint_data.foreign_keys.referenced_table = referenced_table;
    constraint->constraint_data.foreign_keys.referenced_columns = copy_uint32_array(referenced_columns,
                                                                                    amount_referenced_columns);
    constraint->constraint_data.foreign_keys.amount_referenced_columns = amount_referenced_columns;
    
    return constraint;
}

Constraint *constraint_create_unique(char *constraint_name, uint32_t *column_refs,
     uint32_t amount_columns) {
    
    if (column_refs == NULL) {
        return NULL;
    }

    Constraint *constraint = constraint_alloc(constraint_name, UNIQUE);

    constraint->constraint_data.unique_cols.column_refs = copy_uint32_array(column_refs, amount_columns);
    constraint->constraint_data.unique_cols.amount_columns = amount_columns;

    return constraint;
}

Constraint *constraint_create_check(char *constraint_name, ExpressionNode *constraint_expr,
    uint32_t *column_refs, uint32_t amount_columns) {

    if (constraint_expr == NULL) {
        return NULL;
    }

    Constraint *constraint = constraint_alloc(constraint_name, CHECK);

    constraint->constraint_data.check.constraint_expr = expression_node_copy(constraint_expr);
    constraint->constraint_data.check.column_refs = copy_uint32_array(column_refs, amount_columns);
    constraint->constraint_data.check.amount_columns = amount_columns;
    return constraint;
}

Constraint *constraint_create_not_null(char *constraint_name, uint32_t column_ref) {

    Constraint *constraint = constraint_alloc(constraint_name, NOT_NULL);

    constraint->constraint_data.not_null.column_ref = column_ref;

    return constraint;
}

Constraint *constraint_create_default(char *constraint_name, uint32_t column_ref,
    ExpressionNode *default_expr) {
    
    Constraint *constraint = constraint_alloc(constraint_name, DEFAULT);

    constraint->constraint_data.default_value.column_ref = column_ref;
    constraint->constraint_data.default_value.default_expr = expression_node_copy(default_expr);

    return constraint;
}

/* Semantic Binder & Query Planner helper functions. */
bool constraint_has_column(const Constraint *constraint, uint32_t column_index) {

    if (constraint == NULL) {
        return false;
    }

    switch (constraint->type) {
        case PRIMARY_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.primary_key.amount_columns; i++) {
                if (constraint->constraint_data.primary_key.primary_key_columns[i] == column_index) {
                    return true;
                }
            }
            
            break;
        case FOREIGN_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_foreign_keys; i++) {
                if (constraint->constraint_data.foreign_keys.foreign_key_columns[i] == column_index) {
                    return true;
                }
            }

            break;
        case UNIQUE:
            for (uint32_t i = 0; i < constraint->constraint_data.unique_cols.amount_columns; i++) {
                if (constraint->constraint_data.unique_cols.column_refs[i] == column_index) {
                    return true;
                }
            }

            break;
        case CHECK:
            for (uint32_t i = 0; i < constraint->constraint_data.check.amount_columns; i++) {
                if (constraint->constraint_data.check.column_refs[i] == column_index) {
                    return true;
                }
            }

            break;
        case NOT_NULL:
            if (constraint->constraint_data.not_null.column_ref == column_index) {
                return true;
            }
            
            break;
        case DEFAULT:
            if (constraint->constraint_data.default_value.column_ref == column_index) {
                return true;
            }
            break;
        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            return false;
        }

    return false;
}

/* For Foreign-Key only. */
bool constraint_references_table(const Constraint *constraint, uint32_t table_index) {
    if (constraint == NULL || constraint->type != FOREIGN_KEY) {
        return false;
    }

    if (constraint->constraint_data.foreign_keys.referenced_table == table_index) {
        return true;
    }

    return false;
}

/* For all types of constraints. */
bool constraint_references_column(const Constraint *constraint, uint32_t column_ref) {
    if (constraint == NULL) {
        return false;
    }

    switch (constraint->type) {
        case PRIMARY_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.primary_key.amount_columns; i++) {
                if (constraint->constraint_data.primary_key.primary_key_columns[i] == column_ref) {
                    return true;
                }
            }
            break;
        case FOREIGN_KEY:
            for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_foreign_keys; i++) {
                if (constraint->constraint_data.foreign_keys.foreign_key_columns[i] == column_ref) {
                    return true;
                }
            }

            for (uint32_t i = 0; i < constraint->constraint_data.foreign_keys.amount_referenced_columns; i++) {
                if (constraint->constraint_data.foreign_keys.referenced_columns[i] == column_ref) {
                    return true;
                }
            }
            break;
        case UNIQUE:
            for (uint32_t i = 0; i < constraint->constraint_data.unique_cols.amount_columns; i++) {
                if (constraint->constraint_data.unique_cols.column_refs[i] == column_ref) {
                    return true;
                }
            }
            break;
        case CHECK:
            for (uint32_t i = 0; i < constraint->constraint_data.check.amount_columns; i++) {
                if (constraint->constraint_data.check.column_refs[i] == column_ref) {
                    return true;
                }
            }
            break;
        case NOT_NULL:
            if(constraint->constraint_data.not_null.column_ref == column_ref) {
                return true;
            }
            break;
        case DEFAULT:
            if(constraint->constraint_data.default_value.column_ref == column_ref) {
                return true;
            }
            break;
        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            return;
    }
    
    return false;
}

bool constraint_validate_definition(const Constraint *constraint) {
    if (constraint == NULL) {
        return false;
    }

    switch (constraint->type) {
        case PRIMARY_KEY:
            if (constraint->constraint_data.primary_key.primary_key_columns == NULL) {
                return false;
            }

            if (constraint->constraint_data.primary_key.amount_columns == 0){
                return false;
            }

            break;
        case FOREIGN_KEY:
            if (constraint->constraint_data.foreign_keys.foreign_key_columns == NULL) {
                return false;
            }

            if (constraint->constraint_data.foreign_keys.referenced_columns == NULL) {
                return false;
            }

            if (constraint->constraint_data.foreign_keys.amount_foreign_keys != 
                constraint->constraint_data.foreign_keys.amount_referenced_columns) {
                return false;
            }

            if (constraint->constraint_data.foreign_keys.amount_foreign_keys == 0 ||
            constraint->constraint_data.foreign_keys.amount_referenced_columns == 0) {
                return false;
            }

            break;
        case UNIQUE:
            if (constraint->constraint_data.unique_cols.column_refs == NULL) {
                return false;
            }

            if (constraint->constraint_data.unique_cols.amount_columns == 0) {
                return false;
            }

            break;
        case CHECK:
            if (constraint->constraint_data.check.constraint_expr == NULL) {
                return false;
            }    

            if (constraint->constraint_data.check.column_refs == NULL) {
                return false;
            }

            if (constraint->constraint_data.check.amount_columns == 0) {
                return false;
            }

            break;
        case NOT_NULL:
            break;
        case DEFAULT:
            if (constraint->constraint_data.default_value.default_expr == NULL) {
                return false;
            }

            break;
        default:
            printf("constraint type doesn't match existing Constraint types.\n");
            return false;
    }

    return true;
}

/* Free Constraint struct and insides. */
void constraint_free(Constraint *constraint) {
    if (constraint != NULL) {
        switch (constraint->type) {
            case PRIMARY_KEY:
                free(constraint->constraint_data.primary_key.primary_key_columns);
                break;
            case FOREIGN_KEY:
                free(constraint->constraint_data.foreign_keys.foreign_key_columns);
                free(constraint->constraint_data.foreign_keys.referenced_columns);
                break;
            case UNIQUE:
                free(constraint->constraint_data.unique_cols.column_refs);
                break;
            case CHECK:
                free(constraint->constraint_data.check.column_refs);
                expression_node_free(constraint->constraint_data.check.constraint_expr);
                break;
            case NOT_NULL:
                break;
            case DEFAULT:
                expression_node_free(constraint->constraint_data.default_value.default_expr);
                break;
            default:
                printf("constraint type doesn't match existing Constraint types.\n");
                free(constraint);
                return;
        }

        free(constraint);
    }
}