/**
 * @file sql.h
 * @brief SQL related functions including a basic parser
 */

 #pragma once

 #include "cfns.h"

 struct column_def {
     struct string name;
     struct string typename;
     struct string default_value;

     struct string *generated_always_as;
     size_t generated_always_as_len;
 };

 struct column_def **column_defs_of_declrs(int argc, const char *const *argv, size_t *num_columns);
