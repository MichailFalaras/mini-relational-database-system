#ifndef SEMANTIC_BINDER_UTILS_H_
#define SEMANTIC_BINDER_UTILS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct database Database;
typedef struct abstract_syntax_tree_node ASTNode;
typedef struct binding_context BindingContext;
typedef struct expression_node ExpressionNode;
typedef struct index Index;
typedef struct table Table;
typedef struct constraint Constraint;

typedef struct ast_projection ProjectionNode;
typedef struct ast_from FromNode;
typedef struct ast_as AsNode;
typedef struct ast_join JoinNode;
typedef struct ast_on OnNode;
typedef struct ast_where WhereNode;
typedef struct ast_group_by GroupByNode;
typedef struct ast_having HavingNode;
typedef struct ast_order_by OrderByNode;
typedef struct ast_limit LimitNode;
typedef struct ast_offset OffsetNode;
typedef struct ast_into IntoNode;
typedef struct ast_values ValuesNode;
typedef struct ast_set SetNode;
typedef struct ast_create_table CreateTableNode;
typedef struct column_ref ColumnRef;
typedef struct column Column;
typedef struct schema Schema;
typedef struct ast_columns ColumnsNode;
typedef struct ast_column_def ColumnDefNode;
typedef struct ast_constraint ConstraintNode;
typedef struct ast_alter_action AlterActionNode;
typedef struct bound_expression_type BoundExpressionType;
typedef struct bound_projection BoundProjection;
typedef struct bound_insertion_values BoundInsertionValues;
typedef struct bound_alter_table BoundAlterTable;

/* ---------- CREATE TABLE Pending Column Bindings ---------- */

// A pending binding points to an existing AST ColumnRef.
// It does not own the ColumnRef or its ExpressionNode.
typedef struct pending_column_binding {
    ColumnRef *ref;
    int32_t relation_index;
    int32_t column_index;
} PendingColumnBinding;

typedef struct pending_column_bindings {
    PendingColumnBinding *items;
    size_t count;
    size_t capacity;
} PendingColumnBindings;

/* ---------- Binding of ExpressionNode ---------- */

bool bind_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a literal expression
bool bind_literal(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a column reference expression
bool bind_column_ref(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a table reference expression
bool bind_table_ref(ExpressionNode *expr, BindingContext *context, Table **out_table);

// Helper that rejects table references inside scalar expressions
bool bind_scalar_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a unary expression
bool bind_unary(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a binary expression
bool bind_binary(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind an IS NULL expression
bool bind_is_null(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind an IS NOT NULL expression
bool bind_is_not_null(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind an IN expression
bool bind_in(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a BETWEEN expression
bool bind_between(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

// Bind a function expression
bool bind_function(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type);

/* ---------- Binding of each ASTNodeType ---------- */

bool bind_select(ASTNode *root, BindingContext *context, BoundProjection *out_projection);

bool bind_insert(ASTNode *root, BindingContext *context, BoundInsertionValues *out_values);

bool bind_update(ASTNode *root, BindingContext *context, BoundUpdateValues *out_values);

bool bind_delete(ASTNode *root, BindingContext *context, BoundDeleteValues *out_values);

bool bind_create_table(ASTNode *root, BindingContext *context);

bool bind_drop_table(ASTNode *root, BindingContext *context);

bool bind_truncate_table(ASTNode *root, BindingContext *context);

bool bind_alter_table(ASTNode *root, BindingContext *context, BoundAlterTable *out_alter);

bool bind_create_index(ASTNode *root, BindingContext *context);

bool bind_drop_index(ASTNode *root, BindingContext *context);

/* ---------- Parsing of Inner Query Components ---------- */

bool bind_joins(JoinNode **joins, uint32_t num_joins, BindingContext *context);

bool bind_on(OnNode *on, BindingContext *context);

bool bind_projection(ProjectionNode *projection, BindingContext *context, BoundProjection *out_projection);

bool bind_from(FromNode *from, BindingContext *context);

bool bind_where(WhereNode *where, BindingContext *context);

bool bind_group_by(GroupByNode *group_by, BindingContext *context);

bool bind_having(HavingNode *having, BindingContext *context);

bool bind_order_by(OrderByNode *order_by, BindingContext *context);

bool bind_into(IntoNode *into, BindingContext *context);

bool bind_values(ValuesNode *values, BindingContext *context, BoundExpressionType *out_types);

bool bind_set(SetNode *set, BindingContext *context, BoundExpressionType *out_types);

bool bind_columns(ColumnsNode *columns, CreateTableNode *create_table, BindingContext *context);

bool bind_column_def(ColumnDefNode *column_def, CreateTableNode *create_table, 
    BindingContext *context, uint32_t owner_column_index);

bool bind_constraint(ConstraintNode *constraint, CreateTableNode *create_table, 
    BindingContext *context, int32_t owner_column_index);

bool bind_primary_key_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index);

bool bind_foreign_key_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index);

bool bind_unique_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index);

bool bind_check_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index);

bool bind_not_null_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index);

bool bind_default_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index);

bool bind_alter_action(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_add_col(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_drop_col(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_rename_table(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_rename_col(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_modify_col(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_add_constraint(AlterActionNode *action, BindingContext *context, Schema *working_schema);

bool bind_alter_drop_constraint(AlterActionNode *action, BindingContext *context, Schema *working_schema);


/* ---------- Binding Context Management ---------- */

// Register a table in the binding context, used by FROM and JOIN binding
bool register_binding_relation(const char *table_name, const char *alias, BindingContext *context);


/* ---------- SELECT Grouping and Aggregate Validation ---------- */

// Validate Projection expressions against GROUP BY expressions
bool validate_grouped_projection(ProjectionNode *projection, GroupByNode *group_by, 
    BindingContext *context);

// Validate that an expression is legal in a grouped query
bool validate_grouped_expression(ExpressionNode *expr, GroupByNode *group_by);

// Determine whether 2 expressions represent the same grouping expression
bool grouped_expressions_equal(const ExpressionNode *left, const ExpressionNode *right);

// Validate if a expression contains a nested aggregate expression
bool expression_contains_aggregate(const ExpressionNode *expr);

// Validate ORDER BY clause in a grouped query
bool validate_grouped_order_by(OrderByNode *order_by, GroupByNode *group_by);


/* ---------- INSERT Assignment Validation and Output Construction ---------- */

// Check assignment compatibility between columns and corresponding inserted values
bool validate_insert_assignments(IntoNode *into, ValuesNode *values,
    const BoundExpressionType *value_types, BindingContext *context);

// Build insertion values tuple
bool build_bound_insertion_values(IntoNode *into, ValuesNode *values,
    const BoundExpressionType *value_types, BindingContext *context, BoundInsertionValues *out_values);


/* ---------- UPDATE Assignment Validation and Output Construction ---------- */

// Check assignment compatibility between columns and corresponding update values
bool validate_update_assignments(SetNode *set, const BoundExpressionType *value_types, 
    BindingContext *context);

// Build updated values tuple
bool build_bound_update_values(SetNode *set, const BoundExpressionType *value_types, 
    BindingContext *context, BoundUpdateValues *out_values);


/* ---------- DELETE Output Construction ---------- */

// Build delete output
bool build_bound_delete_values(WhereNode *where, BindingContext *context, 
    BoundDeleteValues *out_values);


/* ---------- CREATE TABLE Column Resolution and Constraint Validation ---------- */

// Resolve column index for the new table schema
bool resolve_create_table_column(const CreateTableNode *create_table, const char *column_name,
    uint32_t *out_index);

// Validate a constrant's column references
bool validate_constraint_columns(ExpressionNode **column_refs, uint32_t count,
    const CreateTableNode *create_table);

// Validate one PRIMARY KEY constraint among all column-level and table-level constraints
bool validate_primary_key_count(const CreateTableNode *create_table);

// Retrieve a particular constraint as if column-level and table-level constraints were a single array
const ConstraintNode *get_create_table_constraint(const CreateTableNode *create_table,
    uint32_t position);
    
// Check the uniqueness of constraint names
bool validate_create_table_constraint_names(const CreateTableNode *create_table);


/* ---------- CREATE TABLE Foreign Key Validation ---------- */

// Resolve FOREIGN KEY constraint's referenced columns
bool resolve_fk_referenced_column(const ForeignKeyConstraintNode *fk,
    const CreateTableNode *create_table, const Table *referenced_table,
    const ColumnRef *ref, uint32_t *out_index, DataType *out_type);

// Check whether local column and referenced column have the same data type
// and type parameters accordingly
bool fk_column_types_compatible(const ColumnDefNode *local_column,
    const ColumnDefNode *self_referenced_column, const Column *external_referenced_column);

// Verify whether the referenced key completely matches a PRIMARY KEY or a UNIQUE key
bool fk_ast_key_matches(ExpressionNode **key_refs, uint32_t key_count,
    ExpressionNode **referenced_refs, uint32_t referenced_count);

// Verify that the referenced key matches either a PRIMARY KEY or UNIQUE constraint 
bool fk_matches_create_table_key(const CreateTableNode *create_table, ExpressionNode **referenced_refs,
    uint32_t referenced_count);

// Validates that the FOREIGN KEY matches a PRIMARY KEY or UNIQUE constraint from the existing table
bool fk_matches_schema_key(const Schema *schema, const uint32_t *referenced_indexes,
    uint32_t num_referenced_columns);


/* ---------- CREATE TABLE Default and Nullability Validation ---------- */

// Helper that checks whether an expression is a NULL literal
bool expression_is_explicit_null_literal(const ExpressionNode *expr);

// Helper that checks whether a constriant's key contains a particular column
bool constraint_key_contains_column(ExpressionNode *const *column_refs, uint32_t count,
    const char *column_name);

// Validate conflicting DEFAULT constraints among column-level and table-level constraints 
bool validate_create_table_defaults(const CreateTableNode *create_table);


/* ---------- CREATE TABLE Pending Reference Collection and Commit ---------- */

// Add a validated resolution without modifying the AST
bool pending_binding_add(PendingColumnBindings *pending, ColumnRef *column_ref, uint32_t column_index);

// Collect a reference to a column in the proposed table
bool collect_local_column_ref(ExpressionNode *expr, const CreateTableNode *create_table,
    PendingColumnBindings *pending);

// Collect a reference to an FK's referenced table
bool collect_fk_referenced_column_ref(ExpressionNode *expr, const ForeignKeyConstraintNode *fk,
    const CreateTableNode *create_table, Database *db, PendingColumnBindings *pending);

// Recursively collect column references from CHECK expressions
bool collect_create_table_expression_refs(ExpressionNode *expr, const CreateTableNode *create_table, 
    PendingColumnBindings *pending);

// Collect references belonging to a single constraint
bool collect_constraint_references(ConstraintNode *constraint, const CreateTableNode *create_table, 
    Database *db, PendingColumnBindings *pending);

// Commit all column references' indexes from column definitions and table-level constraints 
bool commit_create_table_references(CreateTableNode *create_table, Database *db);


/* ---------- ALTER TABLE ADD COLUMN Validation and Proposed Schema Construction ---------- */

// Resolve a column in the proposed ALTER TABLE schema
bool resolve_alter_candidate_column(const Schema *working_schema, const ColumnDefNode *new_column,
    const char *column_name, uint32_t *out_index, DataType *out_type);

// Verify whether the new column declares a candidate key
bool alter_new_column_has_key(const ColumnDefNode *column, const char *table_name);

// Validate FOREIGN KEY constraint in an ALTER TABLE action
bool validate_alter_column_fk(const ForeignKeyConstraintNode *fk, const ColumnDefNode *column,
    const Table *table, Schema *working_schema, const BindingContext *context);

// Validate CHECK constraint in ALTER TABLE
bool validate_alter_column_check(ConstraintNode *constraint, ColumnDefNode *column,
    Table *table, Schema *working_schema, const BindingContext *context);

// Construct a runtime constraint for the working schema from the column definition's constraint nodes
Constraint *build_runtime_alter_constraint(const ConstraintNode *constraint, 
    const ColumnDefNode *column, const Schema *working_schema, const BindingContext *context,
    const Table *table, char *runtime_name);

// Add new column to the working schema as a result of an ADD COLUMN action
bool working_schema_add_column(Schema *schema, const ColumnDefNode *column,
    const BindingContext *context, const Table *table);

// Collect all column references from a bound CHECK expression 
// in order to construct the corresponding runtime CHECK constraint
bool collect_bound_check_refs(const ExpressionNode *expr, uint32_t *indices, uint32_t capacity,
    uint32_t *count);

// Generate a constraint name for an unnamed constraint in ALTER TABLE
bool generate_alter_constraint_name(const ConstraintNode *constraint,
    const ColumnDefNode *column, const Table *table, const Schema *working_schema,
    Constraint *const *prepared, uint32_t prepared_count, char *out_name);


/* ---------- ALTER TABLE MODIFY COLUMN Validation Helpers ---------- */

// Validate the compatibity of 2 Column's data types, used by FOREIGN KEY validation
bool fk_runtime_types_compatible(const Column *left, const Column *right);

// Validate a modified FOREIGN KEY constraint in ALTER TABLE MODIFY COLUMN
bool validate_modified_foreign_key(const Constraint *constraint, const Table *table, 
    const Schema *working_schema, const Database *db);

// Reset Column reference indexes (-1) before binding the proposed modified CHECK constraint
bool reset_check_column_refs(ExpressionNode *expr);

// Validate a modified CHECK constraint in ALTER TABLE MODIFY COLUMN
bool validate_modified_check_expression(const Constraint *constraint, Table *table,
    Schema *working_schema, Database *db);

// Validate a modified DEFAULT constraint in ALTER TABLE MODIFY COLUMN
bool validate_modified_default(const Constraint *constraint, DataType new_type,
    DataTypeArgs new_args, BindingContext *context);

// Validate incoming FOREIGN KEYS from other database tables
bool validate_modify_incoming_foreign_keys(const Table *modified_table,
    const Schema *proposed_schema, const Database *db);

// Check that the DEFAULT value validates new data type arguments 
bool value_fits_type_args(const Value *value, DataType target_type, DataTypeArgs target_args);

// Validate one existing index against the proposed table schema.
bool validate_modified_index(const Index *index, Schema *working_schema, uint32_t column_index);

// Orchestrate validation of all indexes that are affected by the modified column
bool validate_modified_column_indexes(Table *table, Schema *working_schema, uint32_t column_index);


/* ---------- ALTER TABLE ADD CONSTRAINT Validation Helpers ---------- */

// Resolve constraint column references against a runtime schema
bool resolve_constraint_column_refs(ExpressionNode *const *refs, uint32_t count,
    const Schema *schema, const char *owner_name, uint32_t *out_indexes);

// Compare two ordered arrays of resolved column indexes
bool constraint_indexes_equal(const uint32_t *left, const uint32_t *right, uint32_t count);

// Check if schema has an equivalent constraint
bool schema_has_equivalent_constraint(const Schema *shema, const Constraint *candidate);


/* ---------- ALTER TABLE DROP COLUMN Validation Helpers ---------- */

// Validate that another table's FOREIGN KEY references the target column
bool has_incoming_fk_dependency(const Database *db, const Table *target_table, uint32_t column_index);

// Remove column from the working schema as a result of a DROP COLUMN action
bool working_schema_drop_column(Schema *schema, uint32_t column_index, const char *table_name);

#endif