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

        case AST_DROP_TABLE: {
            BoundDropTable drop = {0};

            success = bind_drop_table(root, &context, &drop);

            if (success) {
                out_statement->type = BOUND_STATEMENT_DROP_TABLE;
                out_statement->data.drop_table = drop;
            }

            break;
        }
   

        case AST_TRUNCATE_TABLE: {
            BoundTruncateTable truncate = {0};

            success = bind_truncate_table(root, &context, &truncate);

            if (success) {
                out_statement->type = BOUND_STATEMENT_TRUNCATE_TABLE;
                out_statement->data.truncate_table = truncate;
            }

            break;
        }

#if 0
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

void bound_drop_table_free(BoundDropTable *drop_table) {
    if (!drop_table) {
        return;
    }

    drop_table->table = NULL;
    drop_table->drop_table = NULL;
}

void bound_truncate_table_free(BoundTruncateTable *truncate_table) {
    if (!truncate_table) {
        return;
    }

    truncate_table->table = NULL;
    truncate_table->truncate_table = NULL;
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

        case BOUND_STATEMENT_DROP_TABLE:
            bound_drop_table_free(&statement->data.drop_table);
            break;

        case BOUND_STATEMENT_TRUNCATE_TABLE:
            bound_truncate_table_free(&statement->data.truncate_table);
            break;

        default:
            break;
    }

    *statement = (BoundStatement){0};
}


/* ---------- AlterColumnMap Helpers ---------- */

bool alter_column_map_init(AlterColumnMap *map, uint32_t original_count) {
    if (!map || original_count == 0 ||
        (size_t)original_count > SIZE_MAX / sizeof(uint32_t)) {
        return false;
    }

    uint32_t *indexes = malloc((size_t)original_count * sizeof(*indexes));

    if (!indexes) {
        return false;
    }

    for (uint32_t i = 0; i < original_count; i++) {
        indexes[i] = i;
    }

    map->working_to_original = indexes;
    map->num_columns = original_count;

    return true;
}

void alter_column_map_free(AlterColumnMap *map) {
    if (!map) {
        return;
    }

    free(map->working_to_original);

    map->working_to_original = NULL;
    map->num_columns = 0;
}

// Retrieve the original column index based on the working index
bool alter_map_get_original(const AlterColumnMap *map, uint32_t working_index, uint32_t *out_original) {
    if (!map || !map->working_to_original || !out_original || working_index >= map->num_columns) {
        return false;
    }

    *out_original = map->working_to_original[working_index];
    return true;
}

// Retrieve the working column index based on the original index
bool alter_map_find_working(const AlterColumnMap *map, uint32_t original_index, uint32_t *out_working) {
    if (!map || !map->working_to_original ||
        !out_working || original_index == ALTER_NEW_COLUMN) {
        return false;
    }

    for (uint32_t i = 0; i < map->num_columns; i++) {
        if (map->working_to_original[i] == original_index) {
            *out_working = i;
            return true;
        }
    }

    return false;
}

// Add new working index
bool alter_column_map_add(AlterColumnMap *map) {
    if (!map ||
        (map->num_columns > 0 && !map->working_to_original) || map->num_columns == UINT32_MAX || 
        (size_t)map->num_columns + 1 > SIZE_MAX / sizeof(uint32_t)) {
        return false;
    }

    size_t new_count = (size_t)map->num_columns + 1;

    uint32_t *expanded = realloc(map->working_to_original, new_count * sizeof(*expanded));

    if (!expanded) {
        return false;
    }

    map->working_to_original = expanded;
    map->working_to_original[map->num_columns] = ALTER_NEW_COLUMN;
    map->num_columns++;

    return true;
}

// Remove working index
bool alter_column_map_drop(AlterColumnMap *map, uint32_t working_index) {
    if (!map || !map->working_to_original || working_index >= map->num_columns) {
        return false;
    }

    for (uint32_t i = working_index; i + 1 < map->num_columns; i++) {
        map->working_to_original[i] = map->working_to_original[i + 1];
    }

    map->num_columns--;
    return true;
}