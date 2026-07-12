#ifndef AST_H_
#define AST_H_

#include <stdint.h>
#include <stdbool.h>
#include "expressions.h"
#include "schema.h"
#include "data_types.h"

typedef enum ast_node_type {
    AST_SELECT,
    AST_INSERT,
    AST_UPDATE,
    AST_DELETE,
    AST_CREATE_TABLE,
    AST_DROP_TABLE,
    AST_ALTER_TABLE,
    AST_TRUNCATE_TABLE,
    AST_CREATE_INDEX,
    AST_DROP_INDEX,
} ASTNodeType;

typedef struct ast_projection {
    ExpressionNode **expressions;
    uint32_t num_expressions;
} ProjectionNode;

typedef struct ast_from {
    ExpressionNode **expressions;
    uint32_t num_expressions;
} FromNode;

typedef struct ast_where {
    ExpressionNode *expression;
} WhereNode;

typedef struct ast_group_by {
    uint32_t num_column_refs;
    ExpressionNode **column_refs;
} GroupByNode;

typedef struct ast_having {
    ExpressionNode *expression;
} HavingNode;

typedef enum order_by_types {
    ASC,
    DESC
} OrderByTypes;

typedef struct ast_order_by {
    uint32_t num_column_refs;
    ExpressionNode **column_refs;
    OrderByTypes *type;
} OrderByNode;

typedef enum ast_join_types {
    INNER,
    LEFT,
    RIGHT,
    OUTER
} JoinTypes;

typedef struct ast_on {
    ExpressionNode *expression;
} OnNode;

typedef struct ast_join {
    char secondary_table_name[64];
    JoinTypes join_type;
    OnNode *on;
} JoinNode;

typedef struct ast_limit {
    uint32_t limit;
} LimitNode;

typedef struct ast_offset {
    uint32_t offset;
} OffsetNode;

typedef struct ast_select {
    ProjectionNode *projection;
    FromNode *from;
    WhereNode *where;
    GroupByNode *group_by;
    HavingNode *having;
    OrderByNode *order_by;
    JoinNode **joins;
    uint32_t num_joins;
    LimitNode *limit;
    OffsetNode *offset;
} SelectNode;

typedef struct ast_into {
    char table_name[64];
    /* Contains table name in both columns. */
    ExpressionNode **column_refs;
    uint32_t num_column_refs;
} IntoNode;

typedef struct ast_values {
    /* As many values as column references in IntoNode. */
    uint32_t num_values;
    ExpressionNode **values;
} ValuesNode;

typedef struct ast_insert {
    IntoNode *into;
    ValuesNode *values;
} InsertNode;

typedef struct ast_assignment {
    char column_name[64];
    ExpressionNode *value;
} AssignmentNode;

typedef struct ast_set {
    AssignmentNode *assignments;
    uint32_t num_assignments;
} SetNode;

typedef struct ast_update {
    char table_name[64];
    SetNode *set;
    WhereNode *where;
} UpdateNode;

typedef struct ast_delete {
    FromNode *from;
    WhereNode *where;
} DeleteNode;


typedef enum ast_constraint_type {
    AST_CONSTRAINT_PRIMARY_KEY,
    AST_CONSTRAINT_UNIQUE,
    AST_CONSTRAINT_NOT_NULL,
    AST_CONSTRAINT_FOREIGN_KEY,
    AST_CONSTRAINT_CHECK,
    AST_CONSTRAINT_DEFAULT
} ASTConstraintType;

typedef struct ast_primary_key_constraint {
    ExpressionNode **column_refs;
    uint32_t num_columns;
} PrimaryKeyConstraintNode;

typedef struct ast_unique_constraint {
    ExpressionNode **column_refs;
    uint32_t num_columns;
} UniqueConstraintNode;

typedef struct ast_not_null_constraint {
    ExpressionNode *column_ref;
} NotNullConstraintNode;

typedef struct ast_foreign_key_constraint {
    ExpressionNode **local_column_refs;
    uint32_t num_local_columns;
    
    char referenced_table_name[64];

    ExpressionNode **referenced_column_refs;
    uint32_t num_referenced_columns;
} ForeignKeyConstraintNode;

typedef struct ast_check_constraint {
    ExpressionNode *check_expr;
} CheckConstraintNode;

typedef struct ast_default_constraint {
    char column_name[64];
    ExpressionNode *default_expr;
} DefaultConstraintNode;

typedef struct ast_constraints {
    char constraint_name[64];
    ASTConstraintType type;

    union {
        PrimaryKeyConstraintNode primary_key;
        UniqueConstraintNode unique;
        NotNullConstraintNode not_null;
        ForeignKeyConstraintNode foreign_key;
        CheckConstraintNode check;
        DefaultConstraintNode default_value;
    } constraint_data;

} ConstraintsNode;

typedef struct ast_column_def {
    char column_name[64];
    DataType type;
    ConstraintsNode *constraints;
    uint32_t num_constraints;
} ColumnDefNode;

typedef struct ast_columns {
    uint32_t num_column_defs;
    ColumnDefNode **column_defs;
} ColumnsNode;

typedef struct ast_create_table {
    char table_name[64];
    ColumnsNode *columns;
    ConstraintsNode **constraints;
    uint32_t num_constraints;
} CreateTableNode;

typedef struct ast_drop_table {
    char table_name[64];
} DropTableNode;

typedef struct ast_alter_add {
    ColumnDefNode *column;
} AlterAddNode;

typedef struct ast_alter_drop {
    char column_name[64];
} AlterDropNode;

typedef struct ast_alter_rename_table {
    char new_table_name[64];
} AlterRenameTableNode;

typedef struct ast_alter_rename_col {
    char new_col_name[64];
} AlterRenameColNode;

typedef struct ast_alter_modify {
    char column_name[64];
    DataType new_type;
} AlterModifyNode;

typedef struct ast_alter_add_constraint {
    char constraint_name[64];
    ConstraintType new_type;
    char column_name[64];
} AlterAddConstraintNode;

typedef struct ast_alter_drop_constraint {
    char constraint_name[64];
} AlterDropConstraintNode;

typedef enum ast_alter_types {
    AST_ALTER_ADD,
    AST_ALTER_DROP,
    AST_ALTER_RENAME,
    AST_ALTER_MODIFY,
    AST_ALTER_ADD_CONSTRAINT,
    AST_ALTER_DROP_CONSTRAINT
} ASTAlterType;

typedef struct ast_alter_action {
    ASTAlterType type;
    union {
        AlterAddNode alter_add;
        AlterDropNode alter_drop;
        AlterRenameTableNode alter_rename_table;
        AlterRenameColNode alter_rename_col;
        AlterModifyNode alter_modify;
        AlterAddConstraintNode alter_add_constraint;
        AlterDropConstraintNode alter_drop_constraint;
    } alter_contents;
} AlterActionNode;

typedef struct ast_alter_table {
    char table_name[64];
    AlterActionNode *actions;
    uint32_t num_actions;
} AlterTableNode;

typedef struct ast_truncate_table {
    char table_name[64];
} TruncateTableNode;

typedef struct ast_create_index {
    char index_name[64];
    char table_name[64];
    /* No OnNode struct because it isnt an unary/binary expression,
    its an array of columns. */
    ExpressionNode **column_refs;
    uint32_t num_column_refs;
} CreateIndexNode;

typedef struct ast_drop_index {
    char index_name[64];
    char table_name[64];
} DropIndexNode;

typedef struct abstract_syntax_tree_node {
    ASTNodeType type;
    union {
        SelectNode select;
        InsertNode insert;
        UpdateNode update;
        DeleteNode delete;
        CreateTableNode create_table;
        AlterTableNode alter_table;
        TruncateTableNode truncate_table;
        DropTableNode drop_table;
        CreateIndexNode create_index;
        DropIndexNode drop_index;
    } node_contents;
} ASTNode;

#endif