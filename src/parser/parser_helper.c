#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/parser.h"
#include "../../include/tokenizer.h"
#include "../../include/expressions.h"

StatementType ast_to_statement_type(ASTNodeType type) {
    StatementType type;

    switch (type) {
        case AST_SELECT:
            type = STMT_SELECT;
            break;
        case AST_UPDATE:
            type = STMT_UPDATE;
            break;
        case AST_INSERT:
            type = STMT_INSERT;
            break;
        case AST_DELETE:
            type = STMT_DELETE;
            break;
        case AST_CREATE_TABLE:
            type = STMT_CREATE_TABLE;
            break;
        case AST_DROP_TABLE:
            type = STMT_DROP_TABLE;
            break; 
        case AST_ALTER_TABLE:
            type = STMT_ALTER_TABLE;
            break; 
        case AST_TRUNCATE_TABLE:
            type = STMT_TRUNCATE_TABLE;
            break;
        case AST_CREATE_INDEX:
            type = STMT_CREATE_INDEX;
            break;
        case AST_DROP_INDEX:
            type = STMT_DROP_INDEX;
            break;
        default: 
            printf("ASTNodeType doesn't exist\n");
            break;
    }

    return type;
}