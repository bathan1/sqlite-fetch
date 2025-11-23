// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include <sqlite3ext.h>
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

/* utils */
#include "parse.h"

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
  #define FETCH_DBG(...) do { } while (0)
#else
  #define FETCH_DBG(...)                     \
    do {                                     \
      printf("fetch-dbg> " __VA_ARGS__);           \
    } while (0)
#endif

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
void normalize_column_name(char *str, size_t *len) {
    if (str[0] == '"') {
        *len -= 2;
        memmove(str, str + 1, *len);
        str[*len] = 0;
    }
}

struct curl_slist *curl_slist_from_headers_json(char *json_obj_str) {
    if (!json_obj_str) return NULL;

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts(json_obj_str, strlen(json_obj_str), 0, NULL, &err);
    if (!doc) {
        return NULL;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    struct curl_slist *headers = NULL;

    size_t idx, max;
    yyjson_val *k, *v;
    yyjson_obj_foreach(root, idx, max, k, v) {
        const char *key = yyjson_get_str(k);
        if (!key || !*key) continue;

        char numbuf[64];
        const char *val_cstr = NULL;
        char *owned_val = NULL;

        if (yyjson_is_str(v)) {
            val_cstr = yyjson_get_str(v);
        } else if (yyjson_is_null(v)) {
            val_cstr = NULL; // means "Key:"
        } else if (yyjson_is_bool(v)) {
            val_cstr = yyjson_get_bool(v) ? "true" : "false";
        } else if (yyjson_is_int(v)) {
            snprintf(numbuf, sizeof(numbuf), "%lld", (long long)yyjson_get_sint(v));
            val_cstr = numbuf;
        } else if (yyjson_is_uint(v)) {
            snprintf(numbuf, sizeof(numbuf), "%llu", (unsigned long long)yyjson_get_uint(v));
            val_cstr = numbuf;
        } else if (yyjson_is_real(v)) {
            snprintf(numbuf, sizeof(numbuf), "%g", yyjson_get_real(v));
            val_cstr = numbuf;
        } else {
            continue;
        }

        size_t key_len = strlen(key);
        size_t val_len = val_cstr ? strlen(val_cstr) : 0;
        size_t line_len = key_len + (val_cstr ? (2 + 1 + val_len) : 1) + 1; // "Key:" + '\0'
        char *line = (char *)malloc(line_len);
        if (!line) { curl_slist_free_all(headers); headers = NULL; break; }

        if (val_cstr) {
            snprintf(line, line_len, "%s: %s", key, val_cstr);
        } else {
            snprintf(line, line_len, "%s:", key);
        }

        headers = curl_slist_append(headers, line);
        free(line);
        free(owned_val);
        if (!headers) break;
    }

    yyjson_doc_free(doc);
    return headers;
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
    char *name;
    size_t name_len;
    char *typename;
    size_t typename_len;
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
typedef struct {
    sqlite3_vtab_cursor base;
    int count;
    yyjson_val *val;
} fetch_cursor_t;

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
static char *GET(const char *url, size_t *body_size) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }

    char *buffer = NULL;
    FILE *payload = open_memstream(&buffer, body_size);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, payload);
    // default is GET so we don't need to set it
    // curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    CURLcode rc = curl_easy_perform(curl);
    fclose(payload);
    if (rc!= CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(): %s\n",
                curl_easy_strerror(rc)); 
        return NULL;
    }
    curl_easy_cleanup(curl);
    return buffer;
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

static Fetch *fetch_alloc(sqlite3 *db, int argc,
                          const char *const *argv,
                          size_t num_fetch_args,
                          yyjson_doc *payload_doc,
                          size_t payload_size)
{
    Fetch *vtab = sqlite3_malloc(sizeof(Fetch));
    if (!vtab) {
        fprintf(stderr, "sqlite3_malloc() out of memory\n");
        return NULL;
    }
    memset(vtab, 0, sizeof(Fetch));

    int num_columns = argc - (num_fetch_args + FETCH_ARGS_OFFSET);

    yyjson_val *root = yyjson_doc_get_root(payload_doc);
    vtab->payload_len = yyjson_arr_size(root);
    vtab->payload = payload_doc;
    if (num_columns < 0) {
        fprintf(stderr, "computed more fetch arguments than possible %d\n", FETCH_ARGS_OFFSET);
        sqlite3_free(vtab);
        return NULL;
    }

    vtab->columns = malloc(sizeof(column_def *) * num_columns);
    if (num_columns == 0) {
        if (yyjson_arr_size(root) == 0) {
            fprintf(
                stderr,
                "dynamic schema needs at least 1 json element in the response body\n"
            );
            sqlite3_free(vtab);
            return NULL;
        }
        // then we take the first object from the payload
        // and use those keys as the columns
        yyjson_val *head = yyjson_arr_get_first(root);
        if (yyjson_get_type(head) != YYJSON_TYPE_OBJ) {
            fprintf(
                stderr,
                "dynamic schema expected head type to be object but got %s\n",
                yyjson_get_type_desc(head)
            );
            sqlite3_free(vtab);
            return NULL;
        }
        yyjson_val *key = 0, *value = 0;
        int index = 0, max = 0;
        yyjson_obj_foreach(head, index, max, key, value) {
            const char *key_str = yyjson_get_str(key);
            column_def *def = malloc(sizeof(column_def));
            def->name = strdup(key_str);
            def->name_len = strlen(key_str);

            yyjson_type value_type = yyjson_get_type(value);
            switch (value_type) {
                case YYJSON_TYPE_STR: {
                    def->typename = strdup("text");
                    def->json_typename = strdup("string");
                    break;
                }
                case YYJSON_TYPE_ARR: {
                    def->typename = strdup("text");
                    def->json_typename = strdup("array");
                    break;
                }
                case YYJSON_TYPE_OBJ: {
                    def->typename = strdup("text");
                    def->json_typename = strdup("object");
                    break;
                }
                case YYJSON_TYPE_NUM: {
                    if (yyjson_is_sint(value) || yyjson_is_uint(value)) {
                        def->typename = strdup("int");
                        def->typename_len = 3;
                    } else {
                        def->typename = strdup("float");
                        def->typename_len = 5;
                    }
                    def->json_typename = strdup("number");
                    break;
                }
                case YYJSON_TYPE_BOOL: {
                    def->typename = strdup("text");
                    def->typename_len = 4;
                    def->json_typename = strdup("boolean");
                    break;
                }
                default:
                    def->typename = strdup("text");
                    def->typename_len = 4;
                    def->json_typename = strdup("string");
            }

            vtab->columns[index] = def;
        }
        num_columns = index;
    } else {
        int index = 0;
        for (int i = argc - num_fetch_args - 1; i < argc; i++) {
            const char *arg = argv[i];

            int tokens_size = 0;
            int token_lens[2];
            char **tokens = split(arg, ' ', &tokens_size, token_lens);

            char *tok_col = tokens[0];
            char *tok_col_type = tokens[1];
            size_t tok_col_len = token_lens[0];
            int tok_col_type_len = token_lens[1];

            normalize_column_name(tok_col, &tok_col_len);

            column_def *def = malloc(sizeof(column_def));
            def->name = tok_col;
            def->name_len = tok_col_len;
            def->typename = to_lower(tok_col_type, tok_col_type_len);
            def->typename_len = tok_col_len;

            vtab->columns[index++] = def;
        }
    }
    vtab->columns_len = num_columns;

    /* max number of tokens valid inside a single xCreate argument for the table declaration */
    int MAX_ARG_TOKENS = 2;
    sqlite3_str *s = sqlite3_str_new(db);
    sqlite3_str_appendall(s, "CREATE TABLE x(");
    for (int i = 0; i < num_columns; i++) {
        column_def *def = vtab->columns[i];
        sqlite3_str_appendf(s, "%s %s", def->name, def->typename);

        if (i + 1 < num_columns)
            sqlite3_str_appendall(s, ",");
    }
    sqlite3_str_appendall(s, ")");
    vtab->schema = sqlite3_str_finish(s);
    return vtab;
}

char **get_argument_labels(const char *const *argv, int argc) {
    int MAX_NUM_FETCH_ARGS = 3;
    char **fetch_args = calloc(MAX_NUM_FETCH_ARGS, sizeof(char *));
    for (int i = FETCH_ARGS_OFFSET; i < argc; i++) {
        const char *arg = argv[i];
        const char *eq  = strchr(arg, '=');
        if (!eq) {
            continue;
        }

        const char *kbeg = arg, *kend = eq;
        const char *vbeg = eq + 1, *vend = arg + strlen(arg);

        trim_slice(&kbeg, &kend);
        trim_slice(&vbeg, &vend);

        char *key = trim(kbeg, kend);
        char *value = trim(vbeg, vend);
        
        int key_int = index_of_key(key);
        size_t value_len = strlen(value);
        fetch_args[key_int] = remove_all(value, &value_len, '\'');

        free(key);
        free(value);
    }
    return fetch_args;
}

/**
 * Quick check if some argv from SQLite is a column declaration like "text INT", because
 * all column declarations need to have at least 1 whitespace.
 */
static bool has_whitespace(const char *s) {
    for (; *s; s++) {
        if (isspace((unsigned char)*s))
            return 1;
    }
    return 0;
}

char **get_fetch_args(const char *const *argv, int argc, size_t *fetch_args_count) {
    // "fetch (module name)", "main (schema name)", "(table name)"
    char **init = get_argument_labels(argv, argc);
    if (!init[FETCH_URL]) {
        int index = FETCH_ARGS_OFFSET + FETCH_URL;
        // just let curl crash if url is invalid
        const char *url_arg = argv[index];
        size_t url_len = strlen(url_arg);
        init[FETCH_URL] = remove_all(url_arg, &url_len, '\'');
        *fetch_args_count += 1;
    }
    if (!init[FETCH_BODY]) {
        int index = FETCH_ARGS_OFFSET + FETCH_BODY;
        const char *body_arg = argc > index ? argv[index] : "$";
        if (!has_whitespace(body_arg) && strncmp(body_arg, "$", 1) != 0) {
            *fetch_args_count += 1;
        }
        size_t body_len = strlen(body_arg);
        init[FETCH_BODY] = remove_all(body_arg, &body_len, '\'');
    }
    if (!init[FETCH_HEADERS]) {
        int index = FETCH_ARGS_OFFSET + FETCH_HEADERS;
        init[FETCH_HEADERS] = strdup(argc > index ? argv[index] : " ");
        if (!has_whitespace(init[FETCH_HEADERS])) {
            *fetch_args_count += 1;
        }
    }
    return init;
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

    size_t num_fetch_args = 0;
    char **fetch_args = get_fetch_args(argv, argc, &num_fetch_args);

    char *url = fetch_args[FETCH_URL];

    size_t payload_size = 0;
    char *payload = GET(url, &payload_size);
    if (!payload || payload_size == 0) {
        fprintf(stderr, "GET: fetch failed on url %s\n", url);
        return SQLITE_ERROR;
    }
    yyjson_doc *doc = yyjson_read(payload, payload_size, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root) && !yyjson_is_obj(root)) {
        const char *typename = yyjson_get_type_desc(root);
        fprintf(stderr, "Expected PAYLOAD to be an array or an object, but got %s\n", typename);
        return SQLITE_ERROR;
    }
    if (yyjson_is_obj(root)) {
        // set doc to [ {...} ]
        yyjson_doc *new = array_wrap(root);

        yyjson_doc_free(doc);
        doc = new;
        root = yyjson_doc_get_root(doc);
    }

    *pp_vtab = (sqlite3_vtab *) fetch_alloc(pdb, argc, argv, num_fetch_args, doc, payload_size);
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
        pVtab->zErrMsg = sqlite3_mprintf(
            "fetch> Need a url = in the WHERE clause.");
        return SQLITE_ERROR;
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
    pIdxInfo->idxNum = planMask |= 0b01;
    return 0;
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
    cur->val = 0;
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

static int x_next(sqlite3_vtab_cursor *pcursor) {
    Fetch *vtab = (Fetch *) pcursor->pVtab;
    fetch_cursor_t *cursor = (fetch_cursor_t *) pcursor;
    cursor->val = yyjson_arr_get(yyjson_doc_get_root(vtab->payload), cursor->count);
    cursor->count++;
    return SQLITE_OK;
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

/** Populates the Fetch row */
static int x_column(sqlite3_vtab_cursor *pcursor, sqlite3_context *pctx,
                    int icol) {
    fetch_cursor_t *cursor = (fetch_cursor_t *)pcursor;
    Fetch *vtab = (Fetch *)pcursor->pVtab;

    char *column_name = vtab->columns[icol]->name;
    yyjson_val *column = yyjson_obj_get(cursor->val, column_name);
    yyjson_type typename = yyjson_get_type(column);
    switch (typename) {
        case YYJSON_TYPE_STR: {
            sqlite3_result_text(pctx, yyjson_get_str(column), -1, SQLITE_TRANSIENT);
            break;
        }
        case YYJSON_TYPE_NUM:
            sqlite3_result_int(pctx, yyjson_get_int(column));
            break;
        case YYJSON_TYPE_BOOL:
            json_bool_result(pctx, vtab->columns[icol], column);
            break;
        case YYJSON_TYPE_ARR:
        case YYJSON_TYPE_OBJ: {
            char *json = yyjson_val_write(column, -1, NULL);
            if (!json) {
                sqlite3_result_null(pctx);
            } else {
                sqlite3_result_text(pctx, json, -1, SQLITE_TRANSIENT);
                free(json);
            }
            break;
        }
        default:
            sqlite3_result_null(pctx);
    }
    return SQLITE_OK;
}

static int x_eof(sqlite3_vtab_cursor *pcursor) {
    fetch_cursor_t *cursor = (fetch_cursor_t *) pcursor;
    Fetch *vtab = (Fetch *) pcursor->pVtab;
    return cursor->count > vtab->payload_len;
}

/// API: `sqlite3_vtab.xRowid()`
static int x_rowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    *prowid = ((fetch_cursor_t *)pcursor)->count;
    return SQLITE_OK;
}

static int x_filter(sqlite3_vtab_cursor *cur, int index, const char *index_str,
                    int argc, sqlite3_value **argv) {
    return SQLITE_OK;
};

/**
 * Handles every other query that isn't a `SELECT` query.
 */
// static int x_update(sqlite3_vtab *pVTab, int argc, sqlite3_value **argv,
//                     sqlite_int64 *pRowid) {
//     if (X_UPDATE_IS_INSERT(argc, argv)) {
//         if (sqlite3_value_type(argv[FETCH_URL + X_UPDATE_OFFSET]) ==
//             SQLITE_NULL) {
//             pVTab->zErrMsg =
//                 sqlite3_mprintf("fetch.xUpdate: no url to INSERT INTO (lol)");
//             return SQLITE_MISUSE;
//         }
//
//         // open Request streams
//         const char *req_url = (char *) sqlite3_value_text(argv[FETCH_URL + X_UPDATE_OFFSET]);
//         char *req_body_buf = (char *) sqlite3_value_text(argv[FETCH_BODY + X_UPDATE_OFFSET]);
//         FILE *req_body = NULL;
//         if (req_body_buf) {
//             size_t req_body_size = 0;
//             req_body = open_memstream(&req_body_buf, &req_body_size);
//             if (!req_body) return SQLITE_NOMEM;
//         }
//         struct curl_slist *req_headers = curl_slist_from_headers_json((char *) sqlite3_value_text(argv[FETCH_HEADERS + X_UPDATE_OFFSET]));
//
//         FILE *resp_headers, *resp_body;
//         char *resp_headers_buf = NULL, *resp_body_buf = NULL;
//         size_t resp_headers_size = 0, resp_body_size = 0;
//         if (open_response(&resp_headers, &resp_headers_buf, &resp_headers_size, &resp_body, &resp_body_buf, &resp_body_size))
//             return SQLITE_NOMEM;
//
//         CURL *curl = fetchcurl((const char *) req_url, FETCH_METHOD_POST, resp_headers, resp_body, req_headers, req_body);
//         curl_easy_perform(curl);
//
//         curl_easy_cleanup(curl);
//     }
//
//     if (X_UPDATE_IS_DELETE(argc, argv)) {
//         char *url = ((Fetch *) pVTab)->request.url;
//     }
//
//     return SQLITE_OK;
// }

static sqlite3_module sqlite_fetch = {
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
int sqlite3_fetch_init(sqlite3 *db, char **pzErrMsg,
                       const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    // oh yeah baby
    int rc = sqlite3_create_module(db, "fetch", &sqlite_fetch, 0);
    return rc;
}
