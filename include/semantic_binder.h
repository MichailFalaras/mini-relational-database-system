#ifndef SEMANTIC_BINDER_H_
#define SEMANTIC_BINDER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct database Database;
typedef struct table Table;
typedef struct abstract_syntax_tree_node ASTNode;

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
    BIND_CLAUSE_SET = 7
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
} BindingContext;

/* Validate ASTNode's semantic correctness */
extern bool bind_statement(ASTNode *root, Database *db);

#endif