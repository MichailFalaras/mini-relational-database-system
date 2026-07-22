#include <stdbool.h>
#include <stdint.h>

/* Forward Declarations. */
typedef struct database Database;
typedef struct constraint Constraint;

extern uint32_t *copy_uint32_array(const uint32_t *source, uint32_t amount);

extern void constraint_shift_local_column_refs(Constraint *constraint, uint32_t index_threshold);

extern void constraint_shift_referenced_column_refs(Constraint *constraint, uint32_t index_threshold);

extern bool constraint_validate_column_refs(const Database *db, const Constraint *constraint, uint32_t num_columns);
