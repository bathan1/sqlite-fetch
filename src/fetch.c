// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#define NDEBUG
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

// ---- Windows portability
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define pipe(fds) _pipe((fds), 4096, _O_BINARY)
// If you ever need close(), MSYS2 provides it; otherwise map to _close:
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* For working directly with a Response struct */
// #region fetch_schema
#define FETCH_TABLE_SCHEMA \
    "CREATE TABLE fetch (" \
    "body BLOB,"           /* 0 */                                             \
    "body_used INT,"       /* 1 */                                             \
    "ok INT,"              /* 2 */                                             \
    "redirected INT,"      /* 3 */                                             \
    "status INT,"          /* 4 */                                             \
    "status_text TEXT,"    /* 5 */                                             \
    "type TEXT,"           /* 6 */                                             \
    "url TEXT HIDDEN,"      /* 7 */                                             \
    "headers TEXT HIDDEN" /* 8 */                                             \
    ");"
// #endregion fetch_schema
#define FETCH_TABLE_COLUMN_COUNT 9
#define FETCH_BODY 0
#define FETCH_BODY_USED 1
#define FETCH_OK 2
#define FETCH_REDIRECTED 3
#define FETCH_STATUS 4
#define FETCH_STATUS_TEXT 5
#define FETCH_TYPE 6
#define FETCH_URL 7
#define FETCH_HEADERS 8

#define TRY_CALLOC(ptr, nbytes)                                \
    do {                                                       \
        (ptr) = (char*)calloc(1, (nbytes));                    \
    } while (0)
#define ALLOC_FETCH_COLUMN_LIT(VTAB, IDX, LIT)                 \
    do {                                                       \
        /* sizeof("txt") includes the trailing '\0' */         \
        size_t _n__ = sizeof(LIT);                             \
        TRY_CALLOC((VTAB)->columns[(IDX)], _n__);              \
        memcpy((VTAB)->columns[(IDX)], (LIT), _n__);           \
        (VTAB)->column_lens[(IDX)] = _n__;\
    } while (0)

/* For debug logs */
#ifdef NDEBUG
  #define FETCH_DBG(...) do { } while (0)
#else
  #define FETCH_DBG(...)                     \
    do {                                     \
      printf("fetch-dbg> " __VA_ARGS__);           \
    } while (0)
#endif

#define FREE(ptr)             \
    do {                      \
        if (ptr) {            \
            free(ptr);        \
            (ptr) = NULL;     \
        }                     \
    } while (0)

/* max number of tokens valid inside a single xCreate argument for the table declaration */
#define MAX_XCREATE_ARG_TOKENS 2

// ---------------------------------------------------------------------

struct curl_slist *curl_slist_from_headers_json(char *json_obj_str) {
    if (!json_obj_str) return NULL;

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts(json_obj_str, strlen(json_obj_str), 0, NULL, &err);
    if (!doc) {
        // parse error; you could log err.code/err.msg
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

        // Convert value to string (NULL -> remove header)
        char numbuf[64];
        const char *val_cstr = NULL;
        char *owned_val = NULL; // if we allocate

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
            // For arrays/objects, either skip or serialize:
            // owned_val = yyjson_write_opts(doc_for_v,...); // optional
            continue;
        }

        // Build "Key: value" (or "Key:" when val_cstr == NULL)
        size_t key_len = strlen(key);
        size_t val_len = val_cstr ? strlen(val_cstr) : 0;
        // "Key: " + val + '\0'  ->  key_len + 2 + 1 + val_len + 1 = key_len + val_len + 4
        size_t line_len = key_len + (val_cstr ? (2 + 1 + val_len) : 1) + 1; // "Key:" + '\0'
        char *line = (char *)malloc(line_len);
        if (!line) { /* out of mem: clean up and bail */ curl_slist_free_all(headers); headers = NULL; break; }

        if (val_cstr) {
            // "Key: value"
            // Note: HTTP requires exactly "Key: value" format
            snprintf(line, line_len, "%s: %s", key, val_cstr);
        } else {
            // "Key:" -> instruct libcurl to remove header
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

/**
 * Open 2 memstreams for Response headers and body.
 * On success, returns 0 and sets all outputs.
 * On failure, returns an errno-style code with no leaks.
 */
static int open_response(
    FILE **headers_stream, char **headers_buf, size_t *headers_size,
    FILE **body_stream,    char **body_buf,    size_t *body_size
) {
    // Ensure known state for callers
    *headers_stream = NULL; *headers_buf = NULL; *headers_size = 0;
    *body_stream    = NULL; *body_buf    = NULL; *body_size    = 0;

    FILE *hs = open_memstream(headers_buf, headers_size);
    if (!hs)
        return ENOMEM;

    FILE *bs = open_memstream(body_buf, body_size);
    if (!bs) {
        // Clean up headers stream. After fclose, headers_buf may become a malloc'd "" that must be freed.
        fclose(hs);
        if (*headers_buf) free(*headers_buf);
        *headers_buf = NULL; *headers_size = 0;
        return ENOMEM;
    }

    *headers_stream = hs;
    *body_stream    = bs;
    return 0;
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
static inline const char *fetch_status_text(int status) {
    for (size_t i = 0; i < STATUS_TABLE_LEN; ++i) {
        if (STATUS_TABLE[i].status == status)
            return STATUS_TABLE[i].text;
    }
    return "Unknown Status";
}

static const char *RESPONSE_TYPE_BASIC = "basic";
static const char *RESPONSE_TYPE_CORS = "cors";
static const char *RESPONSE_TYPE_OPAQUE = "opaque";
static const char *RESPONSE_TYPE_OPAQUEREDIRECT = "opaqueredirect";
static const char *RESPONSE_TYPE_ERROR = "error";

static int is_same_origin(const char *a, const char *b) {
    // Compares scheme://host:port — NOT full URLs
    // For now, just match the prefix up to the third slash
    if (!a || !b)
        return 0;

    const char *end_a = strstr(a + strlen("https://"), "/");
    const char *end_b = strstr(b + strlen("https://"), "/");

    size_t len_a = end_a ? (size_t)(end_a - a) : strlen(a);
    size_t len_b = end_b ? (size_t)(end_b - b) : strlen(b);

    return len_a == len_b && strncmp(a, b, len_a) == 0;
}

/**
 * The SQLite virtual table
 */
typedef struct {
    sqlite3_vtab base;
    /** Is this vtable from a CREATE VIRTUAL TABLE query? */
    bool is_dynamic;
    /** Index map into column text keys */
    char **columns;
    /** Index map into the length of the ith column from COLUMNS[i] */
    int *column_lens;
    /** Number of COLUMNS entries */
    unsigned long n_columns;
    char *schema;

    struct {
        char *url;
        char *headers;
        char *body;
    } request;

    struct {
        uint16_t status;
        char *body;
        char *url;
        char *headers;
        bool redirected;
    } response;

} Fetch;

/// Cursor
typedef struct {
    sqlite3_vtab_cursor base;
    int count;
    yyjson_doc *elements;
    int n_elements;
} fetch_cursor_t;

#define X_UPDATE_OFFSET 2

/* For xCreate options arguments */
#define IS_KEY_URL(key) (!strncmp("url", key, 3))

/** @sqlite3_malloc() a @Fetch buffer for DB, and write its plain schema text into OUT_SCHEMA */
static Fetch *Fetch_alloc(sqlite3 *db, int argc,
                             const char *const *argv) {
    Fetch *vtab = sqlite3_malloc(sizeof(Fetch));
    if (!vtab)
        return NULL;
    memset(vtab, 0, sizeof(Fetch));
    // Allocate response buffers upfront
    vtab->response.url = malloc(1 << 16);
    vtab->response.body = malloc(1 << 16);
    vtab->response.headers = malloc(1 << 16);

    // Save column data
    vtab->columns = malloc(sizeof(char *) * (1 << 8));
    vtab->column_lens = malloc(sizeof(int) * (1 << 8));
    vtab->n_columns = 0;
    
    int schema_i = 3;
    while (schema_i < argc) {
        const char *arg = argv[schema_i++];
        const char *eq  = strchr(arg, '=');
        if (!eq)
            break;

        const char *kbeg = arg, *kend = eq;
        const char *vbeg = eq + 1, *vend = arg + strlen(arg);

        trim_slice(&kbeg, &kend);
        trim_slice(&vbeg, &vend);

        char *key   = trim(kbeg, kend);
        char *value = trim(vbeg, vend);

        if (!key || !value) {
            free(key);
            free(value);
            break;
        }

        // Write out vtab's fields based on the user key arg
        if (IS_KEY_URL(key)) {
            unsigned long url_size = strlen(value);
            memmove(value, value + 1, url_size - 2);
            value[url_size - 2] = 0;
            vtab->request.url = value;
        }
    }

    // vtab flag if using a plain Response schema, or user provided
    vtab->is_dynamic = schema_i < argc;

    sqlite3_str *s = sqlite3_str_new(db);
    if (!s) {
        FREE(vtab->request.url);
        FREE(vtab->request.body);
        FREE(vtab->request.headers);
        FREE(vtab->response.url);
        FREE(vtab->response.body);
        FREE(vtab->response.headers);

        sqlite3_free(vtab);
        return NULL;
    }

    if (vtab->is_dynamic) {
        sqlite3_str_appendall(s, "CREATE TABLE x(");
        for (int i = schema_i - 1; i < argc; i++) {
            const char *arg = argv[i];
            sqlite3_str_appendall(s, arg);

            int tokens_size = 0;
            int token_lens[MAX_XCREATE_ARG_TOKENS];
            char **tokens = split(arg, ' ', &tokens_size, token_lens);

            char *tok_col = tokens[0];
            int tok_col_len = token_lens[0];
            // Then this is a fully qualified dquote identifier
            if (tok_col[0] == '"') {
                tok_col_len = tok_col_len - 2;
                memmove(tok_col, tok_col + 1, tok_col_len);
                tok_col[tok_col_len] = 0;
            }

            vtab->columns[vtab->n_columns] = tok_col;
            vtab->column_lens[vtab->n_columns] = tok_col_len;
            if (i + 1 < argc) // attach comma for valid CREATE TABLE syntax
                sqlite3_str_appendall(s, ",");
            vtab->n_columns++;
        }
        sqlite3_str_appendall(s, ")");
    } else {
        // Otherwise this is a tvf / eponymous call
        sqlite3_str_appendall(s, FETCH_TABLE_SCHEMA);
        vtab->request.url = calloc(1 << 16, sizeof(char));
        vtab->request.body = calloc(1 << 16, sizeof(char));
        vtab->request.headers = calloc(1 << 16, sizeof(char));

        vtab->n_columns = FETCH_TABLE_COLUMN_COUNT;

        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_BODY, "body");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_BODY_USED, "body_used");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_OK, "ok");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_REDIRECTED, "redirected");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_STATUS, "status");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_STATUS_TEXT, "status_text");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_TYPE, "type");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_URL, "url");
        ALLOC_FETCH_COLUMN_LIT(vtab, FETCH_HEADERS, "headers");
    }

    vtab->schema = sqlite3_str_finish(s);
    return vtab;
}

/**
 * `xCreate` AND `xConnect` methods of the Fetch virtual table so that it can
 * exist as a so-called "eponymous virtual table" (exists in main schema).
 * Meaning... it can handle tvf calls:
 * @example
 * ```sql
 * select
 *     body,
 *     body_used,
 *     headers,
 *     ok,
 *     redirected,
 *     status,
 *     status_text,
 *     type,
 *     url
 * from fetch('https://sqlite-fetch.dev');
 * ```
 * Along with user `CREATE VIRTUAL TABLE` statements...
 * @example
 * ```sql
 * CREATE VIRTUAL TABLE todos using fetch (
 *     id int,
 *     "userId" int,
 *     title text,
 *     description text,
 *
 *     url = 'https://jsonplaceholder.typicode.com/todos'
 * );
 * ```
 */
static int x_connect(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err) {
    FETCH_DBG("BEGIN xCreate/xConnect, argc=%d\n", argc);

    int rc = SQLITE_OK;
    *pp_vtab = (sqlite3_vtab *) Fetch_alloc(pdb, argc, argv);
    if (!(*pp_vtab)) return SQLITE_NOMEM;
    Fetch *vtab = (Fetch *) *pp_vtab;

    rc += sqlite3_declare_vtab(pdb, vtab->schema);
    FETCH_DBG("(%s) Declared virtual schema schema: %s\n", rc == 0 ? "Successfully" : "ERROR", vtab->schema);
    if (rc != SQLITE_OK) {
        free(vtab->request.body);
        free(vtab->request.headers);
        free(vtab->request.url);

        sqlite3_free(vtab);
    }

    FETCH_DBG("END xCreate/xConnect, table \"%s\" set to url = %s\n\n", argv[2], vtab->request.url ? vtab->request.url : "NO_URL");

    return rc;
}

/* To inline index checkers for x_index() */
#define IS_CST_URL_EQ(cst) (cst->iColumn == FETCH_URL && cst->op == SQLITE_INDEX_CONSTRAINT_EQ && cst->usable)
#define IS_CST_HEADERS_EQ(cst) (cst->iColumn == FETCH_HEADERS && cst->op == SQLITE_INDEX_CONSTRAINT_EQ && cst->usable)
/* expected bitmask value of the index_info from xBestIndex */
#define REQUIRED_BITS 0b01

/**
 * Check INDEX_INFO value against @REQUIRED_BITS mask.
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
 * Fetch vtab's @sqlite_module->xBestIndex() callback
 */
static int x_best_index(sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) {
    FETCH_DBG("BEGIN xBestIndex\n");
    int argPos = 1;
    int planMask = 0;
    Fetch *vtab = (Fetch *)pVTab;
    if (vtab->is_dynamic) {
        FETCH_DBG("END xBestIndex, quick case (dynamic vtable): %s\n\n", vtab->request.url);
        pIdxInfo->idxNum = planMask |= 0b01;
        return 0;
    }

    for (int i = 0; i < pIdxInfo->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &pIdxInfo->aConstraint[i];

        if (!cst->usable || cst->op != SQLITE_INDEX_CONSTRAINT_EQ)
            continue;

        struct sqlite3_index_constraint_usage *usage =
            &pIdxInfo->aConstraintUsage[i];

        if (IS_CST_URL_EQ(cst)) {
            usage->omit = 1;
            usage->argvIndex = argPos++;
            planMask |= 0b01;
        }
        if (IS_CST_HEADERS_EQ(cst)) {
            usage->omit = 1;
            usage->argvIndex = argPos++;
            planMask |= 0b10;
        }
    }

    pIdxInfo->idxNum = planMask;

    int rc = check_plan_mask(pIdxInfo, pVTab);
    FETCH_DBG("END xBestIndex, rc=%d\n\n", rc);

    return rc;
}

static int x_disconnect(sqlite3_vtab *pvtab) {
    {
        #ifdef FETCH_DEBUG
            printf("dbg> BEGIN xDisconnect\n");
        #endif
    }

    Fetch *vtab = (Fetch *) pvtab;
    sqlite3_free(pvtab);
    {
        #ifdef FETCH_DEBUG
            printf("dbg> END xDisconnect\n\n");
        #endif
    }
    return SQLITE_OK;
}

static int x_open(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **pp_cursor) {
    FETCH_DBG("BEGIN xOpen\n");
    fetch_cursor_t *cur = sqlite3_malloc(sizeof(fetch_cursor_t));
    if (!cur)
        return SQLITE_NOMEM;
    memset(cur, 0, sizeof(fetch_cursor_t));

    cur->count = 0;

    *pp_cursor = (sqlite3_vtab_cursor *)cur;
    (*pp_cursor)->pVtab = pvtab;
    FETCH_DBG("END xOpen, pvtab=%p\n\n", pvtab);
    return SQLITE_OK;
}

static int x_close(sqlite3_vtab_cursor *cur) {
    #ifdef FETCH_DEBUG
        printf("dbg> BEGIN xClose\n");
    #endif
    fetch_cursor_t *cursor = (fetch_cursor_t *)cur;
    if (cursor)
        sqlite3_free(cursor);
    #ifdef FETCH_DEBUG
        printf("dbg> END xClose\n\n");
    #endif
    return SQLITE_OK;
}

/// API: `sqlite3_vtab.xNext()`
static int x_next(sqlite3_vtab_cursor *pcursor) {
    FETCH_DBG("BEGIN xNext, pcursor->count=%d\n", ((fetch_cursor_t *) pcursor)->count);
    ((fetch_cursor_t *)pcursor)->count++;
    FETCH_DBG("END xNext, pcrusor->count=%d\n\n", ((fetch_cursor_t *) pcursor)->count);
    return SQLITE_OK;
}

/** Populates the Fetch row */
static int x_column(sqlite3_vtab_cursor *pcursor, sqlite3_context *pctx,
                    int icol) {
    fetch_cursor_t *cursor = (fetch_cursor_t *)pcursor;
    Fetch *vtab = (Fetch *)pcursor->pVtab;
    FETCH_DBG("BEGIN xColumn, icol=%d, column_text=%s\n", icol, vtab->columns[icol]);

    if (vtab->is_dynamic) {
        yyjson_val *arr = yyjson_doc_get_root(cursor->elements);
        yyjson_val *el = yyjson_arr_get(arr, cursor->count);
        char *key = vtab->columns[icol];

        yyjson_val *col = yyjson_obj_get(el, key);
        yyjson_type t_col = yyjson_get_type(col);
        if (t_col == YYJSON_TYPE_NONE) {
            sqlite3_result_null(pctx);
        } else if (t_col == YYJSON_TYPE_STR) {
            unsigned long len = 0;
            char *text = yyjson_val_write(col, 0, &len);
            sqlite3_result_text(pctx, text + 1, len - 2, SQLITE_TRANSIENT);
            free(text);
        } else if (t_col == YYJSON_TYPE_NUM) {
            if (yyjson_is_int(col)) {
                sqlite3_result_int(pctx, yyjson_get_int(col));
            } else {
                sqlite3_result_double(pctx, yyjson_get_num(col));
            }
        } else if (t_col == YYJSON_TYPE_BOOL) {
            sqlite3_result_int(pctx, yyjson_get_bool(col));
        }
    } else {
        switch (icol) {
        case FETCH_URL:
            sqlite3_result_text(pctx, (const char *)vtab->response.url, -1, SQLITE_TRANSIENT);
            break;
        case FETCH_HEADERS:
            sqlite3_result_text(pctx, (const char *)vtab->response.headers, -1,
                                SQLITE_TRANSIENT);
            break;
        case FETCH_STATUS:
            sqlite3_result_int(pctx, vtab->response.status);
            break;
        case FETCH_STATUS_TEXT:
            sqlite3_result_text(pctx, fetch_status_text(vtab->response.status), -1,
                                SQLITE_TRANSIENT);
            break;
        case FETCH_OK:
            sqlite3_result_int(pctx, vtab->response.status >= 200 && vtab->response.status < 300);
            break;
        case FETCH_BODY:
            sqlite3_result_text(pctx, vtab->response.body, -1, SQLITE_TRANSIENT);
            break;
        case FETCH_BODY_USED:
            sqlite3_result_int(pctx, 1);
            break;
        case FETCH_REDIRECTED:
            sqlite3_result_int(pctx, vtab->response.redirected);
        }
    }

    FETCH_DBG("END xColumn\n\n");
    return SQLITE_OK;
}

/// API: `sqlite3_vtab.xEof()`
static int x_eof(sqlite3_vtab_cursor *pcursor) {
    FETCH_DBG("BEGIN xEof\n");
    
    fetch_cursor_t *cursor = (fetch_cursor_t *) pcursor;
    bool is_eof = cursor->count >= cursor->n_elements;

    FETCH_DBG("END xEof, cursor->count=%d, cursor->n_elements=%d, is_eof = %d\n\n", cursor->count, cursor->n_elements, is_eof);
    return is_eof;
}

/// API: `sqlite3_vtab.xRowid()`
static int x_rowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    #ifdef FETCH_DEBUG
        printf("dbg> BEGIN xRowid\n");
    #endif
    *prowid = ((fetch_cursor_t *)pcursor)->count;
    #ifdef FETCH_DEBUG
        printf("dbg> END xRowid end\n\n");
    #endif
    return SQLITE_OK;
}

typedef enum {
    FETCH_METHOD_DELETE,
    FETCH_METHOD_GET,
    FETCH_METHOD_PATCH,
    FETCH_METHOD_POST,
    FETCH_METHOD_PUT
} fetch_method_e;

static CURL *fetch_curl_init(const char *url, fetch_method_e method, FILE *req_headers, FILE *req_body, struct curl_slist *res_headers, FILE *res_body) {
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, req_body);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, req_headers);

    switch (method) {
        case FETCH_METHOD_DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case FETCH_METHOD_GET:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
            break;
        case FETCH_METHOD_PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            break;
        case FETCH_METHOD_POST:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            break;
        case FETCH_METHOD_PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            break;
        default:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    }
    return curl;
}

static int x_filter(sqlite3_vtab_cursor *cur, int index, const char *index_str,
                    int argc, sqlite3_value **argv) {
    FETCH_DBG("BEGIN xFilter, argc=%d\n", argc);

    if (!cur || !cur->pVtab || ((index & REQUIRED_BITS) != REQUIRED_BITS)) {
        return SQLITE_ERROR;
    }

    Fetch *vtab = (Fetch *)cur->pVtab;
    fetch_cursor_t *cursor = (fetch_cursor_t *)cur;
    if (!cursor)
        return SQLITE_ERROR;

    const char *request_url = vtab->is_dynamic ? vtab->request.url : (char *) sqlite3_value_text(argv[0]);

    FETCH_DBG("With req_url = %s\n", request_url);

    struct curl_slist *req_headers =
        argc > 1 ? curl_slist_from_headers_json((char *) sqlite3_value_text(argv[1])) : NULL;

    FILE *resp_headers = NULL, *resp_body = NULL;
    char *resp_headers_buf = NULL, *resp_body_buf = NULL;
    size_t resp_headers_size = 0, resp_body_size = 0;

    if (open_response(&resp_headers, &resp_headers_buf, &resp_headers_size, &resp_body, &resp_body_buf, &resp_body_size))
        return SQLITE_NOMEM;

    CURL *curl = fetch_curl_init(request_url, FETCH_METHOD_GET, resp_headers, resp_body, req_headers, NULL);

    curl_easy_perform(curl);

    // Flush bytes to resp_headers and resp_body memory buffers
    fclose(resp_headers);
    fclose(resp_body);
    curl_slist_free_all(req_headers);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &vtab->response.status);
    char *response_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &response_url);
    size_t response_url_len = strlen(response_url);
    if ((response_url_len + 1) > 8096)
        return SQLITE_NOMEM;
    strncpy(vtab->response.url, response_url, response_url_len);
    vtab->response.url[response_url_len] = '\0';

    size_t hdrs_sz = strlen(resp_headers_buf);
    strncpy(vtab->response.headers, resp_headers_buf, hdrs_sz);
    vtab->response.headers[hdrs_sz] = '\0';
    vtab->response.redirected = response_url_len == strlen(request_url)
                          ? strncmp(response_url, request_url, response_url_len)
                          : 0;

    vtab->response.body = resp_body_buf;
    vtab->response.body[resp_body_size] = '\0';

    curl_easy_cleanup(curl);

    if (vtab->is_dynamic) {
        yyjson_doc *doc  = yyjson_read(vtab->response.body, resp_body_size, 0);
        yyjson_val *root = yyjson_doc_get_root(doc);

        assert(yyjson_is_arr(root));

        cursor->elements = doc;
        cursor->n_elements = yyjson_arr_size(root);
        FETCH_DBG("got %d JSON array elements\n", cursor->n_elements);
    } else {
        cursor->n_elements = 1;
    }

    FETCH_DBG("END xFilter\n\n");
    return SQLITE_OK;
};

/* more inline checks */
#define X_UPDATE_IS_DELETE(argc, argv)                                         \
    (argc == 1 && sqlite3_value_type(argv[0]) != SQLITE_NULL)
#define X_UPDATE_IS_INSERT(argc, argv)                                         \
    (argc > 1 && sqlite3_value_type(argv[0]) == SQLITE_NULL)
#define X_UPDATE_IS_UPDATE(argc, argv)                                         \
    (argc > 1 && sqlite3_value_type(argv[0]) != SQLITE_NULL)
#define X_UPDATE_IS_UPDATE_SET_PKEY(argc, argv)                                \
    (argc > 1 && sqlite3_value_type(argv[0]) != SQLITE_NULL &&                 \
     sqlite3_value_type(argv[0]) != sqlite3_value_type(argv[1]))

/**
 * Handles every other query that isn't a `SELECT` query.
 */
static int x_update(sqlite3_vtab *pVTab, int argc, sqlite3_value **argv,
                    sqlite_int64 *pRowid) {
    if (X_UPDATE_IS_INSERT(argc, argv)) {
        if (sqlite3_value_type(argv[FETCH_URL + X_UPDATE_OFFSET]) ==
            SQLITE_NULL) {
            pVTab->zErrMsg =
                sqlite3_mprintf("fetch.xUpdate: no url to INSERT INTO (lol)");
            return SQLITE_MISUSE;
        }

        // open Request streams
        const char *req_url = (char *) sqlite3_value_text(argv[FETCH_URL + X_UPDATE_OFFSET]);
        char *req_body_buf = (char *) sqlite3_value_text(argv[FETCH_BODY + X_UPDATE_OFFSET]);
        FILE *req_body = NULL;
        if (req_body_buf) {
            size_t req_body_size = 0;
            req_body = open_memstream(&req_body_buf, &req_body_size);
            if (!req_body) return SQLITE_NOMEM;
        }
        struct curl_slist *req_headers = curl_slist_from_headers_json((char *) sqlite3_value_text(argv[FETCH_HEADERS + X_UPDATE_OFFSET]));

        FILE *resp_headers, *resp_body;
        char *resp_headers_buf = NULL, *resp_body_buf = NULL;
        size_t resp_headers_size = 0, resp_body_size = 0;
        if (open_response(&resp_headers, &resp_headers_buf, &resp_headers_size, &resp_body, &resp_body_buf, &resp_body_size))
            return SQLITE_NOMEM;

        CURL *curl = fetch_curl_init((const char *) req_url, FETCH_METHOD_POST, resp_headers, resp_body, req_headers, req_body);
        curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }

    if (X_UPDATE_IS_DELETE(argc, argv)) {
        char *url = ((Fetch *) pVTab)->request.url;
    }

    return SQLITE_OK;
}

static sqlite3_module sqlite_fetch = {0,            // iVersion
                                      x_connect,     // xCreate
                                      x_connect,    // xConnect
                                      x_best_index, // xBestIndex
                                      x_disconnect, // xDisconnect
                                      x_disconnect, // xDestroy
                                      x_open,       // xOpen
                                      x_close,      // xClose
                                      x_filter,     // xFilter
                                      x_next,       // xNext
                                      x_eof,        // xEof
                                      x_column,     // xColumn
                                      x_rowid,      // xRowid
                                      x_update,     // xUpdate
                                      NULL,         // xBegin
                                      NULL,         // xSync
                                      NULL,         // xCommit
                                      NULL,         // xRollback
                                      NULL};

// Runtime loadable entry
int sqlite3_fetch_init(sqlite3 *db, char **pzErrMsg,
                       const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    // oh yeah baby
    int rc = sqlite3_create_module(db, "fetch", &sqlite_fetch, 0);
    return rc;
}
