#ifndef ROW_H_
#define ROW_H_

/* Row structure that contains:
 * is_deleted: tombstone flag
 * n_columns: amount of columns
 * columns: type-agnostic columns data array. */
typedef struct row {
    int is_deleted;
    int n_columns;
    void **columns;
} Row;

#endif