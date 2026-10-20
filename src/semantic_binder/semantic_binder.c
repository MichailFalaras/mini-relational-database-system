#include <stdio.h>
#include <stdlib.h>
#include "../../include/semantic_binder.h"
#include "../../include/database.h"
#include "../../include/ast.h"


/* Inside parse_query, last check of AST validity. */
bool bind_statement(ASTNode *root, Database *db) {
    if (!root || 
        root->type < AST_SELECT ||
        root->type > AST_DROP_INDEX ||
        !db || !db->catalog || !db->pager) {
        return false;
    }

    BindingContext context = {0};
    context.db = db;

    bool success = false;

    switch (root->type) {
        case AST_SELECT:
            success = bind_select(root, &context);
            break;

        case AST_INSERT:
            success = bind_insert(root, &context);
            break;

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
            return false;
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
