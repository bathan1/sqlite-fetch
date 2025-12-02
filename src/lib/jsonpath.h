/**
 * @file jsonpath.h
 * @brief [jsoncons](https://danielaparker.github.io/jsoncons/) wrapper for C.
 */

#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Walk PATH on JSON_BUF and write out result(s) to OUT_ITEMS.
 *
 * Given raw JSON text and a JSONPath expression, returns an array 
 * of JSON strings (each malloc'ed).
 *
 * Caller must:
 *   - free(each char*)
 *   - free(out_items)
 *
 * @retval 0 OK
 * @retval 1 Error
 */
int jsonpath_walk(
    const char *json_buf,
    const char *path,
    char ***out_items,
    size_t *out_count
);

#ifdef __cplusplus
}
#endif

