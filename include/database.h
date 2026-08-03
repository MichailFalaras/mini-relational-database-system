#ifndef DATABASE_H_
#define DATABASE_H_

#include <stdint.h>
typedef struct pager Pager;
typedef struct table Table;

#define MAX_TABLES 16  
#define DATABASE_PATH_MAX 256

/* Database struct contains:
 * name: name of the database file
 * pager: pager component that manages page disk I/O
 * tables: tables currently in database
 * table_count: amount of tables in database */
typedef struct database {
    char pathname[DATABASE_PATH_MAX];
    Pager *pager;
    Table **tables;
    uint32_t table_count;
} Database;


/* Opens existing database or initializes a new one 
    
   Persisted table metadata loading is deferred until System Catalog
   serialization and deserialization are implemented.
*/
extern Database *database_open(const char *pathname);

/* Flushes and closes the Pager, frees all loaded Tables and destroys the Database */
extern bool database_close(Database *db);

/* Destroys a partially or fully-initialized Database while ingnoring close-status reporting */
extern void database_free(Database *db);

#endif