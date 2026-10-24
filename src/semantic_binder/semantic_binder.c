#include <stdio.h>
#include <stdlib.h>
#include "../../include/semantic_binder.h"
#include "../../include/database.h"
#include "../../include/ast.h"


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

        case AST_UPDATE:
            success = bind_update(root, &context);
            break;

        case AST_DELETE:
            success = bind_delete(root, &context);
            break;

        case AST_CREATE_TABLE:
            success = bind_create_table(root, &context);
            break;

        case AST_DROP_TABLE:
            success = bind_drop_table(root, &context);
            break;

        case AST_ALTER_TABLE:
            success = bind_alter_table(root, &context);
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

        default:
            break;
    }

    *statement = (BoundStatement){0};
}
