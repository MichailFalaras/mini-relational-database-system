#ifndef CONSTRAINTS_H_
#define CONSTRAINTS_H_

#include <stdint.h>
#include "expressions.h"

/* Constraint Type. */
typedef enum constraint_type {
    PRIMARY_KEY,
    FOREIGN_KEY,
    UNIQUE,
    CHECK,
    NOT_NULL,
    DEFAULT
} ConstraintType;

/* Primary key struct containing:
 * primary_key_columns: array of integers of all columns
 * comprising the primary key
 * amount_columns: amount of columns. */
typedef struct primary_key_constraint {
    uint32_t *primary_key_columns;
    uint8_t amount_columns;
} PrimaryKeyConstraint;

/* Foreign Keys struct containing:
 * foreign_key_columns: array of integers of all columns
 comprising the foreign key
 * amount_foreign_keys: amount of foreign key columns
 * referenced_table: index of referenced table
 * referenced_columns: array of integers of all columns
 being referenced in the referenced table
 * amount_referenced_columns: amount of referenced columns. */
typedef struct foreign_keys_constraint {
    uint32_t *foreign_key_columns;
    uint32_t amount_foreign_keys;

    uint32_t referenced_table;
    uint32_t *referenced_columns;
    uint32_t amount_referenced_columns;
} ForeignKeysConstraint;

/* Unique constraint struct containing:
 * column_ids: array of integers of the columns that
 * are unique. */
typedef struct unique {
    uint32_t *column_refs;
    uint32_t amount_columns;
} UniqueConstraint;

/* Check constraint struct containing:
 * constraint_expr: constraint's expression */
typedef struct check_constraint {
    uint32_t *column_refs;
    uint32_t amount_columns;
    ExpressionNode *constraint_expr;
} CheckConstraint;

typedef struct not_null {
    uint32_t column_ref;
} NotNullConstraint;

typedef struct default_val {
    uint32_t column_ref;
    ExpressionNode *default_expr;
} DefaultConstraint;

/* Constraint struct containing:
 * constraint_name: name of constraint
 * type: constraint type
 * union constraint: contains each possible constraint
 * struct type*/
typedef struct constraint {
    char constraint_name[64];
    ConstraintType type;
    union {
        PrimaryKeyConstraint primary_key;
        ForeignKeysConstraint foreign_keys;
        UniqueConstraint unique_cols;
        CheckConstraint check;
        NotNullConstraint not_null;
        DefaultConstraint default_value;
    } constraint_data;
} Constraint;

extern Constraint *constraint_alloc(char *constraint_name, ConstraintType type);

extern Constraint *constraint_copy(const Constraint *source);

/* Should go into helper .c file. */
extern uint32_t copy_uint32_array(const uint32_t *source, uint32_t amount);

extern Constraint *constraint_create_primary_key(char *constraint_name, uint32_t *column_refs, uint32_t amount_columns);

extern Constraint *constraint_create_foreign_keys(char *constraint_name, uint32_t *foreign_key_columns,
    uint32_t amount_foreign_keys, uint32_t referenced_table, uint32_t *referenced_columns, uint32_t amount_referenced_columns);

extern Constraint *constraint_create_unique(char *constraint_name, uint32_t *column_refs, uint32_t amount_columns);

extern Constraint *constraint_create_check(char *constraint_name, ExpressionNode *constraint_expr,
    uint32_t *column_refs, uint32_t amount_columns);

extern Constraint *constraint_create_not_null(char *constraint_name, uint32_t column_ref);

extern Constraint *constraint_create_default(char *constraint_name, uint32_t column_ref, ExpressionNode *default_expr);

extern bool constraint_has_column(const Constraint *constraint, uint32_t column_index);

extern bool constraint_references_table(const Constraint *constraint, uint32_t table_index);

extern bool constraint_references_column(const Constraint *constraint, uint32_t column_ref);

extern bool constraint_validate_definition(const Constraint *constraint);

extern void constraint_free(Constraint *constraint);

#endif