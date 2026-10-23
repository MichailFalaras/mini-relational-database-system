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
    BIND_CLAUSE_VALUES = 6,
    BIND_CLAUSE_SET = 7,
    BIND_CLAUSE_CHECK = 8,
    BIND_CLAUSE_DEFAULT = 9
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

/* Validate ASTNode's semantic correctness */
extern bool bind_statement(ASTNode *root, Database *db);

/* Free SELECT projection metadata */
extern void bound_projection_free(BoundProjection *projection);

#endif