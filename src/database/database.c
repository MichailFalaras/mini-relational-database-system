#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../include/database.h"
#include "../../include/pager.h"
#include "../../include/table.h"


/*
 * Open a database file or initialize a new one.
 *
 * Current lifecycle scope:
 *
 * - pager_open() creates a new database file when the pathname does not exist.
 * - pager_open() opens an existing database file when it already exists.
 * - The in-memory table registry starts empty.
 * - Loading persisted table metadata from the System Catalog is deferred
 *   until catalog serialization and deserialization are implemented.
 */
Database *database_open(const char *pathname) {
    // Validate input name
    if (!pathname || pathname[0] == '\0') {
        printf("database_open: Invalid database name.\n");
        return NULL;
    }

    if (strlen(pathname) >= DATABASE_PATH_MAX) {
        printf("database_open: Database pathname exceeds the %u-char limit.\n", (unsigned int) (DATABASE_PATH_MAX - 1));
        return NULL;
    }

    Database *db = (Database *) calloc(1, sizeof(Database));
    if (!db) {
        printf("database_open: Database structure could not be allocated.\n");
        return NULL;
    }
    
    strcpy(db->pathname, pathname);
    
    db->tables = (Table **) calloc(MAX_TABLES, sizeof(Table *));
    if (!db->tables) {
        printf("database_open: Table pointer array could not be allocated.\n");
        database_free(db);
        return NULL;
    }

    db->table_count = 0;

    // Although not intended at this version where we create an new empty database
    // pager_open() supports the opening of an existing database file (issue to be implemented later)
    db->pager = pager_open(pathname);
    if (!db->pager) {
        printf("database_open: Pager could not be opened.\n");
        database_free(db);
        return NULL;
    }
    
    return db;
}

/* Close Database */
bool database_close(Database *db) {
    if (!db) {
        printf("database_close: Invalid input database.\n");
        return false;
    }

    bool pager_close_succeeded = db->pager != NULL;

    if (!db->pager) {
        printf("database_close: Database has no Pager.\n");
    }

    // Free Table metadata structures and pointer array
    if (db->tables) {
        for (uint32_t i = 0; i < MAX_TABLES; i++) {
            if (!db->tables[i]) {
                continue;
            }

            table_free(db->tables[i]);
            db->tables[i] = NULL;
        }

        free(db->tables);
        db->tables = NULL;
    }
    
    db->table_count = 0;

    // Attempt to close the (existing) pager and, by extension, free all Index pages in memory
    if (db->pager) {
        Pager *pager = db->pager;
        db->pager = NULL;

        pager_close_succeeded = pager_close(pager);
    }

    free(db);
    return pager_close_succeeded;
}


/* Deallocate Database metadata structure */
void database_free(Database *db) {
    if (!db) {
        return;
    }

    // Free Table metadata structures and pointer array
    if (db->tables) {
        for (uint32_t i = 0; i < MAX_TABLES; i++) {
            if (!db->tables[i]) {
                continue;
            }

            table_free(db->tables[i]);
            db->tables[i] = NULL;
        }

        free(db->tables);
        db->tables = NULL;
    }

    db->table_count = 0;

    // Attempt to close the page and, by extension, free all Index pages in memory
    if (db->pager) {
        Pager *pager = db->pager;
        db->pager = NULL;

        (void) pager_close(pager);
    }

    free(db);
}


/* Add table metadata to database */
bool database_add_table(Database *db, Table *table) {
    // Validate inputs
    if (!db || !db->tables) {
        printf("database_add_table: Invalid or uninitialized database.\n");
        return false;
    }
    
    if (!table || 
        table->name[0] == '\0' || 
        !table->table_schema ||
        !table->secondary_indexes) {
        printf("database_add_table: Invalid or uninitialized new table.\n");
        return false;
    }

    if (db->table_count >= MAX_TABLES) {
        printf("database_add_table: Database is at full Table capacity.\n");
        return false;
    }

    // Traverse the table pointers array and check if table already exists
    for (uint32_t i = 0; i < db->table_count; i++) {
        if (!db->tables[i]) {
            printf("database_add_table: Table array has invalid empty pointer.\n");
            return false;
        }

        // Check for existing table with the same name
        if (strcasecmp(db->tables[i]->name, table->name) == 0) {
            printf("database_add_table: A table with the same name already exists in the database.\n");
            return false;
        }
    }

    // Add new table to the next
    db->tables[db->table_count] = table;
    db->table_count++; 

    return true;
}


/* Check if database contains a Table */
bool database_has_table(Database *db, const char *table_name) {
    return database_find_table_index(db, table_name) != UINT32_MAX;
}


/* Check if database contains a Table, and return it */
Table *database_find_table(Database *db, const char *table_name) {
    uint32_t index_pos = database_find_table_index(db, table_name);
    
    if (index_pos == UINT32_MAX) {
        return NULL;
    }

    return db->tables[index_pos];
}


/* Return a Table's registry index, or UINT32_MAX if not found. */
uint32_t database_find_table_index(Database *db, const char *table_name) {
    // Validate inputs
    if (!db || !db->tables) {
        printf("database_find_table_index: Invalid or uninitialized database.\n");
        return UINT32_MAX;
    }

    if (!table_name || table_name[0] == '\0') {
        printf("database_find_table_index: Invalid input table name.\n");
        return UINT32_MAX;
    }

    if (db->table_count > MAX_TABLES) {
        printf("database_find_table_index: Invalid table count.\n");
        return UINT32_MAX;
    }

    // Traverse the table pointers array and return the target Table metadata
    for (uint32_t i = 0; i < db->table_count; i++) {
        if (!db->tables[i]) {
            printf("database_find_table_index: Table array has invalid empty pointer.\n");
            return UINT32_MAX;
        }

        // Check for existing table with the same name
        if (strcasecmp(db->tables[i]->name, table_name) == 0) {
            return i;
        }
    }

    return UINT32_MAX;    
}


/*
 * Remove and free a table owned by the Database.
 *
 * On success, the table metadata is destroyed and any previous pointer
 * to that Table becomes invalid.
 */
bool database_remove_table(Database *db, const char *table_name) {
    uint32_t index = database_find_table_index(db, table_name);

    if (index == UINT32_MAX) {
        return false;
    }

    table_free(db->tables[index]);

    // Shifting the subsequent pointers to cover up the empty pointer slot in the array
    for (uint32_t i = index; i + 1 < db->table_count; i++) {
        db->tables[i] = db->tables[i + 1];
    }

    db->tables[db->table_count - 1] = NULL;
    db->table_count--;

    return true;
}