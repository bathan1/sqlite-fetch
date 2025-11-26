// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include <sqlite3ext.h>
#include <unistd.h>
SQLITE_EXTENSION_INIT1

// #define NDEBUG
#include <assert.h>

#include <yyjson.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <yajl/yajl_parse.h>

/* utils */
#include "fetch.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

// arbitrary
#define MAX_COLUMN_COUNT 128

#define FETCH_ARGS_OFFSET 3

#define FETCH_URL 0
#define FETCH_BODY 1
#define FETCH_HEADERS 2

/* For debug logs */
#ifdef NDEBUG
  #define println(...) do { } while (0)
#else
  #define println(...)                     \
    do {                                     \
      printf("fetch-dbg> " __VA_ARGS__);           \
        printf("\n");\
    } while (0)
#endif

static void debug_print_json(const char *label, yyjson_val *val) {
#ifndef NDEBUG
    char *s = yyjson_val_write(val, YYJSON_WRITE_PRETTY_TWO_SPACES, NULL);
    if (s) {
        println("%s: %s", label, s);
        free(s);
    } else {
        println("%s: <null or non-serializable>", label);
    }
#endif
}

/**
 * Takes out double quoted column names.
 *
 * @b Example
 * @code
 *     char *userId = "\"userId\"";
 *     normalize_column_name(userId, strlen(userId));
 *     bool is_stripped = strncmp(userId, "userId", 6) == 0;
 *     printf("%s\n", is_stripped ? "yup" : "nah");
 * @endcode
 */
void normalize_column_name(char *str, int *len) {
    if (str[0] == '"') {
        *len -= 2;
        memmove(str, str + 1, *len);
        str[*len] = 0;
    }
}

typedef struct {
    int status;
    const char *text;
} status_entry_t;

// map status int -> status text
static const status_entry_t STATUS_TABLE[] = {
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {204, "No Content"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {500, "Internal Server Error"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
};
#define STATUS_TABLE_LEN sizeof(STATUS_TABLE) / sizeof(STATUS_TABLE[0])

// lookup status text from static `STATUS_TABLE` from a status code
static const char *fetch_status_text(int status) {
    for (size_t i = 0; i < STATUS_TABLE_LEN; ++i) {
        if (STATUS_TABLE[i].status == status)
            return STATUS_TABLE[i].text;
    }
    return "Unknown Status";
}

typedef struct {
    bool is_hidden;

    size_t generated_always_as_size;
    size_t *generated_always_as_len;
    char **generated_always_as;

    size_t name_len;
    char *name;

    size_t typename_len;
    char *typename;

    size_t json_typename_len;
    char *json_typename;
} column_def;

/**
 * The SQLite virtual table
 */
typedef struct {
    /**
     * For SQLite to fill in
     */
    sqlite3_vtab base;

    /**
     * The columns for this row
     */
    column_def **columns;

    /** 
     * Number of COLUMNS entries
     * */
    unsigned long columns_len;

    size_t default_url_len;

    /**
     * A default url if one was set
     */
    char *default_url;

    /**
     * Resolved schema string. 
     */
    char *schema;

    /**
     * Parent json pointer.
     */
    yyjson_doc *payload;

    /**
     * How big is PAYLOAD
     */
    size_t payload_len;
} Fetch;

/// Cursor
typedef struct fetch_cursor {
    sqlite3_vtab_cursor base;

    yyjson_doc   **queue;
    size_t        queue_cap;
    size_t        queue_head;
    size_t        queue_tail;
    size_t        queue_count;

    int sockfd;
    int eof;

    yajl_handle yajl_handle;

    // Temporary builder state for current JSON object
    yyjson_mut_doc *builder;
    char *current_key;

    // Completed row (a fully constructed immutable doc)
    yyjson_doc *next_doc;

    size_t count;
    size_t json_count;
    size_t depth;
    yyjson_mut_val *obj_stack[64];
} fetch_cursor_t;
static void queue_push(fetch_cursor_t *cur, yyjson_doc *doc);
static yyjson_doc *queue_pop(fetch_cursor_t *cur);

#define X_UPDATE_OFFSET 2

static bool is_key_url(char *key) {
    return strncmp("url", key, 3) == 0;
}

static bool is_key_body(char *key) {
    return strncmp("body", key, 4) == 0;
}

// For tokens "fetch" (module name), "main" (schema), "patients" (vtable name), and
// at least 1 argument for the url argument
#define MIN_ARGC 4

// "fetch", "{schema}", "{vtable_name}", "{?url}", "{?body}", 
// "{?headers}", ... optional static column declarations
#define MAX_FETCH_ARGC 6

/**
 * Run a GET request to URL over tcp sockets using curl. Size of body
 * written out to BODY_SIZE.
 */
static buffer *GET(const char *url) {
    int fd = fetch(url);
    if (fd < 0) {
        perror("fetch");
        return NULL;
    }

    size_t cap = 4096;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) {
        close(fd);
        return NULL;
    }

    char buf[4096];
    ssize_t n;

    while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        if (len + n + 1 > cap) {
            size_t newcap = cap * 2;
            while (newcap < len + n + 1)
                newcap *= 2;

            char *tmp = realloc(out, newcap);
            if (!tmp) {
                free(out);
                close(fd);
                return NULL;
            }
            out = tmp;
            cap = newcap;
        }

        memcpy(out + len, buf, n);
        len += n;
    }

    if (n < 0) {
        perror("recv");
        free(out);
        close(fd);
        return NULL;
    }

    out[len] = '\0';
    close(fd);

    buffer *encoded = append(&out, len);
    return encoded;
}

static int index_of_key(char *key) {
    if (is_key_url(key)) {
        return 0;
    } else if (is_key_body(key)) {
        return 1;
    } else {
        // headers
        return 2;
    }
}

static yyjson_doc *array_wrap(yyjson_val *val) {
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(mdoc);
    yyjson_mut_doc_set_root(mdoc, arr);

    yyjson_mut_val *copied = yyjson_val_mut_copy(mdoc, val);
    yyjson_mut_arr_add_val(arr, copied);
    yyjson_doc *im_doc = yyjson_mut_doc_imut_copy(mdoc, NULL);
    yyjson_mut_doc_free(mdoc);
    return im_doc;
}

// id int [default] [default_val]
// id int [generated] [always] [as] ([generated_val])
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

column_def **init_columns(int argc, const char *const *argv) {

    bool has_url_column = false;

    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        // argv[i] is one column definition
        const char *arg = argv[i];
        int tokens_size = 0;
        int token_len[8];
        char **tokens = split(arg, ' ', &tokens_size, token_len);

        if (tokens_size > 0 &&
            token_len[COL_NAME] == 3 &&
            strncmp(tokens[COL_NAME], "url", 3) == 0) {
            has_url_column = true;
        }

        for (int t = 0; t < tokens_size; t++) free(tokens[t]);
        free(tokens);
    }

    int num_columns = (argc - FETCH_ARGS_OFFSET)
                      + (!has_url_column ? 1 : 0);

    column_def **columns = malloc(sizeof(column_def*) * num_columns);

    // index 0 is always url
    column_def *url = calloc(1, sizeof(column_def));
    url->is_hidden = true;
    url->name = strdup("url");
    url->name_len = 3;
    url->typename = strdup("text");
    url->typename_len = 4;
    columns[0] = url;

    return columns;
}

static void split_generated_path(
    const char *expr,
    size_t expr_len,
    char ***out_parts,
    size_t **out_lens,
    size_t *out_count
) {
    // 1. Count segments
    size_t count = 1;
    for (size_t i = 0; i + 1 < expr_len; i++) {
        if (expr[i] == '-' && expr[i+1] == '>') {
            count++;
        }
    }

    char **parts = malloc(sizeof(char*) * count);
    size_t *lens = malloc(sizeof(size_t) * count);

    size_t start = 0;
    size_t idx = 0;

    for (size_t i = 0; i <= expr_len; i++) {
        bool at_arrow =
            (i + 1 < expr_len && expr[i] == '-' && expr[i+1] == '>');
        bool at_end = (i == expr_len);

        if (at_arrow || at_end) {
            size_t seg_len = i - start;

            //
            // First strip *quotes*
            //
            char *tmp = remove_all(expr + start, &seg_len, '\'');

            //
            // Then strip newlines and carriage returns
            //
            size_t cleaned_len = seg_len;
            tmp = remove_all(tmp, &cleaned_len, '\n');
            tmp = remove_all(tmp, &cleaned_len, '\r');

            parts[idx] = tmp;
            lens[idx] = cleaned_len;
            idx++;

            if (at_arrow) {
                i++;
                start = i + 1;
            }
        }
    }

    *out_parts = parts;
    *out_lens = lens;
    *out_count = count;
}

static Fetch *fetch_alloc(sqlite3 *db, int argc,
                          const char *const *argv)
{
    Fetch *vtab = sqlite3_malloc(sizeof(Fetch));
    if (!vtab) {
        fprintf(stderr, "sqlite3_malloc() out of memory\n");
        return NULL;
    }
    memset(vtab, 0, sizeof(Fetch));
    vtab->payload_len = 0;
    vtab->payload = 0;
    vtab->columns = init_columns(argc, argv);

    int index = 1;
    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        const char *arg = argv[i];
        int tokens_size = 0;
        int token_len[6] = {0};
        char **token = split(arg, ' ', &tokens_size, token_len);
        for (int i = 1; i < tokens_size - 1; i++) {
            char *prev = token[i];
            token[i] = to_lower(prev, token_len[i]);
            free(prev);
        }
        if (
            token_len[COL_NAME] == 3 &&
            strncmp(token[COL_NAME], "url", 3) == 0
            &&
            token_len[COL_CST] == 7 && 
            strncmp(token[COL_CST], "default", 7) == 0
        ) {
            vtab->default_url_len = token_len[COL_CST_VAL];
            vtab->default_url = remove_all(token[COL_CST_VAL], &vtab->default_url_len, '\'');
            continue;
        }

        normalize_column_name(token[COL_NAME], &token_len[COL_NAME]);
        column_def *def = malloc(sizeof(column_def));
        def->is_hidden = false;
        def->name = token[COL_NAME];
        def->name_len = token_len[COL_NAME];
        def->typename = token[COL_TYPE];
        def->typename_len = token_len[COL_TYPE];

        if (
            token_len[COL_CST] == 9 &&
            strncmp(token[COL_CST], "generated", 9) == 0
            &&
            token_len[COL_CST_VAL] == 6 &&
            strncmp(token[COL_CST_VAL], "always", 6) == 0
            &&
            token_len[COL_CST_VAL2] == 2 &&
            strncmp(token[COL_CST_VAL2], "as", 2) == 0
            &&
            token_len[COL_GEN_CST_VAL] > 0
        ) {
            char *expr_raw = token[COL_GEN_CST_VAL];
            size_t expr_len = token_len[COL_GEN_CST_VAL];
            if (expr_len >= 2 && expr_raw[0] == '(' && expr_raw[expr_len - 1] == ')') {
                expr_raw++;           // move start
                expr_len -= 2;        // remove both '(' and ')'
            }

            /* make a real owned copy */
            char *expr = malloc(expr_len + 1);
            memcpy(expr, expr_raw, expr_len);
            expr[expr_len] = '\0';

            char **parts;
            size_t *lens;
            size_t count;

            split_generated_path(expr, expr_len, &parts, &lens, &count);

            def->generated_always_as = parts;
            def->generated_always_as_len = lens;
            def->generated_always_as_size = count;
        }

        vtab->columns[index++] = def;
    }
    vtab->columns_len = index;

    /* max number of tokens valid inside a single xCreate argument for the table declaration */
    int MAX_ARG_TOKENS = 2;
    const char *table_name = argv[2];
    char *first_line = string("CREATE TABLE %s(url hidden text,", table_name);
    sqlite3_str *s = sqlite3_str_new(db);
    sqlite3_str_appendall(s, first_line);
    for (int i = 1; i < vtab->columns_len; i++) {
        column_def *def = vtab->columns[i];
        sqlite3_str_appendf(s, "%s %s", def->name, def->typename);

        if (i + 1 < vtab->columns_len)
            sqlite3_str_appendall(s, ",");
    }
    sqlite3_str_appendall(s, ")");
    vtab->schema = sqlite3_str_finish(s);
    free(first_line);

    println("schema: %s", vtab->schema);

    return vtab;
}

/**
 * @example
 * ```sql
 * CREATE VIRTUAL TABLE todos using fetch (
 *     url = 'https://jsonplaceholder.typicode.com/todos',
 *
 *     id int,
 *     "userId" int,
 *     title text,
 *     description text
 * );
 * ```
 */
static int x_connect(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err) {
    if (argc < MIN_ARGC) {
        fprintf(stderr, "Expected %d args but got %d args\n", MIN_ARGC, argc);
        return SQLITE_ERROR;
    }
    int rc = SQLITE_OK;
    *pp_vtab = (sqlite3_vtab *) fetch_alloc(pdb, argc, argv);
    Fetch *vtab = (Fetch *) *pp_vtab;
    if (!vtab) { 
        return SQLITE_NOMEM;
    }

    rc += sqlite3_declare_vtab(pdb, vtab->schema);
    return rc;
}

static int x_create(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err) {
    // same implementation as xConnect, we just 
    // have to point to different fns so this isn't eponymous (can't be called
    // as its own table e.g. SELECT * FROM fetch)
    return x_connect(pdb, paux, argc, argv, pp_vtab, pz_err);
}


/* To inline index checkers for x_index() */
static bool is_url_set_index_constraint(struct sqlite3_index_constraint *cst) {
    return (
        cst->iColumn == FETCH_URL // column index
        && cst->op == // was the operator '='?
            SQLITE_INDEX_CONSTRAINT_EQ
        && cst->usable
    );
}
#define IS_CST_HEADERS_EQ(cst) (cst->iColumn == FETCH_HEADERS && cst->op == SQLITE_INDEX_CONSTRAINT_EQ && cst->usable)
/* expected bitmask value of the index_info from xBestIndex */
#define REQUIRED_BITS 0b01

/**
 * Check INDEX_INFO value against REQUIRED_BITS mask.
 */
static int check_plan_mask(struct sqlite3_index_info *index_info,
                           sqlite3_vtab *pVtab) {
    if ((index_info->idxNum & REQUIRED_BITS) == REQUIRED_BITS)
        return SQLITE_OK;

    int is_url_eq_cst = 0;

    for (int i = 0; i < index_info->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &index_info->aConstraint[i];

        if (cst->iColumn == FETCH_URL) {
            // User passed in a "url" constraint, just not a usable one
            is_url_eq_cst = 1;
            if (!cst->usable) {
                return SQLITE_CONSTRAINT;
            }
        }
    }

    if (!is_url_eq_cst) {
        Fetch *vtab = (void *) pVtab;
        if (!vtab->default_url || vtab->default_url_len == 0) {
            pVtab->zErrMsg = sqlite3_mprintf(
                "yarts> Fetch needs a url = in the WHERE clause if no default url is set.\n");
            return SQLITE_ERROR;
        }
    }

    return SQLITE_OK;
}

/**
 * Fetch vtab's sqlite_module->xBestIndex() callback
 */
static int x_best_index(sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) {
    int argPos = 1;
    int planMask = 0;
    Fetch *vtab = (Fetch *)pVTab;

    for (int i = 0; i < pIdxInfo->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &pIdxInfo->aConstraint[i];

        if (!cst->usable || cst->op != SQLITE_INDEX_CONSTRAINT_EQ)
            continue;

        struct sqlite3_index_constraint_usage *usage =
            &pIdxInfo->aConstraintUsage[i];

        if (is_url_set_index_constraint(cst)) {
            usage->omit = 1;
            usage->argvIndex = argPos++;
            planMask |= 0b01;
        }
    }

    pIdxInfo->idxNum = planMask;
    return check_plan_mask(pIdxInfo, pVTab);
}

/**
 * Cleanup virtual table state pointed to be P_VTAB.
 * Serves as both xDestroy and xDisconnect for the vtable.
 */
static int x_disconnect(sqlite3_vtab *pvtab) {
    Fetch *vtab = (Fetch *) pvtab;
    yyjson_doc_free(vtab->payload);
    for (int i = 0; i < vtab->columns_len; i++) {
        if (vtab->columns[i]) {
            if (vtab->columns[i]->name) {
                free(vtab->columns[i]->name);
            }
        }
    }
    free(vtab->columns);

    sqlite3_free(vtab->schema);
    sqlite3_free(pvtab);
    return SQLITE_OK;
}

/**
 * Initialize fetch cursor at P_VTAB's cursor PP_CURSOR with count = 0.
 */
static int x_open(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **pp_cursor) {
    Fetch *fetch = (Fetch *) pvtab;
    fetch_cursor_t *cur = sqlite3_malloc(sizeof(fetch_cursor_t));
    if (!cur) {
        return SQLITE_NOMEM;
    }
    memset(cur, 0, sizeof(fetch_cursor_t));

    cur->count = 0;
    cur->json_count = 0;
    cur->queue_cap = 16;
    cur->queue = calloc(cur->queue_cap, sizeof(yyjson_doc*));
    cur->queue_head = 0;
    cur->queue_tail = 0;
    cur->queue_count = 0;

    *pp_cursor = (sqlite3_vtab_cursor *)cur;
    (*pp_cursor)->pVtab = pvtab;
    return SQLITE_OK;
}

static int x_close(sqlite3_vtab_cursor *cur) {
    fetch_cursor_t *cursor = (fetch_cursor_t *)cur;
    if (cursor)
        sqlite3_free(cursor);
    return SQLITE_OK;
}

static int x_next(sqlite3_vtab_cursor *cur0) {
    fetch_cursor_t *cur = (fetch_cursor_t*)cur0;

    println("xNext\ncount=%zu json_count=%zu queue_count=%zu",
        cur->count,
        cur->json_count,
        cur->queue_count
    );

    // Try reading from queue immediately
    yyjson_doc *doc = queue_pop(cur);
    if (doc) {
        println("xNext next_doc=%p", doc);
        cur->next_doc = doc;
        cur->count++;
        return SQLITE_OK;
    }

    // Otherwise: read more JSON until something reaches the queue
    char buf[4096];

    while (!cur->eof) {
        ssize_t n = recv(cur->sockfd, buf, sizeof(buf), 0);

        if (n <= 0) {
            cur->eof = 1;
            break;
        }

        yajl_status s = yajl_parse(cur->yajl_handle,
                                   (unsigned char*)buf, n);

        if (s == yajl_status_error) {
            cur->eof = 1;
            return SQLITE_ERROR;
        }

        // After parsing, did we get a new doc?
        doc = queue_pop(cur);
        if (doc) {
            cur->next_doc = doc;
            cur->count++;
            return SQLITE_OK;
        }
    }

    // No more data
    return SQLITE_DONE;
}


static void json_bool_result(
    sqlite3_context *pctx,
    column_def *def,
    yyjson_val *column_val
) {
    if (strncmp(def->typename, "int", 3) == 0 || strncmp(def->typename, "float", 5) == 0) {
        sqlite3_result_int(pctx, yyjson_get_bool(column_val));
    } else {
        sqlite3_result_text(pctx, yyjson_get_bool(column_val) ? "true" : "false", -1, SQLITE_TRANSIENT);
    }
}

static yyjson_val *follow_generated_path(
    yyjson_val *root,
    char **keys,
    size_t *lens,
    size_t count
) {
    yyjson_val *cur = root;

    for (size_t i = 0; i < count; i++) {
        if (!cur || yyjson_get_type(cur) != YYJSON_TYPE_OBJ) {
            return NULL;
        }

        // keys[i] is already null-terminated
        cur = yyjson_obj_getn(cur, keys[i], lens[i]);
    }

    return cur;
}

static int ensure_next_doc(fetch_cursor_t *cur) {
    if (cur->next_doc)
        return SQLITE_OK;

    // First try queue
    yyjson_doc *doc = queue_pop(cur);
    if (doc) {
        cur->next_doc = doc;
        return SQLITE_OK;
    }

    // If no queue data but not EOF: read more from socket
    char buf[4096];
    ssize_t n;

    while (!cur->next_doc &&
           (n = recv(cur->sockfd, buf, sizeof(buf), 0)) > 0)
    {
        yajl_status stat =
            yajl_parse(cur->yajl_handle, (unsigned char*)buf, n);

        if (stat == yajl_status_error) {
            cur->eof = 1;
            return SQLITE_ERROR;
        }

        // callbacks will fill queue
        doc = queue_pop(cur);
        if (doc) {
            cur->next_doc = doc;
            return SQLITE_OK;
        }
    }

    // If we reach EOF and still no doc
    cur->eof = 1;
    return SQLITE_OK;
}

/** Populates the Fetch row */
static int x_column(sqlite3_vtab_cursor *pcursor,
                    sqlite3_context *pctx,
                    int icol)
{
    fetch_cursor_t *cursor = (fetch_cursor_t *)pcursor;
    println("xColumn next_doc=%p\n", cursor->next_doc);

    if (!cursor->next_doc) {
        // No more rows
        sqlite3_result_null(pctx);
        return SQLITE_OK;
    }

    Fetch *vtab = (Fetch *)pcursor->pVtab;
    column_def *def = vtab->columns[icol];

    if (def->is_hidden) {
        return SQLITE_OK;
    }

    yyjson_val *val = yyjson_doc_get_root(cursor->next_doc);

    if (def->generated_always_as_size > 0) {
        val = follow_generated_path(
            val,
            def->generated_always_as,
            def->generated_always_as_len,
            def->generated_always_as_size);
    } else {
        val = yyjson_obj_getn(val, def->name, def->name_len);
    }

    if (!val) {
        sqlite3_result_null(pctx);
        return SQLITE_OK;
    }

    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_STR:
        sqlite3_result_text(pctx, yyjson_get_str(val), -1, SQLITE_TRANSIENT);
        break;

    case YYJSON_TYPE_NUM:
        sqlite3_result_int(pctx, yyjson_get_int(val));
        break;

    case YYJSON_TYPE_BOOL:
        json_bool_result(pctx, def, val);
        break;

    case YYJSON_TYPE_OBJ:
    case YYJSON_TYPE_ARR: {
        char *json = yyjson_val_write(val, 0, NULL);
        if (json) {
            sqlite3_result_text(pctx, json, -1, SQLITE_TRANSIENT);
            free(json);
        } else {
            sqlite3_result_null(pctx);
        }
        break;
    }

    default:
        sqlite3_result_null(pctx);
    }

    return SQLITE_OK;
}


static int x_eof(sqlite3_vtab_cursor *cur) {
    fetch_cursor_t *c = (fetch_cursor_t*)cur;
    return c->eof && c->queue_count == 0;
}


/// API: `sqlite3_vtab.xRowid()`
static int x_rowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    *prowid = ((fetch_cursor_t *)pcursor)->count;
    return SQLITE_OK;
}

static int handle_null(void *ctx) {
    return 1;
}

static int handle_bool(void *ctx, int b) {
    return 1;
}

static int handle_int(void *ctx, long long i) {
    return 1;
}

static int handle_double(void *ctx, double d) {
    return 1;
}

static int handle_number(void *ctx, const char *num, size_t len) {
    fetch_cursor_t *cur = ctx;

    char *key = cur->current_key;

    if (cur->depth == 0) {
        return 1;
    }

    yyjson_mut_val *obj = cur->obj_stack[cur->depth - 1];

    if (!obj || !key) {
        return 1;
    }

    /* Convert number safely:
       YAJL gives numeric tokens as strings. We must parse them. */
    bool is_float = false;
    for (size_t i = 0; i < len; i++) {
        char c = num[i];
        if (c == '.' || c == 'e' || c == 'E') {
            is_float = true;
            break;
        }
    }

    yyjson_mut_val *val;

    if (is_float) {
        double d = strtod(num, NULL);
        val = yyjson_mut_real(cur->builder, d);
    } else {
        long long i = strtoll(num, NULL, 10);
        val = yyjson_mut_sint(cur->builder, i);
    }

    /* Insert into current object */
    yyjson_mut_obj_add_val(cur->builder, obj, key, val);

    /* key is consumed */
    cur->current_key = NULL;

    return 1;
}

static int handle_string(void *ctx,
                         const unsigned char *str, size_t len) {
    fetch_cursor_t *cur = ctx;

    if (!cur->current_key) return 1;

    yyjson_mut_val *val =
        yyjson_mut_strn(cur->builder, (const char*)str, len);

    yyjson_mut_obj_add_val(cur->builder,
                           cur->obj_stack[cur->depth - 1],
                           cur->current_key,
                           val);

    cur->current_key = NULL;

    return 1;
}

static int handle_start_map(void *ctx) {
    fetch_cursor_t *cur = ctx;

    yyjson_mut_val *new_obj;

    // Create a new builder if this is the first (top-level) map
    if (cur->depth == 0) {
        cur->builder = yyjson_mut_doc_new(NULL);
        new_obj = yyjson_mut_obj(cur->builder);
        yyjson_mut_doc_set_root(cur->builder, new_obj);
        println("start obj (json_count + 1) %zu, builder = %p", cur->json_count + 1, cur->builder);
    } else {
        // nested map value
        new_obj = yyjson_mut_obj(cur->builder);

        // attach as value to parent object
        if (cur->current_key) {
            yyjson_mut_obj_add_val(cur->builder,
                                   cur->obj_stack[cur->depth - 1],
                                   cur->current_key,
                                   new_obj);
            cur->current_key = NULL;
        }
    }

    cur->obj_stack[cur->depth] = new_obj;
    cur->depth++;

    return 1;
}

static int handle_map_key(void *ctx,
                       const unsigned char *str,
                       size_t len) {
    fetch_cursor_t *cur = ctx;
    cur->current_key = string("%.*s", (int) len, str);
    return 1;
}

static void queue_push(fetch_cursor_t *cur, yyjson_doc *doc) {
    if (cur->queue_count == cur->queue_cap) {
        size_t newcap = cur->queue_cap * 2;
        yyjson_doc **newbuf = realloc(cur->queue, newcap * sizeof(yyjson_doc*));
        if (!newbuf) return; // OOM, but safer to drop
        cur->queue = newbuf;
        cur->queue_cap = newcap;
    }

    cur->queue[cur->queue_tail] = doc;
    cur->queue_tail = (cur->queue_tail + 1) % cur->queue_cap;
    cur->queue_count++;
}

static yyjson_doc *queue_pop(fetch_cursor_t *cur) {
    if (cur->queue_count == 0)
        return NULL;

    yyjson_doc *doc = cur->queue[cur->queue_head];
    cur->queue_head = (cur->queue_head + 1) % cur->queue_cap;
    cur->queue_count--;

    return doc;
}

static int handle_end_map(void *ctx) {
    fetch_cursor_t *cur = ctx;

    cur->depth--;

    if (cur->depth== 0) {
        cur->json_count++;
        println("end obj %zu, builder = %p", cur->json_count, cur->builder);
        yyjson_doc *final = yyjson_mut_doc_imut_copy(cur->builder, NULL);

        queue_push(cur, final);

        yyjson_mut_doc_free(cur->builder);
        cur->builder = NULL;
        cur->current_key = NULL;
    }

    return 1;
}

static int handle_start_array(void *ctx) {
    return 1;
}

static int handle_end_array(void *ctx) {
    println("YAJL: end array");
    return 1;
}

static yajl_callbacks fetch_callbacks = {
    .yajl_null        = handle_null,
    .yajl_boolean     = handle_bool,
    .yajl_integer     = handle_int,
    .yajl_double      = handle_double,
    .yajl_number      = handle_number,
    .yajl_string      = handle_string,

    .yajl_start_map   = handle_start_map,
    .yajl_map_key     = handle_map_key,
    .yajl_end_map     = handle_end_map,

    .yajl_start_array = handle_start_array,
    .yajl_end_array   = handle_end_array
};

static int x_filter(sqlite3_vtab_cursor *cur0,
                    int idxNum, const char *idxStr,
                    int argc, sqlite3_value **argv)
{
    Fetch *vtab = (Fetch*)cur0->pVtab;
    fetch_cursor_t *Cur = (fetch_cursor_t*)cur0;

    Cur->depth = 0;
    Cur->eof       = 0;
    Cur->count     = 0;
    Cur->json_count = 0;
    Cur->next_doc  = NULL;

    // Extract URL
    if (argc + vtab->default_url_len == 0) {
        cur0->pVtab->zErrMsg =
            sqlite3_mprintf("fetch: need at least 1 argument or default url");
        return SQLITE_ERROR;
    }

    const char *argurl = (argc > 0)
        ? (const char*)sqlite3_value_text(argv[0])
        : vtab->default_url;

    size_t sz = strlen(argurl);
    char *url = remove_all(argurl, &sz, '\'');
    if (!url) url = strdup(vtab->default_url);

    Cur->sockfd = fetch(url);
    free(url);

    if (Cur->sockfd < 0) {
        cur0->pVtab->zErrMsg =
            sqlite3_mprintf("fetch: could not connect");
        Cur->eof = 1;
        return SQLITE_ERROR;
    }

    Cur->yajl_handle =
        yajl_alloc(&fetch_callbacks, NULL, (void*)Cur);
    yajl_config(Cur->yajl_handle, yajl_allow_partial_values, 1);

    if (!Cur->yajl_handle) {
        Cur->eof = 1;
        return SQLITE_NOMEM;
    }

    char buf[4096];
    ssize_t n;

    while (Cur->queue_count == 0 &&
       (n = recv(Cur->sockfd, buf, sizeof(buf), 0)) > 0)
    {
        yajl_status stat =
            yajl_parse(Cur->yajl_handle, (unsigned char*)buf, n);

        if (stat == yajl_status_error) {
            Cur->eof = 1;
            return SQLITE_ERROR;
        }
    }

    if (Cur->queue_count == 0) {
        Cur->eof = 1;
        return SQLITE_OK;
    }

    return SQLITE_OK;
}

static sqlite3_module fetch_vtab_module = {
    .iVersion=0,
    .xCreate=x_create,
    .xConnect=x_connect,
    .xBestIndex=x_best_index,
    .xDisconnect=x_disconnect,
    .xDestroy=x_disconnect,
    .xOpen=x_open,
    .xClose=x_close,
    .xFilter=x_filter,
    .xNext=x_next,
    .xEof=x_eof,
    .xColumn=x_column,
    .xRowid=x_rowid,
    .xUpdate=NULL,
    .xBegin=NULL,
    .xSync=NULL,
    .xCommit=NULL,
    .xRollback=NULL,
    .xFindFunction=NULL
};

// Runtime loadable entry
int sqlite3_yarts_init(sqlite3 *db, char **pzErrMsg,
                       const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    // oh yeah baby
    int rc = sqlite3_create_module(db, "fetch", &fetch_vtab_module, 0);
    return rc;
}
