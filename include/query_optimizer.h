#ifndef QUERY_OPTIMIZER_H_
#define QUERY_OPTIMIZER_H_

#include <stdint.h>
#include "schema.h"
#include "expressions.h"
#include "constraints.h"
#include "index.h"
#include "row.h"
#include "parser.h"
#include "database.h"

/* Operation Types. */
typedef enum operation_type {
    PLAN_CREATE_TABLE,
    PLAN_DROP_TABLE,
    PLAN_ALTER_RENAME_TABLE,
    PLAN_TRUNCATE_TABLE,

    PLAN_ALTER_ADD_COLUMN,
    PLAN_ALTER_DROP_COLUMN,
    PLAN_ALTER_RENAME_COLUMN,
    PLAN_ALTER_MODIFY_COLUMN,

    PLAN_ALTER_ADD_CONSTRAINT,
    PLAN_ALTER_DROP_CONSTRAINT,
    
    PLAN_CREATE_INDEX,
    PLAN_DROP_INDEX,

    PLAN_SEQ_SCAN,
    PLAN_INDEX_SCAN,
    PLAN_FILTER,
    PLAN_PROJECT,
    PLAN_JOIN,
    
    PLAN_INSERT,
    PLAN_UPDATE,
    PLAN_DELETE
} OperationType;

/* Help Struct for JOIN Projection. */
typedef struct column_ref {
    Table *table;
    uint32_t column_index;
} ColumnRef;

/* Operation-Specific Data. */
typedef struct plan_create_table {
    char table_name[64];
    Schema *schema;
} CreateTable;

typedef struct plan_drop_table {
    Table *table;
} DropTable;

typedef struct plan_alter_rename_table {
    // AlterActionNode containts both the old and the new name.
    Table *table;
    char new_table_name[64];
} AlterRenameTable;

typedef struct plan_truncate_table {
    Table *table;
} TruncateTable;

typedef struct plan_alter_add_column {
    Table *table;
    Column *new_column_definition;
    Constraint **new_constraints;
    uint32_t num_constraints;
} AlterAddColumn;

typedef struct plan_alter_drop_column {
    ColumnRef column;
} AlterDropColumn;

typedef struct plan_alter_rename_column {
    ColumnRef column;
    char new_col_name[64];
} AlterRenameColumn;

typedef struct plan_alter_modify_column {
    ColumnRef column;
    Column *new_column_definition;
} AlterModifyColumn;

typedef struct plan_alter_add_constraint {
    Table *table;
    Constraint *constraint;
} AlterAddConstraint;

typedef struct plan_alter_drop_constraint {
    Table *table;
    uint32_t constraint_array_index;
} AlterDropConstraint;

typedef struct plan_create_index {
    Table *table;
    Index *new_index;
} CreateIndex;

typedef struct plan_drop_index {
    Table *table;
    // primary or secondary, that's why its kept a pointer
    Index *index;
} DropIndex;

typedef struct plan_seq_scan {
    Table *table;
} SeqScan;

typedef struct plan_index_scan {
    Table *table;
    // primary or secondary again
    Index *index;
} IndexScan;

/* WHERE Condition */
typedef struct plan_filter {
    ExpressionNode *expression;
    /* Direct access to columns used in expression. */
    ColumnRef *columns;
    uint32_t amount_column_refs;
} Filter;

/* SELECT Columns */
typedef struct plan_project {
    // aggregate functions, etc
    ExpressionNode **expressions;
    uint32_t num_expressions;

    ColumnRef *columns;
    uint32_t amount_columns;
} Project;

typedef enum plan_join_types {
    INNER,
    LEFT,
    RIGHT,
    OUTER
} PlanTypes;

typedef struct plan_join {
    Table *left_table;
    Table *right_table;
    PlanTypes join_type;
    Filter *on_node;
} Join;

typedef struct plan_insert {
    ColumnRef *columns;
    uint32_t num_columns;
    Row *rows;
} Insert;

typedef struct plan_update {
    ColumnRef *columns;
    Row *new_values;
    uint32_t num_columns;
    Filter *filter;
} Update;

typedef struct plan_delete {
    Table *table;
    Filter *filter;
} Delete;

typedef struct plan_group_by {
    ColumnRef *columns;
} GroupBy;

typedef struct plan_order_by {
    ExpressionNode *expr;
    uint32_t amount_expr;
    ColumnRef *columns;
    OrderByTypes *type;
} OrderBy;

typedef struct plan_limit {
    uint32_t limit;
} Limit;

typedef struct plan_offset {
    uint32_t offset;
} Offset;

/* Plan Nodes containing:
 * type: operation type of plan node
 * estimated_cost: estimated cost of plan node's operation
 * union operation: operation specific structs holding
 * important data for each operation
 * next: pointer to next plan node in the query plan. */
typedef struct plan_node {
    OperationType type;
    double estimated_cost;
    union {
        CreateTable plan_create_table;
        DropTable plan_drop_table;
        AlterRenameTable plan_alter_rename_table;
        TruncateTable plan_truncate_table;
        AlterAddColumn plan_alter_add_column;
        AlterDropColumn plan_alter_drop_column;
        AlterRenameColumn plan_alter_rename_column;
        AlterModifyColumn plan_alter_modify_column;
        AlterAddConstraint plan_alter_add_constraint;
        AlterDropConstraint plan_alter_drop_constraint;
        CreateIndex plan_create_index;
        DropIndex plan_drop_index;
        SeqScan plan_seq_scan;
        IndexScan plan_index_scan;
        Filter plan_filter;
        Project plan_project;
        Join plan_join;
        Insert plan_insert;
        Update plan_update;
        Delete plan_delete;
        GroupBy plan_group_by;
        OrderBy plan_order_by;
        Limit plan_limit;
        Offset plan_offset;
    } operation;
    struct plan_node *next;
} PlanNode;

/* Query Plan containing: 
 * head: list of Plan Nodes
 * amount_plan_nodes: amount of plan nodes. */
typedef struct query_plan {
    PlanNode *head;
    uint32_t amount_plan_nodes;
} QueryPlan;

extern QueryPlan *query_plan_init();

extern bool query_planner(Statement *statement, Database *db);

/* TODO: static functions in .c file */
PlanNode *plan_node_create(ASTNode *node, Database *db);
PlanNode *plan_node_create_table(CreateTableNode node, Database *db);
PlanNode *plan_node_drop_table(DropTableNode node, Database *db);
PlanNode *plan_node_truncate_table(TruncateTableNode node, Database *db);
PlanNode *plan_node_alter_action(AlterTableNode node, Database *db);
PlanNode *plan_node_alter_add_column(AlterAddNode node, Database *db);
PlanNode *plan_node_alter_drop_column(AlterDropNode node, Database *db);
PlanNode *plan_node_alter_rename_table(AlterRenameTableNode node, Database *db);
PlanNode *plan_node_alter_rename_column(AlterRenameColNode node, Database *db);
PlanNode *plan_node_alter_modify_column(AlterModifyNode node, Database *db);
PlanNode *plan_node_alter_add_constraint(AlterAddConstraintNode node, Database *db);
PlanNode *plan_node_alter_drop_constraint(AlterDropConstraint node, Database *db);
PlanNode *plan_node_create_index(CreateIndexNode node, Database *db);
PlanNode *plan_node_drop_index(DropIndexNode node, Database *db);
PlanNode *plan_node_filter(WhereNode node, Database *db);
PlanNode *plan_node_project(SelectNode node, Database *db);
PlanNode *plan_node_join(JoinNode node, Database *db);
PlanNode *plan_node_insert(InsertNode node, Database *db);
PlanNode *plan_node_delete(DeleteNode node, Database *db);
PlanNode *plan_node_update(UpdateNode node, Database *db);
PlanNode *plan_node_group_by(GroupByNode node, Database *db);
PlanNode *plan_node_order_by(OrderByNode node, Database *db);
PlanNode *plan_node_having(HavingNode node, Database *db);
PlanNode *plan_node_limit(LimitNode node, Database *db);
PlanNode *plan_node_offset(OffsetNode node, Database *db);
PlanNode *plan_node_seq_scan(Database *db);
PlanNode *plan_node_index_scan(Database *db);

extern bool query_plan_connect_node(QueryPlan *query_plan, PlanNode *new_node);

extern void query_plan_free(QueryPlan *query_plan);

#endif