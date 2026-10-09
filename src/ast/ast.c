#include <stdlib.h>
#include "../../include/ast.h"
#include "ast_utils.h"

/* Free an AST node */
void ast_free_node(ASTNode *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_SELECT:
            ast_free_select(&node->node_contents.select);
            break;

        case AST_INSERT:
            ast_free_insert(&node->node_contents.insert);
            break;

        case AST_UPDATE:
            ast_free_update(&node->node_contents.update);
            break;

        case AST_DELETE:
            ast_free_delete(&node->node_contents.delete);
            break;

        case AST_CREATE_TABLE:
            ast_free_create_table(&node->node_contents.create_table);
            break;

        case AST_ALTER_TABLE:
            ast_free_alter_table(&node->node_contents.alter_table);
            break;

        case AST_CREATE_INDEX:
            ast_free_create_index(&node->node_contents.create_index);
            break;

        /* DROP TABLE, TRUNCATE TABLE, and DROP INDEX contain only fixed-size char[64] fields,
           so their top-level destructors actually have nothing to do */
        case AST_DROP_TABLE:
        case AST_TRUNCATE_TABLE:
        case AST_DROP_INDEX:
            break;

        default:
            return;
    }

    free(node);
}