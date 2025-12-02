#include "sql.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FETCH_ARGS_OFFSET 3

enum {
    // id
    COL_NAME = 0,
    // int
    COL_TYPE,
    // generated
    COL_CST,
    // always
    COL_CST_VAL,
    // as
    COL_CST_VAL2,
    // ([generated_val])
    COL_GEN_CST_VAL
};

column_def **column_defs_of_declrs(int argc, const char *const *argv,
                                   size_t *num_columns)
{
    column_def *columns[64 + 3] = {0};
    // columns always has [URL, HEADERS, BODY] at start
    size_t col_index = 3;

    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        bool is_url_column = false;
        bool has_default = false;
        bool has_generated_value = false;
        // argv[i] is one column definition
        const char *arg = argv[i];
        size_t tokens_size = 0;
        struct string *tokens = splitch(
            (struct string) {.hd=(char *) arg, .length=strlen(arg)},
            ' ',
            &tokens_size
        );
        if (!tokens || tokens_size < 1) {
            return NULL;
        }

        // 0   1    2       3      4  5
        // id int default   0
        // id int generated always as ()
        for (int i = 1; i < MIN(5, tokens_size); i++) {
            if (tokens[i].hd[0] != '\'') {
                if (lowercase(&tokens[i]) != 0) {
                    for (int i = 0; i < tokens_size; i++) free(tokens[i].hd);
                    free(tokens);
                    return NULL;
                }
            }
        }

        const char *column_name = tokens[COL_NAME].hd;
        if (column_name[COL_NAME] == '\"') {
            if (column_name[tokens[COL_NAME].length - 1] != '\"') {
                fprintf(stderr, "Open dquote missing closing dquote in column name %s\n", column_name);
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }
            if (rmch(&tokens[COL_NAME], '\"') != 0) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return perror_rc(NULL, "rmch", 0);
            }
        }

        if (tokens_size > 0 &&
            tokens[COL_NAME].length == 3 &&
            strncmp(tokens[COL_NAME].hd, "url", 3) == 0)
        {
            is_url_column = true;
        }

        if (tokens_size >= 3 &&
            tokens[COL_CST].length == 7 &&
                strncmp(tokens[COL_CST].hd, "default", 7) == 0)
        {
            if (tokens_size != 4) {
                if (tokens_size < 4) {
                    fprintf(stderr, "Default value missing for column %s\n", tokens[COL_NAME].hd);
                } else {
                    fprintf(stderr, "Too many arguments for default value of column %s\n", tokens[COL_NAME].hd);
                }
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }
            has_default = true;
        }

        if (
            tokens_size >= 3 &&
            tokens[COL_CST].length == 9 &&
            strncmp(tokens[COL_CST].hd, "generated", 9) == 0
            &&
            tokens[COL_CST_VAL].length == 6 &&
            strncmp(tokens[COL_CST_VAL].hd, "always", 6) == 0
            &&
            tokens[COL_CST_VAL2].length == 2 &&
            strncmp(tokens[COL_CST_VAL2].hd, "as", 2) == 0
            &&
            tokens[COL_GEN_CST_VAL].length > 0
        ) {
            has_generated_value = true;
        }

        size_t i = is_url_column ? 0 : col_index;

        columns[i] = calloc(1, sizeof(column_def));
        if (!columns[i]) {
            for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
            free(tokens);
            return NULL;
        }

        columns[i]->name = tokens[COL_NAME];
        columns[i]->typename = tokens[COL_TYPE];
        if (has_default) {
            if (rmch(&tokens[COL_CST_VAL], '\'') != 0) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }
            columns[i]->default_value = tokens[COL_CST_VAL];
        }
        if (has_generated_value) {
            char *expr_raw = tokens[COL_GEN_CST_VAL].hd;
            size_t expr_len = tokens[COL_GEN_CST_VAL].length;
            if (expr_len >= 2 && expr_raw[0] == '(' && expr_raw[expr_len - 1] == ')') {
                expr_raw++;           // move start
                expr_len -= 2;        // remove both '(' and ')'
            }
            struct string adjusted = {.hd=expr_raw, .length=expr_len};
            struct string arrow_pattern = {.hd = "->", .length = 2};

            size_t path_count = 0;
            struct string *paths = split(adjusted, arrow_pattern, &path_count);
            if (!paths) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t].hd);
                free(tokens);
                return NULL;
            }

            columns[i]->generated_always_as = paths;
            columns[i]->generated_always_as_len = path_count;
        }

        // if we're at url, then we wrote to index 0, so we DON'T increment index counter
        col_index = is_url_column ? col_index : col_index + 1;
    }

    if (columns[0] == NULL) {
        columns[0] = calloc(1, sizeof(column_def));
        // then we write url col in ourselves
        columns[0]->name = (struct string) {.hd = strndup("url", 3), .length = 3};
        columns[0]->typename = (struct string) {.hd = strndup("text", 4), .length = 4};
    }
    if (columns[1] == NULL) {
        columns[1] = calloc(1, sizeof(column_def));
        columns[1]->name = (struct string) {.hd = strndup("headers", 7), .length = 7};
        columns[1]->typename = (struct string) {.hd = strndup("text", 4), .length = 4};
    }
    if (columns[2] == NULL) {
        columns[2] = calloc(1, sizeof(column_def));
        columns[2]->name = (struct string) {.hd = strndup("body", 4), .length = 4};
        columns[2]->typename = (struct string) {.hd = strndup("text", 4), .length = 4};
    }

    column_def **heap_columns = calloc(1, sizeof(column_def) * col_index);
    memcpy(heap_columns, columns, sizeof(column_def) * col_index);
    if (num_columns) *num_columns = col_index;

    return heap_columns;
}
