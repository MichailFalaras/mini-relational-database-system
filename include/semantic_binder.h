#ifndef SEMANTIC_BINDER_H_
#define SEMANTIC_BINDER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct database Database;
typedef struct table Table;
typedef struct abstract_syntax_tree_node ASTNode;

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
} BindingContext;

/* Validate ASTNode's semantic correctness */
extern bool bind_statement(ASTNode *root, Database *db);

#endif