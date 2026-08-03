#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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

