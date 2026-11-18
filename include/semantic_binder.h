#ifndef SEMANTIC_BINDER_H_
#define SEMANTIC_BINDER_H_

#include <stdint.h>
#include <stdbool.h>
#include "data_types.h"
#include "ast.h"

typedef struct database Database;
typedef struct table Table;
typedef struct schema Schema;
typedef struct ast_create_table CreateTableNode;
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

    // Added for binding CHECK expressions 
    const CreateTableNode *create_table;

    // Only populated during ALTER TABLE CHECK binding
    Schema *proposed_schema;
    ColumnDefNode *proposed_column;
    char proposed_table_name[64];
} BindingRelation;

#define ALTER_NEW_COLUMN UINT32_MAX

// Map structure between the working and actual column indexes
// Useful for ALTER TABLE operations that can add/remove/rename columns
typedef struct AlterColumnMap {
    uint32_t *working_to_original;
    uint32_t num_columns;
} AlterColumnMap;

// Context used by bind utilities to verify table ownership of columns
// that are presented in the various expressions
typedef struct binding_context {
    Database *db;
    BindingRelation *relations;
    uint32_t num_relations;

    BindingClause current_clause;
    uint32_t aggregate_depth;

    AlterColumnMap *alter_column_map;
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

// Metadata for the deleted values
typedef struct bound_delete_values {
    Table *table;

    // Non-owning pointer of WHERE AST node
    WhereNode *where;
} BoundDeleteValues;

// Metadata for a validated CREATE TABLE statement
typedef struct bound_create_table {
    // Non-owning pointer to the CREATE TABLE AST node
    CreateTableNode *create_table;
} BoundCreateTable;


// Metadata for a validated ALTER TABLE statement
typedef struct bound_alter_table {
    // Existing table being altered (non-owning)
    Table *table;

    // Validated ALTER TABLE AST node (non-owning)
    AlterTableNode *alter_table;
    Schema *result_schema;        // Owning
} BoundAlterTable;

// Metadata for a validated DROP TABLE statement
typedef struct bound_drop_table {
    // Existing table being dropped (non-owning)
    Table *table;

    // Validated DROP TABLE AST node (non-owning)
    DropTableNode *drop_table;
} BoundDropTable;

// Metadata for a validated TRUNCATE TABLE statement
typedef struct bound_truncate_table {
    // Existing table being truncated (non-owning)
    Table *table;

    // Validated TRUNCATE TABLE AST node (non-owning)
    TruncateTableNode *truncate_table;
} BoundTruncateTable;

typedef struct bound_create_index {
    Table *table;                  // Non-owning
    CreateIndexNode *create_index; // Non-owning AST pointer

    // Resolved index columns, in the requested order
    uint32_t *column_indexes;      // Owning
    uint32_t num_columns;
} BoundCreateIndex;

// Final Bound Statement
typedef enum bound_statement_type {
    BOUND_STATEMENT_NONE = 0,
    BOUND_STATEMENT_SELECT = 1,
    BOUND_STATEMENT_INSERT = 2,
    BOUND_STATEMENT_UPDATE = 3,
    BOUND_STATEMENT_DELETE = 4,
    BOUND_STATEMENT_CREATE_TABLE = 5,
    BOUND_STATEMENT_ALTER_TABLE = 6,
    BOUND_STATEMENT_DROP_TABLE = 7,
    BOUND_STATEMENT_TRUNCATE_TABLE = 8,
    BOUND_STATEMENT_CREATE_INDEX = 9
} BoundStatementType;

typedef struct bound_statement {
    BoundStatementType type;

    union {
        BoundProjection projection;
        BoundInsertionValues insertion;
        BoundUpdateValues update;
        BoundDeleteValues delete;
        BoundCreateTable create_table;
        BoundAlterTable alter_table;
        BoundDropTable drop_table;
        BoundTruncateTable truncate_table;
        BoundCreateIndex create_index;
    } data;
} BoundStatement;

/* Validate ASTNode's semantic correctness */
extern bool bind_statement(ASTNode *root, Database *db, BoundStatement *out_statement);

/* ---------- Metadata Deallocation Helpers ---------- */

extern void bound_projection_free(BoundProjection *projection);

extern void bound_insertion_values_free(BoundInsertionValues *insertion);

extern void bound_update_values_free(BoundUpdateValues *update);

extern void bound_delete_values_free(BoundDeleteValues *delete);

extern void bound_create_table_free(BoundCreateTable *create_table);

extern void bound_alter_table_free(BoundAlterTable *alter_table);

extern void bound_drop_table_free(BoundDropTable *drop_table);

extern void bound_truncate_table_free(BoundTruncateTable *drop_table);

extern void bound_create_index_free(BoundCreateIndex *index);

extern void bound_statement_free(BoundStatement *statement);


/* ---------- AlterColumnMap Helpers ---------- */

extern bool alter_column_map_init(AlterColumnMap *map, uint32_t original_count);

extern void alter_column_map_free(AlterColumnMap *map);

// Retrieve the original column index based on the working index
extern bool alter_map_get_original(const AlterColumnMap *map, uint32_t working_index, 
    uint32_t *out_original);

// Retrieve the working column index based on the original index
extern bool alter_map_find_working(const AlterColumnMap *map, uint32_t original_index,
    uint32_t *out_working);

// Add new working index
extern bool alter_column_map_add(AlterColumnMap *map);

// Remove working index
extern bool alter_column_map_drop(AlterColumnMap *map, uint32_t working_index);


#endif