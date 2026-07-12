#include <stdio.h>
#include <stdlib.h>
#include "../../include/parser.h"
#include "../../include/tokenizer.h"

Parser *parser_init(TokenArray *token_array) {

    if (token_array == NULL) {
        return NULL;
    }

    Parser *parser = (Parser *) malloc(sizeof(Parser));
    if (parser == NULL) {
        perror("Parser");
        exit(1);
    }

    parser->token_array = token_array;
    parser->current_position = 0;

    return parser;
}

Statement *statement_init(ASTNode *root, StatementType type) {
    Statement *statement = (Statement *) malloc(sizeof(Statement));
    if (statement == NULL) {
        perror("Statement");
        exit(1);
    }

    statement->root = root;
    statement->type = type;

    return statement;
}

Statement *parse_query(Parser *parser, Database *db) {
    Statement *statement = NULL;

    ASTNode *root = parse(parser);
    if (root == NULL) {
        return NULL;
    }
    
    if (!semantic_binder(root, db)) {
        return NULL;
    }
    
    return statement_init(root, ast_to_statement_type(root->type));
}

ASTNode *parse(Parser *parser) {
    ASTNode *root = NULL;

    if (parser->token_array->tokens[parser->current_position]->type != KEYWORD) {
        return NULL;
    }

    if (!strcasecmp(parser->token_array->tokens[0]->token, "SELECT")) {
        root = parse_select(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "UPDATE")) {
        root = parse_update(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "INSERT")) {
        root = parse_insert(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "DELETE")) {
        root = parse_delete(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "CREATE TABLE")) {
        root = parse_create_table(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "DROP TABLE")) {
        root = parse_drop_table(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "ALTER TABLE")) {
        root = parse_alter_table(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "CREATE INDEX")) {
        root = parse_create_index(parser);
    } else if (!strcasecmp(parser->token_array->tokens[0]->token, "DROP INDEX")) {
        root = parse_drop_index(parser);
    } else {
        return NULL; // Tokenizer issue if it reaches this point. 
    }

    return root;
}

void parser_free(Parser *parser) {
    if (parser != NULL) {
        token_array_free(parser->token_array);
        free(parser);
    }
}

