/**
 * @file sql.h
 * @brief SQL related functions including a basic parser
 */

 #include "prefix.h"

 typedef struct {
     pre *name;
     pre *typename;
     pre *default_value;

     pre **generated_always_as;
     size_t generated_always_as_len;
 } column_def;

 column_def **column_defs_of_declrs(int argc, const char *const *argv, size_t *num_columns);
