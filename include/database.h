#ifndef DATABASE_H_
#define DATABASE_H_

#include <stdint.h>
#include <stdbool.h>

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


/* Add table metadata to the Database. On success, ownership of table transfers to the Database.
   On failure, the caller retains ownership. */ 
extern bool database_add_table(Database *db, Table *table);

/* Check if database contains a Table */
extern bool database_has_table(Database *db, const char *table_name);

/* Check if database contains a Table, and return it */
extern Table *database_find_table(Database *db, const char *table_name);
    
/* Return a Table's registry index, or UINT32_MAX if not found. */
extern uint32_t database_find_table_index(Database *db, const char *table_name);

/* Remove and free table metadata owned by the Database. On success, existing pointers to the removed Table become invalid. */
extern bool database_remove_table(Database *db, const char *table_name);

#endif