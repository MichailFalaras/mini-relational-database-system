#ifndef SEMANTIC_BINDER_H_
#define SEMANTIC_BINDER_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"

typedef struct database Database;
typedef struct table Table;
typedef struct abstract_syntax_tree_node ASTNode;
typedef struct expression_node ExpressionNode;


// Used to track the current clause and apply rules to expressions
// depending on where they're used
typedef enum binding_clause {
    BIND_CLAUSE_SELECT = 0,
    BIND_CLAUSE_WHERE = 1,
    BIND_CLAUSE_JOIN_ON = 2,
    BIND_CLAUSE_GROUP_BY = 3,
    BIND_CLAUSE_HAVING = 4,
    BIND_CLAUSE_ORDER_BY = 5,
    BIND_CLAUSE_INSERT_VALUES = 6,
    BIND_CLAUSE_SET = 7,
    BIND_CLAUSE_CHECK = 8,
    BIND_CLAUSE_DEFAULT = 9,
} BindingClause;

typedef struct binding_relation {
    Table *table;
    char alias[64];
} BindingRelation;

// Context used by bind utilities to verify table ownership of columns
// that are presented in the various expressions
typedef struct binding_context {
    Database *db;
    BindingRelation *relations;
    uint32_t num_relations;

    BindingClause current_clause;
    uint32_t aggregate_depth;
} BindingContext;

// Result type of a bound expression
typedef struct bound_expression_type {
    DataType type;
    bool is_untyped_null;
} BoundExpressionType;


// Metadata for a single projected output column
typedef struct bound_projection_column {
    ExpressionNode *expression; // Non-owning AST pointer
    char output_name[64];  // Alias or derived name
    BoundExpressionType result_type;
} BoundProjectionColumn;

// Metadata for the complete SELECT projection
typedef struct bound_projection {
    BoundProjectionColumn *columns;
    uint32_t num_columns;
} BoundProjection;


// Metadata for a single insert value
typedef enum bound_insert_value_kind {
    BOUND_INSERT_EXPRESSION,
    BOUND_INSERT_DEFAULT,
    BOUND_INSERT_NULL
} BoundInsertValueKind;

typedef struct bound_insert_value {
    BoundInsertValueKind kind;

    // Non-owning reference to an AST expression
    ExpressionNode *expression;
    BoundExpressionType result_type;
} BoundInsertValue;

// Metadata for the inserted VALUES
typedef struct bound_insertion_values {
    Table *table;

    // One entry per schema column, in schema order
    BoundInsertValue *values;
    uint32_t num_values;
} BoundInsertionValues;

typedef struct bound_update_value {
    uint32_t column_index;

    // Non-owning reference to an AST expression
    ExpressionNode *expression;
    BoundExpressionType result_type;
} BoundUpdateValue;

// Metadata for the updated VALUES
typedef struct bound_update_values {
    Table *table;

    BoundUpdateValue *values;
    uint32_t num_values;
} BoundUpdateValues;

// Final Bound Statement
typedef enum bound_statement_type {
    BOUND_STATEMENT_NONE = 0,
    BOUND_STATEMENT_SELECT = 1,
    BOUND_STATEMENT_INSERT = 2,
    BOUND_STATEMENT_UPDATE = 3,
} BoundStatementType;

typedef struct bound_statement {
    BoundStatementType type;

    union {
        BoundProjection projection;
        BoundInsertionValues insertion;
        BoundUpdateValues update;
    } data;
} BoundStatement;

/* Validate ASTNode's semantic correctness */
extern bool bind_statement(ASTNode *root, Database *db, BoundStatement *out_statement);

/* ---------- Metadata Deallocation Helpers ---------- */

extern void bound_projection_free(BoundProjection *projection);

extern void bound_insertion_values_free(BoundInsertionValues *insertion);

extern void bound_update_values_free(BoundUpdateValues *update);

extern void bound_statement_free(BoundStatement *statement);

#endif