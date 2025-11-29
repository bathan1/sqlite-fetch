/**
 * @defgroup functions Functions
 * @brief All public functions available to `yarts.c`
 */

/**
 * @defgroup types Types & Structs
 * @brief All user-defined types available to `yarts.c`
 */

#pragma once

#include <errno.h>
#include <stddef.h>
#include <string.h>

/** Sets {@link errno} to ERROR_CODE so you don't have to every time. */
static void *null(int error_code) {
    errno = error_code;
    return NULL;
}

static long long zero (int error_code) {
    return (long long) null(error_code);
}

static long long neg1 (int error_code) {
    return zero(error_code) - 1;
}

