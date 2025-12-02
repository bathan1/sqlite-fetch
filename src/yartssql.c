// Copyright 2025 Nathanael Oh. All Rights Reserved.
#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "yarts.h"
#include "lib/sql.h"

// uncomment to remove all debug prints
#define NDEBUG
#include <assert.h>
#include <asm-generic/errno.h>
#include <unistd.h>
#include <openssl/types.h>
#include <yyjson.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

yyjson_doc *read_next_json_object(FILE *stream, char **errmsg) {
    char *buf = NULL;
    size_t cap = 0;

    ssize_t n = getline(&buf, &cap, stream);
    if (n == -1) {
        if (errmsg) {
            *errmsg = sqlite3_mprintf("fetch: no body");
        }
        free(buf);
        return NULL;
    }

    /* Trim trailing newline ONLY — standard for NDJSON */
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
        n -= 1;
    }

    /* Parse */
    yyjson_doc *doc = yyjson_read(buf, n, 0);
    if (!doc) {
        if (errmsg) {
            *errmsg = sqlite3_mprintf("fetch: invalid json object");
        }
        free(buf);
        return NULL;
    }

    free(buf);
    return doc;
}

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
    size_t columns_len;

    /**
     * Resolved schema string.
     */
    char *schema;
} Fetch;

/// Cursor
typedef struct fetch_cursor {
    sqlite3_vtab_cursor base;
    FILE *stream;
    unsigned int count;
    int eof;

    // Completed row (a fully constructed immutable doc)
    yyjson_doc *next_doc;
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
                          const char *const *argv)
{
    Fetch *vtab = sqlite3_malloc(sizeof(Fetch));
    if (!vtab) {
        fprintf(stderr, "sqlite3_malloc() out of memory\n");
        return NULL;
    }
    memset(vtab, 0, sizeof(Fetch));
    vtab->columns = column_defs_of_declrs(argc, argv, &vtab->columns_len);

    /* max number of tokens valid inside a single xCreate argument for the table declaration */
    int MAX_ARG_TOKENS = 2;
    const char *table_name = argv[2];
    struct string first_line = dynamic("CREATE TABLE %s(url hidden text,headers hidden text,body hidden text,", table_name);
    sqlite3_str *s = sqlite3_str_new(db);
    sqlite3_str_appendall(s, first_line.hd);
    for (int i = 3; i < vtab->columns_len; i++) {
        column_def *def = vtab->columns[i];
        const char *name = def->name.hd;
        const char *typename = def->typename.hd;
        sqlite3_str_appendf(s, "%s %s", def->name.hd, def->typename.hd);

        if (i + 1 < vtab->columns_len)
            sqlite3_str_appendall(s, ",");
    }
    sqlite3_str_appendall(s, ")");
    vtab->schema = sqlite3_str_finish(s);
    free(first_line.hd);

    println("schema: %s", vtab->schema);

    return vtab;
}

static int xConnect(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err) {
    println("xConnect begin, argc = %d\n", argc);
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
    println("xConnect end for table:\n%s\n", vtab->schema);
    return rc;
}

static int xCreate(sqlite3 *pdb, void *paux, int argc,
                     const char *const *argv, sqlite3_vtab **pp_vtab,
                     char **pz_err) {
    // same implementation as xConnect, we just
    // have to point to different fns so this isn't eponymous (can't be called
    // as its own table e.g. SELECT * FROM fetch)
    return xConnect(pdb, paux, argc, argv, pp_vtab, pz_err);
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
        if (!vtab->columns[FETCH_URL]->default_value.hd) {
            pVtab->zErrMsg = sqlite3_mprintf(
                "fetch SELECT needs a `WHERE url = 'something'` clause when no default url is set.\n");
            return SQLITE_ERROR;
        }
    }

    return SQLITE_OK;
}

/**
 * Fetch vtab's sqlite_module->xBestIndex() callback
 */
static int xBestIndex(sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) {
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
static int xDisconnect(sqlite3_vtab *pvtab) {
    println("xDisconnect begin");
    Fetch *vtab = (Fetch *) pvtab;
    for (int i = 0; i < vtab->columns_len; i++) {
        if (vtab->columns[i]) {
            if (vtab->columns[i]->name.hd) {
                free(vtab->columns[i]->name.hd);
                vtab->columns[i]->name.hd = 0;
            }
            if (vtab->columns[i]->typename.hd) {
                free(vtab->columns[i]->typename.hd);
                vtab->columns[i]->typename.hd = 0;
            }
            free(vtab->columns[i]);
            vtab->columns[i] = 0;
        }
    }
    free(vtab->columns);
    vtab->columns = 0;
    vtab->columns_len = 0;

    sqlite3_free(vtab->schema);
    sqlite3_free(pvtab);
    println("xDisconnect end");
    return SQLITE_OK;
}

/**
 * Initialize fetch cursor at P_VTAB's cursor PP_CURSOR with count = 0.
 */
static int xOpen(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **pp_cursor) {
    Fetch *fetch = (Fetch *) pvtab;
    fetch_cursor_t *cur = sqlite3_malloc(sizeof(fetch_cursor_t));
    if (!cur) {
        return SQLITE_NOMEM;
    }
    memset(cur, 0, sizeof(fetch_cursor_t));

    cur->count = 0;

    *pp_cursor = (sqlite3_vtab_cursor *)cur;
    (*pp_cursor)->pVtab = pvtab;
    return SQLITE_OK;
}

static int xClose(sqlite3_vtab_cursor *cur) {
    println("xClose begin");
    fetch_cursor_t *cursor = (fetch_cursor_t *)cur;
    if (cursor) {
        if (cursor->next_doc) {
            yyjson_doc_free(cursor->next_doc);
        }
        if (cursor->stream) {
            fclose(cursor->stream);
        }
        sqlite3_free(cur);
    }
    println("xClose end");
    return SQLITE_OK;
}

static int xNext(sqlite3_vtab_cursor *cur0) {
    fetch_cursor_t *cur = (fetch_cursor_t*)cur0;
    Fetch *vtab = (void*) cur->base.pVtab;

    println("xNext (%u -> %u) begin", cur->count, cur->count + 1);

    // Sanity: next_doc must always contain the row returned previously.
    if (!cur->next_doc) {
        vtab->base.zErrMsg = sqlite3_mprintf(
            "unexpected NULL next_doc in cursor"
        );
        return SQLITE_ERROR;
    }

    yyjson_doc *prev = cur->next_doc;
    char *errmsg = NULL;
    cur->next_doc = read_next_json_object(cur->stream, &errmsg);
    yyjson_doc_free(prev);

    return SQLITE_OK;
}

static void json_bool_result(
    sqlite3_context *pctx,
    column_def *def,
    yyjson_val *column_val
) {
    if (strncmp(def->typename.hd, "int", 3) == 0 || strncmp(def->typename.hd, "float", 5) == 0) {
        sqlite3_result_int(pctx, yyjson_get_bool(column_val));
    } else {
        sqlite3_result_text(pctx, yyjson_get_bool(column_val) ? "true" : "false", -1, SQLITE_TRANSIENT);
    }
}

static yyjson_val *follow_generated_path(
    yyjson_val *root,
    struct string *keys,
    size_t count
) {
    yyjson_val *cur = root;

    for (size_t i = 0; i < count; i++) {
        if (!cur || yyjson_get_type(cur) != YYJSON_TYPE_OBJ) {
            return NULL;
        }

        /* Pass exact byte count to yyjson */
        cur = yyjson_obj_getn(cur, keys[i].hd, keys[i].length);
    }

    return cur;
}

/** Populates the Fetch row */
static int xColumn(sqlite3_vtab_cursor *pcursor,
                    sqlite3_context *pctx,
                    int icol)
{
    // println("xColumn(column = %d)", icol);

    fetch_cursor_t *cursor = (fetch_cursor_t *)pcursor;
    Fetch *vtab = (void *) cursor->base.pVtab;

    if (!cursor->next_doc) {
        fprintf(stderr, "expected a JSON pointer in next_doc but got 0\n");
        return SQLITE_ERROR;
    }

    if (icol < 3) {
        // Skip hidden columns
        return SQLITE_OK;
    }

    column_def *def = vtab->columns[icol];
    yyjson_val *val = yyjson_doc_get_root(cursor->next_doc);

    if (def->generated_always_as_len > 0) {
        val = follow_generated_path(
            val,
            def->generated_always_as,
            def->generated_always_as_len);
    } else {
        val = yyjson_obj_getn(val, def->name.hd, def->name.length);
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

    // println("xColumn end");
    return SQLITE_OK;
}


static int xEof(sqlite3_vtab_cursor *cur) {
    fetch_cursor_t *c = (fetch_cursor_t*)cur;
    int rc = c->next_doc == NULL;
    println("xEof end %s", rc == 1 ? "true" : "false");
    return rc;
}

/// API: `sqlite3_vtab.xRowid()`
static int xRowid(sqlite3_vtab_cursor *pcursor, sqlite3_int64 *prowid) {
    *prowid = ((fetch_cursor_t *)pcursor)->count;
    return SQLITE_OK;
}

static int xFilter(sqlite3_vtab_cursor *cur0,
                    int idxNum, const char *idxStr,
                    int argc, sqlite3_value **argv)
{
    println("xFilter begin");
    Fetch *vtab = (Fetch*)cur0->pVtab;
    fetch_cursor_t *Cur = (fetch_cursor_t*)cur0;

    Cur->eof       = 0;
    Cur->count     = 0;
    Cur->next_doc  = NULL;

    // Extract URL
    if (argc == 0 && !vtab->columns[FETCH_URL]->default_value.hd) {
        cur0->pVtab->zErrMsg =
            sqlite3_mprintf("fetch: need at least 1 argument or default url");
        return SQLITE_ERROR;
    }

    const char *url = (argc > 0)
        ? (const char*)sqlite3_value_text(argv[0])
        : vtab->columns[FETCH_URL]->default_value.hd;

    Cur->stream = fetch(url, (const char *[]){0});

    char *errmsg = NULL;
    Cur->next_doc = read_next_json_object(Cur->stream, &errmsg);

    if (!Cur->next_doc) {
        cur0->pVtab->zErrMsg = errmsg;
        return SQLITE_ERROR;
    }

    println("xFilter end");
    return SQLITE_OK;
}

static sqlite3_module fetch_vtab_module = {
    .iVersion=0,
    .xCreate=xCreate,
    .xConnect=xConnect,
    .xBestIndex=xBestIndex,
    .xDisconnect=xDisconnect,
    .xDestroy=xDisconnect,
    .xOpen=xOpen,
    .xClose=xClose,
    .xFilter=xFilter,
    .xNext=xNext,
    .xEof=xEof,
    .xColumn=xColumn,
    .xRowid=xRowid,
    .xUpdate=NULL,
    .xBegin=NULL,
    .xSync=NULL,
    .xCommit=NULL,
    .xRollback=NULL,
    .xFindFunction=NULL
};

// Runtime loadable entry
int sqlite3_yartssql_init(sqlite3 *db, char **pzErrMsg,
                       const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    // oh yeah baby
    int rc = sqlite3_create_module(db, "fetch", &fetch_vtab_module, 0);
    return rc;
}
