#include <stdio.h>
#include <stdlib.h>
#include "../../include/semantic_binder.h"
#include "./semantic_binder_utils.h"
#include "../../include/database.h"
#include "../../include/ast.h"
#include "../../include/schema.h"


/* Inside parse_query, last check of AST validity. */
bool bind_statement(ASTNode *root, Database *db, BoundStatement *out_statement) {
    if (!root || 
        root->type < AST_SELECT ||
        root->type > AST_DROP_INDEX ||
        !db || !db->catalog || !db->pager) {
        printf("bind_statement: Invalid inputs.\n");
        return false;
    }

    if (!out_statement || out_statement->type != BOUND_STATEMENT_NONE) {
        printf("bind_statement: Invalid output structure.\n");
        return false;
    }

    BindingContext context = {0};
    context.db = db;

    bool success = false;

    switch (root->type) {
        case AST_SELECT: {
            BoundProjection projection = {0};

            success = bind_select(root, &context, &projection);

            if (success) {
                out_statement->type = BOUND_STATEMENT_SELECT;
                out_statement->data.projection = projection;
            }

            break;
        }

        case AST_INSERT: {
            BoundInsertionValues insertion = {0};

            success = bind_insert(root, &context, &insertion);

            if (success) {
                out_statement->type = BOUND_STATEMENT_INSERT;
                out_statement->data.insertion = insertion;
            }

            break;
        }

        case AST_UPDATE: {
            BoundUpdateValues update = {0};
            success = bind_update(root, &context, &update);

            if (success) {
                out_statement->type = BOUND_STATEMENT_UPDATE;
                out_statement->data.update = update;
            }
            
            break;
        }

        case AST_DELETE: {
            BoundDeleteValues delete = {0};
            success = bind_delete(root, &context, &delete);

            if (success) {
                out_statement->type = BOUND_STATEMENT_DELETE;
                out_statement->data.delete = delete; 
            }

            break;
        }

        case AST_CREATE_TABLE: {
            success = bind_create_table(root, &context);

            if (success) {
                out_statement->type = BOUND_STATEMENT_CREATE_TABLE;
                out_statement->data.create_table.create_table = &root->node_contents.create_table;
            }

            break;
        }

        case AST_ALTER_TABLE: {
            BoundAlterTable alter = {0};

            success = bind_alter_table(root, &context, &alter);

            if (success) {
                out_statement->type = BOUND_STATEMENT_ALTER_TABLE;
                out_statement->data.alter_table = alter;
            }

            break;
        }


#if 0
        case AST_DROP_TABLE:
            success = bind_drop_table(root, &context);
            break;

        

        case AST_TRUNCATE_TABLE:
            success = bind_truncate_table(root, &context);
            break;
            
        case AST_CREATE_INDEX:
            success = bind_create_index(root, &context);
            break;

        case AST_DROP_INDEX:
            success = bind_drop_index(root, &context);
            break;
#endif           
        default:
            fprintf(stderr, "ASTNodeType does not match.\n");
            success = false;
            break;
    }

    free(context.relations);
    return success;
}

/* Free SELECT projection metadata */
void bound_projection_free(BoundProjection *projection) {
    if (!projection) {
        return;
    }

    free(projection->columns);
    projection->columns = NULL;
    projection->num_columns = 0;
}

/* Free INSERT values metadata */
void bound_insertion_values_free(BoundInsertionValues *insertion) {
    if (!insertion) {
        return;
    }

    free(insertion->values);

    insertion->values = NULL;
    insertion->num_values = 0;
    insertion->table = NULL;
}

/* Free UPDATE values metadata */
void bound_update_values_free(BoundUpdateValues *update) {
    if (!update) {
        return;
    }

    free(update->values);

    update->values = NULL;
    update->num_values = 0;
    update->table = NULL;
}

void bound_delete_values_free(BoundDeleteValues *delete) {
    if (!delete) {
        return;
    }

    delete->table = NULL;
    delete->where = NULL;
}

void bound_create_table_free(BoundCreateTable *create_table) {
    if (!create_table) {
        return;
    }

    create_table->create_table = NULL;
}

void bound_alter_table_free(BoundAlterTable *alter_table) {
    if (!alter_table) {
        return;
    }

    schema_free(alter_table->result_schema);

    alter_table->result_schema = NULL;
    alter_table->table = NULL;
    alter_table->alter_table = NULL;
}

/* Free bound statement metadata */
void bound_statement_free(BoundStatement *statement) {
    if (!statement) {
        return;
    }

    switch (statement->type) {
        case BOUND_STATEMENT_SELECT:
            bound_projection_free(&statement->data.projection);
            break;

        case BOUND_STATEMENT_INSERT:
            bound_insertion_values_free(&statement->data.insertion);
            break;

        case BOUND_STATEMENT_UPDATE:
            bound_update_values_free(&statement->data.update);
            break;

        case BOUND_STATEMENT_DELETE:
            bound_delete_values_free(&statement->data.delete);
            break;

        case BOUND_STATEMENT_CREATE_TABLE:
            bound_create_table_free(&statement->data.create_table);
            break;

        case BOUND_STATEMENT_ALTER_TABLE:
            bound_alter_table_free(&statement->data.alter_table);
            break;

        default:
            break;
    }

    *statement = (BoundStatement){0};
}
