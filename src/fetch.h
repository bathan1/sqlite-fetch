#ifndef SQLITE_FETCH_H
#define SQLITE_FETCH_H

#include <stdbool.h>
#include <stdint.h>

/**
 * The Response struct the fetch() implementation returns
 */
typedef struct {
    char *body;
    bool body_used;

    /** 
     * https://developer.mozilla.org/en-US/docs/Web/API/Headers
     *
     * Should be malloced or dynamically allocated otherwise, so that the @fetch() CALLER is in charge of freeing the @headers buffer
     */
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
    char *status_text;

    // |---------------|
    // | Response.type |
    // |---------------|
    char *type;

    // |--------------|
    // | Response.url |
    // |--------------|
    char *url;
} response_t;

#endif
