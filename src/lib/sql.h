/**
 * @file sql.h
 * @brief SQL related functions including a basic parser
 */

 #include "cfns.h"

 typedef struct {
     struct string name;
     struct string typename;
     struct string default_value;

     struct string *generated_always_as;
     size_t generated_always_as_len;
 } column_def;

 column_def **column_defs_of_declrs(int argc, const char *const *argv, size_t *num_columns);
