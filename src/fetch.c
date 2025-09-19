// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include <ctype.h>
#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#define FETCH_DEBUG

#include <yyjson.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <uthash.h>

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
int open_response(
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
    bool is_dynamic;
    char *schema;

    struct {
        char *buffer;
        size_t size;
    } *columns;
    size_t columns_size;

    uint16_t status;
    char *body;
    char *url;
    char *headers;
    bool redirected;
} Fetch;

/// Cursor
typedef struct {
    sqlite3_vtab_cursor base;
    int count;
} fetch_cursor_t;

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

#define X_UPDATE_OFFSET 2

#define FETCH_BODY 0
#define FETCH_BODY_USED 1
#define FETCH_OK 2
#define FETCH_REDIRECTED 3
#define FETCH_STATUS 4
#define FETCH_STATUS_TEXT 5
#define FETCH_TYPE 6
#define FETCH_URL 7
#define FETCH_HEADERS 8

static void trim_slice(const char **begin, const char **end) {
    // *begin .. *end is half-open, end points one past last char
    while (*begin < *end && isspace((unsigned char)**begin)) (*begin)++;
    while (*end > *begin && isspace((unsigned char)*((*end) - 1))) (*end)--;
}

static char *trim(sqlite3 *db, const char *b, const char *e) {
    const ptrdiff_t len = e - b;
    char *out = (char *)malloc((int)len + 1);
    if (!out) return NULL;
    memcpy(out, b, (size_t)len);
    out[len] = '\0';
    return out;
}

/* For xCreate options arguments */
#define IS_KEY_URL(key) (strncmp("url", key, 3))

/** @sqlite3_malloc() a @Fetch buffer for DB */
static Fetch *Fetch_alloc(sqlite3 *db, int argc,
                             const char *const *argv) {
    Fetch *vtab = sqlite3_malloc(sizeof(Fetch));
    if (!vtab)
        return NULL;
    memset(vtab, 0, sizeof(Fetch));
    
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

        char *key   = trim(db, kbeg, kend);
        char *value = trim(db, vbeg, vend);
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
            vtab->url = value;
        }
    }

    // vtab flag if using a plain Response schema, or user provided
    vtab->is_dynamic = schema_i < argc;

    sqlite3_str *s = sqlite3_str_new(db);
    if (!s) {
        if (vtab->url) free(vtab->url);
        sqlite3_free(vtab);
        return NULL;
    }

    if (vtab->is_dynamic) {
        sqlite3_str_appendall(s, "CREATE TABLE x(");

        int colno = 0;
        for (int i = schema_i; i < argc; i++) {
            sqlite3_str_appendall(s, argv[i]);
            if (i + 1 < argc)
                sqlite3_str_appendall(s, ",");
        }
        sqlite3_str_appendall(s, ")");
    } else {
        sqlite3_str_appendall(s, FETCH_TABLE_SCHEMA);
        vtab->url = malloc(1 << 16);
        vtab->body = malloc(1 << 16);
        vtab->headers = malloc(1 << 16);
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
    {
        #ifdef FETCH_DEBUG
            printf("dbg> BEGIN xCreate/xConnect, argc=%d\n", argc);
        #endif
    }

    int rc = SQLITE_OK;
    *pp_vtab = (sqlite3_vtab *) Fetch_alloc(pdb, argc, argv);
    if (!(*pp_vtab)) return SQLITE_NOMEM;
    Fetch *vtab = (Fetch *) *pp_vtab;

    rc += sqlite3_declare_vtab(pdb, vtab->schema);
    if (rc != SQLITE_OK) {
        free(vtab->body);
        free(vtab->headers);
        free(vtab->url);

        sqlite3_free(vtab);
    }

    {
        #ifdef FETCH_DEBUG
            printf("dbg> END xCreate/xConnect, rc=%d, schema=%s\n, vtab=%p\n\n", rc, vtab->schema, vtab);
        #endif
    }

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

static int x_best_index(sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) {
    {
        #ifdef FETCH_DEBUG
            printf("dbg> BEGIN xBestIndex\n");
        #endif
    }
    int argPos = 1;
    int planMask = 0;

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

    {
        #ifdef FETCH_DEBUG
            printf("dbg> END xBestIndex, rc=%d\n\n", rc);
        #endif
    }

    return rc;
}

static int x_disconnect(sqlite3_vtab *pvtab) {
    {
        #ifdef FETCH_DEBUG
            printf("dbg> BEGIN xDisconnect\n");
        #endif
    }

    Fetch *vtab = (Fetch *) pvtab;

    free(vtab->body);
    free(vtab->headers);
    free(vtab->url);

    sqlite3_free(pvtab);
    {
        #ifdef FETCH_DEBUG
            printf("dbg> END xDisconnect\n\n");
        #endif
    }
    return SQLITE_OK;
}

static int x_open(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **pp_cursor) {
    #ifdef FETCH_DEBUG
        printf("dbg> BEGIN xOpen\n");
    #endif
    fetch_cursor_t *cur = sqlite3_malloc(sizeof(fetch_cursor_t));
    if (!cur)
        return SQLITE_NOMEM;
    memset(cur, 0, sizeof(fetch_cursor_t));

    cur->count = 0;

    *pp_cursor = (sqlite3_vtab_cursor *)cur;
    (*pp_cursor)->pVtab = pvtab;
    #ifdef FETCH_DEBUG
        printf("dbg> END xOpen, pvtab=%p\n\n", pvtab);
    #endif
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
    #ifdef FETCH_DEBUG
        printf("dbg> BEGIN xNext\n");
    #endif
    ((fetch_cursor_t *)pcursor)->count++;
    #ifdef FETCH_DEBUG
        printf("dbg> END xNext\n");
    #endif
    return SQLITE_OK;
}

/** Populates the Fetch row */
static int x_column(sqlite3_vtab_cursor *pcursor, sqlite3_context *pctx,
                    int icol) {
    {
        #ifdef FETCH_DEBUG
            printf("dbg> BEGIN xColumn\n");
        #endif
    }

    fetch_cursor_t *cursor = (fetch_cursor_t *)pcursor;
    Fetch *tbl = (Fetch *)pcursor->pVtab;

    switch (icol) {
    case FETCH_URL:
        sqlite3_result_text(pctx, (const char *)tbl->url, -1, SQLITE_TRANSIENT);
        break;
    case FETCH_HEADERS:
        sqlite3_result_text(pctx, (const char *)tbl->headers, -1,
                            SQLITE_TRANSIENT);
        break;
    case FETCH_STATUS:
        sqlite3_result_int(pctx, tbl->status);
        break;
    case FETCH_STATUS_TEXT:
        sqlite3_result_text(pctx, fetch_status_text(tbl->status), -1,
                            SQLITE_TRANSIENT);
        break;
    case FETCH_OK:
        sqlite3_result_int(pctx, tbl->status >= 200 && tbl->status < 300);
        break;
    case FETCH_BODY:
        sqlite3_result_text(pctx, tbl->body, -1, SQLITE_TRANSIENT);
        break;
    case FETCH_BODY_USED:
        sqlite3_result_int(pctx, 1);
        break;
    case FETCH_REDIRECTED:
        sqlite3_result_int(pctx, tbl->redirected);
    }

    #ifdef FETCH_DEBUG
        printf("dbg> END xColumn\n\n");
    #endif
    return SQLITE_OK;
}

/// API: `sqlite3_vtab.xEof()`
static int x_eof(sqlite3_vtab_cursor *pcursor) {
    #ifdef FETCH_DEBUG
        printf("dbg> BEGIN xEof\n");
    #endif
    bool is_eof = ((fetch_cursor_t *)pcursor)->count >= 1;
    #ifdef FETCH_DEBUG
        printf("dbg> END xEof, is_eof = %d\n\n", is_eof);
    #endif
    return is_eof;
}

/// API: `sqlite3_vtab.xRowid()`
static int x_rowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    #ifdef FETCH_DEBUG
        printf("dbg> xRowid start\n");
    #endif
    *prowid = ((fetch_cursor_t *)pcursor)->count;
    #ifdef FETCH_DEBUG
        printf("dbg> xRowid end\n");
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
    {
        #ifdef FETCH_DEBUG
            printf("dbg> BEGIN xFilter\n");
        #endif
    }

    if (argc != 1 || !cur || !cur->pVtab || ((index & REQUIRED_BITS) != REQUIRED_BITS))
        return SQLITE_ERROR;

    Fetch *tbl = (Fetch *)cur->pVtab;
    fetch_cursor_t *cursor = (fetch_cursor_t *)cur;
    if (!cursor)
        return SQLITE_ERROR;

    const char *req_url = (char *) sqlite3_value_text(argv[0]);

    {
        #ifdef FETCH_DEBUG
            printf("dbg> With req_url = %s\n", req_url);
        #endif
    }

    struct curl_slist *req_headers =
        argc > 1 ? curl_slist_from_headers_json((char *) sqlite3_value_text(argv[1])) : NULL;

    FILE *resp_headers = NULL, *resp_body = NULL;
    char *resp_headers_buf = NULL, *resp_body_buf = NULL;
    size_t resp_headers_size = 0, resp_body_size = 0;

    if (open_response(&resp_headers, &resp_headers_buf, &resp_headers_size, &resp_body, &resp_body_buf, &resp_body_size))
        return SQLITE_NOMEM;

    CURL *curl = fetch_curl_init(req_url, FETCH_METHOD_GET, resp_headers, resp_body, req_headers, NULL);

    curl_easy_perform(curl);

    // Flush bytes to resp_headers and resp_body memory buffers
    fclose(resp_headers);
    fclose(resp_body);
    curl_slist_free_all(req_headers);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &tbl->status);
    char *response_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &response_url);
    size_t response_url_len = strlen(response_url);
    if ((response_url_len + 1) > 8096)
        return SQLITE_NOMEM;
    strncpy(tbl->url, response_url, response_url_len);
    tbl->url[response_url_len] = '\0';

    size_t hdrs_sz = strlen(resp_headers_buf);
    strncpy(tbl->headers, resp_headers_buf, hdrs_sz);
    tbl->headers[hdrs_sz] = '\0';

    tbl->redirected = response_url_len == strlen(req_url)
                          ? strncmp(response_url, req_url, response_url_len)
                          : 0;

    tbl->body = resp_body_buf;
    tbl->body[resp_body_size] = '\0';

    curl_easy_cleanup(curl);

    {
        #ifdef FETCH_DEBUG
            printf("dbg> END xFilter\n\n");
        #endif
    }
    return SQLITE_OK;
};

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
        char *url = ((Fetch *) pVTab)->url;
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
