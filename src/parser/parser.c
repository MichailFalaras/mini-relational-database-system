#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/tokenizer.h"
#include "../../include/parser.h"
#include "parser_utils.h"
#include "../../include/expressions.h"

/* Allocate and initialize Parser component. */
Parser *parser_init(TokenArray *token_array) {
    if (!token_array || !token_array->amount_tokens
        || !token_array->tokens) {
        return NULL;
    }

    Parser *parser = (Parser *) calloc(1, sizeof(Parser));
    if (!parser) {
        return NULL;
    }

    parser->token_array = token_array;
    parser->current_position = 0;

    return parser;
}

/* Parsing orchestration function.
 *
 * Identify top-level keyword, create Abstract Syntax Tree
 * and validate it through Semantic Binder. */
Statement *parse_query(Parser *parser, Database *db) {
    if (!db || !db->catalog || !db->pager) {
        return NULL;
    }

    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens) {
        return NULL;
    }

    ASTNode *root = parse(parser);
    if (!root) {
        return NULL;
    }
    
    if (!bind_statement(root, db)) {
        free(root);
        return NULL;
    }
    
    return statement_init(root, ast_to_statement_type(root->type));
}

/* Parse top-level keyword and create Abstract Syntax Tree. */
ASTNode *parse(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || parser->current_position != 0) {
        return NULL;
    }

    /* Fail if token is not a keyword. */
    ASTNode *root = NULL;
    if (parser->token_array->tokens[parser->current_position]->type != KEYWORD) {
        return NULL;
    }

    /* Identify top-level keyword. */
    char *token_str = parser->token_array->tokens[parser->current_position]->token;
    if (!strcasecmp(token_str, "SELECT")) {
        root = parse_select(parser);

    } else if (!strcasecmp(token_str, "UPDATE")) {
        root = parse_update(parser);

    } else if (!strcasecmp(token_str, "INSERT")) {
        root = parse_insert(parser);
        
    } else if (!strcasecmp(token_str, "DELETE")) {
        root = parse_delete(parser);

    } else if (!strcasecmp(token_str, "CREATE TABLE")) {
        root = parse_create_table(parser);

    } else if (!strcasecmp(token_str, "DROP TABLE")) {
        root = parse_drop_table(parser);

    } else if (!strcasecmp(token_str, "ALTER TABLE")) {
        root = parse_alter_table(parser);

    } else if (!strcasecmp(token_str, "TRUNCATE TABLE")) {
        root = parse_alter_table(parser);

    } else if (!strcasecmp(token_str, "CREATE INDEX")) {
        root = parse_create_index(parser);
        
    } else if (!strcasecmp(token_str, "DROP INDEX")) {
        root = parse_drop_index(parser);

    } else {
        fprintf(stderr, "Token string is not a keyword.\n");
        return NULL;
    }

    return root;
}

/* Parse expression.
 *
 * Operator Precedence:
 * -- highest --
 * unary (+ - ~)
 * * / %
 * + -
 * comparison
 * NOT
 * AND
 * OR
 * -- lowest --. */
ExpressionNode *parse_expression(Parser *parser) {
    if (!parser || !parser->token_array 
        || !parser->token_array->amount_tokens
        || !parser->token_array->tokens
        || !parser->current_position) { 
        return NULL;
    }

    if (parser->current_position >= parser->token_array->amount_tokens) {
        return NULL;
    }

    /* KEYWORD Token Type technically allowed since 
     * TRUE/FALSE Boolean literals are technically literals. */

    return parse_or(parser); // Each expression parsing helper is calling the next.
}

/* Allocate and initialize Statement. */
Statement *statement_init(ASTNode *root, StatementType type) {
    if (!root || type > STMT_DELETE) {
        return NULL;
    }

    Statement *statement = (Statement *) calloc(1, sizeof(Statement));
    if (!statement) {
        return NULL;
    }

    statement->root = root;
    statement->type = type;

    return statement;
}

/* Deallocate Parser component*/
void parser_free(Parser *parser) {
    if (parser) {
        TokenArray *token_array = parser->token_array;
        parser->token_array = NULL;

        token_array_free(token_array);
        free(parser);
    }
}