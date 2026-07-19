#ifndef DATABASE_H_
#define DATABASE_H_

#include <stdint.h>
typedef struct pager Pager;
typedef struct table Table;

#define MAX_TABLES 16  

/* Database struct contains:
 * name: name of the database file
 * pager: pager component that manages page disk I/O
 * tables: tables currently in database
 * table_count: amount of tables in database */
typedef struct database {
    char name[64];
    Pager *pager;
    Table **tables;
    uint32_t table_count;
} Database;

#endif