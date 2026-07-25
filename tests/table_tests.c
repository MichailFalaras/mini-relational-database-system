#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/table.h"
#include "../include/schema.h"
#include "../include/index.h"
#include "../include/constraints.h"
#include "../include/data_types.h"
#include "../include/database.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return 1;  \
    }


/* ---------- Column/Schema/Table/Database Creation Helpers ---------- */

static Column *create_test_column(char *name, DataType type) {
    Column *column = column_alloc(name, type, 0, 0);
    return column;   
}

static Schema *create_test_schema_with_constraints() {
    Column id = {.name = "id", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};
    Column email = {.name = "email", .type = TEXT, .non_null_rows = 0, .null_rows = 0};
    Column age = {.name = "age", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};

    Column *columns[] = {&id, &email, &age};

    uint32_t primary_columns[] = {0};
    uint32_t unique_columns[] = {1};

    Constraint *primary_key = constraint_create_primary_key("pk_users", primary_columns, 1);
    Constraint *unique_email = constraint_create_unique("uq_users_email", unique_columns, 1);

    if (!primary_key || !unique_email) {
        constraint_free(primary_key);
        constraint_free(unique_email);
        return NULL;
    }

    Constraint *constraints[] = { primary_key, unique_email };

    Schema *schema = schema_create(columns, constraints, 3, 2);

    constraint_free(primary_key);
    constraint_free(unique_email);

    return schema;
}

static Schema *create_test_schema_without_constraints() {
    Column id = {.name = "id", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};
    Column name = {.name = "name", .type = TEXT, .non_null_rows = 0, .null_rows = 0};

    Column *columns[] = {&id, &name};

    return schema_create(columns, NULL, 2, 0);
}

static Table *create_test_table() {
    Schema *input_schema = create_test_schema_with_constraints();
    Table *table = table_metadata_create("users", input_schema);
    return table;
}

static Database *create_test_database(Table *table) {
    Database *db = (Database *) calloc(1, sizeof(Database));
    if (!db) {
        return NULL;
    }

    db->table_count = 1;

    db->tables = malloc(sizeof(Table *));
    if (!db->tables) {
        return NULL;
    }

    db->tables[0] = table;
    return db;
}


/* ---------- table_metadata_create unit tests ---------- */

static int test_table_metadata_create() {
    Schema *input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    Table *table = table_metadata_create("users", input_schema);
    
    ASSERT(table != NULL);
    ASSERT(strcmp(table->name, "users") == 0);

    ASSERT(table->table_schema != NULL);
    ASSERT(table->table_schema != input_schema);
    ASSERT(table->table_schema->num_columns == 3);
    ASSERT(table->table_schema->num_constraints == 2);

    ASSERT(table->row_count == 0);
    ASSERT(table->is_deleted == false);

    ASSERT(table->primary_index != NULL);
    ASSERT(strcmp(table->primary_index->name, "pk_users") == 0);
    ASSERT(table->primary_index->type == PRIMARY_INDEX);
    ASSERT(table->primary_index->root_page_num == INVALID_ROOT_PAGE);

    ASSERT(table->primary_index->key != NULL);
    ASSERT(table->primary_index->key->num_columns == 1);
    ASSERT(table->primary_index->key->column_index_array[0] == 0);

    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes[0] != NULL);

    ASSERT(strcmp(table->secondary_indexes[0]->name, "uq_users_email") == 0);
    ASSERT(table->secondary_indexes[0]->type == SECONDARY_INDEX);
    ASSERT(table->secondary_indexes[0]->root_page_num == INVALID_ROOT_PAGE);

    ASSERT(table->secondary_indexes[0]->key != NULL);
    ASSERT(table->secondary_indexes[0]->key->num_columns == 1);
    ASSERT(table->secondary_indexes[0]->key->column_index_array[0] == 1);

    for (uint32_t i = 1; i < MAX_INDEXES; i++) {
        ASSERT(table->secondary_indexes[i] == NULL);
    }

    table_free(table);
    schema_free(input_schema);
    return 0;
}

static int test_table_metadata_create_without_constraints() {
    Schema *input_schema = create_test_schema_without_constraints();
    ASSERT(input_schema != NULL);

    Table *table = table_metadata_create("plain_table", input_schema);
    
    ASSERT(table != NULL);
    ASSERT(strcmp(table->name, "plain_table") == 0);

    ASSERT(table->table_schema != NULL);
    ASSERT(table->table_schema != input_schema);
    ASSERT(table->table_schema->num_columns == 2);
    ASSERT(table->table_schema->num_constraints == 0);

    ASSERT(table->primary_index == NULL);
    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->total_secondary_indexes == 0);

    for (uint32_t i = 0; i < MAX_INDEXES; i++) {
        ASSERT(table->secondary_indexes[i] == NULL);
    }

    table_free(table);
    schema_free(input_schema);
    return 0;
}

static int test_table_metadata_create_deep_copy() {
    Schema *input_schema = create_test_schema_with_constraints();
    ASSERT(input_schema != NULL);

    Table *table = table_metadata_create("users", input_schema);
    ASSERT(table != NULL);

    // Validate that the input schema is deep copied, meaning
    // the new table has a separate copy of the schema in different memory addresses
    ASSERT(table->table_schema != input_schema);
    ASSERT(table->table_schema->columns != input_schema->columns);
    ASSERT(table->table_schema->constraints != input_schema->constraints);

    for (uint32_t i = 0; i < input_schema->num_columns; i++) {
        ASSERT(table->table_schema->columns[i] != input_schema->columns[i]);
    }

    for (uint32_t i = 0; i < input_schema->num_constraints; i++) {
        ASSERT(table->table_schema->constraints[i] != input_schema->constraints[i]);
    }

    ASSERT(table->table_schema->constraints[0]->constraint_data.primary_key.primary_key_columns !=
           input_schema->constraints[0]->constraint_data.primary_key.primary_key_columns);

    ASSERT(table->table_schema->constraints[1]->constraint_data.unique_cols.column_refs !=
           input_schema->constraints[1]->constraint_data.unique_cols.column_refs);

    
    // Validating that a change in the input schema doesn't affect the table's schema
    strcpy(input_schema->columns[0]->name, "changed_id");

    input_schema->constraints[0]->constraint_data.primary_key.primary_key_columns[0] = 2;

    ASSERT(strcmp(table->table_schema->columns[0]->name, "id") == 0);

    ASSERT(table->table_schema->constraints[0]->constraint_data.primary_key.primary_key_columns[0] == 0);
    ASSERT(table->primary_index->key->column_index_array[0] == 0);

    table_free(table);
    schema_free(input_schema);
    return 0;
}

static int test_table_metadata_create_invalid_name() {
    Schema *schema = create_test_schema_without_constraints();
    ASSERT(schema != NULL);

    ASSERT(!table_metadata_create(NULL, schema));
    ASSERT(!table_metadata_create("", schema));

    // Name exceeds length limit
    char long_name[65];
    memset(long_name, 'a', 64);
    long_name[64] = '\0';

    ASSERT(!table_metadata_create(long_name, schema));

    schema_free(schema);
    return 0;
}

static int test_table_metadata_create_null_schema() {
    ASSERT(!table_metadata_create("users", NULL));

    return 0;
}

/* ---------- table_has_column unit tests ---------- */

static int test_table_has_column_existing() {
    Table *table = create_test_table();

    ASSERT(table != NULL);
    ASSERT(table_has_column(table, "id"));
    ASSERT(table_has_column(table, "email"));

    table_free(table);
    return 0;
}

static int test_table_has_column_missing() {
    Table *table = create_test_table();

    ASSERT(table != NULL);
    ASSERT(!table_has_column(table, "unknown"));

    table_free(table);
    return 0;
}

static int test_table_has_column_invalid_input() {
    Table *table = create_test_table();

    ASSERT(table != NULL);

    // Invalid table or column
    ASSERT(!table_has_column(NULL, "unknown"));
    ASSERT(!table_has_column(table, NULL));
    ASSERT(!table_has_column(table, ""));

    // Invalid schema
    Schema *schema = table->table_schema;
    table->table_schema = NULL;

    ASSERT(!table_has_column(table, "id"));

    table->table_schema = schema;
    table_free(table);
    return 0;
}

/* ---------- table_find_column unit tests ---------- */

static int test_table_find_column_existing() {
    Table *table = create_test_table();

    Column *column = table_find_column(table, "email");

    ASSERT(column != NULL);
    ASSERT(strcmp(column->name, "email") == 0);
    ASSERT(column == table->table_schema->columns[1]);

    table_free(table);
    return 0;
}

static int test_table_find_column_missing() {
    Table *table = create_test_table();

    ASSERT(!table_find_column(table, NULL));

    table_free(table);
    return 0;
}

static int test_table_find_column_invalid_input() {
    Table *table = create_test_table();

    // Invalid table or column
    ASSERT(!table_find_column(NULL, "id"));
    ASSERT(!table_find_column(table, NULL));
    ASSERT(!table_find_column(table, ""));

    // Invalid schema
    Schema *schema = table->table_schema;
    table->table_schema = NULL;

    ASSERT(table_find_column(table, "id") == NULL);

    table->table_schema = schema;
    table_free(table);
    return 0;
}

/* ---------- table_find_index unit tests ---------- */

static int test_table_find_primary_index() {
    Table *table = create_test_table();

    ASSERT(table != NULL);
    ASSERT(table->primary_index != NULL);

    Index *index = table_find_index(table, table->primary_index->name);

    ASSERT(index != NULL);
    ASSERT(index == table->primary_index);

    table_free(table);
    return 0;
}

static int test_table_find_secondary_index() {
    Table *table = create_test_table();

    ASSERT(table != NULL);
    ASSERT(table->total_secondary_indexes > 0);
    ASSERT(table->secondary_indexes[0] != NULL);

    Index *index = table_find_index(table, table->secondary_indexes[0]->name);

    ASSERT(index != NULL);
    ASSERT(index == table->secondary_indexes[0]);
    
    table_free(table);
    return 0;
}

static int test_table_find_index_missing() {
    Table *table = create_test_table();

    ASSERT(!table_find_index(table, "missing_index"));

    table_free(table);
    return 0;
}

static int test_table_find_index_null_secondary_entry() {
    Table *table = create_test_table();

    table->total_secondary_indexes = 1;
    table->secondary_indexes[0] = NULL;

    ASSERT(table_find_index(table, "missing") == NULL);

    table_free(table);
    return 0;
}

static int test_table_find_index_invalid_input() {
    Table *table = create_test_table();

    // Invalid table or index name
    ASSERT(!table_find_index(NULL, "idx"));
    ASSERT(!table_find_index(table, NULL));
    ASSERT(!table_find_index(table, ""));

    table_free(table);
    return 0;
}

/* ---------- table_alter_rename unit tests ---------- */

static int test_table_alter_rename() {
    Table *table = create_test_table();

    ASSERT(table_alter_rename(table, "customers"));
    ASSERT(strcmp(table->name, "customers") == 0);

    table_free(table);
    return 0;
}

static int test_table_alter_rename_invalid_name() {
    Table *table = create_test_table();

    char original_name[sizeof(table->name)];
    strcpy(original_name, table->name);

    ASSERT(!table_alter_rename(table, NULL));
    ASSERT(strcmp(table->name, original_name) == 0);

    ASSERT(!table_alter_rename(table, ""));
    ASSERT(strcmp(table->name, original_name) == 0);

    table_free(table);
    return 0;
}

static int test_table_alter_rename_oversized_name() {
    Table *table = create_test_table();

    char original_name[sizeof(table->name)];
    strcpy(original_name, table->name);

    // Oversised name
    char long_name[sizeof(table->name) + 1];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    ASSERT(!table_alter_rename(table, long_name));
    ASSERT(strcmp(table->name, original_name) == 0);

    table_free(table);
    return 0;
}

/* ---------- table_alter_add_col unit tests ---------- */

static int test_table_alter_add_col() {
    Table *table = create_test_table();
    uint32_t original_num_columns = table->table_schema->num_columns;

    Column *new_column = create_test_column("phone", TEXT);
    ASSERT(new_column != NULL);

    ASSERT(table_alter_add_col(table, new_column));

    ASSERT(table->table_schema->num_columns == original_num_columns + 1);

    Column *stored_column = table_find_column(table, "phone");

    ASSERT(stored_column != NULL);
    ASSERT(strcmp(stored_column->name, "phone") == 0);
    ASSERT(stored_column->type == TEXT);

    free(new_column);
    table_free(table);
    return 0;
}

static int test_table_alter_add_col_deep_copy() {
    Table *table = create_test_table();
    ASSERT(table != NULL);

    Column *new_column = create_test_column("phone", TEXT);
    ASSERT(new_column != NULL);

    ASSERT(table_alter_add_col(table, new_column));

    Column *stored_column = table_find_column(table, "phone");

    ASSERT(stored_column != NULL);
    ASSERT(stored_column != new_column);

    strcpy(new_column->name, "changed");
    new_column->type = INTEGER;
    new_column->non_null_rows = 100;
    new_column->null_rows = 50;

    ASSERT(strcmp(stored_column->name, "phone") == 0);
    ASSERT(stored_column->type == TEXT);
    ASSERT(stored_column->non_null_rows == 0);
    ASSERT(stored_column->null_rows == 0);

    free(new_column);
    table_free(table);
    return 0;
}

static int test_table_alter_add_duplicate_col() {
    Table *table = create_test_table();
    uint32_t original_num_columns = table->table_schema->num_columns;

    Column *duplicate = create_test_column("id", INTEGER);
    ASSERT(duplicate != NULL);

    ASSERT(!table_alter_add_col(table, duplicate));

    ASSERT(table->table_schema->num_columns == original_num_columns);

    ASSERT(table_find_column(table, "id"));

    free(duplicate);
    table_free(table);
    return 0;
}

static int test_table_alter_add_col_invalid_input() {
    Table *table = create_test_table();

    Column *column = create_test_column("phone", INTEGER);
    ASSERT(column != NULL);

    ASSERT(!table_alter_add_col(NULL, column));
    ASSERT(!table_alter_add_col(table, NULL));

    Schema *saved_schema = table->table_schema;
    table->table_schema = NULL;

    ASSERT(!table_alter_add_col(table, column));

    table->table_schema = saved_schema;

    free(column);
    table_free(table);
    return 0;
}

/* ---------- table_alter_drop_col unit tests ---------- */

static int test_table_alter_drop_col() {
    Table *table = create_test_table();
    ASSERT(table != NULL);

    Database *db = create_test_database(table);
    ASSERT(db != NULL);

    uint32_t original_num_columns = table->table_schema->num_columns;

    // Drop non-primary & non-secondary index column
    ASSERT(table_has_column(table, "age"));
    ASSERT(table_alter_drop_col(table, db, "age"));
    ASSERT(table->table_schema->num_columns == original_num_columns - 1);

    ASSERT(!table_has_column(table, "age"));
    ASSERT(table_has_column(table, "id"));
    ASSERT(table_has_column(table, "email"));

    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

static int test_table_alter_drop_primary_index_col() {
    Table *table = create_test_table();
    ASSERT(table != NULL);

    Database *db = create_test_database(table);
    ASSERT(db != NULL);

    uint32_t original_num_columns = table->table_schema->num_columns;
    uint32_t original_key_column = table->primary_index->key->column_index_array[0];

    // Fails to drop primary key column
    ASSERT(!table_alter_drop_col(table, db, "id"));
    ASSERT(table->table_schema->num_columns == original_num_columns);
    ASSERT(table_has_column(table, "id"));
    ASSERT(table->primary_index->key->column_index_array[0] == original_key_column);
    
    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

static int test_table_alter_drop_secondary_index_col() {
    Table *table = create_test_table();
    ASSERT(table != NULL);

    Database *db = create_test_database(table);
    ASSERT(db != NULL);

    uint32_t original_num_columns = table->table_schema->num_columns;
    uint32_t original_key_column = table->secondary_indexes[0]->key->column_index_array[0];

    // Fails to drop secondary key column
    ASSERT(!table_alter_drop_col(table, db, "email"));
    ASSERT(table->table_schema->num_columns == original_num_columns);
    ASSERT(table_has_column(table, "email"));
    ASSERT(table->secondary_indexes[0]->key->column_index_array[0] == original_key_column);

    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

static int test_table_alter_drop_col_shifts_indexes() {
    // Using a custom schema and not the helper schema creation function
    Column temporary = {.name = "temporary", .type = TEXT, .non_null_rows = 0, .null_rows = 0};
    Column id = {.name = "id", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};
    Column email = {.name = "email", .type = TEXT, .non_null_rows = 0, .null_rows = 0};
    Column age = {.name = "age", .type = INTEGER, .non_null_rows = 0, .null_rows = 0};
    
    Column *columns[] = {&temporary, &id, &email, &age};

    //Index positions in the original four-column schema.
    uint32_t primary_columns[] = {1};
    uint32_t unique_columns[] = {2};

    Constraint *primary_key = constraint_create_primary_key("pk_temp_users", primary_columns, 1);
    Constraint *unique_email = constraint_create_unique("uq_temp_users_email", unique_columns, 1);

    ASSERT(primary_key != NULL);
    ASSERT(unique_email != NULL);

    Constraint *constraints[] = {primary_key, unique_email};
    Schema *input_schema = schema_create(columns,constraints,4,2);

    // Deep-copied constraints, the local copies can be freed
    constraint_free(primary_key);
    constraint_free(unique_email);

    ASSERT(input_schema != NULL);

    Table *table = table_metadata_create("temporary_users",input_schema);
    ASSERT(table != NULL);

    // Deep-copied schema, the local copy can be freed
    schema_free(input_schema);

    Database *db = create_test_database(table);
    ASSERT(db != NULL);

    ASSERT(table->table_schema->num_columns == 4);
    ASSERT(strcmp(table->table_schema->columns[0]->name, "temporary") == 0);
    ASSERT(strcmp(table->table_schema->columns[1]->name, "id") == 0);
    ASSERT(strcmp(table->table_schema->columns[2]->name, "email") == 0);
    ASSERT(strcmp(table->table_schema->columns[3]->name, "age") == 0);

    // Validate the initial index positions. 
    ASSERT(table->primary_index != NULL);
    ASSERT(table->primary_index->key != NULL);
    ASSERT(table->primary_index->key->num_columns == 1);
    ASSERT(table->primary_index->key->column_index_array[0] == 1);

    ASSERT(table->total_secondary_indexes == 1);
    ASSERT(table->secondary_indexes != NULL);
    ASSERT(table->secondary_indexes[0] != NULL);
    ASSERT(table->secondary_indexes[0]->key != NULL);
    ASSERT(table->secondary_indexes[0]->key->num_columns == 1);
    ASSERT(
        table->secondary_indexes[0]->key->column_index_array[0] == 2
    );

    // Drop the unindexed leading column.
    ASSERT(table_alter_drop_col(table, db, "temporary"));

    // Validate the updated schema. 
    ASSERT(table->table_schema->num_columns == 3);
    ASSERT(!table_has_column(table, "temporary"));

    ASSERT(strcmp(table->table_schema->columns[0]->name, "id") == 0);
    ASSERT(strcmp(table->table_schema->columns[1]->name, "email") == 0);
    ASSERT(strcmp(table->table_schema->columns[2]->name, "age") == 0);

    // Both index references must shift down by one because their
    // columns originally appeared after the dropped column.
    ASSERT(table->primary_index->key->column_index_array[0] == 0);
    ASSERT(table->secondary_indexes[0]->key->column_index_array[0] == 1);

    // The schema constraints should also have shifted because
    // schema_drop_column() calls shift_column_refs_after_drop().
    ASSERT(table->table_schema->num_constraints == 2);
    ASSERT(table->table_schema->constraints[0]->constraint_data.primary_key.primary_key_columns[0] == 0);
    ASSERT(table->table_schema->constraints[1]->constraint_data.unique_cols.column_refs[0] == 1);

    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

/* ---------- table_alter_rename_col unit tests ---------- */

static int test_table_alter_rename_col() {
    Table *table = create_test_table();
    
    int32_t original_position = schema_find_column_index(table->table_schema, "age");
    ASSERT(original_position >= 0);

    ASSERT(table_alter_rename_col(table, "age", "years"));
    ASSERT(!table_has_column(table, "age"));
    ASSERT(table_has_column(table, "years"));

    ASSERT(schema_find_column_index(table->table_schema, "years") == original_position);

    table_free(table);
    return 0;
}

static int test_table_alter_rename_col_preserves_index() {
    Table *table = create_test_table();

    uint32_t original_key_position = table->secondary_indexes[0]->key->column_index_array[0];

    ASSERT(table_alter_rename_col(table, "email", "contact_email"));

    ASSERT(!table_has_column(table, "email"));
    ASSERT(table_has_column(table, "contact_email"));

    ASSERT(table->secondary_indexes[0]->key->column_index_array[0] == original_key_position);

    table_free(table);
    return 0;
}

static int test_table_alter_rename_col_duplicate() {
    Table *table = create_test_table();

    uint32_t original_num_columns = table->table_schema->num_columns;
    ASSERT(!table_alter_rename_col(table, "age", "email"));

    ASSERT(table->table_schema->num_columns == original_num_columns);

    ASSERT(table_has_column(table, "age"));
    ASSERT(table_has_column(table, "email"));

    table_free(table);
    return 0;
}

static int test_table_alter_rename_col_missing() {
    Table *table = create_test_table();

    ASSERT(!table_alter_rename_col(table, "missing", "renamed"));

    ASSERT(!table_has_column(table, "renamed"));

    table_free(table);
    return 0;
}

static int test_table_alter_rename_col_oversized() {
    Table *table = create_test_table();

    // New oversized column name
    char oversized_name[sizeof(((Column *) 0)->name) + 1];
    memset(oversized_name,'a', sizeof(oversized_name) - 1);
    oversized_name[sizeof(oversized_name) - 1] = '\0';

    ASSERT(!table_alter_rename_col(table, "age", oversized_name));

    ASSERT(table_has_column(table, "age"));

    table_free(table);
    return 0;
}

static int test_table_alter_rename_col_invalid_input() {
    Table *table = create_test_table();

    // Invalid table, old column name, and new column name
    ASSERT(!table_alter_rename_col(NULL, "age", "years"));
    ASSERT(!table_alter_rename_col(table, NULL, "years"));
    ASSERT(!table_alter_rename_col(table, "", "years"));
    ASSERT(!table_alter_rename_col(table, "age", NULL));
    ASSERT(!table_alter_rename_col(table, "age", ""));

    // Invalid schema
    Schema *saved_schema = table->table_schema;
    table->table_schema = NULL;

    ASSERT(!table_alter_rename_col(table, "age", "years"));

    table->table_schema = saved_schema;

    table_free(table);
    return 0;
}

/* ---------- table_alter_modify_col unit tests ---------- */

static int test_table_alter_modify_col() {
    Table *table = create_test_table();
    Database *db = create_test_database(table);

    Column *replacement = create_test_column("age", INTEGER);

    ASSERT(replacement != NULL);
    ASSERT(table_alter_modify_col(table, db, "age", replacement));

    Column *stored = table_find_column(table, "age");

    ASSERT(stored != NULL);
    ASSERT(stored->type == INTEGER);

    free(replacement);
    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

static int test_table_alter_modify_col_deep_copy() {
    Table *table = create_test_table();
    Database *db = create_test_database(table);

    Column *replacement = create_test_column("age", INTEGER);

    ASSERT(table_alter_modify_col(table, db, "age", replacement));

    Column *stored = table_find_column(table, "age");

    ASSERT(stored != NULL);

    // Validating the new column was deep-copied, by checking the pointer addresses
    // and changing the data type in the replacement local copy
    // but the data type of the replaced new column isn't changed
    ASSERT(stored != replacement);
    replacement->type = TEXT;

    ASSERT(stored->type == INTEGER);

    free(replacement);
    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

static int test_table_alter_modify_col_missing() {
    Table *table = create_test_table();
    Database *db = create_test_database(table);
    
    uint32_t original_count = table->table_schema->num_columns;

    Column *replacement = create_test_column("missing", INTEGER);

    ASSERT(!table_alter_modify_col(table, db, "missing", replacement));

    // The original columns are retained when a column failed to be modified
    ASSERT(table->table_schema->num_columns == original_count);
    ASSERT(table_has_column(table, "id"));
    ASSERT(table_has_column(table, "email"));
    ASSERT(table_has_column(table, "age"));

    free(replacement);
    table_free(table);
    free(db->tables);
    free(db);
    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}


int main(int argc, char *argv[]) {
    int result;

    /* ---------- table_metadata_create unit tests ---------- */
    result = test_table_metadata_create();
    generate_output(result, 0, "test_table_metadata_create");
    result = test_table_metadata_create_without_constraints();
    generate_output(result, 1, "test_table_metadata_create_without_constraints");
    result = test_table_metadata_create_deep_copy();
    generate_output(result, 2, "test_table_metadata_create_deep_copy");
    result = test_table_metadata_create_invalid_name();
    generate_output(result, 3, "test_table_metadata_create_invalid_name");
    result = test_table_metadata_create_null_schema();
    generate_output(result, 4, "test_table_metadata_create_null_schema");

    /* ---------- table_has_column unit tests ---------- */
    result = test_table_has_column_existing();
    generate_output(result, 5, "test_table_has_column_existing");
    result = test_table_has_column_missing();
    generate_output(result, 6, "test_table_has_column_missing");
    result = test_table_has_column_invalid_input();
    generate_output(result, 7, "test_table_has_column_invalid_input");

    /* ---------- table_find_column unit tests ---------- */
    result = test_table_find_column_existing();
    generate_output(result, 8, "test_table_find_column_existing");
    result = test_table_find_column_missing();
    generate_output(result, 9, "test_table_find_column_missing");
    result = test_table_find_column_invalid_input();
    generate_output(result, 10, "test_table_find_column_invalid_input");

    /* ---------- table_find_index unit tests ---------- */
    result = test_table_find_primary_index();
    generate_output(result, 11, "test_table_find_primary_index");
    result = test_table_find_secondary_index();
    generate_output(result, 12, "test_table_find_secondary_index");
    result = test_table_find_index_missing();
    generate_output(result, 13, "test_table_find_index_missing");
    result = test_table_find_index_null_secondary_entry();
    generate_output(result, 14, "test_table_find_index_null_secondary_entry");
    result = test_table_find_index_invalid_input();
    generate_output(result, 15, "test_table_find_index_invalid_input");

    /* ---------- table_alter_rename unit tests ---------- */
    result = test_table_alter_rename();
    generate_output(result, 16, "test_table_alter_rename");
    result = test_table_alter_rename_invalid_name();
    generate_output(result, 17, "test_table_alter_rename_invalid_name");
    result = test_table_alter_rename_oversized_name();
    generate_output(result, 18, "test_table_alter_rename_oversized_name");

    /* ---------- table_alter_add_col unit tests ---------- */
    result = test_table_alter_add_col();
    generate_output(result, 19, "test_table_alter_add_col");
    result = test_table_alter_add_col_deep_copy();
    generate_output(result, 20, "test_table_alter_add_col_deep_copy");
    result = test_table_alter_add_duplicate_col();
    generate_output(result, 21, "test_table_alter_add_duplicate_col");
    result = test_table_alter_add_col_invalid_input();
    generate_output(result, 22, "test_table_alter_add_col_invalid_input");

    /* ---------- table_alter_drop_col unit tests ---------- */
    result = test_table_alter_drop_col();
    generate_output(result, 23, "test_table_alter_drop_col");
    result = test_table_alter_drop_primary_index_col();
    generate_output(result, 24, "test_table_alter_drop_primary_index_col");
    result = test_table_alter_drop_secondary_index_col();
    generate_output(result, 25, "test_table_alter_drop_secondary_index_col");
    result = test_table_alter_drop_col_shifts_indexes();
    generate_output(result, 26, "test_table_alter_drop_col_shifts_indexes");

    /* ---------- table_alter_rename_col unit tests ---------- */
    result = test_table_alter_rename_col();
    generate_output(result, 27, "test_table_alter_rename_col");
    result = test_table_alter_rename_col_preserves_index();
    generate_output(result, 28, "test_table_alter_rename_col_preserves_index");
    result = test_table_alter_rename_col_duplicate();
    generate_output(result, 29, "test_table_alter_rename_col_duplicate");
    result = test_table_alter_rename_col_missing();
    generate_output(result, 30, "test_table_alter_rename_col_missing");
    result = test_table_alter_rename_col_oversized();
    generate_output(result, 31, "test_table_alter_rename_col_oversized");
    result = test_table_alter_rename_col_invalid_input();
    generate_output(result, 32, "test_table_alter_rename_col_invalid_input");

    /* ---------- table_alter_modify_col unit tests ---------- */
    result = test_table_alter_modify_col();
    generate_output(result, 33, "test_table_alter_modify_col");
    result = test_table_alter_modify_col_deep_copy();
    generate_output(result, 34, "test_table_alter_modify_col_deep_copy");
    result = test_table_alter_modify_col_missing();
    generate_output(result, 35, "test_table_alter_modify_col_missing");
    
    return 0;
}
