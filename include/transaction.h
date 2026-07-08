#ifndef TRANSACTION_H_
#define TRANSACTION_H_

#include <stdint.h>
#include "database.h"

typedef enum transaction_state {
    TRANSACTION_NONE,
    TRANSACTION_ACTIVE,
    TRANSACTION_COMMITTED,
    TRANSACTION_ABORTED
} TransactionState;

typedef struct transaction {
    uint64_t id;
    TransactionState state;
} Transaction;

extern Transaction *transaction_begin(Database *db);

extern bool *transaction_commit(Database *db);

extern bool *transaction_rollback(Database *db);

#endif