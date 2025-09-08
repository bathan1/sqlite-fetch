// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <curl/curl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "strmap.h"

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
    //
    sqlite3_vtab base;

    uint16_t status;
    char *body;
    char *url;
    char *headers;
    bool redirected;
} Fetch;

typedef struct {
    // |---------------|
    // | Response.body |
    // |---------------|
    char *body;

    // |-------------------|
    // | Response.bodyUsed |
    // |-------------------|
    bool body_used;

    // |------------------|
    // | Response.headers |
    // |------------------|
    char *headers;

    // |-------------|
    // | Response.ok |
    // |-------------|
    bool ok;

    // |---------------------|
    // | Response.redirected |
    // |---------------------|
    bool redirected;

    // |-----------------|
    // | Response.status |
    // |-----------------|
    uint16_t status;

    // |---------------------|
    // | Response.statusText |
    // |---------------------|
    const char *status_text;

    // |---------------|
    // | Response.type |
    // |---------------|
    const char *type;

    // |--------------|
    // | Response.url |
    // |--------------|
    const char *url;
} response_t;

/// Cursor
typedef struct {
    sqlite3_vtab_cursor base;
    int irow;
} fetch_cursor_t;

// #region fetch_schema
#define FETCH_TABLE_SCHEMA                                                     \
    "CREATE TABLE fetch ("                                                     \
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

static char *build_schema_from_args(sqlite3 *db, int argc,
                                    const char *const *argv) {
    sqlite3_str *s = sqlite3_str_new(db);
    if (!s)
        return 0;

    sqlite3_str_appendall(s, "CREATE TABLE x(");

    int colno = 0;
    for (int i = 3; i < argc; i++) {
        sqlite3_str_appendall(s, argv[i]);
        if (i + 1 < argc)
            sqlite3_str_appendall(s, ",");
    }

    sqlite3_str_appendall(s, ")");
    return sqlite3_str_finish(s); /* must be freed with sqlite3_free() */
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
 * CREATE VIRTUAL TABLE "https://sqlite-fetch.dev" using fetch (text as html);
 * SELECT html from "https://sqlite-fetch.dev";
 * ```
 */
static int x_connect(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err) {
    int rc = SQLITE_OK;
    rc += sqlite3_declare_vtab(pdb, FETCH_TABLE_SCHEMA);

    if (rc == SQLITE_OK) {
        *pp_vtab = sqlite3_malloc(sizeof(Fetch));
        if (*pp_vtab == NULL)
            return SQLITE_NOMEM;
        memset(*pp_vtab, 0, sizeof(Fetch));

        Fetch *tbl = (Fetch *)*(pp_vtab);

        // arbitrary upfront byte sizes
        tbl->url = malloc(1024 * 2);
        tbl->headers = malloc(1024 * 4);
        tbl->body = malloc(1024 * 8);
    }
    return rc;
}

// Check if given Sqlite3 index constraint `cst` corresponds
// to a `"resource" = 'somevalue'` clause in tvf call fetch('resourceURL')
// or its full select query equivalent.
static int
is_cst_url_eq(struct sqlite3_index_constraint *cst) {
    return (cst->iColumn == FETCH_URL && cst->op == SQLITE_INDEX_CONSTRAINT_EQ &&
            cst->usable);
}

// expected bitmask value of the index_info from xBestIndex for tvf
#define REQUIRED_BITS 0b01

/// Checks `index_info->idxNum` and sees
static int check_plan_mask(struct sqlite3_index_info *index_info,
                           sqlite3_vtab *pVtab) {
    if ((index_info->idxNum & REQUIRED_BITS) == REQUIRED_BITS)
        return SQLITE_OK;

    int found_resource_constraint = 0;

    for (int i = 0; i < index_info->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &index_info->aConstraint[i];

        if (cst->iColumn == FETCH_URL) {
            // User passed in a "resource" constraint, just not a usable one
            found_resource_constraint = 1;
            if (!cst->usable) {
                return SQLITE_CONSTRAINT;
            }
        }
    }

    if (!found_resource_constraint) {
        pVtab->zErrMsg = sqlite3_mprintf(
            "fetch() incorrect usage: missing 1st argument 'resource'");
        return SQLITE_ERROR;
    }

    return SQLITE_OK;
}

static int x_best_index(sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) {
    int argPos = 1;
    int planMask = 0;

    for (int i = 0; i < pIdxInfo->nConstraint; i++) {
        struct sqlite3_index_constraint *cst = &pIdxInfo->aConstraint[i];

        if (!cst->usable || cst->op != SQLITE_INDEX_CONSTRAINT_EQ)
            continue;

        struct sqlite3_index_constraint_usage *usage =
            &pIdxInfo->aConstraintUsage[i];

        if (is_cst_url_eq(cst)) {
            usage->omit = 1;
            usage->argvIndex = argPos++;
            planMask |= 0b01;
        }
    }

    pIdxInfo->idxNum = planMask;

    return check_plan_mask(pIdxInfo, pVTab);
}

static int x_disconnect(sqlite3_vtab *pvtab) {
    sqlite3_free(pvtab);
    return SQLITE_OK;
}

static int x_open(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **pp_cursor) {
    fetch_cursor_t *cur = sqlite3_malloc(sizeof(fetch_cursor_t));
    if (!cur)
        return SQLITE_NOMEM;
    memset(cur, 0, sizeof(fetch_cursor_t));

    cur->irow = 0;

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

/// API: `sqlite3_vtab.xNext()`
static int x_next(sqlite3_vtab_cursor *pcursor) {
    ((fetch_cursor_t *)pcursor)->irow++;
    return SQLITE_OK;
}

/** Populates the Fetch row */
static int x_column(sqlite3_vtab_cursor *pcursor, sqlite3_context *pctx,
                    int icol) {
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

    return SQLITE_OK;
}

/// API: `sqlite3_vtab.xEof()`
static int x_eof(sqlite3_vtab_cursor *pcursor) {
    return ((fetch_cursor_t *)pcursor)->irow >= 1;
}

/// API: `sqlite3_vtab.xRowid()`
static int x_rowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    *prowid = ((fetch_cursor_t *)pcursor)->irow;
    return SQLITE_OK;
}

typedef enum {
    FETCH_METHOD_DELETE,
    FETCH_METHOD_GET,
    FETCH_METHOD_PATCH,
    FETCH_METHOD_POST,
    FETCH_METHOD_PUT
} fetch_method_e;

static CURL *fetch_curl_init(const char *url, fetch_method_e method, FILE *response_headers, FILE *response_body) {
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_body);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, response_headers);

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
    if (argc < 1 || argc > 2 || !cur || !cur->pVtab || ((index & REQUIRED_BITS) != REQUIRED_BITS))
        return SQLITE_ERROR;

    Fetch *tbl = (Fetch *)cur->pVtab;
    fetch_cursor_t *cursor = (fetch_cursor_t *)cur;
    if (!cursor)
        return SQLITE_ERROR;

    const char *req_url = (char *)sqlite3_value_text(argv[0]);
    strmap_t *headers =
        argc > 1 ? sqlite3_value_pointer(argv[1], "Headers") : NULL;

    // mem2 -- fetch() response memory buffers
    char *__resp_headers = malloc(1);
    size_t resp_headers_size = 0;
    FILE *resp_headers = open_memstream(&__resp_headers, &resp_headers_size);

    if (!resp_headers)
        return ENOMEM;

    char *__resp_body = malloc(1);
    size_t resp_body_size = 0;
    FILE *resp_body = open_memstream(&__resp_body, &resp_body_size);
    // end mem2

    if (!resp_body) {
        fclose(resp_headers);
        return ENOMEM;
    }

    CURL *curl = fetch_curl_init(req_url, FETCH_METHOD_GET, resp_headers, resp_body);
    curl_easy_perform(curl);

    // Write out bytes to __resp_headers and __resp_body
    fclose(resp_headers);
    fclose(resp_body);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &tbl->status);
    char *resp_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &resp_url);
    size_t resp_url_sz = strlen(resp_url);
    if ((resp_url_sz + 1) > 8096)
        return SQLITE_NOMEM;
    strncpy(tbl->url, resp_url, resp_url_sz);
    tbl->url[resp_url_sz] = '\0';

    size_t hdrs_sz = strlen(__resp_headers);
    strncpy(tbl->headers, __resp_headers, hdrs_sz);
    tbl->headers[hdrs_sz] = '\0';

    tbl->redirected = resp_url_sz == strlen(req_url)
                          ? strncmp(resp_url, req_url, resp_url_sz)
                          : 0;

    tbl->body = __resp_body;
    tbl->body[resp_body_size] = '\0';

    curl_easy_cleanup(curl);

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

static int x_update(sqlite3_vtab *pVTab, int argc, sqlite3_value **argv,
                    sqlite_int64 *pRowid) {
    if (X_UPDATE_IS_INSERT(argc, argv)) {
        if (sqlite3_value_type(argv[FETCH_URL + X_UPDATE_OFFSET]) ==
            SQLITE_NULL) {
            pVTab->zErrMsg =
                sqlite3_mprintf("fetch.xUpdate: no url to INSERT INTO (lol)");
            return SQLITE_MISUSE;
        }
        const unsigned char *url = sqlite3_value_text(argv[FETCH_URL + X_UPDATE_OFFSET]);
        printf("%s\n", url);

        // request_t post = {.url = sqlite3_value_text(argv[FETCH_COLUMN_URL]),
        //                   .method = "POST"};
        // if (sqlite3_value_type(argv[FETCH_COLUMN_BODY]) != SQLITE_NULL) {
        //     const char *req_body =
        //     sqlite3_value_blob(argv[FETCH_COLUMN_BODY]); size_t req_body_sz =
        //     sqlite3_value_bytes(argv[FETCH_COLUMN_BODY]); post.body =
        //     open_memstream((char **) &req_body, &req_body_sz);
        // }
        response_t response;
    }
    return SQLITE_OK;
}

static sqlite3_module sqlite_fetch = {0,            // iVersion
                                      NULL,         // xCreate
                                      x_connect,    // xConnect
                                      x_best_index, // xBestIndex
                                      x_disconnect, // xDisconnect
                                      NULL,         // xDestroy
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
