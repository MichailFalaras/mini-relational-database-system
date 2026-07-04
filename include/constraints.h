#ifndef CONSTRAINTS_H_
#define CONSTRAINTS_H_

#include <stdint.h>
#include "expressions.h"

/* Constraint Type. */
typedef enum constraint_type {
    PRIMARY_KEY,
    FOREIGN_KEY,
    UNIQUE,
    CHECK
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
 * comprising the foreign key
 * referenced_tables: array of integers of the referenced
 * columns' tables.
 * referenced_columns: array of integers of the referenced
 * columns. */
typedef struct foreign_keys_constraint {
    uint32_t *foreign_key_columns;
    uint8_t *referenced_tables;
    uint8_t *referenced_columns; 
} ForeignKeysConstraint;

/* Unique constraint struct containing:
 * column_ids: array of integers of the columns that
 * are unique. */
typedef struct unique {
    uint32_t *column_ids;
} UniqueConstraint;

/* Check constraint struct containing:
 * constraint_expr: constraint's expression */
typedef struct check_constraint {
    ExpressionNode *constraint_expr;
} CheckConstraint;

/* Constraint struct containing:
 * constraint_name: name of constraint
 * type: constraint type
 * union constraint: contains each possible constraint
 * struct type*/
typedef struct constraint {
    char constraint_name[64];
    ConstraintType type;
    union {
        PrimaryKeyConstraint *primary_key;
        ForeignKeysConstraint *foreign_keys;
        UniqueConstraint *unique_cols;
        CheckConstraint *check;
    } constraint;
} Constraint;

#endif