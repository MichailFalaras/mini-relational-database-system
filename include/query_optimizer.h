#ifndef QUERY_OPTIMIZER_H_
#define QUERY_OPTIMIZER_H_

#include <stdint.h>
#include "schema.h"
#include "expressions.h"
#include "constraints.h"
#include "index.h"
#include "row.h"

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

/* Operation-Specific Data. */
typedef struct plan_create_table {
    char table_name[64];
    Schema *schema;
} CreateTable;

typedef struct plan_drop_table {
    char table_name[64];
} DropTable;

typedef struct plan_alter_rename_table {
    char old_table_name[64];
    char new_table_name[64];
} AlterRenameTable;

typedef struct plan_truncate_table {
    char table_name[64];
} TruncateTable;

typedef struct plan_alter_add_column {
    char table_name[64];
    Column *new_column;
} AlterAddColumn;

typedef struct plan_alter_drop_column {
    char table_name[64];
    char col_name[64];
} AlterDropColumn;

typedef struct plan_alter_rename_column {
    char table_name[64];
    char old_col_name[64];
    char new_col_name[64];
} AlterRenameColumn;

typedef struct plan_alter_modify_column {
    char table_name[64];
    char old_column_name[64];
    Column *new_column_definition;
} AlterModifyColumn;

typedef struct plan_alter_add_constraint {
    char table_name[64];
    Constraint *constraint;
} AlterAddConstraint;

typedef struct plan_alter_drop_constraint {
    char table_name[64];
    char constraint_name[64];
} AlterDropConstraint;

typedef struct plan_create_index {
    char table_name[64];
    Index *new_index;
} CreateIndex;

typedef struct plan_drop_index {
    char table_name[64];
    char index_name[64];
} DropIndex;

typedef struct plan_seq_scan {
    char table_name[64];
} SeqScan;

typedef struct plan_index_scan {
    char table_name[64];
    char index_name[64];
} IndexScan;

/* WHERE Condition */
typedef struct plan_filter {
    ExpressionNode *expression;
} Filter;

/* SELECT Columns */
typedef struct plan_project {
    uint32_t *columns;
    uint32_t amount_columns;
} Project;

typedef struct plan_join {
    char left_table_name[64];
    char right_table_name[64];
    ExpressionNode *join_condition;
} Join;

typedef struct plan_insert {
    char table[64];
    Row *rows;
} Insert;

typedef struct plan_update {
    char table_name[64];
    Row *new_values;
} Update;

typedef struct plan_delete {
    char table_name[64];
} Delete;

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

#endif