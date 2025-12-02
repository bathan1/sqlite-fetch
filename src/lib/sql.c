#include "sql.h"
#include "cfns.h"
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

static void split_generated_path(
    const pre *expr,
    pre ***out_parts,
    size_t *out_count
);
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
        pre **tokens = split_on_ch(arg, ' ', &tokens_size);
        if (!tokens || tokens_size < 1) {
            return NULL;
        }

        // 0   1    2       3      4  5
        // id int default   0
        // id int generated always as ()
        for (int i = 1; i < MIN(5, tokens_size); i++) {
            if (str(tokens[i])[0] != '\'') {
                tokens[i] = lowercase_own(tokens[i]);
            }
        }

        const char *column_name = str(tokens[COL_NAME]);
        if (column_name[COL_NAME] == '\"') {
            if (column_name[len(tokens[COL_NAME]) - 1] != '\"') {
                fprintf(stderr, "Open dquote missing closing dquote in column name %s\n", column_name);
                for (int t = 0; t < tokens_size; t++) free(tokens[t]);
                free(tokens);
                return NULL;
            }
            tokens[COL_NAME] = rmch_own(tokens[COL_NAME], '\"');
            if (!tokens[COL_NAME]) {
                for (int t = 0; t < tokens_size; t++) free(tokens[t]);
                return perror_rc(NULL, "prefix_static_own", 0);
            }
        }

        if (tokens_size > 0 &&
            len(tokens[COL_NAME]) == 3 &&
            strncmp(str(tokens[COL_NAME]), "url", 3) == 0)
        {
            is_url_column = true;
        }

        if (tokens_size >= 3 &&
            len(tokens[COL_CST]) == 7 &&
                strncmp(str(tokens[COL_CST]), "default", 7) == 0)
        {
            if (tokens_size != 4) {
                if (tokens_size < 4) {
                    fprintf(stderr, "Default value missing for column %s\n", str(tokens[COL_NAME]));
                } else {
                    fprintf(stderr, "Too many arguments for default value of column %s\n", str(tokens[COL_NAME]));
                }
                for (int t = 0; t < tokens_size; t++) free(tokens[t]);
                free(tokens);
                return NULL;
            }
            has_default = true;
        }

        if (
            tokens_size >= 3 &&
            len(tokens[COL_CST]) == 9 &&
            strncmp(str(tokens[COL_CST]), "generated", 9) == 0
            &&
            len(tokens[COL_CST_VAL]) == 6 &&
            strncmp(str(tokens[COL_CST_VAL]), "always", 6) == 0
            &&
            len(tokens[COL_CST_VAL2]) == 2 &&
            strncmp(str(tokens[COL_CST_VAL2]), "as", 2) == 0
            &&
            len(tokens[COL_GEN_CST_VAL]) > 0
        ) {
            has_generated_value = true;
        }

        size_t i = is_url_column ? 0 : col_index;

        columns[i] = calloc(1, sizeof(column_def));
        if (!columns[i]) {
            for (int t = 0; t < tokens_size; t++) free(tokens[t]);
            free(tokens);
            return NULL;
        }

        columns[i]->name = tokens[COL_NAME];
        columns[i]->typename = tokens[COL_TYPE];
        if (has_default) {
            pre *normalized = rmch(tokens[COL_CST_VAL], '\'');
            free(tokens[COL_CST_VAL]); // don't need anymore
            columns[i]->default_value = normalized;
        }
        if (has_generated_value) {
            const char *expr_raw = str(tokens[COL_GEN_CST_VAL]);
            size_t expr_len = len(tokens[COL_GEN_CST_VAL]);
            if (expr_len >= 2 && expr_raw[0] == '(' && expr_raw[expr_len - 1] == ')') {
                expr_raw++;           // move start
                expr_len -= 2;        // remove both '(' and ')'
            }

            pre *expr = prefix_static(expr_raw, expr_len);
            char **parts = 0;
            size_t count = 0;

            split_generated_path(expr, &parts, &count);

            columns[i]->generated_always_as = parts;
            columns[i]->generated_always_as_len = count;

            free(expr);
        }

        // if we're at url, then we wrote to index 0, so we DON'T increment index counter
        col_index = is_url_column ? col_index : col_index + 1;
    }

    if (columns[0] == NULL) {
        columns[0] = calloc(1, sizeof(column_def));
        // then we write url col in ourselves
        columns[0]->name = prefix_static("url", 3);
        columns[0]->typename = prefix_static("text", 4);
    }
    if (columns[1] == NULL) {
        columns[1] = calloc(1, sizeof(column_def));
        columns[1]->name = prefix_static("headers", 7);
        columns[1]->typename = prefix_static("text", 4);
    }
    if (columns[2] == NULL) {
        columns[2] = calloc(1, sizeof(column_def));
        columns[2]->name = prefix_static("body", 4);
        columns[2]->typename = prefix_static("text", 4);
    }

    column_def **heap_columns = calloc(1, sizeof(column_def) * col_index);
    memcpy(heap_columns, columns, sizeof(column_def) * col_index);
    if (num_columns) *num_columns = col_index;
    return heap_columns;
}

static void split_generated_path(
    const pre *expr,
    pre ***out_parts,
    size_t *out_count
) {
    /* Length *excluding* prefix */
    size_t expr_len = len(expr);

    /* ------------------------------------------------------
       1. Count number of segments split by "->"
       ------------------------------------------------------ */
    size_t count = 1;
    for (size_t i = 0; i + 1 < expr_len; i++) {
        if (str(expr)[i] == '-' && str(expr)[i+1] == '>') {
            count++;
        }
    }

    /* Allocate array of pre* */
    pre **parts = malloc(sizeof(pre*) * count);
    if (!parts) {
        *out_parts = NULL;
        *out_count = 0;
        return;
    }

    /* ------------------------------------------------------
       2. Extract each segment into a NEW pre string
       ------------------------------------------------------ */
    size_t start = 0;
    size_t idx = 0;
    const char *s = str(expr); /* raw char* payload */

    for (size_t i = 0; i <= expr_len; i++) {

        bool at_arrow =
            (i + 1 < expr_len && s[i] == '-' && s[i+1] == '>');
        bool at_end = (i == expr_len);

        if (at_arrow || at_end) {

            size_t seg_len = i - start;

            /* --------------------------------------------
               2A. Copy substring into its own pre str
               -------------------------------------------- */
            pre *token = prefix_static(s + start, seg_len);
            if (!token) {
                /* cleanup partial results */
                for (size_t k = 0; k < idx; k++) free(parts[k]);
                free(parts);
                *out_parts = NULL;
                *out_count = 0;
                return;
            }

            /* --------------------------------------------
               2B. Clean it with remove_all()
               remove_all() always returns a NEW buffer.
               -------------------------------------------- */
            pre *clean1 = rmch(token, '\'');
            free(token);
            token = clean1;

            pre *clean2 = rmch(token, '\n');
            free(token);
            token = clean2;

            pre *clean3 = rmch(token, '\r');
            free(token);
            token = clean3;

            parts[idx++] = token;

            /* Advance past "->" */
            if (at_arrow) {
                i++;        /* skip '>' */
                start = i + 1;
            }
        }
    }

    *out_parts = parts;
    *out_count = count;
}
