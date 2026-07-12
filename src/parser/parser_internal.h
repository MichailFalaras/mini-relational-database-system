#ifndef PARSER_INTERNAL_H_
#define PARSER_INTERNAL_H
#include "../../include/parser.h"
#include "../../include/tokenizer.h" 

extern StatementType ast_to_statement_type(ASTNodeType type);

/* SELECT */
extern ASTNode *parse_select(Parser *parser);

extern ProjectionNode *parse_projection(Parser *parser);

extern FromNode *parse_from(Parser *parser);

extern WhereNode *parse_where(Parser *parser);

extern GroupByNode *parse_group_by(Parser *parser);

extern HavingNode *parse_having(Parser *parser);

extern OrderByNode *parse_order_by(Parser *parser);

// Might need to check how many JOINs there are
extern JoinNode *parse_joins(Parser *parser);

extern LimitNode *parse_limit(Parser *parser);

extern OffsetNode *parse_offset(Parser *parser);

/* INSERT */
extern InsertNode *parse_insert(Parser *parser);

extern IntoNode *parse_into(Parser *parser);

extern ValuesNode *parse_values(Parser *parser);

/* UPDATE */
extern UpdateNode *parse_update(Parser *parser);

extern SetNode *parse_set(Parser *parser);

/* DELETE */
extern DeleteNode *parse_delete(Parser *parser);

/* CREATE TABLE */
extern CreateTableNode *parse_create_table(Parser *parser);

/* DROP TABLE */
extern DropTableNode *parse_drop_table(Parser *parser);

/* ALTER TABLE */
extern AlterTableNode *parse_alter_table(Parser *parser);

extern AlterAddNode *parse_alter_add_col(Parser *parser);

extern AlterDropNode *parse_alter_drop_col(Parser *parser);

extern AlterRenameTableNode *parse_alter_rename_table(Parser *parser);

extern AlterRenameColNode *parse_alter_rename_col(Parser *parser);

extern AlterModifyNode *parse_alter_modify_col(Parser *parser);

extern AlterAddConstraintNode *parse_alter_add_constraint(Parser *parser);

extern AlterDropConstraintNode *parse_alter_drop_constraint(Parser *parser);

/* CREATE INDEX */
extern CreateIndexNode *parse_create_index(Parser *parser);

/* DROP INDEX */
extern DropIndexNode *parse_drop_index(Parser *parser);

/* Free for all types of AST nodes. (Switch-Statement)*/
void free_ast_node(ASTNode *node);

void free_ast_select(SelectNode *node); 

void free_ast_projection(ProjectionNode *node);

void free_ast_from(FromNode *node); 

void free_ast_where(WhereNode *node); 

void free_ast_group_by(GroupByNode *node);

void free_ast_having(HavingNode *node);

void free_ast_order_by(OrderByNode *node);

void free_ast_joins(JoinNode *node);

void free_ast_limit(LimitNode *node);

void free_ast_offset(OffsetNode *node);

void free_ast_insert(InsertNode *node);

void free_ast_into(IntoNode *node);

void free_ast_values(ValuesNode *node);

void free_ast_update(UpdateNode *node);

void free_ast_set(SetNode *node);

void free_ast_delete(DeleteNode *node);

void free_ast_create_table(CreateTableNode *node);

void free_ast_columns(ColumnsNode *node);

void free_ast_column_defs(ColumnDefNode *node);

void free_ast_constraints(ConstraintsNode *node);

void free_ast_drop_table(DropTableNode *node);

void free_ast_alter_table(AlterTableNode *node);

void free_ast_alter_add_col(AlterAddNode *node);

void free_ast_alter_drop_col(AlterDropNode *node);

void free_ast_alter_rename_col(AlterRenameColNode *node);

void free_ast_alter_rename_table(AlterRenameTableNode *node);

void free_ast_alter_modify_col(AlterModifyNode *node);

void free_ast_alter_add_constraint(AlterAddConstraintNode *node);

void free_ast_alter_drop_constraint(AlterDropConstraintNode *node);

void free_ast_create_index(CreateIndexNode *node);

void free_ast_drop_index(DropIndexNode *node);

#endif