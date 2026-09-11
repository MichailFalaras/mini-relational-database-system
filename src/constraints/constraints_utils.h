#include <stdbool.h>
#include <stdint.h>

/* Forward Declarations. */
typedef struct database Database;
typedef struct schema Schema;
typedef struct pager Pager;
typedef struct row Row;
typedef struct table Table;
typedef struct evaluation_context EvaluationContext;


extern uint32_t *copy_uint32_array(const uint32_t *source, uint32_t amount);

extern void constraint_shift_local_column_refs(Constraint *constraint, uint32_t index_threshold);

extern void constraint_shift_referenced_column_refs(Constraint *constraint, uint32_t index_threshold);

extern bool constraint_validate_column_refs(const Database *db, const Schema *schema, const Constraint *constraint);

extern bool constraint_column_refs_are_unique(const uint32_t *column_refs, uint32_t amount_columns);

extern bool constraint_validate_foreign_key(const Database *db, const Schema *local_schema, 
    const Constraint *constraint);

extern bool constraint_validate_row(Pager *pager, const Constraint *constraint, const Schema *schema,
    const Row *row, const EvaluationContext *context);

extern bool constraint_is_referenced_by_foreign_key(const Database *db, const Table *table, 
    const Constraint *constraint);